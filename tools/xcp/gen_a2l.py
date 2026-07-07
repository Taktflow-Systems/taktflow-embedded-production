#!/usr/bin/env python3
"""
A2L Generator — produces ASAM MCD-2MC A2L file from ELF symbols.

Scans the firmware ELF for exported measurement/calibration variables
and generates an A2L file that pyXCP can use for named access.

Usage:
    python tools/xcp/gen_a2l.py firmware.elf --ecu CVC --output cvc.a2l

    # TI linker map input + YAML symbol allowlist (TMS570 SC, big-endian):
    python tools/xcp/gen_a2l.py --map build/tms570/sc.map \
        --symbols tools/xcp/xcp_symbols.yaml --byte-order MSB_FIRST \
        --ecu SC -o build/tms570/sc_xcp.a2l

Variables are discovered by naming convention:
  - g_dbg_*          → MEASUREMENT (debug counters)
  - com_tx_send_*    → MEASUREMENT (Com TX counters)
  - com_rx_*         → MEASUREMENT (Com RX state)
  - Swc_*            → MEASUREMENT (SWC state)
  - *_Cfg*           → CHARACTERISTIC (calibration — read-only for now)
or, with --symbols, taken exactly from the YAML allowlist (this also
resolves file-static symbols, which TI maps list only as section names).

Requires: arm-none-eabi-nm (for bare-metal ELF) or nm (for POSIX ELF);
the --map path needs no external tools.

Tool qualification (ISO 26262-8): TCL1 — bench-only measurement tooling,
output is human-reviewed before use; no safety decisions derive from it.

@traces_to Phase 4: XCP measurement and calibration
@traces_to docs/plans/plan-sc-ethernet-roadmap.md S-XCP-03
"""

import argparse
import re
import subprocess
import sys
from datetime import datetime

# Variable patterns to include in A2L
MEASUREMENT_PATTERNS = [
    (re.compile(r'^g_dbg_'), 'Debug counter'),
    (re.compile(r'^com_tx_send_count'), 'Com TX send count'),
    (re.compile(r'^com_rx_pdu_quality'), 'Com RX PDU quality'),
    (re.compile(r'^g_dbg_com_'), 'Com debug counter'),
    (re.compile(r'^g_dbg_xcp_'), 'XCP debug counter'),
    (re.compile(r'^Hb_AliveCounter'), 'Heartbeat alive counter'),
    (re.compile(r'^Hb_CycleCounter'), 'Heartbeat cycle counter'),
    (re.compile(r'^Safety_Status'), 'Safety status'),
    (re.compile(r'^Brake_Position'), 'Brake position'),
    (re.compile(r'^Brake_Fault'), 'Brake fault code'),
    (re.compile(r'^Pedal_Position'), 'Pedal position'),
    (re.compile(r'^Pedal_Fault'), 'Pedal fault code'),
]

# Size → A2L data type mapping
SIZE_MAP = {
    1: ('UBYTE', 'UWORD', 0, 255),
    2: ('UWORD', 'UWORD', 0, 65535),
    4: ('ULONG', 'ULONG', 0, 4294967295),
}

BYTE_ORDER_CHOICES = ('MSB_LAST', 'MSB_FIRST')  # little-endian / big-endian

# TI linker map section-allocation rows carry address, size, and the
# owning input section, e.g.:
#   08005d88  00000001  sc_eth_telemetry.o (.data.g_sc_eth_telemetry_sequence)
#   08005d4c  00000004  (.common:g_dbg_ethrx_polls)
TI_MAP_SECTION_RE = re.compile(
    r'^\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+\S*\s*'
    r'\((?:\.(?:data|bss)\.([A-Za-z_]\w*)|\.common:([A-Za-z_]\w*))\)\s*$'
)


