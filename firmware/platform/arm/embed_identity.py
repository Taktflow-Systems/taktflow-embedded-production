#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B — embed an ECU identity TOML into a flash-resident C header.

Background
----------
Each ECU loads its VIN and ECU short-name from an `<ecu>_identity.toml`
config file so that no firmware C source inlines the VIN literal (a
regression scanner under tests/phase4/test_no_hardcoded_vin_in_src.py
enforces this rule).

On POSIX builds, main.c reads the file from disk. On ARM builds there
is no filesystem, so this script generates a header that embeds the
TOML file contents as a const byte array. main.c then calls
<Ecu>_Identity_InitFromBuffer() with that array on the !PLATFORM_POSIX
path.

The byte array is emitted as decimal/hex integers (e.g. `0x54, 0x41, ...`).
The hardcoded-VIN regression test scans for the contiguous 17-character
ISO 3779 VIN shape `[A-HJ-NPR-Z0-9]{17}` and the exact VIN literal — a
hex byte stream cannot match either pattern, so the generated header is
safe even without an explicit exclusion. As an additional belt-and-braces
guard the header lives entirely under the build/ tree, which the
regression scanner skips by directory name.

Usage
-----
    python embed_identity.py <input.toml> <output.h> [--namespace NAME]

NAME defaults to "cvc_identity" so the pre-Phase 5 D7 call sites keep
working unchanged.  Pass --namespace fzc_identity for FZC, and so on.

The generated header defines:
    static const unsigned char <NAME>_toml_data[<N>];
    static const unsigned long  <NAME>_toml_len = <N>;
with an upper-case include guard derived from NAME.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _emit_header(
    blob: bytes,
    src_posix: str,
    namespace: str,
) -> str:
    """Render the C header text for the supplied namespace."""
    guard = namespace.upper() + "_DATA_H"
    data_sym = namespace + "_toml_data"
    len_sym = namespace + "_toml_len"

    lines: list[str] = []
    lines.append("/* AUTO-GENERATED -- do not edit. */")
    lines.append("/* Source: " + src_posix + " */")
    lines.append("/* Generator: firmware/platform/arm/embed_identity.py */")
    lines.append("#ifndef " + guard)
    lines.append("#define " + guard)
    lines.append("")
    lines.append("static const unsigned char " + data_sym + "[] = {")

    # 12 bytes per row keeps the diff readable when the TOML is updated.
    row: list[str] = []
    for i, b in enumerate(blob):
        row.append("0x{:02x}".format(b))
        if (i + 1) % 12 == 0:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")
    lines.append("")
    lines.append(
        "static const unsigned long " + len_sym + " = "
        + str(len(blob))
        + "u;"
    )
    lines.append("")
    lines.append("#endif /* " + guard + " */")
    lines.append("")

    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Embed an ECU identity TOML into a flash-resident C header",
    )
    parser.add_argument("input", help="path to <ecu>_identity.toml")
    parser.add_argument("output", help="path to generated C header")
    parser.add_argument(
        "--namespace",
        default="cvc_identity",
        help="C symbol prefix. Defaults to cvc_identity for backwards "
             "compatibility with pre-D7 Makefile wiring.",
    )
    args = parser.parse_args(argv[1:])

    in_path = Path(args.input)
    out_path = Path(args.output)

    if not in_path.is_file():
        print(f"embed_identity: input not found: {in_path}", file=sys.stderr)
        return 1

    blob = in_path.read_bytes()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    header = _emit_header(
        blob=blob,
        src_posix=in_path.as_posix(),
        namespace=args.namespace,
    )
    out_path.write_text(header, encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
