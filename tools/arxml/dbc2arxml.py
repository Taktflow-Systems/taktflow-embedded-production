#!/usr/bin/env python3
"""
DBC -> ARXML Converter for Taktflow Embedded Platform

Reads gateway/taktflow_vehicle.dbc (+ optional ecu_model.json) and generates
AUTOSAR R22-11 compliant ARXML with full SWC architecture:
  - Platform data types:   SwBaseTypes, ImplementationDataTypes
  - System topology:       ECU instances, CAN cluster, physical channel
  - Communication:         ISignalIPdus, ISignals, CAN frames, frame triggerings
  - Data types:            CompuMethods (enums + linear scales)
  - S/R Interfaces:        Per-signal typed SenderReceiverInterfaces
  - SWC definitions:       ApplicationSwComponentType per SWC with P/R ports
  - Internal behaviors:    RunnableEntity + TimingEvent/InitEvent per runnable
  - E2E annotations:       E2E protection metadata

Usage:
    python tools/arxml/dbc2arxml.py <dbc> <output-dir> [ecu-model-json]
"""

import sys
import os
import json
import re
import tempfile
from collections import Counter

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

import cantools
import autosar_data as asr
import autosar_data.abstraction as abst
from pipeline_diagnostics import (
    DiagnosticBag,
    PipelineDiagnosticError,
    locate_text,
    portable_path,
)
from arxml_validation import (
    StrictArxmlValidationError,
    validate_arxml_strict,
)
from conversion_assumptions import AssumptionsCollector
from arxml.swc_mapping import (
    SwcMappingError,
    SwcMappingValidationError,
    load_swc_mapping,
    validate_swc_mapping,
)
from autosar_data.abstraction.communication import (
    CanAddressingMode, CanFrameType, CyclicTiming, IpduTiming,
    TransmissionModeTiming,
)
from autosar_data.abstraction.datatype import (
    BaseTypeEncoding, CompuMethodContent_TextTable, CompuMethodContent_Linear,
    CompuScaleDirection, ImplementationDataCategory,
    ImplementationDataTypeSettings_Value,
)


def safe_name(name):
    """Sanitize a DBC name for ARXML SHORT-NAME."""
    return name.replace(" ", "_").replace("-", "_")

def dbc_byte_order(signal):
    """Map cantools byte order to AUTOSAR ByteOrder."""
    if signal.byte_order == "little_endian":
        return abst.ByteOrder.MostSignificantByteLast
    return abst.ByteOrder.MostSignificantByteFirst

def _location(path, *needles):
    if not path:
        return "", None, None
    for needle in needles:
        location = locate_text(path, str(needle))
        if location[1] is not None:
            return location
    return portable_path(path), None, None


def _portable_error(exc, *paths):
    message = str(exc)
    for path in paths:
        if not path:
            continue
        replacement = portable_path(path)
        message = message.replace(str(path), replacement)
        message = message.replace(str(path).replace("\\", "/"), replacement)
    return message


def get_msg_attr_value(
    msg, attr_name, default=None, diagnostics=None, source_path=None
):
    """Get a message attribute or fail explicitly when its container is malformed."""
    try:
        attrs = msg.dbc.attributes
        if attrs and attr_name in attrs:
            attr = attrs[attr_name]
            return attr.value if hasattr(attr, "value") else attr
    except (AttributeError, TypeError) as exc:
        if diagnostics is None:
            raise
        source = "DBC message '%s' attribute '%s'" % (msg.name, attr_name)
        path, line, column = _location(source_path, msg.name, attr_name)
        raise diagnostics.error(
            "DBC001", source, "cannot read value: %s" % exc,
            path=path, line=line, column=column,
        )
    return default


KNOWN_GEN_ATTRIBUTES = {"GenMsgCycleTime", "GenMsgSendType"}
SUPPORTED_SEND_TYPES = {"cyclic", "event", "nomsgsendtype"}
RECOGNIZED_MESSAGE_ATTRIBUTES = (
    "ASIL",
    "E2E_DataID",
    "E2E_MaxDeltaCounter",
    "GenMsgCycleTime",
    "GenMsgSendType",
    "Satisfies",
)
DEFAULT_REASONS = {
    "ASIL": "no safety classification was supplied; the converter uses QM",
    "E2E_DataID": "no E2E data identifier was supplied",
    "E2E_MaxDeltaCounter": "no E2E maximum delta was supplied",
    "GenMsgCycleTime": "no message cycle time was supplied",
    "GenMsgSendType": "no send type was supplied; cycle-time fallback applies",
    "Satisfies": "no requirement trace was supplied",
}


def validate_dbc_input(database, diagnostics, source_path=None):
    """Reject ambiguous names and values before any AUTOSAR object is created."""
    normalized_messages = {}
    normalized_signals = {}
    for msg in database.messages:
        normalized = safe_name(msg.name)
        previous = normalized_messages.get(normalized)
        if previous is not None and previous != msg.name:
            names = sorted((previous, msg.name))
            source = "DBC messages '%s', '%s'" % tuple(names)
            path, line, column = _location(source_path, *names)
            raise diagnostics.error(
                "DBC002", source, "normalize to the same SHORT-NAME '%s'" % normalized,
                path=path, line=line, column=column,
            )
        normalized_messages[normalized] = msg.name

        for sig in msg.signals:
            if getattr(sig, "is_multiplexer", False) or getattr(
                sig, "multiplexer_ids", None
            ):
                path, line, column = _location(source_path, sig.name, msg.name)
                raise diagnostics.error(
                    "DBC007",
                    "DBC message '%s' signal '%s'" % (msg.name, sig.name),
                    "multiplexing requires a MULTIPLEXED-I-PDU representation",
                    path=path, line=line, column=column,
                )
            signal_name = safe_name(sig.name)
            previous_signal = normalized_signals.get(signal_name)
            if previous_signal is not None and previous_signal != sig.name:
                names = sorted((previous_signal, sig.name))
                source = "DBC signals '%s', '%s'" % tuple(names)
                path, line, column = _location(source_path, *names)
                raise diagnostics.error(
                    "DBC002",
                    source,
                    "normalize to the same SHORT-NAME '%s'" % signal_name,
                    path=path, line=line, column=column,
                )
            normalized_signals[signal_name] = sig.name

        send_type = get_msg_attr_value(
            msg, "GenMsgSendType", None, diagnostics=diagnostics,
            source_path=source_path,
        )
        if send_type is not None:
            normalized_send_type = str(send_type).strip().lower()
            if normalized_send_type not in SUPPORTED_SEND_TYPES:
                source = "DBC message '%s' attribute 'GenMsgSendType'" % msg.name
                path, line, column = _location(
                    source_path, '"GenMsgSendType"', msg.name
                )
                raise diagnostics.error(
                    "DBC003", source, "unsupported value '%s'" % send_type,
                    path=path, line=line, column=column,
                )

    dbc_specifics = getattr(database, "dbc", None)
    definitions = getattr(dbc_specifics, "attribute_definitions", {}) or {}
    for name in sorted(definitions):
        if name.startswith("Gen") and name not in KNOWN_GEN_ATTRIBUTES:
            path, line, column = _location(source_path, name)
            diagnostics.warning(
                "DBC004",
                "DBC attribute definition '%s'" % name,
                "convention is not interpreted by this pipeline",
                path=path, line=line, column=column,
            )

