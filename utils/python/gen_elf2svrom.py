#!/usr/bin/env bash
#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import subprocess
from dataclasses import dataclass
from math import ceil, log2
from pathlib import Path

from elftools.elf.elffile import ELFFile


SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4


@dataclass(frozen=True)
class RomSection:
    name: str
    vma: int
    lma: int
    data: bytes
    executable: bool

    @property
    def size(self) -> int:
        return len(self.data)

    @property
    def vma_end(self) -> int:
        return self.vma + self.size

    @property
    def lma_end(self) -> int:
        return self.lma + self.size


@dataclass(frozen=True)
class RomImage:
    base_lma: int
    image: bytes
    sections: tuple[RomSection, ...]

    @property
    def size_bytes(self) -> int:
        return len(self.image)


def _containing_load_segment(segments, section):
    sh_off = int(section["sh_offset"])
    sh_size = int(section["sh_size"])
    if sh_size == 0:
        return None

    for seg in segments:
        p_off = int(seg["p_offset"])
        p_filesz = int(seg["p_filesz"])
        if p_off <= sh_off and (sh_off + sh_size) <= (p_off + p_filesz):
            return seg
    return None


def _collect_rom_sections(elf_path: str) -> tuple[RomSection, ...]:
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)

        load_segments = [
            seg
            for seg in elf.iter_segments()
            if seg["p_type"] == "PT_LOAD" and int(seg["p_filesz"]) > 0
        ]
        if not load_segments:
            raise ValueError("ELF contains no non-empty PT_LOAD segments")

        rom_sections: list[RomSection] = []

        for sec in elf.iter_sections():
            sh_flags = int(sec["sh_flags"])
            sh_size = int(sec["sh_size"])
            sh_type = sec["sh_type"]

            if sh_size == 0:
                continue
            if not (sh_flags & SHF_ALLOC):
                continue
            if sh_type == "SHT_NOBITS":
                continue

            seg = _containing_load_segment(load_segments, sec)
            if seg is None:
                continue

            vma = int(sec["sh_addr"])
            lma = int(seg["p_paddr"]) + (int(sec["sh_offset"]) - int(seg["p_offset"]))

            rom_sections.append(
                RomSection(
                    name=sec.name,
                    vma=vma,
                    lma=lma,
                    data=sec.data(),
                    executable=bool(sh_flags & SHF_EXECINSTR),
                )
            )

        if not rom_sections:
            raise ValueError("ELF contains no ROM-resident allocated sections")

        return tuple(rom_sections)


def _matches_section_kind(section: RomSection, section_kind: str) -> bool:
    if section_kind == "all":
        return True
    if section_kind == "exec":
        return section.executable
    if section_kind == "nonexec":
        return not section.executable
    raise ValueError(f"Unsupported section kind: {section_kind}")


def build_rom_image(
    elf_path: str,
    data_width: int = 32,
    section_kind: str = "all",
) -> RomImage:
    if data_width % 8 != 0:
        raise ValueError("data_width must be a multiple of 8")

    word_bytes = data_width // 8
    sections = tuple(
        sec
        for sec in _collect_rom_sections(elf_path)
        if _matches_section_kind(sec, section_kind)
    )

    if not sections:
        return RomImage(base_lma=0, image=bytes(word_bytes), sections=())

    base_lma = min(sec.lma for sec in sections)
    end_lma = max(sec.lma_end for sec in sections)

    rom_bytes = end_lma - base_lma
    rom_bytes = max(word_bytes, ceil(rom_bytes / word_bytes) * word_bytes)

    image = bytearray(rom_bytes)

    for sec in sections:
        start = sec.lma - base_lma
        end = start + sec.size

        old = image[start:end]
        new = sec.data
        for i, (a, b) in enumerate(zip(old, new)):
            if a != 0 and a != b:
                raise ValueError(
                    f"Overlapping ROM bytes with different contents in section "
                    f"{sec.name} at ROM offset 0x{start + i:x}"
                )

        image[start:end] = new

    return RomImage(base_lma=base_lma, image=bytes(image), sections=sections)


def _parse_objdump_disassembly(text: str) -> dict[int, str]:
    mapping: dict[int, str] = {}
    line_re = re.compile(
        r"^\s*([0-9a-fA-F]+):\s+(?:[0-9a-fA-F]{2,8}\s+)+(.+?)\s*$"
    )

    for line in text.splitlines():
        m = line_re.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        asm = m.group(2).strip()
        mapping[addr] = asm

    return mapping


def _run_objdump(
    elf_path: str,
    objdump: str = "riscv32-unknown-elf-objdump",
) -> dict[int, str]:
    cmd = [objdump, "-d", "-z", "-M", "no-aliases", elf_path]
    proc = subprocess.run(
        cmd,
        check=True,
        capture_output=True,
        text=True,
    )
    return _parse_objdump_disassembly(proc.stdout)


def _build_word_annotations(
    rom: RomImage,
    data_width: int,
    disasm_by_vma: dict[int, str] | None,
) -> dict[int, str]:
    if not disasm_by_vma:
        return {}

    word_bytes = data_width // 8
    annotations: dict[int, str] = {}

    for sec in rom.sections:
        if not sec.executable:
            continue

        for vma, asm in disasm_by_vma.items():
            if not (sec.vma <= vma < sec.vma_end):
                continue

            lma = sec.lma + (vma - sec.vma)
            rom_offset = lma - rom.base_lma

            if rom_offset % word_bytes != 0:
                continue

            word_index = rom_offset // word_bytes
            if word_index not in annotations:
                annotations[word_index] = asm

    return annotations


