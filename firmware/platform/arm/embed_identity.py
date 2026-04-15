#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up — embed CVC identity TOML into a flash-resident
C header.

Background
----------
The CVC ECU loads its VIN and ECU short-name from `cvc_identity.toml` so
that no firmware C source inlines the VIN literal (a regression scanner
under tests/phase4/test_no_hardcoded_vin_in_src.py enforces this rule).
On the POSIX build, main.c reads the file from disk via
Cvc_Identity_InitFromFile(). On the ARM build, there is no filesystem,
so this script generates a header that embeds the TOML file contents as a
const byte array. main.c then calls Cvc_Identity_InitFromBuffer() with
that array on the !PLATFORM_POSIX path.

The byte array is emitted as decimal integers (e.g. `0x54, 0x41, ...`).
The hardcoded-VIN regression test scans for the contiguous 17-character
ISO 3779 VIN shape `[A-HJ-NPR-Z0-9]{17}` and the exact VIN literal — a
hex byte stream cannot match either pattern, so the generated header is
safe even without an explicit exclusion. As an additional belt-and-braces
guard the header lives entirely under the build/ tree, which the
regression scanner skips by directory name.

Usage
-----
    python embed_identity.py <input.toml> <output.h>

The generated header defines:
    static const unsigned char cvc_identity_toml_data[<N>];
    static const unsigned long cvc_identity_toml_len = <N>;
"""
from __future__ import annotations

import sys
from pathlib import Path


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(
            "usage: embed_identity.py <input.toml> <output.h>",
            file=sys.stderr,
        )
        return 2

    in_path = Path(argv[1])
    out_path = Path(argv[2])

    if not in_path.is_file():
        print(f"embed_identity: input not found: {in_path}", file=sys.stderr)
        return 1

    blob = in_path.read_bytes()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = []
    lines.append("/* AUTO-GENERATED -- do not edit. */")
    lines.append("/* Source: " + in_path.as_posix() + " */")
    lines.append("/* Generator: firmware/platform/arm/embed_identity.py */")
    lines.append("#ifndef CVC_IDENTITY_DATA_H")
    lines.append("#define CVC_IDENTITY_DATA_H")
    lines.append("")
    lines.append("static const unsigned char cvc_identity_toml_data[] = {")

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
        "static const unsigned long cvc_identity_toml_len = "
        + str(len(blob))
        + "u;"
    )
    lines.append("")
    lines.append("#endif /* CVC_IDENTITY_DATA_H */")
    lines.append("")

    out_path.write_text("\n".join(lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