def signal_idt_name(sig):
    """Choose ImplementationDataType name based on signal bit length."""
    if sig.length <= 1:
        return "boolean_T"
    elif sig.length <= 8:
        return "sint8_T" if sig.is_signed else "uint8_T"
    elif sig.length <= 16:
        return "sint16_T" if sig.is_signed else "uint16_T"
    else:
        return "sint32_T" if sig.is_signed else "uint32_T"


# Signal-to-SWC domain mapping for port assignment heuristic
DOMAIN_MAP = {
    "steer": ["steer", "steering"],
    "brake": ["brake"],
    "lidar": ["lidar"],
    "pedal": ["pedal"],
    "motor": ["motor", "torque"],
    "heartbeat": ["heartbeat", "hb", "alive"],
    "estop": ["estop", "e_stop"],
    "vehicle": ["vehiclestate", "vehicle_state"],
    "battery": ["battery", "batt"],
    "temp": ["temp", "temperature"],
    "encoder": ["encoder", "speed"],
    "current": ["current"],
    "buzzer": ["buzzer"],
    "light": ["light", "headlamp", "tail"],
    "indicator": ["indicator", "turn"],
    "door": ["door", "lock"],
    "dashboard": ["dashboard", "display", "rpm", "torque_pct"],
    "dtc": ["dtc", "fault"],
    "uds": ["uds"],
    "com": ["com", "can"],
    "safety": ["safety", "fault_mask", "cutoff"],
    "nvm": ["nvm", "cal"],
}


