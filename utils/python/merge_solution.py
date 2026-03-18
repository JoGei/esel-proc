#!/usr/bin/env python3
# utils/python/merge_solution.py

import argparse
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--template", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument(
        "--module",
        action="append",
        default=[],
        help="NAME=path/to/file.sv"
    )
    args = ap.parse_args()

    text = Path(args.template).read_text()

    for item in args.module:
        key, path = item.split("=", 1)
        contents = Path(path).read_text()
        if path[-2:] in [".c", ".h", ".cc", ".cpp", ".hpp"]:
            contents = f"/*\n{contents}\n*/"

        replacement = f"// ---- BEGIN {key} ----\n{contents}\n// ---- END {key} ----"
        text = text.replace(f"@{key}@", replacement)

    Path(args.output).write_text(text)

if __name__ == "__main__":
    main()