"""Versioned schema definitions for odx-gen input artifacts.

Each module under `odx_gen.schemas` exposes a JSON Schema dict, a
typed in-memory model, and a `load_*` helper. The schemas are the
authoritative interop contracts — every YAML / JSON input the
toolchain accepts must validate against the matching schema.
"""

from __future__ import annotations
