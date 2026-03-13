# SIL Test Scenarios

YAML-based test scenarios for Software-in-the-Loop testing.

## Structure

Each `.yaml` file defines:
- **preconditions**: Initial ECU states, signal values
- **stimulus**: CAN messages to inject, state transitions to trigger
- **expected**: Signal values, DTC states, timing constraints
- **timeout**: Maximum test duration

## Running

```bash
python test/framework/src/sil_runner.py test/sil/scenarios/<scenario>.yaml
```
