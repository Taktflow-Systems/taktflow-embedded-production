# Bench Tools

This directory holds small bench/HIL utilities that are safe to commit. Local
machine maps, generated captures, and other bench-specific files stay ignored.

## SC Ethernet Telemetry Receiver

`eth_telemetry_rx.py` decodes the SC Ethernet `SCET` UDP telemetry payload
defined in `docs/api/sc-eth-telemetry-format.md`.

Example:

```powershell
python tools/bench/eth_telemetry_rx.py --duration 60 --csv tmp/sc_eth_telemetry_capture.csv
```

Useful options:

- `--bind 0.0.0.0` listens on all local interfaces.
- `--port 55001` selects the UDP destination port used by the SC firmware.
- `--fail-on-gap` returns a non-zero exit code if sequence gaps are observed.
- `--expect-rate-hz 100 --rate-tolerance 0.01` checks the measured receive
  rate against a configured cadence.

The receiver omits sender IP addresses from console and CSV output to avoid
capturing private bench network details.