class Dbc2Arxml:
    """Convert a DBC file to ARXML using autosar-data abstraction layer."""

    def __init__(self, dbc_path, ecu_model_path=None, swc_mapping=None):
        self.diagnostics = DiagnosticBag()
        self.source_path = dbc_path
        try:
            self.db = cantools.database.load_file(dbc_path)
        except Exception as exc:
            raise self.diagnostics.error(
                "DBC000", "DBC input",
                "cannot parse input: %s" % _portable_error(exc, dbc_path),
                path=portable_path(dbc_path),
            ) from exc
        validate_dbc_input(self.db, self.diagnostics, dbc_path)
        for diagnostic in self.diagnostics.items:
            print(str(diagnostic), file=sys.stderr)
        self._signal_name_counts = Counter(
            safe_name(sig.name)
            for msg in self.db.messages
            for sig in msg.signals
        )
        self.am = abst.AutosarModelAbstraction.create("AUTOSAR_00051")
        self.system = None
        self.cluster = None
        self.channel = None

        # Communication
        self.ecus = {}
        self.controllers = {}
        self.ipdus = {}
        self.isignals = {}
        self.frames = {}
        self.sys_signals = {}
        self.compu_methods = {}
        self.units = {}
        self.e2e_messages = []

        # Platform types
        self.base_types = {}
        self.impl_data_types = {}

        # SWC architecture
        self.sr_interfaces = {}
        self.sr_interfaces_by_name = {}
        self.data_elements = {}
        self.swc_types = {}
        self.swc_behaviors = {}
        self.swc_runnables = {}
        self.swc_count = 0
        self.runnable_count = 0
        self.port_count = 0
        self.interface_count = 0
        self.timing_event_count = 0
        self.init_event_count = 0
        self.explicit_swc_mapping = swc_mapping

        # ECU model (optional)
        self.ecu_model = None
        if ecu_model_path and os.path.isfile(ecu_model_path):
            try:
                with open(ecu_model_path, "r", encoding="utf-8") as f:
                    self.ecu_model = json.load(f)
            except (OSError, TypeError, ValueError) as exc:
                raise self.diagnostics.error(
                    "MODEL001", "ECU model",
                    "cannot read JSON: %s" % _portable_error(exc, ecu_model_path),
                    path=portable_path(ecu_model_path),
                ) from exc

        self.assumptions = AssumptionsCollector(
            dbc_path, ecu_model_path if self.ecu_model is not None else None
        )
        self._record_assumption_inputs()
        self._build_routing_maps()

    def _record_assumption_inputs(self):
        """Record recognized conventions and consumed sidecar values."""
        dbc_specifics = getattr(self.db, "dbc", None)
        definitions = getattr(dbc_specifics, "attribute_definitions", {}) or {}
        for name in RECOGNIZED_MESSAGE_ATTRIBUTES:
            explicit_instances = sum(
                name in (getattr(msg.dbc, "attributes", {}) or {})
                for msg in self.db.messages
            )
            self.assumptions.record_convention(
                name, "DBC message attribute", name in definitions,
                explicit_instances,
            )

        ecus = (self.ecu_model or {}).get("ecus", {})
        self.assumptions.record_convention(
            "ECU model JSON", "sidecar", self.ecu_model is not None, len(ecus)
        )
        if self.ecu_model is None:
            self.assumptions.record_default(
                "conversion:SWC architecture",
                "ECU model",
                "not generated",
                "no ECU-model sidecar was supplied",
            )
            return

        if "ecus" not in self.ecu_model:
            self.assumptions.record_default(
                "sidecar:ECU model",
                "ecus",
                {},
                "sidecar has no ECU map; no SWC architecture is generated",
            )

        for ecu_name in sorted(ecus):
            ecu_data = ecus[ecu_name]
            ecu_identity = "ECU:%s" % safe_name(ecu_name)
            if "runnables" not in ecu_data:
                self.assumptions.record_default(
                    ecu_identity,
                    "runnables",
                    [],
                    "sidecar ECU has no runnable list",
                )
            if "swcs" not in ecu_data:
                self.assumptions.record_default(
                    ecu_identity,
                    "swcs",
                    [],
                    "sidecar ECU has no software-component list",
                )
            for runnable in sorted(
                ecu_data.get("runnables", []),
                key=lambda item: item.get("function", ""),
            ):
                identity = "ECU:%s/runnable:%s" % (
                    safe_name(ecu_name), runnable.get("function", "<unnamed>")
                )
                if "period_ms" in runnable:
                    self.assumptions.record_override(
                        identity,
                        "period_ms",
                        runnable["period_ms"],
                        "sidecar supplies runnable timing",
                    )
                else:
                    self.assumptions.record_default(
                        identity,
                        "period_ms",
                        0,
                        "sidecar runnable has no period; no TimingEvent is emitted",
                    )
            for swc in sorted(
                ecu_data.get("swcs", []), key=lambda item: item.get("name", "")
            ):
                identity = "ECU:%s/SWC:%s" % (
                    safe_name(ecu_name), swc.get("name", "<unnamed>")
                )
                if "functions" in swc:
                    self.assumptions.record_override(
                        identity,
                        "functions",
                        sorted(swc["functions"]),
                        "sidecar supplies functions used to create init runnables",
                    )
                else:
                    self.assumptions.record_default(
                        identity,
                        "functions",
                        [],
                        "sidecar SWC has no function list",
                    )

    def _build_routing_maps(self):
        """Build per-ECU TX/RX signal maps from DBC sender/receiver info."""
        self.ecu_tx_signals = {}
        self.ecu_rx_signals = {}
        for msg in self.db.messages:
            msg_name = safe_name(msg.name)
            senders = msg.senders if hasattr(msg, "senders") else []
            for sig in msg.signals:
                sn = safe_name(sig.name)
                for sender in senders:
                    self.ecu_tx_signals.setdefault(
                        safe_name(sender), []
                    ).append((msg_name, sn, sig))
                for receiver in (sig.receivers or []):
                    self.ecu_rx_signals.setdefault(
                        safe_name(receiver), []
                    ).append((msg_name, sn, sig))

    def convert_base(self):
        """Step 2: Generate ARXML base (signals, PDUs, frames, ECUs)."""
        self._create_platform_types()
        self._create_system()
        self._create_ecus()
        self._create_data_types()
        self._create_sr_interfaces()
        self._create_communication()

    def convert_enrich(self):
        """Step 3: Enrich ARXML with E2E + SWCs (requires base first)."""
        self._create_e2e_annotations()
        self._create_swc_types()

    def convert(self):
        """Run the full conversion pipeline (step 2 + step 3)."""
        self.convert_base()
        self.convert_enrich()

    def _fail(self, code, source, action, exc, location_path=None):
        """Stop conversion with one stable, object-addressed diagnostic."""
        source_path = location_path or getattr(self, "source_path", "")
        needles = re.findall(r"'([^']+)'", source)
        normalized_needles = [item.rsplit("/", 1)[-1] for item in needles]
        path, line, column = _location(source_path, *reversed(normalized_needles))
        detail = _portable_error(exc, source_path)
        raise self.diagnostics.error(
            code, source, "%s: %s" % (action, detail),
            path=path, line=line, column=column,
        ) from exc

    def _message_attr(self, msg, name, default=None, applied=True):
        value = get_msg_attr_value(
            msg, name, default, self.diagnostics, self.source_path
        )
        attrs = getattr(getattr(msg, "dbc", None), "attributes", {}) or {}
        if applied and name not in attrs:
            self.assumptions.record_default(
                "message:%s" % safe_name(msg.name),
                name,
                default,
                DEFAULT_REASONS.get(name, "input value was not supplied"),
            )
        return value

    def write(self, output_dir, output_name="TaktflowSystem.arxml"):
        """Atomically replace the output after complete serialization."""
        self.diagnostics.raise_if_errors()
        os.makedirs(output_dir, exist_ok=True)
        files = self.am.model.serialize_files()
        if len(files) != 1:
            raise self.diagnostics.error(
                "ARXML018", "serialized ARXML model",
                "expected one output file, got %d" % len(files),
            )
        content = next(iter(files.values()))
        filepath = os.path.join(output_dir, output_name)
        temp_path = None
        try:
            fd, temp_path = tempfile.mkstemp(
                prefix=".%s." % output_name,
                suffix=".tmp",
                dir=output_dir,
                text=True,
            )
            with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            validate_arxml_strict(temp_path)
            os.replace(temp_path, filepath)
            temp_path = None
        except StrictArxmlValidationError as exc:
            if temp_path and os.path.exists(temp_path):
                try:
                    os.unlink(temp_path)
                except OSError as cleanup_exc:
                    exc.message += "; temporary-file cleanup failed: %s" % (
                        _portable_error(cleanup_exc, temp_path)
                    )
            raise self.diagnostics.error(
                exc.code,
                "ARXML output '%s'" % output_name,
                exc.message,
                path=output_name,
                line=exc.line,
                column=exc.column,
            ) from exc
        except (OSError, UnicodeError) as exc:
            cleanup_error = None
            if temp_path and os.path.exists(temp_path):
                try:
                    os.unlink(temp_path)
                except OSError as cleanup_exc:
                    cleanup_error = cleanup_exc
            action = "atomic write failed"
            if cleanup_error is not None:
                action += "; temporary-file cleanup also failed"
            self._fail(
                "IO001", "ARXML output '%s'" % output_name,
                action, exc, location_path=output_name,
            )
        print("  Written: %s (%d bytes)" % (portable_path(filepath), len(content)))

        report_name = "%s.assumptions.md" % output_name
        report_path = os.path.join(output_dir, report_name)
        self.assumptions.record_diagnostics(self.diagnostics.items)
        try:
            self.assumptions.write_atomic(report_path)
        except (OSError, UnicodeError) as exc:
            self._fail(
                "IO002",
                "conversion assumptions report '%s'" % report_name,
                "atomic write failed",
                exc,
                location_path=report_name,
            )
        print("  Written: %s" % portable_path(report_path))

    # -- Platform data types ------------------------------------------------

    def _create_platform_types(self):
        """Create AUTOSAR platform base types and implementation data types."""
        bt_pkg = self.am.get_or_create_package("/AUTOSAR/Platform/BaseTypes")
        idt_pkg = self.am.get_or_create_package(
            "/AUTOSAR/Platform/ImplementationDataTypes"
        )
        for name, bits, enc in [
            ("boolean", 8, BaseTypeEncoding.Boolean),
            ("uint8", 8, BaseTypeEncoding.NoEncoding),
            ("uint16", 16, BaseTypeEncoding.NoEncoding),
            ("uint32", 32, BaseTypeEncoding.NoEncoding),
            ("sint8", 8, BaseTypeEncoding.TwosComplement),
            ("sint16", 16, BaseTypeEncoding.TwosComplement),
            ("sint32", 32, BaseTypeEncoding.TwosComplement),
        ]:
            bt = bt_pkg.create_sw_base_type("%s_bt" % name, bits, enc)
            self.base_types[name] = bt
            idt = idt_pkg.create_implementation_data_type(
                ImplementationDataTypeSettings_Value(
                    name="%s_T" % name, base_type=bt,
                    compu_method=None, data_constraint=None,
                )
            )
            self.impl_data_types["%s_T" % name] = idt

    # -- System topology ---------------------------------------------------

    def _create_system(self):
        sys_pkg = self.am.get_or_create_package("/Taktflow/System")
        self.system = sys_pkg.create_system(
            "TaktflowSystem", abst.SystemCategory.SystemExtract
        )
        comm_pkg = self.am.get_or_create_package("/Taktflow/Communication")
        self.cluster = self.system.create_can_cluster("CAN_500k", comm_pkg)
        try:
            self.cluster.baudrate = 500000
        except (AttributeError, TypeError) as exc:
            self._fail("ARXML001", "CAN cluster 'CAN_500k'", "cannot set baudrate", exc)
        self.channel = self.cluster.create_physical_channel("CAN_Physical")

    def _create_ecus(self):
        ecu_pkg = self.am.get_or_create_package("/Taktflow/ECUs")
        for node in self.db.nodes:
            name = safe_name(node.name)
            ecu = self.system.create_ecu_instance(name, ecu_pkg)
            self.ecus[name] = ecu
            ctrl = ecu.create_can_communication_controller("%s_CanCtrl" % name)
            try:
                ctrl.connect_physical_channel("%s_Conn" % name, self.channel)
            except Exception as exc:
                self._fail(
                    "ARXML002", "ECU '%s' CAN controller" % name,
                    "cannot connect physical channel", exc,
                )
            self.controllers[name] = ctrl

    # -- Data types (CompuMethods) -----------------------------------------

    def _create_data_types(self):
        dt_pkg = self.am.get_or_create_package("/Taktflow/DataTypes")
        created = set()
        for msg in self.db.messages:
            for sig in msg.signals:
                cm_name = "CM_%s" % safe_name(sig.name)
                if cm_name in created:
                    continue
                if sig.choices:
                    self._create_enum_cm(dt_pkg, cm_name, sig)
                    created.add(cm_name)
                elif sig.scale != 1.0 or sig.offset != 0.0:
                    self._create_linear_cm(dt_pkg, cm_name, sig)
                    created.add(cm_name)

    def _create_enum_cm(self, pkg, name, sig):
        try:
            cm = pkg.create_compu_method(name, CompuMethodContent_TextTable(texts=[]))
            elem = cm.element
            citp = elem.get_or_create_sub_element("COMPU-INTERNAL-TO-PHYS")
            scales = citp.get_or_create_sub_element("COMPU-SCALES")
            for val, label in sorted(sig.choices.items()):
                sc = scales.create_sub_element("COMPU-SCALE")
                sc.get_or_create_sub_element("LOWER-LIMIT").character_data = str(int(val))
                sc.get_or_create_sub_element("UPPER-LIMIT").character_data = str(int(val))
                cc = sc.get_or_create_sub_element("COMPU-CONST")
                cc.get_or_create_sub_element("VT").character_data = str(label)
            self.compu_methods[name] = cm
        except Exception as exc:
            self._fail(
                "ARXML003", "CompuMethod '%s'" % name,
                "cannot create enumeration conversion", exc,
            )

    def _create_linear_cm(self, pkg, name, sig):
        try:
            cm = pkg.create_compu_method(name, CompuMethodContent_Linear(
                direction=CompuScaleDirection.IntToPhys,
                factor=float(sig.scale), offset=float(sig.offset), divisor=1.0,
            ))
            elem = cm.element
            citp = elem.get_or_create_sub_element("COMPU-INTERNAL-TO-PHYS")
            scales = citp.get_or_create_sub_element("COMPU-SCALES")
            sc = scales.get_or_create_sub_element("COMPU-SCALE")
            if sig.minimum is not None:
                sc.get_or_create_sub_element("LOWER-LIMIT").character_data = str(float(sig.minimum))
            if sig.maximum is not None:
                sc.get_or_create_sub_element("UPPER-LIMIT").character_data = str(float(sig.maximum))
            if sig.unit:
                try:
                    unit_key = sig.unit.casefold()
                    unit = self.units.get(unit_key)
                    if unit is None:
                        unit_token = sig.unit.replace("%", "pct").replace("/", "_per_")
                        unit_token = re.sub(r"[^A-Za-z0-9_]", "_", unit_token).strip("_")
                        unit_pkg = self.am.get_or_create_package("/Taktflow/Units")
                        unit = unit_pkg.create_unit(
                            "Unit_%s" % (unit_token or "unnamed"),
                            display_name=sig.unit,
                        )
                        self.units[unit_key] = unit
                    cm.unit = unit
                except Exception as exc:
                    self._fail(
                        "ARXML004", "CompuMethod '%s' unit" % name,
                        "cannot create or reference unit", exc,
                    )
            self.compu_methods[name] = cm
        except PipelineDiagnosticError:
            raise
        except Exception as exc:
            self._fail(
                "ARXML003", "CompuMethod '%s'" % name,
                "cannot create linear conversion", exc,
            )

    # -- Sender/Receiver Interfaces ----------------------------------------

    def _create_sr_interfaces(self):
        """Create SenderReceiverInterface for each unique DBC signal."""
        iface_pkg = self.am.get_or_create_package("/Taktflow/Interfaces")
        created = set()
        for msg in self.db.messages:
            for sig in msg.signals:
                sn = safe_name(sig.name)
                if sn in created:
                    continue
                created.add(sn)
                try:
                    sr = iface_pkg.create_sender_receiver_interface("SRI_%s" % sn)
                    idt = self.impl_data_types.get(signal_idt_name(sig))
                    if idt is None:
                        raise self.diagnostics.error(
                            "ARXML005", "signal '%s'" % sn,
                            "has no implementation data type",
                        )
                    de = sr.create_data_element(sn, idt)
                    self.sr_interfaces[sn] = sr
                    self.sr_interfaces_by_name["SRI_%s" % sn] = sr
                    self.data_elements[sn] = de
                    self.interface_count += 1
                except PipelineDiagnosticError:
                    raise
                except Exception as exc:
                    self._fail(
                        "ARXML005", "signal '%s' sender-receiver interface" % sn,
                        "cannot create interface", exc,
                    )

    # -- Communication layer -----------------------------------------------

    def _create_communication(self):
        ipdu_pkg = self.am.get_or_create_package("/Taktflow/Communication/IPdus")
        sig_pkg = self.am.get_or_create_package("/Taktflow/Communication/Signals")
        syssig_pkg = self.am.get_or_create_package("/Taktflow/Communication/SystemSignals")
        frame_pkg = self.am.get_or_create_package("/Taktflow/Communication/Frames")

        for msg in self.db.messages:
            mn = safe_name(msg.name)
            ipdu = self.system.create_isignal_ipdu(mn, ipdu_pkg, msg.length)
            self.ipdus[mn] = ipdu

            cycle_ms = self._message_attr(msg, "GenMsgCycleTime", 0)
            try:
                cycle_ms = int(cycle_ms or 0)
            except (TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' attribute 'GenMsgCycleTime'" % msg.name,
                    "value is not an integer", exc,
                )
            send_type = str(
                self._message_attr(msg, "GenMsgSendType", "noMsgSendType")
            ).strip().lower()
            emit_cyclic_timing = cycle_ms > 0
            if cycle_ms > 0:
                decision = "Cyclic timing emitted every %d ms" % cycle_ms
                if send_type == "event":
                    timing_reason = (
                        "positive cycle drives ARXML timing; event controls "
                        "downstream C transmission mode"
                    )
                elif send_type == "cyclic":
                    timing_reason = "explicit cyclic send type and positive cycle"
                else:
                    timing_reason = "positive cycle used as send-type fallback"
            elif send_type == "event":
                decision = "No timing specification emitted"
                timing_reason = "event maps to DIRECT downstream with no cycle"
            else:
                decision = "No timing specification emitted"
                timing_reason = "no positive cyclic timing input was available"
            self.assumptions.record_timing(
                "message:%s" % mn,
                send_type,
                cycle_ms,
                decision,
                timing_reason,
            )

            if emit_cyclic_timing:
                try:
                    ipdu.set_timing(
                        IpduTiming(
                            transmission_mode_true_timing=TransmissionModeTiming(
                                cyclic_timing=CyclicTiming(float(cycle_ms) / 1000.0)
                            )
                        )
                    )
                except Exception as exc:
                    self._fail(
                        "ARXML006", "I-PDU '%s' timing" % mn,
                        "cannot set cycle time", exc,
                    )

            for sig in msg.signals:
                sn = safe_name(sig.name)
                communication_name = (
                    "%s_%s" % (mn, sn)
                    if self._signal_name_counts[sn] > 1
                    else sn
                )
                try:
                    ss = syssig_pkg.create_system_signal("SS_%s" % communication_name)
                    self.sys_signals[(mn, sig.name)] = ss
                except Exception as exc:
                    self._fail(
                        "ARXML007", "system signal '%s/%s'" % (mn, sn),
                        "cannot create system signal", exc,
                    )
                try:
                    isig = self.system.create_isignal(
                        communication_name, sig_pkg, sig.length, ss
                    )
                    self.isignals[(mn, sig.name)] = isig
                except Exception as exc:
                    self._fail(
                        "ARXML008", "I-Signal '%s/%s'" % (mn, sn),
                        "cannot create I-Signal", exc,
                    )
                try:
                    ipdu.map_signal(isig, sig.start, dbc_byte_order(sig))
                except Exception as exc:
                    self._fail(
                        "ARXML009", "I-Signal mapping '%s/%s'" % (mn, sn),
                        "cannot map signal to I-PDU", exc,
                    )

            try:
                frame = self.system.create_can_frame("%s_Frame" % mn, frame_pkg, msg.length)
                self.frames[mn] = frame
                try:
                    frame.map_pdu(ipdu, 0, abst.ByteOrder.MostSignificantByteLast)
                except Exception as exc:
                    self._fail(
                        "ARXML010", "frame '%s_Frame'" % mn,
                        "cannot map I-PDU", exc,
                    )
                try:
                    addressing_mode = (
                        CanAddressingMode.Extended
                        if getattr(msg, "is_extended_frame", False)
                        else CanAddressingMode.Standard
                    )
                    frame_type = (
                        CanFrameType.CanFd
                        if getattr(msg, "is_fd", False)
                        else CanFrameType.Can20
                    )
                    self.channel.trigger_frame(
                        frame, msg.frame_id, addressing_mode, frame_type
                    )
                except Exception as exc:
                    self._fail(
                        "ARXML011", "frame triggering '%s'" % mn,
                        "cannot create CAN triggering", exc,
                    )
            except PipelineDiagnosticError:
                raise
            except Exception as exc:
                self._fail(
                    "ARXML012", "CAN frame '%s'" % mn,
                    "cannot create frame", exc,
                )

    # -- E2E Protection Set (AUTOSAR Approach A) -----------------------------

    def _create_e2e_annotations(self):
        """Create END-TO-END-PROTECTION-SET with per-PDU E2E Profile P01 config.

        Reads from DBC attributes:
          - E2E_DataID (BA_ per message) → DATA-ID-VALUE
          - E2E_MaxDeltaCounter (BA_ per message, default 2) → MAX-DELTA-COUNTER-INIT
          - ASIL (BA_ per message) → ADMIN-DATA/SDGS/SD[@GID='ASIL']
          - Satisfies (BA_ per message) → ADMIN-DATA/SDGS/SD[@GID='SATISFIES']
          - GenMsgCycleTime → used in DESC for documentation

        E2E signal layout (consistent across all messages):
          - DataID nibble: bit 0, 4 bits
          - AliveCounter: bit 4, 4 bits
          - CRC8: bit 8, 8 bits

        Traceability: each END-TO-END-PROTECTION traces to:
          - DBC message (via I-SIGNAL-I-PDU-REF)
          - Safety Goal (via ADMIN-DATA/SDGS/SATISFIES)
          - HARA hazardous event (via Safety Goal chain)

        @traces_to TSR-022 (E2E protection on safety-relevant CAN messages)
        @satisfies SG-001, SG-003, SG-004, SG-008 (per-message, see DBC Satisfies attr)
        """
        e2e_pkg = self.am.get_or_create_package("/Taktflow/E2E")
        raw_pkg = e2e_pkg.element

        # Create END-TO-END-PROTECTION-SET as a top-level element
        elems = raw_pkg.get_or_create_sub_element("ELEMENTS")
        e2e_set = elems.create_named_sub_element(
            "END-TO-END-PROTECTION-SET", "E2E_ProtectionSet_P01"
        )
        prots = e2e_set.create_sub_element("END-TO-END-PROTECTIONS")

        e2e_count = 0
        for msg in self.db.messages:
            mn = safe_name(msg.name)

            # Only messages with E2E signals (DataID + AliveCounter + CRC8)
            sig_names = set()
            for s in msg.signals:
                # Match suffix after message prefix: e.g. EStop_Broadcast_E2E_DataID
                short = s.name
                if "E2E_DataID" in short:
                    sig_names.add("E2E_DataID")
                if "E2E_AliveCounter" in short:
                    sig_names.add("E2E_AliveCounter")
                if "E2E_CRC8" in short:
                    sig_names.add("E2E_CRC8")

            if not ({"E2E_DataID", "E2E_AliveCounter", "E2E_CRC8"} <= sig_names):
                continue

            self.e2e_messages.append(mn)

            # Read DBC attributes
            try:
                data_id = int(self._message_attr(msg, "E2E_DataID", "0"))
                max_delta = int(self._message_attr(msg, "E2E_MaxDeltaCounter", "2"))
                cycle_ms = int(self._message_attr(msg, "GenMsgCycleTime", "0"))
            except PipelineDiagnosticError:
                raise
            except (TypeError, ValueError) as exc:
                self._fail(
                    "DBC001", "DBC message '%s' E2E/timing attributes" % msg.name,
                    "numeric attribute is invalid", exc,
                )
            asil = self._message_attr(msg, "ASIL", "QM")
            satisfies = self._message_attr(msg, "Satisfies", "")

            # Find E2E signal bit positions
            crc_offset = 8    # default
            counter_offset = 4  # default
            dataid_offset = 0   # default
            for s in msg.signals:
                if "E2E_CRC8" in s.name:
                    crc_offset = s.start
                elif "E2E_AliveCounter" in s.name:
                    counter_offset = s.start
                elif "E2E_DataID" in s.name:
                    dataid_offset = s.start

            try:
                # Create END-TO-END-PROTECTION for this message
                prot = prots.create_named_sub_element(
                    "END-TO-END-PROTECTION", "E2E_%s" % mn
                )

                # CATEGORY = profile identifier
                prot.create_sub_element("CATEGORY").character_data = "E2E_PROFILE_01"

                # END-TO-END-PROFILE with all parameters
                profile = prot.create_sub_element("END-TO-END-PROFILE")
                profile.create_sub_element("CATEGORY").character_data = "PROFILE_01"
                profile.create_sub_element("DATA-ID-MODE").character_data = 0  # 0=ALL-16-BIT

                # DATA-IDS (each DATA-ID is a direct value element)
                data_ids_elem = profile.create_sub_element("DATA-IDS")
                did_elem = data_ids_elem.create_sub_element("DATA-ID")
                did_elem.character_data = str(data_id)

                # Signal layout
                profile.create_sub_element("DATA-LENGTH").character_data = str(msg.length * 8)
                profile.create_sub_element("CRC-OFFSET").character_data = str(crc_offset)
                profile.create_sub_element("COUNTER-OFFSET").character_data = str(counter_offset)
                profile.create_sub_element("DATA-ID-NIBBLE-OFFSET").character_data = str(dataid_offset)

                # E2E timing parameters (from DBC E2E_MaxDeltaCounter)
                profile.create_sub_element("MAX-DELTA-COUNTER-INIT").character_data = str(max_delta)
                profile.create_sub_element("MAX-NO-NEW-OR-REPEATED-DATA").character_data = str(max_delta + 1)
                profile.create_sub_element("SYNC-COUNTER-INIT").character_data = "0"

                # ADMIN-DATA for traceability (ASIL + Satisfies + I-PDU reference)
                # Note: I-SIGNAL-I-PDUS not supported in END-TO-END-PROTECTION in R22-11.
                # I-PDU link stored in ADMIN-DATA/SDGS as custom metadata.
                admin = prot.create_sub_element("ADMIN-DATA")
                sdgs = admin.create_sub_element("SDGS")
                sdg = sdgs.create_sub_element("SDG")
                sdg.set_attribute("GID", "SAFETY")

                sd_asil = sdg.create_sub_element("SD")
                sd_asil.set_attribute("GID", "ASIL")
                sd_asil.character_data = str(asil)

                if satisfies:
                    sd_sat = sdg.create_sub_element("SD")
                    sd_sat.set_attribute("GID", "SATISFIES")
                    sd_sat.character_data = str(satisfies)

                sd_can = sdg.create_sub_element("SD")
                sd_can.set_attribute("GID", "CAN_ID")
                sd_can.character_data = "0x%03X" % msg.frame_id

                sd_cycle = sdg.create_sub_element("SD")
                sd_cycle.set_attribute("GID", "CYCLE_MS")
                sd_cycle.character_data = str(cycle_ms)

                # I-PDU path reference (custom metadata — R22-11 doesn't
                # support I-SIGNAL-I-PDUS under END-TO-END-PROTECTION)
                if mn in self.ipdus:
                    sd_ipdu = sdg.create_sub_element("SD")
                    sd_ipdu.set_attribute("GID", "I-PDU-PATH")
                    sd_ipdu.character_data = self.ipdus[mn].element.path

                e2e_count += 1

            except Exception as exc:
                self._fail(
                    "ARXML013", "E2E protection '%s'" % mn,
                    "cannot create protection metadata", exc,
                )

        print("    E2E protections: %d (Profile P01, END-TO-END-PROTECTION-SET)" % e2e_count)

    # -- Software Components -----------------------------------------------

    def _create_swc_types(self):
        """Create ApplicationSwComponentType per SWC with ports, runnables, events."""
        if getattr(self, "explicit_swc_mapping", None) is not None:
            self._create_explicit_swc_types()
            return
        if self.ecu_model is None:
            return

        for ecu_name, ecu_data in self.ecu_model.get("ecus", {}).items():
            eu = ecu_name.upper()
            swc_pkg = self.am.get_or_create_package("/Taktflow/SWCs/%s" % eu)

            # Sorted for deterministic port order — bare set iteration follows
            # randomized str hashing and reshuffles the ARXML on every run.
            tx_sigs = sorted(set(sn for _, sn, _ in self.ecu_tx_signals.get(eu, [])))
            rx_sigs = sorted(set(sn for _, sn, _ in self.ecu_rx_signals.get(eu, [])))

            # Map runnables to SWC by function name prefix
            run_map = {}
            for r in ecu_data.get("runnables", []):
                parts = r["function"].split("_")
                key = "%s_%s" % (parts[0], parts[1]) if len(parts) >= 3 else r["function"]
                run_map.setdefault(key, []).append(r)

            for swc_info in ecu_data.get("swcs", []):
                try:
                    self._create_one_swc(
                        swc_pkg, eu, swc_info["name"],
                        swc_info.get("functions", []),
                        run_map.get(swc_info["name"], []),
                        tx_sigs, rx_sigs,
                    )
                except PipelineDiagnosticError:
                    raise
                except Exception as exc:
                    self._fail(
                        "ARXML014", "SWC '%s/%s'" % (eu, swc_info["name"]),
                        "cannot create software component", exc,
                    )

    def _create_explicit_swc_types(self):
        """Emit only the already validated exact mapping model."""
        for ecu in self.explicit_swc_mapping.ecus:
            swc_pkg = self.am.get_or_create_package(
                "/Taktflow/SWCs/%s" % ecu.name.upper()
            )
            for mapped_swc in ecu.swcs:
                try:
                    swc = swc_pkg.create_application_sw_component_type(
                        mapped_swc.arxml_short_name
                    )
                    self.swc_types[mapped_swc.arxml_short_name] = swc
                    self.swc_count += 1
                    for port in mapped_swc.ports:
                        interface = self.sr_interfaces_by_name[port.interface]
                        if port.direction == "provided":
                            swc.create_p_port(port.name, interface)
                        else:
                            swc.create_r_port(port.name, interface)
                        self.port_count += 1

                    behavior = swc.create_swc_internal_behavior(
                        "%s_IB" % mapped_swc.arxml_short_name
                    )
                    self.swc_behaviors[mapped_swc.arxml_short_name] = behavior
                    for runnable in mapped_swc.runnables:
                        entity = behavior.create_runnable_entity(runnable.function)
                        self.swc_runnables[runnable.function] = entity
                        self.runnable_count += 1
                        if runnable.trigger == "init":
                            behavior.create_init_event(
                                "IE_%s" % runnable.function, entity
                            )
                            self.init_event_count += 1
                        elif runnable.trigger == "periodic":
                            behavior.create_timing_event(
                                "TE_%s_%dms" % (
                                    runnable.function, runnable.period_ms
                                ),
                                entity,
                                float(runnable.period_ms) / 1000.0,
                            )
                            self.timing_event_count += 1
                except PipelineDiagnosticError:
                    raise
                except Exception as exc:
                    self._fail(
                        "ARXML014",
                        "SWC '%s/%s'" % (ecu.name, mapped_swc.name),
                        "cannot create explicit software component",
                        exc,
                    )
    def _create_one_swc(self, pkg, ecu, swc_name, functions, runnables, tx_sigs, rx_sigs):
        full = "%s_%s" % (ecu, swc_name)
        swc = pkg.create_application_sw_component_type(full)
        self.swc_types[full] = swc
        self.swc_count += 1

        swc_low = swc_name.lower()
        added = set()

        # Provide ports (TX)
        for sn in tx_sigs:
            if self._sig_matches(sn, swc_low) and sn not in added:
                sr = self.sr_interfaces.get(sn)
                if sr:
                    try:
                        swc.create_p_port("PP_%s" % sn, sr)
                        self.port_count += 1
                        added.add(sn)
                    except Exception as exc:
                        self._fail(
                            "ARXML015", "SWC '%s' P-port '%s'" % (full, sn),
                            "cannot create port", exc,
                        )

        # Require ports (RX)
        for sn in rx_sigs:
            if self._sig_matches(sn, swc_low) and sn not in added:
                sr = self.sr_interfaces.get(sn)
                if sr:
                    try:
                        swc.create_r_port("RP_%s" % sn, sr)
                        self.port_count += 1
                        added.add(sn)
                    except Exception as exc:
                        self._fail(
                            "ARXML015", "SWC '%s' R-port '%s'" % (full, sn),
                            "cannot create port", exc,
                        )

        # Internal behavior
        beh = swc.create_swc_internal_behavior("%s_IB" % full)
        self.swc_behaviors[full] = beh

        # Periodic runnables
        for r in runnables:
            func = r["function"]
            try:
                run = beh.create_runnable_entity(func)
                self.swc_runnables[func] = run
                self.runnable_count += 1
                ms = r.get("period_ms", 0)
                if ms > 0:
                    beh.create_timing_event("TE_%s_%dms" % (func, ms), run, float(ms) / 1000.0)
                    self.timing_event_count += 1
            except Exception as exc:
                self._fail(
                    "ARXML016", "runnable '%s'" % func,
                    "cannot create runnable or timing event", exc,
                )

        # Init runnables
        for func in functions:
            if func.endswith("_Init") and func not in self.swc_runnables:
                try:
                    run = beh.create_runnable_entity(func)
                    self.swc_runnables[func] = run
                    self.runnable_count += 1
                    beh.create_init_event("IE_%s" % func, run)
                    self.init_event_count += 1
                except Exception as exc:
                    self._fail(
                        "ARXML017", "init runnable '%s'" % func,
                        "cannot create runnable or init event", exc,
                    )

    def _sig_matches(self, sig_name, swc_lower):
        """Heuristic: does this signal belong to this SWC domain?"""
        sl = sig_name.lower()
        for domain, kws in DOMAIN_MAP.items():
            if domain in swc_lower:
                for kw in kws:
                    if kw in sl:
                        return True
        if any(k in swc_lower for k in ["com", "canmonitor", "main"]):
            return True
        return False

    # -- Reporting ---------------------------------------------------------

    def report(self):
        print("")
        print("=" * 60)
        print("DBC -> ARXML Conversion Summary")
        print("=" * 60)
        print("  [Communication]")
        print("    ECU instances:    %d" % len(self.ecus))
        print("    Messages (IPdus): %d" % len(self.ipdus))
        print("    Signals:          %d" % len(self.isignals))
        print("    CAN Frames:       %d" % len(self.frames))
        print("    CompuMethods:     %d" % len(self.compu_methods))
        print("    System Signals:   %d" % len(self.sys_signals))
        print("    E2E protected:    %d" % len(self.e2e_messages))
        print("  [Platform]")
        print("    Base types:       %d" % len(self.base_types))
        print("    Impl data types:  %d" % len(self.impl_data_types))
        print("  [Software Components]")
        print("    S/R Interfaces:   %d" % self.interface_count)
        print("    SWC types:        %d" % self.swc_count)
        print("    Ports:            %d" % self.port_count)
        print("    Runnables:        %d" % self.runnable_count)
        print("    Timing events:    %d" % self.timing_event_count)
        print("    Init events:      %d" % self.init_event_count)

        asil = {}
        for msg in self.db.messages:
            a = self._message_attr(msg, "ASIL", "QM", applied=False)
            asil[a] = asil.get(a, 0) + 1
        print("  [Safety]")
        print("    ASIL distribution: %s" % asil)

        print("")
        print("  ECU signal routing:")
        for node in self.db.nodes:
            tx = sum(1 for m in self.db.messages if hasattr(m, "senders") and node.name in m.senders)
            rx = sum(1 for m in self.db.messages for s in m.signals if node.name in (s.receivers or []))
            print("    %s: TX=%d msgs, RX=%d signals" % (node.name, tx, rx))

        errs = self.am.model.check_references()
        idents = list(self.am.model.identifiable_elements)
        print("")
        print("  [Validation]")
        print("    Identifiable elements: %d" % len(idents))
        print("    Reference errors:      %d" % len(errs))
        for e in errs[:5]:
            print("      %s" % e)
        print("=" * 60)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="DBC to ARXML converter")
    parser.add_argument("dbc", help="DBC file path")
    parser.add_argument("output", help="Output directory")
    parser.add_argument("model", nargs="?", help="ECU model JSON (optional)")
    parser.add_argument("--step", choices=["base", "enrich", "all"], default="all",
                        help="Pipeline step: base (step 2), enrich (step 3), all (both)")
    parser.add_argument(
        "--swc-mapping",
        help="explicit signal-to-SWC sidecar path (required with mapping mode)",
    )
    parser.add_argument(
        "--swc-mapping-mode",
        choices=["shadow", "preferred"],
        help="validate explicit mapping for shadow or preferred emission",
    )
    args = parser.parse_args()

    if bool(args.swc_mapping) != bool(args.swc_mapping_mode):
        parser.error("--swc-mapping and --swc-mapping-mode must be supplied together")
    if args.swc_mapping and not args.model:
        parser.error("--swc-mapping requires an ECU model JSON input")

    if not os.path.isfile(args.dbc):
        print("Error: DBC file not found: %s" % args.dbc)
        return 1

    print("Converting %s -> ARXML (step=%s)..." % (portable_path(args.dbc), args.step))
    if args.model:
        print("  ECU model: %s" % portable_path(args.model))
    else:
        print("  WARNING: no ECU model JSON given — SWC model will NOT be "
              "merged.\n"
              "  The output ARXML will lack all APPLICATION-SW-COMPONENT-TYPEs "
              "(ports, runnables).\n"
              "  Full pipeline: pass arxml_v2/swc_model.json as the third "
              "argument (see .github/workflows/ci.yml).")

    try:
        mapping = None
        if args.swc_mapping_mode in ("shadow", "preferred"):
            mapping = load_swc_mapping(args.swc_mapping)
            database = cantools.database.load_file(args.dbc)
            production_ecus = {"cvc", "fzc", "rzc", "sc", "bcm", "icu", "tcu"}
            governed_ecus = sorted(
                production_ecus
                & {
                    (node.name if hasattr(node, "name") else str(node)).lower()
                    for node in database.nodes
                }
            )
            validate_swc_mapping(
                mapping,
                args.dbc,
                args.model,
                governed_ecus=governed_ecus,
            )
            if args.swc_mapping_mode == "shadow":
                print(
                    "  SWC mapping: shadow validation passed; "
                    "legacy emission remains active"
                )
            else:
                print("  SWC mapping: preferred explicit emission active")

        c = Dbc2Arxml(
            args.dbc,
            args.model,
            swc_mapping=mapping if args.swc_mapping_mode == "preferred" else None,
        )

        if args.step == "base":
            c.convert_base()
        elif args.step == "enrich":
            c.convert_enrich()
        else:
            c.convert()

        c.report()
        c.write(args.output)
        print("\nDone.")
        return 0
    except PipelineDiagnosticError as exc:
        print(str(exc), file=sys.stderr)
        return 2
    except (SwcMappingError, SwcMappingValidationError, OSError) as exc:
        print(_portable_error(exc, args.swc_mapping, args.model, args.dbc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