def emit_sv_rom_from_elf(
    elf_path: str,
    sv_path: str,
    module_name: str = "eselproc_wb_rom",
    data_width: int = 32,
    section_kind: str = "all",
    annotate_asm: bool = False,
    objdump: str = "riscv32-unknown-elf-objdump",
) -> None:
    rom = build_rom_image(
        elf_path=elf_path,
        data_width=data_width,
        section_kind=section_kind,
    )

    disasm_by_vma = _run_objdump(elf_path, objdump=objdump) if annotate_asm else None
    annotations = _build_word_annotations(
        rom=rom,
        data_width=data_width,
        disasm_by_vma=disasm_by_vma,
    )

    word_bytes = data_width // 8
    depth = rom.size_bytes // word_bytes
    addr_width = max(1, ceil(log2(depth)))
    hex_digits = data_width // 4

    words = [
        int.from_bytes(rom.image[i:i + word_bytes], byteorder="little", signed=False)
        for i in range(0, rom.size_bytes, word_bytes)
    ]

    init_lines: list[str] = []

    def lma_to_vma(lma: int):
        for sec in rom.sections:
            if sec.lma <= lma < sec.lma_end:
                return sec.vma + (lma - sec.lma)
        return None

    for addr, word in enumerate(words):
        rom_offset = addr * word_bytes
        lma = rom.base_lma + rom_offset
        vma = lma_to_vma(lma)

        comment_parts = [f"0x{(vma if vma is not None else lma):08x}"]
        if addr in annotations:
            comment_parts.append(annotations[addr])

        comment = " // " + ": ".join(comment_parts)
        init_lines.append(
            f"        mem[{addr}] = {data_width}'h{word:0{hex_digits}x};{comment}"
        )

    inits = "\n".join(init_lines)

    sv = f"""module {module_name} #(
    parameter BASE_ADDR = 32'h{rom.base_lma:08x},
    parameter MEM_WORDS = {depth}
) (
    input  wire        clk,
    input  wire        reset,

    input  wire        i_wb_cyc,
    input  wire        i_wb_stb,
    input  wire        i_wb_we,
    input  wire [3:0]  i_wb_sel,
    input  wire [31:0] i_wb_adr,
    input  wire [31:0] i_wb_dat,
    output wire [31:0] o_wb_dat,
    output wire        o_wb_ack
);

    reg [31:0] mem [0:MEM_WORDS-1];
    integer i;

    wire        wb_req;
    wire [31:0] word_addr_full;
    wire        addr_valid;

    assign wb_req         = i_wb_cyc && i_wb_stb;
    assign word_addr_full = (i_wb_adr - BASE_ADDR) >> 2;
    assign addr_valid     = (word_addr_full < MEM_WORDS);

    initial begin
        for (i = 0; i < MEM_WORDS; i = i + 1)
            mem[i] = 32'h00000000;
{inits}
    end

    assign o_wb_dat = (!i_wb_we && addr_valid) ? mem[word_addr_full] : 32'h00000000;
    assign o_wb_ack = wb_req;

endmodule
"""
    Path(sv_path).write_text(sv)


def build_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate a Wishbone-compatible SystemVerilog ROM from an ELF file."
    )
    parser.add_argument("elf", help="Input ELF file")
    parser.add_argument("sv", help="Output SystemVerilog file")
    parser.add_argument(
        "--module-name",
        default="boot_rom",
        help="Generated SystemVerilog module name",
    )
    parser.add_argument(
        "--data-width",
        type=int,
        default=32,
        help="ROM word width in bits (must be 32 for this Wishbone wrapper)",
    )
    parser.add_argument(
        "--section-kind",
        choices=("all", "exec", "nonexec"),
        default="all",
        help="Which allocated ROM-backed ELF sections to emit",
    )
    parser.add_argument(
        "--annotate-asm",
        action="store_true",
        help="Annotate ROM entries with RISC-V disassembly comments",
    )
    parser.add_argument(
        "--objdump",
        default="riscv32-unknown-elf-objdump",
        help="Path to GNU objdump for disassembly annotation",
    )
    return parser


def main() -> int:
    parser = build_argparser()
    args = parser.parse_args()

    if args.data_width != 32:
        parser.exit(status=1, message="error: this generator currently requires --data-width 32\n")

    try:
        emit_sv_rom_from_elf(
            elf_path=args.elf,
            sv_path=args.sv,
            module_name=args.module_name,
            data_width=args.data_width,
            section_kind=args.section_kind,
            annotate_asm=args.annotate_asm,
            objdump=args.objdump,
        )
    except Exception as exc:
        parser.exit(status=1, message=f"error: {exc}\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())


'''
#!/usr/bin/env python3
elf2svrom.py firmware.elf boot_rom.sv
elf2svrom.py firmware.elf boot_rom.sv --annotate-asm
elf2svrom.py firmware.elf boot_rom.sv --annotate-asm --objdump riscv64-unknown-elf-objdump
'''

if __name__ == "__main__":
    raise SystemExit(main())
