"""Read-only sidecar YAML cache regression tests."""

from __future__ import annotations

import os
from types import MappingProxyType

import pytest
import yaml

from tools.arxmlgen.reader import _SIDECAR_YAML_CACHE, _load_sidecar_yaml


def test_sidecar_cache_reuses_one_immutable_snapshot(tmp_path):
    sidecar = tmp_path / "sidecar.yaml"
    sidecar.write_text("ecus:\n  cvc:\n    values: [1, 2]\n", encoding="utf-8")
    _SIDECAR_YAML_CACHE.clear()

    first = _load_sidecar_yaml(str(sidecar))
    second = _load_sidecar_yaml(str(sidecar))

    assert first is second
    assert isinstance(first, MappingProxyType)
    assert first["ecus"]["cvc"]["values"] == (1, 2)
    with pytest.raises(TypeError):
        first["ecus"] = {}  # type: ignore[index]
    with pytest.raises(TypeError):
        first["ecus"]["cvc"]["values"][0] = 9  # type: ignore[index]


def test_sidecar_cache_invalidates_without_returning_stale_data(tmp_path):
    sidecar = tmp_path / "sidecar.yaml"
    sidecar.write_text("value: first\n", encoding="utf-8")
    _SIDECAR_YAML_CACHE.clear()
    first = _load_sidecar_yaml(str(sidecar))

    previous = sidecar.stat()
    sidecar.write_text("value: second\n", encoding="utf-8")
    os.utime(
        sidecar,
        ns=(previous.st_atime_ns, max(previous.st_mtime_ns + 1, sidecar.stat().st_mtime_ns)),
    )
    second = _load_sidecar_yaml(str(sidecar))

    assert second is not first
    assert first["value"] == "first"
    assert second["value"] == "second"

    sidecar.write_text("value: [\n", encoding="utf-8")
    with pytest.raises(yaml.YAMLError):
        _load_sidecar_yaml(str(sidecar))
