"""
arxmlgen generator registry.

Generators are discovered by name and instantiated on demand.
Each generator module must export a class with:
  - name: str          — generator name (matches config key)
  - generate(ecu, config, engine) → list[OutputFile]
"""

from __future__ import annotations

# Registry: generator name → (module_path, class_name)
# Modules are imported lazily to avoid circular imports and allow extension.
REGISTRY: dict[str, tuple[str, str]] = {
    "com":    ("tools.arxmlgen.generators.com_cfg",     "ComCfgGenerator"),
    "rte":    ("tools.arxmlgen.generators.rte_cfg",     "RteCfgGenerator"),
    "cfg":    ("tools.arxmlgen.generators.cfg_header",  "CfgHeaderGenerator"),
    "canif":  ("tools.arxmlgen.generators.canif_cfg",   "CanIfCfgGenerator"),
    "pdur":   ("tools.arxmlgen.generators.pdur_cfg",    "PduRCfgGenerator"),
    "e2e":    ("tools.arxmlgen.generators.e2e_cfg",     "E2ECfgGenerator"),
    "swc":    ("tools.arxmlgen.generators.swc_skeleton", "SwcSkeletonGenerator"),
}


def get_generator(name: str):
    """
    Get a generator class by name.

    @param name  Generator name (e.g., "com", "rte")
    @return Generator class instance, or None if not registered
    """
    if name not in REGISTRY:
        return None

    module_path, class_name = REGISTRY[name]
    import importlib

    mod = importlib.import_module(module_path)
    cls = getattr(mod, class_name)
    return cls()


def available_generators() -> list[str]:
    """Return list of registered generator names."""
    return sorted(REGISTRY.keys())