def parse_ti_map(map_text: str) -> list[dict]:
    """Parse TI linker .map section-allocation rows into a symbol list.

    Uses the per-input-section rows (not GLOBAL SYMBOLS) because they
    carry sizes and include file-static objects.
    """
    symbols = []
    seen = set()
    for line in map_text.splitlines():
        m = TI_MAP_SECTION_RE.match(line)
        if not m:
            continue
        name = m.group(3) or m.group(4)
        if name in seen:
            continue
        seen.add(name)
        symbols.append({
            'name': name,
            'addr': int(m.group(1), 16),
            'size': int(m.group(2), 16),
            'type': 'D',
        })
    return symbols


def load_allowlist(yaml_path: str) -> dict:
    """Load the YAML symbol allowlist: {symbol_name: description}."""
    import yaml
    with open(yaml_path, encoding='utf-8') as fh:
        doc = yaml.safe_load(fh)
    entries = (doc or {}).get('symbols', [])
    allow = {}
    for entry in entries:
        if isinstance(entry, str):
            allow[entry] = 'Allowlisted measurement'
        elif isinstance(entry, dict) and 'name' in entry:
            allow[entry['name']] = entry.get('desc', 'Allowlisted measurement')
    return allow


def parse_nm_output(nm_output: str) -> list[dict]:
    """Parse `nm -S` output into symbol list."""
    symbols = []
    for line in nm_output.strip().split('\n'):
        parts = line.split()
        if len(parts) < 4:
            continue
        addr_str, size_str, sym_type, name = parts[0], parts[1], parts[2], parts[3]
        # Only include data symbols (B=BSS, D=Data, G=small data, R=rodata)
        if sym_type.upper() not in ('B', 'D', 'G', 'R', 'T'):
            continue
        # Skip code symbols for now
        if sym_type.upper() == 'T':
            continue
        try:
            addr = int(addr_str, 16)
            size = int(size_str, 16)
        except ValueError:
            continue
        symbols.append({'name': name, 'addr': addr, 'size': size, 'type': sym_type})
    return symbols


def match_measurement(name: str) -> str | None:
    """Check if symbol name matches a measurement pattern."""
    for pattern, description in MEASUREMENT_PATTERNS:
        if pattern.search(name):
            return description
    return None


