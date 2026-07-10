"""
arxmlgen ARXML reader — extracts data model from standard ARXML files.

Discovers elements by AUTOSAR type (not hardcoded package paths), so it works
with any ARXML layout that follows the R4.0+ schema.
"""

from __future__ import annotations

import os
import re
import sys
from collections import defaultdict
from collections.abc import Mapping
from types import MappingProxyType
from typing import Any

import autosar_data as ad
import yaml
try:
    from tools.pipeline_diagnostics import (
        DiagnosticBag,
        DiagnosticSeverity,
        PipelineDiagnostic,
        PipelineDiagnosticError,
        locate_text,
        portable_path,
    )
except ModuleNotFoundError:
    from pipeline_diagnostics import (
        DiagnosticBag,
        DiagnosticSeverity,
        PipelineDiagnostic,
        PipelineDiagnosticError,
        locate_text,
        portable_path,
    )

from .config import ProjectConfig
from .model import Ecu, Pdu, Port, ProjectModel, Runnable, Signal, Swc
from .policy import apply_rate_monotonic_banding


_SIDECAR_YAML_CACHE: dict[str, tuple[int, int, Any]] = {}


def _freeze_sidecar(value: Any) -> Any:
    """Make cached YAML read-only so readers cannot contaminate later loads."""

    if isinstance(value, dict):
        return MappingProxyType(
            {key: _freeze_sidecar(item) for key, item in value.items()}
        )
    if isinstance(value, list):
        return tuple(_freeze_sidecar(item) for item in value)
    return value


def _load_sidecar_yaml(path: str) -> Any:
    """Load one immutable sidecar snapshot, invalidated by file metadata."""

    resolved = os.path.realpath(path)
    stat = os.stat(resolved)
    cached = _SIDECAR_YAML_CACHE.get(resolved)
    if cached is not None and cached[:2] == (stat.st_mtime_ns, stat.st_size):
        return cached[2]
    with open(resolved, "r", encoding="utf-8") as stream:
        loaded = yaml.safe_load(stream)
    frozen = _freeze_sidecar(loaded)
    _SIDECAR_YAML_CACHE[resolved] = (stat.st_mtime_ns, stat.st_size, frozen)
    return frozen


class ArxmlReadError(PipelineDiagnosticError):
    """Failed to parse or extract data from ARXML."""


