"""Orchestrator: run all parsers for one ECU and return a populated model."""

from __future__ import annotations

from pathlib import Path

from .discover import dcm_cfg_path_for
from .model import EcuDiagnosticModel
from .parsers.dcm_cfg import parse_dcm_cfg
from .parsers.dcm_service_table import parse_services
from .parsers.dem_cfg import parse_dem_cfg
from .parsers.ecu_cfg_header import resolve_symbol


DCM_HEADER_REL = Path("firmware") / "bsw" / "services" / "Dcm" / "include" / "Dcm.h"
DCM_SOURCE_REL = Path("firmware") / "bsw" / "services" / "Dcm" / "src" / "Dcm.c"


def extract_ecu(repo_root: Path | str, ecu_name: str) -> EcuDiagnosticModel:
    """Run every parser for a given ECU and return a fully populated model.

    Raises FileNotFoundError if the Dcm_Cfg file for that ECU is missing.
    """
    repo_root = Path(repo_root)

    # 1. DID table + config struct
    cfg_path = dcm_cfg_path_for(repo_root, ecu_name)
    model = parse_dcm_cfg(cfg_path, ecu_name=ecu_name)

    # 2. Resolve TX PDU ID symbol via header chain (if needed)
    if model.tx_pdu_id is None and model.tx_pdu_id_symbol:
        value, src, todos = resolve_symbol(
            repo_root, ecu_name, model.tx_pdu_id_symbol
        )
        model.tx_pdu_id = value
        model.unresolved_todos.extend(todos)

    # 3. Resolve RX PDU ID symbol if present
    if model.rx_pdu_id is None and model.rx_pdu_id_symbol:
        value, src, todos = resolve_symbol(
            repo_root, ecu_name, model.rx_pdu_id_symbol
        )
        model.rx_pdu_id = value
        model.unresolved_todos.extend(todos)

    # 4. UDS service set from shared Dcm.{h,c}
    dcm_h = repo_root / DCM_HEADER_REL
    dcm_c = repo_root / DCM_SOURCE_REL
    if dcm_h.is_file() and dcm_c.is_file():
        services, todos = parse_services(dcm_h, dcm_c)
        model.services = services
        model.unresolved_todos.extend(todos)
    else:
        model.unresolved_todos.append(
            f"TODO: missing shared Dcm sources at {dcm_h} or {dcm_c}; "
            f"no UDS services extracted"
        )

    # 5. DTCs from Dem_Cfg if present
    dtcs, todos = parse_dem_cfg(repo_root, ecu_name)
    model.dtcs = dtcs
    model.unresolved_todos.extend(todos)

    return model