def generate_a2l(symbols: list[dict], ecu_name: str,
                 byte_order: str = 'MSB_LAST',
                 allowlist: dict | None = None) -> str:
    """Generate A2L file content from symbol list.

    With an allowlist, exactly the allowlisted names are emitted (their
    YAML descriptions win); otherwise the naming-convention patterns
    select measurements.
    """
    now = datetime.now().strftime('%Y-%m-%d %H:%M')
    measurements = []

    for sym in symbols:
        if allowlist is not None:
            desc = allowlist.get(sym['name'])
        else:
            desc = match_measurement(sym['name'])
        if desc is None:
            continue
        size = sym['size']
        if size > 4 or size == 0:
            # Skip arrays and large structures (handle individually if needed)
            # For arrays, we could generate indexed measurements, but keep it simple
            if size <= 64:
                # Small array — generate as UBYTE array measurement
                measurements.append({
                    'name': sym['name'],
                    'addr': sym['addr'],
                    'size': 1,
                    'count': size,
                    'desc': desc,
                    'array': True,
                })
            continue
        if size not in SIZE_MAP:
            continue
        measurements.append({
            'name': sym['name'],
            'addr': sym['addr'],
            'size': size,
            'count': 1,
            'desc': desc,
            'array': False,
        })

    lines = []
    lines.append(f'/* A2L file generated by gen_a2l.py — {now} */')
    lines.append(f'/* ECU: {ecu_name} — {len(measurements)} measurements */')
    lines.append('')
    lines.append('ASAP2_VERSION 1 71')
    lines.append('')
    lines.append(f'/begin PROJECT {ecu_name}_XCP "Taktflow {ecu_name} XCP Project"')
    lines.append(f'  /begin MODULE {ecu_name} "Taktflow {ecu_name} ECU"')
    lines.append('')
    lines.append('    /begin MOD_PAR ""')
    lines.append(f'      EPK "{ecu_name} V1.0"')
    lines.append('    /end MOD_PAR')
    lines.append('')
    lines.append('    /begin MOD_COMMON ""')
    lines.append(f'      BYTE_ORDER {byte_order}')  # MSB_LAST=LE (STM32), MSB_FIRST=BE (TMS570)
    lines.append('    /end MOD_COMMON')
    lines.append('')

    # COMPU_METHODs
    lines.append('    /begin COMPU_METHOD CM_IDENTICAL ""')
    lines.append('      IDENTICAL "%3.0" ""')
    lines.append('    /end COMPU_METHOD')
    lines.append('')

    # MEASUREMENT definitions
    for m in measurements:
        dt, dep, lo, hi = SIZE_MAP.get(m['size'], ('UBYTE', 'UWORD', 0, 255))
        lines.append(f'    /begin MEASUREMENT {m["name"]} "{m["desc"]}"')
        lines.append(f'      {dt} CM_IDENTICAL 0 0 {lo} {hi}')
        lines.append(f'      ECU_ADDRESS 0x{m["addr"]:08X}')
        if m.get('array') and m['count'] > 1:
            lines.append(f'      MATRIX_DIM {m["count"]}')
        lines.append(f'    /end MEASUREMENT')
        lines.append('')

    lines.append(f'  /end MODULE')
    lines.append(f'/end PROJECT')
    lines.append('')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Generate A2L from ELF or TI map')
    parser.add_argument('elf', nargs='?', default=None,
                        help='Path to ELF file (or use --map)')
    parser.add_argument('--map', dest='map_file', default=None,
                        help='TI linker .map file (alternative to ELF+nm)')
    parser.add_argument('--symbols', default=None,
                        help='YAML symbol allowlist (see tools/xcp/xcp_symbols.yaml)')
    parser.add_argument('--byte-order', choices=BYTE_ORDER_CHOICES,
                        default='MSB_LAST',
                        help='A2L BYTE_ORDER: MSB_LAST=little-endian (STM32), '
                             'MSB_FIRST=big-endian (TMS570). Default: MSB_LAST')
    parser.add_argument('--ecu', default='CVC', help='ECU name (default: CVC)')
    parser.add_argument('--output', '-o', default=None, help='Output A2L file')
    parser.add_argument('--nm', default='nm', help='nm tool path (default: nm)')
    args = parser.parse_args()

    if (args.elf is None) == (args.map_file is None):
        print("Error: give exactly one of ELF path or --map.", file=sys.stderr)
        return 2

    if args.map_file:
        with open(args.map_file, encoding='utf-8', errors='replace') as fh:
            symbols = parse_ti_map(fh.read())
        print(f"Parsed {len(symbols)} data symbols from {args.map_file}")
    else:
        # Run nm -S to get symbol sizes
        try:
            result = subprocess.run(
                [args.nm, '-S', '--defined-only', args.elf],
                capture_output=True, text=True, check=True
            )
        except FileNotFoundError:
            print(f"Error: '{args.nm}' not found. Install binutils or specify --nm path.", file=sys.stderr)
            return 1
        except subprocess.CalledProcessError as e:
            print(f"Error running nm: {e.stderr}", file=sys.stderr)
            return 1
        symbols = parse_nm_output(result.stdout)
        print(f"Parsed {len(symbols)} symbols from {args.elf}")

    allowlist = None
    if args.symbols:
        allowlist = load_allowlist(args.symbols)
        found = {s['name'] for s in symbols}
        missing = sorted(set(allowlist) - found)
        for name in missing:
            print(f"WARNING: allowlisted symbol not found: {name}",
                  file=sys.stderr)
        matched = [s for s in symbols if s['name'] in allowlist]
        if not matched:
            print("Error: no allowlisted symbol resolved — refusing to emit "
                  "an empty A2L (fail-closed).", file=sys.stderr)
            return 1
    else:
        matched = [s for s in symbols if match_measurement(s['name'])]
    print(f"Matched {len(matched)} measurement variables")

    a2l = generate_a2l(symbols, args.ecu.upper(),
                       byte_order=args.byte_order, allowlist=allowlist)

    output = args.output or f"{args.ecu.lower()}.a2l"
    with open(output, 'w') as f:
        f.write(a2l)
    print(f"Written: {output}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
