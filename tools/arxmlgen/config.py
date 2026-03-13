"""
arxmlgen configuration — loads and validates project.yaml.
"""

from __future__ import annotations

import os
import sys
from dataclasses import dataclass, field

import yaml


class ConfigError(Exception):
    """Invalid or missing project configuration."""


@dataclass
class OutputConfig:
    base_dir: str = "firmware/ecu"
    header_dir: str = "include"
    cfg_dir: str = "cfg"
    src_dir: str = "src"


@dataclass
class EcuConfig:
    name: str = ""
    prefix: str = ""
    include_in: list[str] | None = None


@dataclass
class GenConfig:
    enabled: bool = True
    settings: dict = field(default_factory=dict)


E2E_SOURCES = ("dbc", "sidecar")


@dataclass
class ProjectConfig:
    name: str = ""
    version: str = "0.0.0"
    standard: str = ""
    arxml_paths: list[str] = field(default_factory=list)
    dbc_path: str | None = None
    sidecar_path: str | None = None
    e2e_source: str = "sidecar"  # "dbc" or "sidecar"
    output: OutputConfig = field(default_factory=OutputConfig)
    ecus: dict[str, EcuConfig] = field(default_factory=dict)
    generators: dict[str, GenConfig] = field(default_factory=dict)
    template_search_path: list[str] | None = None
    project_root: str = ""


KNOWN_GENERATORS = {"com", "rte", "canif", "pdur", "e2e", "swc", "cfg"}


def load_config(path: str) -> ProjectConfig:
    """
    Load and validate project.yaml.

    @param  path  Path to project.yaml
    @return Validated ProjectConfig with resolved paths
    @raises ConfigError on invalid config
    """
    if not os.path.isfile(path):
        raise ConfigError(f"Config file not found: {path}")

    with open(path, "r", encoding="utf-8") as f:
        raw = yaml.safe_load(f)

    if not isinstance(raw, dict):
        raise ConfigError("Config must be a YAML mapping")

    project_root = os.path.dirname(os.path.abspath(path))

    # Project section
    proj = raw.get("project", {})
    if not proj.get("name"):
        raise ConfigError("project.name is required")

    cfg = ProjectConfig(
        name=proj["name"],
        version=str(proj.get("version", "0.0.0")),
        standard=proj.get("standard", ""),
        project_root=project_root,
    )

    # Input section
    inp = raw.get("input", {})
    arxml_list = inp.get("arxml", [])
    if not arxml_list:
        raise ConfigError("input.arxml requires at least one ARXML file")

    for arxml_path in arxml_list:
        resolved = _resolve(project_root, arxml_path)
        if not os.path.isfile(resolved):
            raise ConfigError(f"ARXML file not found: {arxml_path} (resolved: {resolved})")
        cfg.arxml_paths.append(resolved)

    dbc = inp.get("dbc")
    if dbc:
        resolved = _resolve(project_root, dbc)
        if not os.path.isfile(resolved):
            raise ConfigError(f"DBC file not found: {dbc}")
        cfg.dbc_path = resolved

    sidecar = inp.get("sidecar")
    if sidecar:
        resolved = _resolve(project_root, sidecar)
        if not os.path.isfile(resolved):
            raise ConfigError(f"Sidecar file not found: {sidecar}")
        cfg.sidecar_path = resolved

    # E2E source
    e2e_src = inp.get("e2e_source", "sidecar")
    if e2e_src not in E2E_SOURCES:
        raise ConfigError(f"input.e2e_source must be one of {E2E_SOURCES}, got '{e2e_src}'")
    cfg.e2e_source = e2e_src

    # Output section
    out = raw.get("output", {})
    cfg.output = OutputConfig(
        base_dir=out.get("base_dir", "firmware/ecu"),
        header_dir=out.get("header_dir", "include"),
        cfg_dir=out.get("cfg_dir", "cfg"),
        src_dir=out.get("src_dir", "src"),
    )

    # ECU section
    ecus_raw = raw.get("ecus", {})
    if not ecus_raw:
        raise ConfigError("At least one ECU must be defined in 'ecus'")

    for ecu_name, ecu_data in ecus_raw.items():
        if not isinstance(ecu_data, dict):
            raise ConfigError(f"ECU '{ecu_name}' must be a mapping")
        prefix = ecu_data.get("prefix")
        if not prefix:
            raise ConfigError(f"ECU '{ecu_name}' requires 'prefix'")
        include_in = ecu_data.get("include_in")
        cfg.ecus[ecu_name] = EcuConfig(
            name=ecu_name,
            prefix=prefix,
            include_in=include_in,
        )

    # Generators section
    gens_raw = raw.get("generators", {})
    for gen_name, gen_data in gens_raw.items():
        if gen_name not in KNOWN_GENERATORS:
            _warn(f"Unknown generator '{gen_name}', known: {', '.join(sorted(KNOWN_GENERATORS))}")
            continue
        if not isinstance(gen_data, dict):
            gen_data = {}
        enabled = gen_data.pop("enabled", True)
        cfg.generators[gen_name] = GenConfig(enabled=enabled, settings=gen_data)

    # Default: all generators enabled if not specified
    for gen_name in KNOWN_GENERATORS:
        if gen_name not in cfg.generators:
            cfg.generators[gen_name] = GenConfig(enabled=True)

    # Templates section
    tmpl = raw.get("templates", {})
    if tmpl and tmpl.get("search_path"):
        cfg.template_search_path = [
            _resolve(project_root, p) for p in tmpl["search_path"]
        ]

    return cfg


def _resolve(root: str, path: str) -> str:
    """Resolve a path relative to project root."""
    if os.path.isabs(path):
        return path
    return os.path.normpath(os.path.join(root, path))


def _warn(msg: str) -> None:
    print(f"  WARNING: {msg}", file=sys.stderr)
