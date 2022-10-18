#!/usr/bin/env python3
import os
import re
import sys
import shutil
import pathlib
import subprocess
from typing import Dict, List

__dir__ = os.path.dirname(os.path.abspath(__file__))


def mkdirp(path):
    return pathlib.Path(path).mkdir(parents=True, exist_ok=True)


def parse_ldd(output: str) -> Dict[str, str]:
    ldd_line_rx = re.compile(
        r"^\s*([^=\s]+)\s*(?:=> ([^\(\)\s]+)?)?\s*\((0x[0-9A-Fa-f]+)\)\s*$"
    )
    dlls = {}
    for line in output.splitlines():
        match = ldd_line_rx.match(line)
        if match is None:
            continue
        wanted = match[1]
        found = match[2]

        dlls[wanted] = found

    return dlls


def get_excludes() -> List[str]:
    excludelist_path = os.path.join(__dir__, "excludelist")
    excluded_dlls = []
    for line in open(excludelist_path):
        try:
            comment_start = line.index("#")
            line = line[:comment_start]
        except ValueError:
            pass
        line = line.rstrip()
        if line == "":
            continue
        excluded_dlls.append(line)
    return excluded_dlls


def copy_libs(dlls: Dict[str, str], to: str):
    try:
        shutil.rmtree(to)
    except FileNotFoundError:
        pass
    mkdirp(to)

    for wanted, target in dlls.items():
        if target is None:
            continue
        dst = os.path.join(to, wanted)
        shutil.copyfile(target, dst)
        print(f"Copied {target} to {dst}.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path/to/binary>", file=sys.stderr)
        exit(os.EX_USAGE)

    binary = os.path.abspath(sys.argv[1])
    binary_dir = os.path.dirname(binary)
    output_dir = os.path.join(binary_dir, "shared_libs")

    ldd_out = subprocess.check_output(["ldd", binary]).decode("utf8")
    all_dlls = parse_ldd(ldd_out)
    excluded_dlls = get_excludes()
    dlls = {k: v for k, v in all_dlls.items() if k not in excluded_dlls}
    copy_libs(dlls, output_dir)
