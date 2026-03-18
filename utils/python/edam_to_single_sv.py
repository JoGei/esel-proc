#!/usr/bin/env python3
# utils/python/edam_to_single_sv.py

import argparse
from pathlib import Path
import yaml

VERILOG_TYPES = {
    "verilogSource",
    "systemVerilogSource",
    "verilogSource-2005",
    "systemVerilogSource-2005",
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--edam", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    edam_path = Path(args.edam)
    out_path = Path(args.out)

    with edam_path.open("r") as f:
        edam = yaml.safe_load(f)

    files = edam.get("files", [])
    selected = []

    for entry in files:
        name = entry.get("name")
        file_type = entry.get("file_type")
        is_include_file = entry.get("is_include_file", False)

        if not name or file_type not in VERILOG_TYPES or is_include_file:
            continue

        p = Path(name)
        if not p.is_absolute():
            p = (edam_path.parent / p).resolve()

        selected.append(p)

    if not selected:
        raise SystemExit("ERROR: No Verilog/SystemVerilog source files found in EDAM")

    merged = []
    seen = set()

    for path in selected:
        if path in seen:
            continue
        seen.add(path)

        if not path.exists():
            raise SystemExit(f"ERROR: Source file listed in EDAM not found: {path}")

        merged.append(f"// ---- BEGIN ----\n")
        merged.append(path.read_text())
        if not merged[-1].endswith("\n"):
            merged.append("\n")
        merged.append(f"// ---- END ----\n\n")

    out_path.write_text("".join(merged))

if __name__ == "__main__":
    main()