class ArxmlReader:
    """Reads ARXML files and populates the internal data model."""

    def __init__(self, config: ProjectConfig):
        self.config = config
        self.model = ad.AutosarModel()
        self.diagnostics = DiagnosticBag()
        self.warnings: list[str] = []

        # Internal lookup tables built during reading
        self._frame_can_ids: dict[str, int] = {}       # frame_path → CAN ID
        self._frame_dlcs: dict[str, int] = {}           # frame_path → DLC
        self._frame_to_pdu: dict[str, str] = {}         # frame_path → pdu_path
        self._pdu_to_frame: dict[str, str] = {}         # pdu_path → frame_path
        self._pdu_to_can_id: dict[str, int] = {}        # pdu_path → CAN ID
        self._pdu_to_dlc: dict[str, int] = {}           # pdu_path → DLC
        self._dbc_tx_map: dict[str, str] = {}           # message_name → sender_ecu (uppercase)
        self._dbc_rx_map: dict[str, list[str]] = {}     # message_name → list of receiver ECUs
        self._dbc_e2e_map: dict[str, int] = {}          # message_name → E2E data ID (from DBC attr)
        self._dbc_e2e_max_delta: dict[str, int] = {}  # message_name → E2E MaxDeltaCounter
        self._dbc_cycle_map: dict[str, int] = {}       # message_name → GenMsgCycleTime (ms)
        self._dbc_satisfies_map: dict[str, str] = {}   # message_name → Satisfies string
        self._dbc_asil_map: dict[str, str] = {}        # message_name → ASIL level
        self._additional_tx_ecus: dict[str, list[str]] = {}  # message_name → extra TX ECUs
        self._dbc_send_type_map: dict[str, str] = {}   # message_name → GenMsgSendType (lowercase)
        self._tx_mode_overrides: dict[str, str] = {}   # message_name → forced Com TX mode (sidecar)
        self._dbc_signal_types: dict[tuple[str, str], str] = {}
        self._mapped_swcs: dict[str, tuple[str, str]] = {}
        self._mapped_ports: dict[tuple[str, str], tuple[str, str]] = {}

    def read(self) -> ProjectModel:
        """Read all ARXML files and build the project model."""
        # Load ARXML
        for arxml_path in self.config.arxml_paths:
            try:
                self.model.load_file(arxml_path)
            except Exception as exc:
                self._fail(
                    "ARXML100", "ARXML input",
                    "cannot parse or load model", exc,
                    location_path=arxml_path,
                )
            _info(f"  Loaded: {os.path.basename(arxml_path)}")

        self._reject_unsupported_pdu_constructs()
        self._reject_broken_references()

        # Load DBC for TX/RX routing (optional but recommended)
        if self.config.dbc_path:
            self._load_dbc_routing()
        if self.config.sidecar_path:
            self._load_explicit_mapping_index()

        # Extract data in dependency order
        platform_types = self._extract_platform_types()
        ecu_names = self._extract_ecu_instances()
        sr_interfaces = self._extract_sr_interfaces()

        # Build frame → CAN ID mapping
        self._extract_frame_triggerings()

        # Build PDU → frame mapping and PDU → CAN ID
        self._extract_frames()

        # Build ECU objects with PDUs
        ecus = self._build_ecus(ecu_names)

        # Extract SWCs and assign to ECUs
        self._extract_swcs(ecus)

        # Resolve port data types from matching signals
        self._resolve_port_types(ecus)

        # Assign signal IDs
        bsw_reserved = self.config.generators.get("com", None)
        bsw_count = 16
        if bsw_reserved and bsw_reserved.settings.get("bsw_reserved_signals"):
            bsw_count = int(bsw_reserved.settings["bsw_reserved_signals"])
        self._assign_signal_ids(ecus, bsw_reserved=bsw_count)

        # Load sidecar data (always load for DTC, enums, thresholds, runnables)
        if self.config.sidecar_path:
            self._read_sidecar(ecus)

        # Rate-monotonic re-banding of runnable priorities — applied to
        # the resolved model so every generator (rte_cfg, os_cfg) shares
        # one documented order (docs/plans/memo-rm-reband-trace.md).
        for ecu in ecus.values():
            apply_rate_monotonic_banding(ecu)

        # Apply E2E data IDs based on configured source
        if self.config.e2e_source == "arxml":
            self._apply_arxml_e2e(ecus)
        elif self.config.e2e_source == "dbc":
            self._apply_dbc_e2e(ecus)
            _info(f"  E2E source: DBC attributes ({len(self._dbc_e2e_map)} messages)")
        else:
            # sidecar E2E is already applied in _read_sidecar via _apply_pdu_e2e_map
            _info(f"  E2E source: sidecar pdu_e2e_map")

        # Re-populate e2e_data_ids from PDUs after E2E source is applied
        # (auto-generation at PDU sort time runs before E2E source is set)
        for ecu in ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                if pdu.e2e_data_id is not None:
                    safe_name = pdu.name.upper().replace(" ", "_").replace("-", "_")
                    define_name = f"{ecu.prefix}_E2E_{safe_name}_DATA_ID"
                    if define_name not in ecu.e2e_data_ids:
                        ecu.e2e_data_ids[define_name] = pdu.e2e_data_id

        # Compute E2E MaxDeltaCounter and RX timeout from scheduler periods
        self._compute_e2e_params(ecus)

        # Apply traceability (Satisfies, ASIL) from DBC to all PDUs
        self._apply_dbc_traceability(ecus)

        # Build project model
        total_signals = 0
        total_pdus = 0
        total_swcs = 0
        total_runnables = 0
        for ecu in ecus.values():
            total_signals += len(ecu.all_signals)
            total_pdus += len(ecu.tx_pdus) + len(ecu.rx_pdus)
            total_swcs += len(ecu.swcs)
            total_runnables += sum(len(s.runnables) for s in ecu.swcs)

        project = ProjectModel(
            name=self.config.name,
            ecus=ecus,
            platform_types=platform_types,
            sr_interfaces=sr_interfaces,
            total_signals=total_signals,
            total_pdus=total_pdus,
            total_swcs=total_swcs,
            total_runnables=total_runnables,
        )

        if self.warnings:
            _info(f"  Warnings: {len(self.warnings)}")
            for w in self.warnings[:10]:
                _info(f"    {w}")
            if len(self.warnings) > 10:
                _info(f"    ... and {len(self.warnings) - 10} more")

        return project

    def _reject_unsupported_pdu_constructs(self):
        """Reject valid AUTOSAR PDU kinds that this C model cannot represent."""
        unsupported = {
            "CONTAINER-I-PDU",
            "DCM-I-PDU",
            "GENERAL-PURPOSE-I-PDU",
            "MULTIPLEXED-I-PDU",
            "NM-PDU",
            "N-PDU",
            "SECURED-I-PDU",
        }
        for item in self.model.elements_dfs:
            elem = item[1] if isinstance(item, tuple) else item
            element_name = str(getattr(elem, "element_name", ""))
            if element_name not in unsupported:
                continue
            object_path = getattr(elem, "path", "") or element_name
            self._fail(
                "ARXML111",
                "%s '%s'" % (element_name, object_path),
                "construct is unsupported by the generated C communication model",
                element=elem,
            )

    def _reject_broken_references(self):
        """Reject every unresolved AUTOSAR reference before model extraction."""
        errors = self.model.check_references()
        if not errors:
            return
        element = errors[0]
        target = getattr(element, "character_data", "") or "<missing target>"
        self._fail(
            "ARXML102",
            "ARXML reference '%s'" % target,
            "%d unresolved reference(s) prevent C generation" % len(errors),
            element=element,
        )

    # ------------------------------------------------------------------
    # DBC routing
    # ------------------------------------------------------------------

    def _load_dbc_routing(self):
        """Load DBC to determine which ECU transmits which message."""
        try:
            import cantools
        except ImportError as exc:
            self._fail(
                "DBC100", "DBC routing", "cantools dependency is unavailable", exc
            )

        db = cantools.database.load_file(self.config.dbc_path)
        definitions = getattr(getattr(db, "dbc", None), "attribute_definitions", {}) or {}
        for name in sorted(definitions):
            if name.startswith("Gen") and name not in ("GenMsgCycleTime", "GenMsgSendType"):
                self._warn(
                    "convention is not interpreted by this pipeline",
                    code="DBC004",
                    source="DBC attribute definition '%s'" % name,
                )
        for msg in db.messages:
            for signal in msg.signals:
                self._dbc_signal_types[(msg.name, signal.name)] = _bits_to_c_type(
                    signal.length
                )
            senders = msg.senders or []
            if senders:
                self._dbc_tx_map[msg.name] = senders[0].upper()
            # All ECUs that are not the sender are potential receivers
            all_nodes = {n.name.upper() for n in db.nodes}
            receivers = all_nodes - {senders[0].upper()} if senders else all_nodes
            self._dbc_rx_map[msg.name] = list(receivers)

            # Extract E2E_DataID attribute if present (cantools: msg.dbc.attributes)
            try:
                e2e_attr = msg.dbc.attributes.get("E2E_DataID")
                if e2e_attr is not None:
                    e2e_val = int(e2e_attr.value if hasattr(e2e_attr, 'value') else e2e_attr)
                    if e2e_val >= 0:
                        self._dbc_e2e_map[msg.name] = e2e_val
                    else:
                        self._fail(
                            "DBC001", "DBC message '%s' attribute 'E2E_DataID'" % msg.name,
                            "value must be non-negative",
                        )
            except (AttributeError, TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' attribute 'E2E_DataID'" % msg.name,
                    "value is not a non-negative integer", exc,
                )

            # Extract E2E_MaxDeltaCounter attribute (default 2 if not present)
            try:
                maxd_attr = msg.dbc.attributes.get("E2E_MaxDeltaCounter")
                if maxd_attr is not None:
                    maxd_val = int(maxd_attr.value if hasattr(maxd_attr, 'value') else maxd_attr)
                    if maxd_val < 0:
                        self._fail(
                            "DBC001", "DBC message '%s' attribute 'E2E_MaxDeltaCounter'" % msg.name,
                            "value must be non-negative",
                        )
                    self._dbc_e2e_max_delta[msg.name] = maxd_val
            except (AttributeError, TypeError, ValueError) as exc:
                self._fail(
                    "DBC001",
                    "DBC message '%s' attribute 'E2E_MaxDeltaCounter'" % msg.name,
                    "value is not an integer", exc,
                )

            # Extract GenMsgCycleTime attribute (TX cycle period in ms)
            try:
                cycle_attr = msg.dbc.attributes.get("GenMsgCycleTime")
                if cycle_attr is not None:
                    cycle_val = int(cycle_attr.value if hasattr(cycle_attr, 'value') else cycle_attr)
                    if cycle_val < 0:
                        self._fail(
                            "DBC001", "DBC message '%s' attribute 'GenMsgCycleTime'" % msg.name,
                            "value must be non-negative",
                        )
                    if cycle_val > 0:
                        self._dbc_cycle_map[msg.name] = cycle_val
            except (AttributeError, TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' attribute 'GenMsgCycleTime'" % msg.name,
                    "value is not a positive integer", exc,
                )

            # Extract GenMsgSendType attribute ("event" → COM_TX_MODE_DIRECT)
            try:
                st_attr = msg.dbc.attributes.get("GenMsgSendType")
                if st_attr is not None:
                    st_val = st_attr.value if hasattr(st_attr, 'value') else st_attr
                    normalized = str(st_val).strip().lower()
                    if normalized not in ("cyclic", "event", "nomsgsendtype"):
                        self._fail(
                            "DBC003",
                            "DBC message '%s' attribute 'GenMsgSendType'" % msg.name,
                            "unsupported value '%s'" % st_val,
                        )
                    self._dbc_send_type_map[msg.name] = normalized
                    if normalized == "nomsgsendtype":
                        self._warn(
                            "value is intentionally unspecified; cycle-time heuristic applies",
                            code="DBC005",
                            source="DBC message '%s' attribute 'GenMsgSendType'" % msg.name,
                        )
            except ArxmlReadError:
                raise
            except (AttributeError, TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' attribute 'GenMsgSendType'" % msg.name,
                    "cannot read value", exc,
                )

            # Extract Satisfies attribute (requirement traceability)
            try:
                sat_attr = msg.dbc.attributes.get("Satisfies")
                if sat_attr is not None:
                    sat_val = str(sat_attr.value if hasattr(sat_attr, 'value') else sat_attr)
                    if sat_val:
                        self._dbc_satisfies_map[msg.name] = sat_val
            except (AttributeError, TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' attribute 'Satisfies'" % msg.name,
                    "cannot read value", exc,
                )

            # Extract ASIL attribute
            try:
                asil_attr = msg.dbc.attributes.get("ASIL")
                if asil_attr is not None:
                    asil_val = str(asil_attr.value if hasattr(asil_attr, 'value') else asil_attr)
                    if asil_val:
                        self._dbc_asil_map[msg.name] = asil_val
            except (AttributeError, TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' attribute 'ASIL'" % msg.name,
                    "cannot read value", exc,
                )

        e2e_count = len(self._dbc_e2e_map)
        sat_count = len(self._dbc_satisfies_map)
        _info(f"  DBC routing: {len(db.messages)} messages, {len(db.nodes)} nodes, {e2e_count} E2E data IDs, {sat_count} Satisfies")

        # Load message_routing overrides from sidecar (multi-sender messages)
        if self.config.sidecar_path and os.path.exists(self.config.sidecar_path):
            try:
                sidecar = _load_sidecar_yaml(self.config.sidecar_path) or {}
                routing = sidecar.get("message_routing", {})
                for msg_name, cfg in routing.items():
                    extra_tx = cfg.get("additional_tx_ecus", [])
                    if extra_tx:
                        self._additional_tx_ecus[msg_name] = [
                            e.upper() for e in extra_tx
                        ]
                if routing:
                    _info(f"  Routing overrides: {len(routing)} messages")

                # TX-mode overrides: force the Com TX mode for the sender ECU
                # (e.g. 'none' = keep PDU slot but never transmit)
                tx_modes = sidecar.get("message_tx_mode", {}) or {}
                for msg_name, mode in tx_modes.items():
                    mode_up = str(mode).strip().upper()
                    if mode_up not in ("NONE", "DIRECT", "PERIODIC"):
                        self._fail(
                            "SIDECAR001",
                            "sidecar message_tx_mode[%s]" % msg_name,
                            "unsupported value '%s'; expected none, direct, or periodic" % mode,
                        )
                    self._tx_mode_overrides[msg_name] = mode_up
                if self._tx_mode_overrides:
                    _info(f"  TX-mode overrides: {len(self._tx_mode_overrides)} messages")
            except ArxmlReadError:
                raise
            except Exception as exc:
                self._fail(
                    "SIDECAR002", "sidecar routing", "cannot read overrides", exc
                )

    # ------------------------------------------------------------------
    # Platform types
    # ------------------------------------------------------------------

    def _msg_attr_lookup(self, pdu_name: str, mapping: dict[str, str]) -> str | None:
        """Look up a per-message attribute, tolerating _Ipdu/_Frame/_Pdu suffixes."""
        if pdu_name in mapping:
            return mapping[pdu_name]
        for suffix in ("_Ipdu", "_Frame", "_Pdu"):
            stripped = pdu_name.replace(suffix, "")
            if stripped in mapping:
                return mapping[stripped]
        return None

    def _extract_platform_types(self) -> list[str]:
        """Extract platform implementation data type names."""
        types = []
        for path, elem in self.model.identifiable_elements:
            if elem.element_name == "IMPLEMENTATION-DATA-TYPE":
                types.append(elem.item_name)
        return sorted(types)

    # ------------------------------------------------------------------
    # ECU instances
    # ------------------------------------------------------------------

    def _extract_ecu_instances(self) -> dict[str, str]:
        """Extract ECU instance names. Returns {lowercase_name: arxml_path}."""
        ecus = {}
        for path, elem in self.model.identifiable_elements:
            if elem.element_name == "ECU-INSTANCE":
                name = elem.item_name
                ecus[name.lower()] = path
        return ecus

    # ------------------------------------------------------------------
    # S/R interfaces
    # ------------------------------------------------------------------

    def _extract_sr_interfaces(self) -> list[str]:
        """Extract sender-receiver interface names."""
        interfaces = []
        for path, elem in self.model.identifiable_elements:
            if elem.element_name == "SENDER-RECEIVER-INTERFACE":
                interfaces.append(elem.item_name)
        return sorted(interfaces)

    # ------------------------------------------------------------------
    # Frames and frame triggerings (CAN ID mapping)
    # ------------------------------------------------------------------

    def _extract_frame_triggerings(self):
        """Extract CAN frame triggerings to get CAN IDs."""
        for path, elem in self.model.identifiable_elements:
            if elem.element_name == "CAN-FRAME-TRIGGERING":
                can_id = None
                frame_ref_path = None

                for sub in elem.sub_elements:
                    if sub.element_name == "IDENTIFIER":
                        try:
                            can_id = int(sub.character_data)
                        except (TypeError, ValueError) as exc:
                            self._fail(
                                "ARXML101", "CAN frame triggering '%s' identifier" % path,
                                "value is not an integer", exc, element=elem,
                            )
                    elif sub.element_name == "FRAME-REF" and sub.is_reference:
                        try:
                            frame_ref_path = sub.reference_target.path
                        except Exception as exc:
                            self._fail(
                                "ARXML102", "CAN frame triggering '%s' FRAME-REF" % path,
                                "reference cannot be resolved", exc, element=elem,
                            )

                if can_id is None:
                    self._fail(
                        "ARXML112", "CAN frame triggering '%s'" % path,
                        "required IDENTIFIER is missing", element=elem,
                    )
                if frame_ref_path is None:
                    self._fail(
                        "ARXML102", "CAN frame triggering '%s'" % path,
                        "required FRAME-REF is missing or unresolved", element=elem,
                    )
                self._frame_can_ids[frame_ref_path] = can_id

    def _extract_frames(self):
        """Extract CAN frames to get DLC and PDU-to-frame mapping."""
        for path, elem in self.model.identifiable_elements:
            if elem.element_name in ("CAN-FRAME", "FRAME"):
                dlc = None
                mapped_pdu = False
                for sub in elem.sub_elements:
                    if sub.element_name == "FRAME-LENGTH":
                        try:
                            dlc = int(sub.character_data)
                        except (TypeError, ValueError) as exc:
                            self._fail(
                                "ARXML103", "CAN frame '%s' length" % path,
                                "value is not an integer", exc, element=elem,
                            )
                    elif sub.element_name == "PDU-TO-FRAME-MAPPINGS":
                        for mapping in sub.sub_elements:
                            if mapping.element_name == "PDU-TO-FRAME-MAPPING":
                                mapping_has_pdu = False
                                for m_sub in mapping.sub_elements:
                                    if m_sub.element_name == "PDU-REF" and m_sub.is_reference:
                                        try:
                                            pdu_path = m_sub.reference_target.path
                                            self._pdu_to_frame[pdu_path] = path
                                            self._frame_to_pdu[path] = pdu_path
                                            mapping_has_pdu = True
                                            mapped_pdu = True
                                        except Exception as exc:
                                            self._fail(
                                                "ARXML102",
                                                "CAN frame '%s' PDU-REF" % path,
                                                "reference cannot be resolved", exc,
                                                element=mapping,
                                            )
                                    elif m_sub.element_name == "PDU-REF":
                                        self._fail(
                                            "ARXML102", "CAN frame '%s' PDU-REF" % path,
                                            "reference cannot be resolved", element=mapping,
                                        )
                                if not mapping_has_pdu:
                                    self._fail(
                                        "ARXML102", "CAN frame '%s' mapping" % path,
                                        "required PDU-REF is missing", element=mapping,
                                    )

                if dlc is None:
                    self._fail(
                        "ARXML112", "CAN frame '%s'" % path,
                        "required FRAME-LENGTH is missing", element=elem,
                    )
                if not mapped_pdu:
                    self._fail(
                        "ARXML113", "CAN frame '%s'" % path,
                        "has no PDU-to-frame routing", element=elem,
                    )
                self._frame_dlcs[path] = dlc

        # Resolve PDU → CAN ID via PDU → frame → CAN ID
        for pdu_path, frame_path in self._pdu_to_frame.items():
            if frame_path in self._frame_can_ids:
                self._pdu_to_can_id[pdu_path] = self._frame_can_ids[frame_path]
            if frame_path in self._frame_dlcs:
                self._pdu_to_dlc[pdu_path] = self._frame_dlcs[frame_path]

    # ------------------------------------------------------------------
    # Build ECUs with PDUs and signals
    # ------------------------------------------------------------------

    def _build_ecus(self, ecu_names: dict[str, str]) -> dict[str, Ecu]:
        """Build ECU objects with TX/RX PDUs from ARXML + DBC routing."""
        # Only build ECUs that are in the config
        ecus: dict[str, Ecu] = {}
        for ecu_name, ecu_cfg in self.config.ecus.items():
            ecus[ecu_name] = Ecu(name=ecu_name, prefix=ecu_cfg.prefix)

        if ecu_name not in ecu_names:
            # ECU not in ARXML — might be fine (sidecar-only)
            pass

        # Extract all IPdus with their signals
        all_pdus: dict[str, Pdu] = {}  # pdu_path → Pdu
        for path, elem in self.model.identifiable_elements:
            if elem.element_name != "I-SIGNAL-I-PDU":
                continue

            pdu_name = elem.item_name
            pdu_length = None

            signals = []
            for sub in elem.sub_elements:
                if sub.element_name == "LENGTH":
                    try:
                        pdu_length = int(sub.character_data)
                    except (TypeError, ValueError) as exc:
                        self._fail(
                            "ARXML104", "I-PDU '%s' length" % path,
                            "value is not an integer", exc, element=elem,
                        )
                elif sub.element_name in ("I-SIGNAL-TO-PDU-MAPPINGS", "I-SIGNAL-TO-I-PDU-MAPPINGS"):
                    for mapping in sub.sub_elements:
                        sig = self._parse_signal_mapping(mapping)
                        if sig:
                            signals.append(sig)

            if pdu_length is None:
                self._fail(
                    "ARXML112", "I-PDU '%s'" % path,
                    "required LENGTH is missing", element=elem,
                )
            if path not in self._pdu_to_can_id or path not in self._pdu_to_dlc:
                self._fail(
                    "ARXML113", "I-PDU '%s'" % path,
                    "has no complete CAN frame routing", element=elem,
                )
            can_id = self._pdu_to_can_id[path]
            dlc = self._pdu_to_dlc[path]

            # Check E2E: if any signal name contains E2E_DataID, mark PDU
            e2e = any("E2E_DataID" in s.name for s in signals)
            e2e_data_id = None
            e2e_counter_bit = None
            e2e_crc_bit = None
            if e2e:
                for s in signals:
                    if "E2E_DataID" in s.name:
                        e2e_data_id = s.init_value if s.init_value else None
                    elif "E2E_AliveCounter" in s.name:
                        e2e_counter_bit = s.bit_position
                    elif "E2E_CRC8" in s.name:
                        e2e_crc_bit = s.bit_position
                for s in signals:
                    s.e2e_protected = True

            pdu_cycle = self._dbc_cycle_map.get(pdu_name, 0)
            if pdu_cycle == 0 and can_id > 0:
                # Try stripping _Ipdu or _Frame suffix
                for suffix in ("_Ipdu", "_Frame", "_Pdu"):
                    stripped = pdu_name.replace(suffix, "")
                    if stripped in self._dbc_cycle_map:
                        pdu_cycle = self._dbc_cycle_map[stripped]
                        break
                # pdu_cycle == 0 means event-triggered (DTC, UDS, etc.) — no throttle

            # TX mode: sidecar override wins over DBC GenMsgSendType=event;
            # empty string keeps the legacy cycle_ms heuristic in the template
            pdu_tx_mode = self._msg_attr_lookup(pdu_name, self._tx_mode_overrides) or ""
            if not pdu_tx_mode:
                send_type = self._msg_attr_lookup(pdu_name, self._dbc_send_type_map) or ""
                if send_type == "event":
                    pdu_tx_mode = "DIRECT"
                elif send_type == "cyclic":
                    pdu_tx_mode = "PERIODIC"

            pdu = Pdu(
                name=pdu_name,
                can_id=can_id,
                dlc=dlc,
                cycle_ms=pdu_cycle,
                tx_mode=pdu_tx_mode,
                signals=sorted(signals, key=lambda s: s.bit_position),
                e2e_protected=e2e,
                e2e_data_id=e2e_data_id,
                e2e_counter_bit=e2e_counter_bit,
                e2e_crc_bit=e2e_crc_bit,
                satisfies=self._dbc_satisfies_map.get(pdu_name, ""),
                asil=self._dbc_asil_map.get(pdu_name, "QM"),
            )
            all_pdus[path] = pdu

        # Route PDUs to ECUs using DBC TX map
        for pdu_path, pdu in all_pdus.items():
            tx_ecu = self._dbc_tx_map.get(pdu.name)
            if not tx_ecu:
                # Try matching by frame name (strip _Frame suffix)
                frame_path = self._pdu_to_frame.get(pdu_path)
                if frame_path:
                    frame_name = frame_path.rsplit("/", 1)[-1]
                    base = frame_name.replace("_Frame", "")
                    tx_ecu = self._dbc_tx_map.get(base)

            if not tx_ecu:
                continue

            tx_ecu_lower = tx_ecu.lower()

            # Assign as TX to sender
            if tx_ecu_lower in ecus:
                tx_pdu = Pdu(
                    name=pdu.name,
                    pdu_id=len(ecus[tx_ecu_lower].tx_pdus),
                    can_id=pdu.can_id,
                    dlc=pdu.dlc,
                    cycle_ms=pdu.cycle_ms,
                    tx_mode=pdu.tx_mode,
                    direction="TX",
                    signals=pdu.signals,
                    e2e_protected=pdu.e2e_protected,
                    e2e_data_id=pdu.e2e_data_id,
                    e2e_counter_bit=pdu.e2e_counter_bit,
                    e2e_crc_bit=pdu.e2e_crc_bit,
                )
                ecus[tx_ecu_lower].tx_pdus.append(tx_pdu)

            # Assign as RX to all other configured ECUs
            for ecu_name, ecu in ecus.items():
                if ecu_name == tx_ecu_lower:
                    continue
                rx_pdu = Pdu(
                    name=pdu.name,
                    pdu_id=len(ecu.rx_pdus),
                    can_id=pdu.can_id,
                    dlc=pdu.dlc,
                    direction="RX",
                    signals=pdu.signals,
                    e2e_protected=pdu.e2e_protected,
                    e2e_data_id=pdu.e2e_data_id,
                    e2e_counter_bit=pdu.e2e_counter_bit,
                    e2e_crc_bit=pdu.e2e_crc_bit,
                )
                ecu.rx_pdus.append(rx_pdu)

        # Apply routing overrides: add TX PDUs for additional senders
        for pdu_path, pdu in all_pdus.items():
            extra_ecus = self._additional_tx_ecus.get(pdu.name, [])
            for extra_ecu in extra_ecus:
                ecu_lower = extra_ecu.lower()
                if ecu_lower not in ecus:
                    continue
                # Skip if already a TX PDU for this ECU
                if any(p.name == pdu.name for p in ecus[ecu_lower].tx_pdus):
                    continue
                tx_pdu = Pdu(
                    name=pdu.name,
                    pdu_id=len(ecus[ecu_lower].tx_pdus),
                    can_id=pdu.can_id,
                    dlc=pdu.dlc,
                    cycle_ms=pdu.cycle_ms,
                    tx_mode=pdu.tx_mode,
                    direction="TX",
                    signals=pdu.signals,
                    e2e_protected=pdu.e2e_protected,
                    e2e_data_id=pdu.e2e_data_id,
                    e2e_counter_bit=pdu.e2e_counter_bit,
                    e2e_crc_bit=pdu.e2e_crc_bit,
                )
                ecus[ecu_lower].tx_pdus.append(tx_pdu)

        # Sort PDUs by CAN ID and reassign PDU IDs
        for ecu in ecus.values():
            ecu.tx_pdus.sort(key=lambda p: p.can_id)
            ecu.rx_pdus.sort(key=lambda p: p.can_id)
            for i, pdu in enumerate(ecu.tx_pdus):
                pdu.pdu_id = i
            for i, pdu in enumerate(ecu.rx_pdus):
                pdu.pdu_id = i

            # Auto-populate E2E DataIDs from DBC for ALL E2E-protected PDUs
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                if pdu.e2e_data_id is not None:
                    # Generate define name: <ECU>_E2E_<MSG>_DATA_ID
                    safe_name = pdu.name.upper().replace(" ", "_").replace("-", "_")
                    define_name = f"{ecu.prefix}_E2E_{safe_name}_DATA_ID"
                    ecu.e2e_data_ids[define_name] = pdu.e2e_data_id

            # Build flat signal list
            seen = set()
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                for sig in pdu.signals:
                    if sig.name not in seen:
                        ecu.all_signals.append(sig)
                        seen.add(sig.name)

        return ecus

    def _parse_signal_mapping(self, mapping_elem) -> Signal | None:
        """Parse an I-SIGNAL-TO-I-PDU-MAPPING element into a Signal."""
        name = ""
        bit_pos = None
        sig_length = None
        signal_ref_seen = False
        byte_order = "little_endian"

        for sub in mapping_elem.sub_elements:
            if sub.element_name == "SHORT-NAME":
                name = sub.character_data or ""
            elif sub.element_name == "START-POSITION":
                try:
                    bit_pos = int(sub.character_data)
                except (TypeError, ValueError) as exc:
                    self._fail(
                        "ARXML105", "signal mapping '%s' start position" % name,
                        "value is not an integer", exc, element=mapping_elem,
                    )
            elif sub.element_name == "PACKING-BYTE-ORDER":
                if sub.character_data and "MOST-SIGNIFICANT-BYTE-FIRST" in sub.character_data:
                    byte_order = "big_endian"
            elif sub.element_name == "I-SIGNAL-REF":
                signal_ref_seen = True
                if not sub.is_reference:
                    self._fail(
                        "ARXML102", "signal mapping '%s' I-SIGNAL-REF" % name,
                        "reference cannot be resolved", element=mapping_elem,
                    )
                try:
                    isignal = sub.reference_target
                    for isub in isignal.sub_elements:
                        if isub.element_name == "LENGTH":
                            sig_length = int(isub.character_data)
                except Exception as exc:
                    self._fail(
                        "ARXML102", "signal mapping '%s' I-SIGNAL-REF" % name,
                        "reference cannot be resolved", exc, element=mapping_elem,
                    )

        if not name:
            self._fail(
                "ARXML112", "I-SIGNAL mapping",
                "required SHORT-NAME is missing", element=mapping_elem,
            )
        if bit_pos is None:
            self._fail(
                "ARXML112", "signal mapping '%s'" % name,
                "required START-POSITION is missing", element=mapping_elem,
            )
        if not signal_ref_seen:
            self._fail(
                "ARXML102", "signal mapping '%s'" % name,
                "required I-SIGNAL-REF is missing", element=mapping_elem,
            )
        if sig_length is None:
            self._fail(
                "ARXML112", "signal mapping '%s' referenced I-SIGNAL" % name,
                "required LENGTH is missing", element=mapping_elem,
            )

        # Determine C data type from bit size
        data_type = _bits_to_c_type(sig_length)

        return Signal(
            name=name,
            bit_position=bit_pos,
            bit_size=sig_length,
            byte_order=byte_order,
            data_type=data_type,
        )

    # ------------------------------------------------------------------
    # SWCs
    # ------------------------------------------------------------------

    def _load_explicit_mapping_index(self):
        """Index exact mapped ownership and signal identity from the sidecar."""
        sidecar = _load_sidecar_yaml(self.config.sidecar_path)
        section = sidecar.get("swc_signal_mapping") if isinstance(sidecar, Mapping) else None
        if not isinstance(section, Mapping):
            return
        for ecu_name, ecu_data in section.get("ecus", {}).items():
            for swc_name, swc_data in ecu_data.get("swcs", {}).items():
                short_name = swc_data.get("arxml_short_name")
                if isinstance(short_name, str):
                    self._mapped_swcs[short_name] = (str(ecu_name).lower(), str(swc_name))
                for port in swc_data.get("ports", ()):
                    values = (short_name, port.get("name"), port.get("message"), port.get("signal"))
                    if all(isinstance(value, str) for value in values):
                        self._mapped_ports[(values[0], values[1])] = (values[2], values[3])

    def _extract_swcs(self, ecus: dict[str, Ecu]):
        """Extract SWC types from ARXML and assign to ECUs by naming convention."""
        for path, elem in self.model.identifiable_elements:
            if elem.element_name != "APPLICATION-SW-COMPONENT-TYPE":
                continue

            short_name = elem.item_name  # e.g., "CVC_Swc_Pedal"

            # Determine owning ECU from prefix: "CVC_Swc_Pedal" → "cvc"
            mapped_owner = self._mapped_swcs.get(short_name)
            if mapped_owner is None:
                self._fail(
                    "ARXML106", "SWC '%s'" % short_name,
                    "has no exact explicit mapping owner",
                )

            # Strip ECU prefix to get clean SWC name
            ecu_name, swc_name = mapped_owner

            # Extract ports
            ports = []
            for sub in elem.sub_elements:
                if sub.element_name == "PORTS":
                    for port_elem in sub.sub_elements:
                        port = self._parse_port(port_elem, short_name)
                        if port:
                            ports.append(port)

            # Extract runnables and events from internal behavior
            runnables = []
            for sub in elem.sub_elements:
                if sub.element_name == "INTERNAL-BEHAVIORS":
                    for beh in sub.sub_elements:
                        if beh.element_name == "SWC-INTERNAL-BEHAVIOR":
                            runnables = self._parse_behavior(beh)

            # Detect ASIL from admin data or default
            asil = self._detect_asil(elem)

            swc = Swc(
                name=swc_name,
                short_name=short_name,
                asil=asil,
                ports=ports,
                runnables=runnables,
            )
            ecus[ecu_name].swcs.append(swc)

    def _parse_port(self, port_elem, swc_short_name="") -> Port | None:
        """Parse a P-PORT-PROTOTYPE or R-PORT-PROTOTYPE."""
        elem_name = port_elem.element_name
        if elem_name not in ("P-PORT-PROTOTYPE", "R-PORT-PROTOTYPE"):
            return None

        direction = "PROVIDED" if elem_name == "P-PORT-PROTOTYPE" else "REQUIRED"
        name = port_elem.item_name or ""
        interface_name = ""

        for sub in port_elem.sub_elements:
            ref_names = ("PROVIDED-INTERFACE-TREF", "REQUIRED-INTERFACE-TREF")
            if sub.element_name in ref_names and sub.is_reference:
                try:
                    interface_name = sub.reference_target.item_name
                except Exception as exc:
                    self._fail(
                        "ARXML102", "port '%s' interface reference" % name,
                        "reference cannot be resolved", exc,
                    )

        # Derive signal name from interface: "SRI_PedalRaw1" → "PedalRaw1"
        mapped_identity = self._mapped_ports.get((swc_short_name, name))
        if mapped_identity is None:
            self._fail(
                "ARXML108", "port '%s/%s'" % (swc_short_name, name),
                "has no exact explicit mapping identity",
            )

        return Port(
            name=name,
            direction=direction,
            interface_name=interface_name,
            signal_name=mapped_identity[1],
            message_name=mapped_identity[0],
        )

    def _parse_behavior(self, beh_elem) -> list[Runnable]:
        """Parse SWC-INTERNAL-BEHAVIOR for runnables and events."""
        runnables_map: dict[str, Runnable] = {}  # path → Runnable

        # First pass: extract runnable entities
        for sub in beh_elem.sub_elements:
            if sub.element_name == "RUNNABLES":
                for run_elem in sub.sub_elements:
                    if run_elem.element_name == "RUNNABLE-ENTITY":
                        name = run_elem.item_name or ""
                        runnables_map[run_elem.path] = Runnable(name=name)

        # Second pass: extract events to set period and is_init
        for sub in beh_elem.sub_elements:
            if sub.element_name == "EVENTS":
                for evt in sub.sub_elements:
                    runnable_ref_path = None
                    period = 0

                    for evt_sub in evt.sub_elements:
                        if evt_sub.element_name == "START-ON-EVENT-REF" and evt_sub.is_reference:
                            try:
                                runnable_ref_path = evt_sub.reference_target.path
                            except Exception as exc:
                                self._fail(
                                    "ARXML102", "event '%s' runnable reference" % evt.path,
                                    "reference cannot be resolved", exc,
                                )
                        elif evt_sub.element_name == "PERIOD":
                            try:
                                period = float(evt_sub.character_data)
                            except (TypeError, ValueError) as exc:
                                self._fail(
                                    "ARXML107", "event '%s' period" % evt.path,
                                    "value is not numeric", exc,
                                )

                    if runnable_ref_path and runnable_ref_path in runnables_map:
                        r = runnables_map[runnable_ref_path]
                        if evt.element_name == "TIMING-EVENT":
                            r.period_ms = int(period * 1000)
                        elif evt.element_name == "INIT-EVENT":
                            r.is_init = True

        # Filter out legacy Com bridge runnables — replaced by TX auto-pull
        # and RX auto-push in Com_MainFunction_Tx / Com_RxIndication.
        # Pattern: *Com_Receive, *Com_TransmitSchedule
        filtered = [r for r in runnables_map.values()
                    if not (r.name.endswith("Com_Receive") or
                            r.name.endswith("Com_TransmitSchedule"))]
        return filtered

    def _detect_asil(self, swc_elem) -> str:
        """Detect ASIL level from ADMIN-DATA annotations. Default QM."""
        for depth, elem in swc_elem.elements_dfs:
            if elem.element_name == "ADMIN-DATA":
                for _, inner in elem.elements_dfs:
                    cd = inner.character_data or ""
                    if cd in ("A", "B", "C", "D", "QM"):
                        return cd
        return "QM"

    # ------------------------------------------------------------------
    # Signal ID assignment
    # ------------------------------------------------------------------

    def _assign_signal_ids(self, ecus: dict[str, Ecu], bsw_reserved: int = 16):
        """Assign RTE and Com signal IDs per ECU."""
        for ecu in ecus.values():
            # RTE signal IDs: BSW reserved 0..(N-1), then ECU signals sorted by name
            sig_names = sorted(set(s.name for s in ecu.all_signals))
            rte_id = bsw_reserved
            for name in sig_names:
                ecu.rte_signal_map[name] = rte_id
                rte_id += 1

            # Com signal IDs: sequential by PDU order, then bit position
            com_id = 0
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                for sig in pdu.signals:
                    if sig.name not in ecu.com_signal_map:
                        ecu.com_signal_map[sig.name] = com_id
                        com_id += 1

    # ------------------------------------------------------------------
    # Sidecar
    # ------------------------------------------------------------------

    def _read_sidecar(self, ecus: dict[str, Ecu]):
        """Merge sidecar YAML into existing ECU models."""
        sidecar = _load_sidecar_yaml(self.config.sidecar_path)

        if not isinstance(sidecar, Mapping):
            self._fail(
                "SIDECAR003", "sidecar root", "expected a YAML mapping"
            )

        sidecar_ecus = sidecar.get("ecus", {})

        # Collect all pdu_e2e_map entries into a global map first,
        # so broadcast CAN RX PDUs on other ECUs also get their data IDs.
        global_pdu_e2e_map: dict[str, int] = {}

        for ecu_name, ecu_data in sidecar_ecus.items():
            if ecu_name not in ecus:
                self._fail(
                    "SIDECAR004", "sidecar ECU '%s'" % ecu_name,
                    "ECU is not present in project configuration",
                )

            ecu = ecus[ecu_name]

            # DTC events
            if "dtc_events" in ecu_data:
                ecu.dtc_events.update(ecu_data["dtc_events"])

            # UDS diagnostic PDU names (for PduR CanTp routing)
            if "uds_rx_pdu_name" in ecu_data:
                ecu.uds_rx_pdu_name = ecu_data["uds_rx_pdu_name"]
            if "uds_tx_pdu_name" in ecu_data:
                ecu.uds_tx_pdu_name = ecu_data["uds_tx_pdu_name"]

            # RTE/Com MainFunction period (from *_RTE_PERIOD_MS in thresholds or constants)
            for section in ("thresholds", "constants"):
                data = ecu_data.get(section, {})
                rte_key = f"{ecu.prefix}_RTE_PERIOD_MS"
                if rte_key in data:
                    ecu.rte_period_ms = int(data[rte_key])
                    break

            # E2E data IDs — store named constants in ECU dict
            if "e2e_data_ids" in ecu_data:
                ecu.e2e_data_ids.update(ecu_data["e2e_data_ids"])

            # Collect PDU → E2E data ID mappings into global map
            if "pdu_e2e_map" in ecu_data:
                global_pdu_e2e_map.update(ecu_data["pdu_e2e_map"])

            # E2E failure → DTC event mapping (PDU name → DTC event name)
            if "e2e_dem_map" in ecu_data:
                e2e_dem = ecu_data["e2e_dem_map"]
                for pdu in ecu.rx_pdus:
                    if pdu.name in e2e_dem:
                        dtc_name = e2e_dem[pdu.name]
                        if dtc_name in ecu.dtc_events:
                            pdu.e2e_dem_event_id = ecu.dtc_events[dtc_name]

            # Comm status RTE signal mapping (PDU name → RTE signal name)
            # When RX deadline fires, write COMM_TIMEOUT to this RTE signal
            if "comm_status_map" in ecu_data:
                cs_map = ecu_data["comm_status_map"]
                for pdu in ecu.rx_pdus:
                    if pdu.name in cs_map:
                        sig_name = cs_map[pdu.name]
                        if sig_name in ecu.rte_signal_map:
                            pdu.comm_status_rte_signal_id = ecu.rte_signal_map[sig_name]

            # Enums
            if "enums" in ecu_data:
                ecu.enums.update(ecu_data["enums"])

            # Thresholds
            if "thresholds" in ecu_data:
                ecu.thresholds.update(ecu_data["thresholds"])

            # RTE aliases — map application-level names to generated signal names
            if "rte_aliases" in ecu_data:
                ecu.rte_aliases.update(ecu_data["rte_aliases"])

            # ECU-internal RTE signals (named list or legacy count)
            if "rte_internal_signals" in ecu_data:
                ecu.rte_internal_signals = list(ecu_data["rte_internal_signals"])
                ecu.rte_internal_signal_count = len(ecu.rte_internal_signals)
            elif "rte_internal_signal_count" in ecu_data:
                ecu.rte_internal_signal_count = int(ecu_data["rte_internal_signal_count"])

            # OS section — per-ECU OSEK config (S-OS-11 schema extension).
            # Absent section keeps EcuOsConfig defaults (backward compatible).
            if "os" in ecu_data:
                os_data = ecu_data["os"] or {}
                ecu.os.bsw_slot_periods = [
                    int(p) for p in (os_data.get("bsw_slot_periods") or [])
                ]
                ecu.os.default_task_stack_bytes = int(
                    os_data.get("default_task_stack_bytes", 1024)
                )
                ecu.os.task_stack_bytes = {
                    int(k): int(v)
                    for k, v in (os_data.get("task_stack_bytes") or {}).items()
                }
                scheduling = str(os_data.get("scheduling", "FULL")).upper()
                if scheduling not in ("FULL", "NON"):
                    self._fail(
                        "SIDECAR005", "sidecar ECU '%s' os.scheduling" % ecu_name,
                        "unsupported value '%s'; expected FULL or NON" % scheduling,
                    )
                ecu.os.scheduling = scheduling
                ecu.os.applications = list(os_data.get("applications") or [])

            # Extract Com_MainFunction_Rx period for E2E param computation
            if "runnables" in ecu_data:
                com_rx = ecu_data["runnables"].get("Com_MainFunction_Rx", {})
                if "period_ms" in com_rx:
                    ecu.com_rx_period_ms = int(com_rx["period_ms"])

            # Runnable scheduling — override ARXML runnables or create from sidecar
            if "runnables" in ecu_data:
                runnable_overrides = ecu_data["runnables"]
                # Try to override existing ARXML-defined runnables
                found_names = set()
                for swc in ecu.swcs:
                    for r in swc.runnables:
                        if r.name in runnable_overrides:
                            found_names.add(r.name)
                            override = runnable_overrides[r.name]
                            if "priority" in override:
                                r.priority = override["priority"]
                            if "wdgm_se_id" in override:
                                r.wdgm_se_id = override["wdgm_se_id"]
                            if "period_ms" in override:
                                r.period_ms = override["period_ms"]

                # Create runnables from sidecar when ARXML has none
                # Skip legacy Com bridge runnables (replaced by auto-pull/push)
                # unless the sidecar entry carries `keep: true` — used for
                # load-bearing bridge runnables (Swc_FzcCom_Receive resets
                # the CanMon silence counter; commit f22eb15).
                _skip_com_bridge = lambda n: n.endswith("Com_Receive") or n.endswith("Com_TransmitSchedule")
                _kept = lambda d: isinstance(d, Mapping) and bool(d.get("keep", False))
                missing = set(runnable_overrides.keys()) - found_names
                if missing:
                    sidecar_runnables = []
                    for rname in runnable_overrides:
                        if rname not in found_names and (
                                not _skip_com_bridge(rname)
                                or _kept(runnable_overrides[rname])):
                            rdata = runnable_overrides[rname]
                            sidecar_runnables.append(Runnable(
                                name=rname,
                                period_ms=rdata.get("period_ms", 10),
                                priority=rdata.get("priority", 5),
                                wdgm_se_id=rdata.get("wdgm_se_id", 0xFF),
                            ))
                    # Attach to existing SWC or create a synthetic one
                    if ecu.swcs:
                        ecu.swcs[0].runnables.extend(sidecar_runnables)
                    else:
                        ecu.swcs.append(Swc(
                            name=f"Swc_{ecu_name.upper()}",
                            short_name=ecu_name,
                            runnables=sidecar_runnables,
                        ))

        # Apply the global E2E map across ALL ECUs (handles broadcast CAN)
        # Only when e2e_source is "sidecar" — DBC mode uses _apply_dbc_e2e instead
        if global_pdu_e2e_map and self.config.e2e_source == "sidecar":
            for ecu in ecus.values():
                self._apply_pdu_e2e_map(ecu, global_pdu_e2e_map)

    # ------------------------------------------------------------------
    # E2E data ID mapping
    # ------------------------------------------------------------------

    def _apply_pdu_e2e_map(self, ecu, pdu_map: dict):
        """Apply explicit PDU name → E2E data ID mapping from sidecar.

        The pdu_e2e_map section maps PDU names directly to data IDs:
          pdu_e2e_map:
            EStop_Broadcast: 0x01
            CVC_Heartbeat: 0x04
        """
        for pdu in ecu.tx_pdus + ecu.rx_pdus:
            if pdu.name in pdu_map:
                pdu.e2e_data_id = pdu_map[pdu.name]
            elif pdu.e2e_protected and pdu.e2e_data_id is None:
                self._fail(
                    "SIDECAR006", "E2E PDU '%s'" % pdu.name,
                    "has no data ID in sidecar pdu_e2e_map",
                )

    def _apply_arxml_e2e(self, ecus: dict[str, Ecu]):
        """Apply E2E data IDs from ARXML END-TO-END-PROTECTION-SET.

        Reads the structured E2E protection elements generated by dbc2arxml.py.
        This is the proper AUTOSAR flow: DBC → ARXML → codegen.
        The ARXML END-TO-END-PROTECTION-SET contains per-message:
          - DATA-ID (E2E DataID value)
          - MAX-DELTA-COUNTER-INIT
          - CRC-OFFSET, COUNTER-OFFSET, DATA-ID-NIBBLE-OFFSET
          - ADMIN-DATA/SDGS with ASIL, Satisfies, CAN_ID, I-PDU-PATH

        @traces_to Phase 1 codegen — reads ARXML not DBC for E2E params
        """
        import autosar_data as ar

        # Parse ARXML for END-TO-END-PROTECTION elements
        e2e_map = {}  # pdu_name → data_id
        e2e_max_delta = {}  # pdu_name → max_delta

        model = self.model
        if model is None:
            self._fail(
                "ARXML110", "ARXML E2E model", "no model is loaded"
            )

        count = 0
        for item in model.elements_dfs:
            # elements_dfs returns (depth, element) tuples
            if isinstance(item, tuple):
                elem = item[1]
            else:
                elem = item
            if not hasattr(elem, 'element_name'):
                continue
            if str(elem.element_name) != "END-TO-END-PROTECTION":
                continue

            # Extract SHORT-NAME (e.g., "E2E_EStop_Broadcast")
            short_name = None
            pdu_name = None
            data_id = None
            max_delta = 2  # default

            for sub in elem.sub_elements:
                sname = str(sub.element_name) if hasattr(sub, 'element_name') else ""

                if sname == "SHORT-NAME":
                    short_name = sub.character_data or ""
                    # Strip "E2E_" prefix to get PDU name
                    if short_name.startswith("E2E_"):
                        pdu_name = short_name[4:]

                elif sname == "END-TO-END-PROFILE":
                    for prof_sub in sub.sub_elements:
                        psname = str(prof_sub.element_name) if hasattr(prof_sub, 'element_name') else ""
                        if psname == "DATA-IDS":
                            for did_sub in prof_sub.sub_elements:
                                dsname = str(did_sub.element_name) if hasattr(did_sub, 'element_name') else ""
                                if dsname == "DATA-ID":
                                    try:
                                        data_id = int(did_sub.character_data)
                                    except (TypeError, ValueError) as exc:
                                        self._fail(
                                            "ARXML108", "E2E protection '%s' DATA-ID" % (pdu_name or short_name),
                                            "value is not an integer", exc,
                                        )
                        elif psname == "MAX-DELTA-COUNTER-INIT":
                            try:
                                max_delta = int(prof_sub.character_data)
                            except (TypeError, ValueError) as exc:
                                self._fail(
                                    "ARXML109", "E2E protection '%s' MAX-DELTA-COUNTER-INIT" % (pdu_name or short_name),
                                    "value is not an integer", exc,
                                )

            if pdu_name and data_id is not None:
                e2e_map[pdu_name] = data_id
                e2e_max_delta[pdu_name] = max_delta
                count += 1

        # Apply to ECU PDUs
        applied = 0
        for ecu in ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                if pdu.name in e2e_map:
                    pdu.e2e_data_id = e2e_map[pdu.name]
                    pdu.e2e_protected = True
                    pdu.e2e_max_delta = e2e_max_delta.get(pdu.name, 2)
                    applied += 1

        _info(f"  E2E source: ARXML END-TO-END-PROTECTION-SET ({count} protections, {applied} PDUs matched)")

    def _apply_dbc_e2e(self, ecus: dict[str, Ecu]):
        """Apply E2E data IDs from DBC E2E_DataID attribute.

        Uses the _dbc_e2e_map populated during _load_dbc_routing().
        Only messages with an explicit E2E_DataID >= 0 get a data ID.
        This is the OEM-aligned approach: CAN matrix → DBC → codegen.
        """
        for ecu in ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                if pdu.name in self._dbc_e2e_map:
                    pdu.e2e_data_id = self._dbc_e2e_map[pdu.name]
                    pdu.e2e_max_delta = self._dbc_e2e_max_delta.get(pdu.name, 2)
                elif pdu.e2e_protected and pdu.e2e_data_id is None:
                    self._fail(
                        "DBC006", "E2E PDU '%s'" % pdu.name,
                        "has no E2E_DataID attribute in the DBC",
                    )

    def _compute_e2e_params(self, ecus: dict[str, Ecu]):
        """Derive E2E MaxDeltaCounter and RX timeout from scheduler periods.

        AUTOSAR approach: E2E parameters are computed from the relationship
        between TX cycle time (DBC GenMsgCycleTime) and RX poll period
        (Com_MainFunction_Rx call rate from sidecar runnables).

        MaxDeltaCounter = ceil(tx_cycle_ms / rx_poll_ms) + margin
            - Allows for one full TX cycle of jitter before E2E rejects
            - margin=2 covers CAN arbitration delay + scheduler jitter

        timeout_ms = tx_cycle_ms * 3
            - Standard AUTOSAR: 3× the TX period before declaring timeout
            - Sufficient for bus-off recovery (1 retry cycle)

        This makes E2E params independent of the OS — changing the scheduler
        period in sidecar.yaml and re-running the pipeline auto-adjusts.
        """
        import math

        updated = 0
        for ecu in ecus.values():
            rx_poll_ms = ecu.com_rx_period_ms
            if rx_poll_ms <= 0:
                continue

            for pdu in ecu.rx_pdus:
                if not pdu.e2e_protected:
                    continue
                # RX PDUs have cycle_ms=0 (receiver side) — look up TX cycle from DBC
                tx_cycle_ms = self._dbc_cycle_map.get(pdu.name, pdu.cycle_ms)
                if tx_cycle_ms <= 0:
                    continue

                # MaxDeltaCounter: how many RX polls fit in one TX cycle + margin
                computed_delta = math.ceil(tx_cycle_ms / rx_poll_ms) + 2
                # Clamp to 4-bit alive counter range (0-15)
                computed_delta = min(computed_delta, 14)

                # RX timeout: 3× the TX period (AUTOSAR default)
                computed_timeout = tx_cycle_ms * 3

                # E2E SM window params: derived from FTTI / cycle_time
                # WindowSizeValid: consecutive OKs to recover from INVALID
                #   Default: 3 (standard AUTOSAR), raise for fast messages
                # WindowSizeInvalid: consecutive errors before INVALID
                #   Default: max(3, 100ms / cycle_ms) — tolerate jitter
                computed_sm_valid = 3
                computed_sm_invalid = max(3, math.ceil(100.0 / tx_cycle_ms))

                # Only override if computed value differs from DBC default
                if pdu.e2e_max_delta != computed_delta:
                    pdu.e2e_max_delta = computed_delta
                    updated += 1

                if pdu.e2e_sm_window_valid == 0:
                    pdu.e2e_sm_window_valid = computed_sm_valid
                if pdu.e2e_sm_window_invalid == 0:
                    pdu.e2e_sm_window_invalid = computed_sm_invalid

                if pdu.timeout_ms == 0:
                    pdu.timeout_ms = computed_timeout

        if updated > 0:
            _info(f"  E2E params: {updated} RX PDUs updated "
                  f"(MaxDeltaCounter derived from scheduler periods)")

    def _apply_dbc_traceability(self, ecus: dict[str, Ecu]):
        """Apply Satisfies and ASIL from DBC attributes to PDU objects."""
        applied = 0
        for ecu in ecus.values():
            for pdu in ecu.tx_pdus + ecu.rx_pdus:
                if pdu.name in self._dbc_satisfies_map:
                    pdu.satisfies = self._dbc_satisfies_map[pdu.name]
                    applied += 1
                if pdu.name in self._dbc_asil_map:
                    pdu.asil = self._dbc_asil_map[pdu.name]
        if applied > 0:
            _info(f"  Traceability: {applied} PDUs linked to requirements")

    # ------------------------------------------------------------------
    # Port type resolution
    # ------------------------------------------------------------------

    def _resolve_port_types(self, ecus: dict[str, Ecu]):
        """Resolve Port.data_type from the matching CAN signal's type.

        Port signal_name comes from the S/R interface (e.g., "VehicleState").
        CAN signal names are PDU-prefixed (e.g., "Vehicle_State_VehicleState").
        Match by stripping the PDU name prefix from signal names.
        """
        for ecu in ecus.values():
            # Two lookups: exact match and suffix match (strip PDU prefix)
            for swc in ecu.swcs:
                for port in swc.ports:
                    mapped_type = self._dbc_signal_types.get(
                        (port.message_name, port.signal_name)
                    )
                    if mapped_type is not None:
                        port.data_type = mapped_type

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _warn(self, msg: str, code: str = "PIPELINE001", source: str = "ARXML reader"):
        path, line, column = self._source_location(source)
        diagnostic = self.diagnostics.warning(
            code, source, msg, path=path, line=line, column=column
        )
        self.warnings.append(str(diagnostic))

    def _element_location(self, element):
        if element is None:
            return "", None, None
        membership = getattr(element, "file_membership", (False, frozenset()))
        files = membership[1] if len(membership) > 1 else frozenset()
        filenames = sorted(
            str(getattr(item, "filename", "")) for item in files
            if getattr(item, "filename", "")
        )
        if not filenames:
            return "", None, None

        cursor = element
        needle = ""
        while cursor is not None:
            try:
                needle = getattr(cursor, "item_name", "") or ""
            except Exception as exc:
                needle = ""
                _ = exc
            if needle:
                break
            cursor = getattr(cursor, "parent", None)

        filename = filenames[0]
        if needle:
            location = locate_text(
                filename, "<SHORT-NAME>%s</SHORT-NAME>" % needle
            )
            if location[1] is not None:
                return location
            return locate_text(filename, needle)
        return portable_path(filename), None, None

    def _source_location(self, source, element=None, location_path=None):
        element_location = self._element_location(element)
        if element_location[0]:
            return element_location

        if location_path:
            candidate = location_path
        elif source.lower().startswith("sidecar") and self.config.sidecar_path:
            candidate = self.config.sidecar_path
        elif source.startswith("DBC") and self.config.dbc_path:
            candidate = self.config.dbc_path
        elif self.config.arxml_paths:
            candidate = self.config.arxml_paths[0]
        else:
            return "", None, None

        quoted = re.findall(r"'([^']+)'", source)
        needle = quoted[-1].rsplit("/", 1)[-1] if quoted else ""
        if needle:
            location = locate_text(candidate, needle)
            if location[1] is not None:
                return location
        return portable_path(candidate), None, None

    def _portable_exception(self, exc):
        message = str(exc)
        paths = list(self.config.arxml_paths)
        paths.extend(
            path for path in (self.config.dbc_path, self.config.sidecar_path) if path
        )
        for path in paths:
            replacement = portable_path(path)
            message = message.replace(str(path), replacement)
            message = message.replace(str(path).replace("\\", "/"), replacement)
        return message

    def _fail(
        self,
        code: str,
        source: str,
        message: str,
        exc=None,
        *,
        element=None,
        location_path=None,
    ):
        path, line, column = self._source_location(
            source, element=element, location_path=location_path
        )
        detail = self._portable_exception(exc) if exc is not None else ""
        if exc is not None and line is None:
            coordinates = re.search(
                r"line\s+(\d+)(?:,|:)\s*column\s+(\d+)", detail,
                flags=re.IGNORECASE,
            )
            if coordinates:
                line = int(coordinates.group(1))
                column = int(coordinates.group(2))
            else:
                coordinates = re.search(
                    r"\b[^:\s]+:(\d+)(?::(\d+))?:", detail
                )
                if coordinates:
                    line = int(coordinates.group(1))
                    column = (
                        int(coordinates.group(2))
                        if coordinates.group(2) is not None
                        else None
                    )
        diagnostic = PipelineDiagnostic(
            code=code,
            severity=DiagnosticSeverity.ERROR,
            source=source,
            message="%s: %s" % (message, detail) if exc is not None else message,
            path=path,
            line=line,
            column=column,
        )
        self.diagnostics.add(diagnostic)
        error = ArxmlReadError([diagnostic])
        if exc is None:
            raise error
        raise error from exc


def _bits_to_c_type(bits: int) -> str:
    """Map bit size to C type string."""
    if bits <= 1:
        return "boolean"
    elif bits <= 8:
        return "uint8_t"
    elif bits <= 16:
        return "uint16_t"
    elif bits <= 32:
        return "uint32_t"
    return "uint32_t"


def _info(msg: str):
    print(msg)
