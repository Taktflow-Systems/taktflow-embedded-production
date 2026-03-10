#!/usr/bin/env python3
"""
DBC -> ARXML Converter for Taktflow Embedded Platform

Reads gateway/taktflow.dbc (+ optional ecu_model.json) and generates
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
import cantools
import autosar_data as asr
import autosar_data.abstraction as abst
from autosar_data.abstraction.communication import (
    CanAddressingMode, CanFrameType, CycleRepetition,
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

def get_msg_attr_value(msg, attr_name, default=None):
    """Get attribute value from message, handling DBC attribute objects."""
    try:
        attrs = msg.dbc.attributes
        if attrs and attr_name in attrs:
            attr = attrs[attr_name]
            return attr.value if hasattr(attr, "value") else attr
    except (AttributeError, TypeError):
        pass
    return default

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

    def __init__(self, dbc_path, ecu_model_path=None):
        self.db = cantools.database.load_file(dbc_path)
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
        self.e2e_messages = []

        # Platform types
        self.base_types = {}
        self.impl_data_types = {}

        # SWC architecture
        self.sr_interfaces = {}
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

        # ECU model (optional)
        self.ecu_model = None
        if ecu_model_path and os.path.isfile(ecu_model_path):
            with open(ecu_model_path, "r", encoding="utf-8") as f:
                self.ecu_model = json.load(f)

        self._build_routing_maps()

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

    def convert(self):
        """Run the full conversion pipeline."""
        self._create_platform_types()
        self._create_system()
        self._create_ecus()
        self._create_data_types()
        self._create_sr_interfaces()
        self._create_communication()
        self._create_e2e_annotations()
        self._create_swc_types()

    def write(self, output_dir, output_name="TaktflowSystem.arxml"):
        """Write ARXML file to output directory."""
        os.makedirs(output_dir, exist_ok=True)
        files = self.am.model.serialize_files()
        for _fn, content in files.items():
            filepath = os.path.join(output_dir, output_name)
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(content)
            print("  Written: %s (%d bytes)" % (filepath, len(content)))

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
        except (AttributeError, TypeError):
            pass
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
            except Exception:
                pass
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
        except Exception as e:
            print("  Warning: enum CM %s: %s" % (name, e))

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
                    elem.get_or_create_sub_element("DISPLAY-FORMAT").character_data = sig.unit
                except Exception:
                    pass
            self.compu_methods[name] = cm
        except Exception as e:
            print("  Warning: linear CM %s: %s" % (name, e))

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
                        continue
                    de = sr.create_data_element(sn, idt)
                    self.sr_interfaces[sn] = sr
                    self.data_elements[sn] = de
                    self.interface_count += 1
                except Exception as e:
                    print("  Warning: S/R iface %s: %s" % (sn, e))

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

            cycle_ms = get_msg_attr_value(msg, "GenMsgCycleTime", 0)
            if cycle_ms and int(cycle_ms) > 0:
                try:
                    ipdu.set_timing(float(int(cycle_ms)) / 1000.0, CycleRepetition.CycleRepetitionOf1)
                except Exception:
                    pass

            for sig in msg.signals:
                sn = "%s_%s" % (mn, safe_name(sig.name))
                try:
                    ss = syssig_pkg.create_system_signal("SS_%s" % sn)
                    self.sys_signals[(mn, sig.name)] = ss
                except Exception:
                    continue
                try:
                    isig = self.system.create_isignal(sn, sig_pkg, sig.length, ss)
                    self.isignals[(mn, sig.name)] = isig
                except Exception as e:
                    print("  Warning: ISignal %s: %s" % (sn, e))
                    continue
                try:
                    ipdu.map_signal(isig, sig.start, dbc_byte_order(sig))
                except Exception as e:
                    print("  Warning: map %s: %s" % (sn, e))

            try:
                frame = self.system.create_can_frame("%s_Frame" % mn, frame_pkg, msg.length)
                self.frames[mn] = frame
                try:
                    frame.map_pdu(ipdu, 0, abst.ByteOrder.MostSignificantByteLast)
                except Exception:
                    pass
                try:
                    self.channel.trigger_frame(frame, msg.frame_id, CanAddressingMode.Standard, CanFrameType.Can20)
                except Exception as e:
                    print("  Warning: trigger %s: %s" % (mn, e))
            except Exception as e:
                print("  Warning: frame %s: %s" % (mn, e))

    # -- E2E annotations ---------------------------------------------------

    def _create_e2e_annotations(self):
        e2e_pkg = self.am.get_or_create_package("/Taktflow/E2E")
        for msg in self.db.messages:
            mn = safe_name(msg.name)
            sigs = [s.name for s in msg.signals]
            if not ("E2E_DataID" in sigs and "E2E_AliveCounter" in sigs and "E2E_CRC8" in sigs):
                continue
            self.e2e_messages.append(mn)
            asil = get_msg_attr_value(msg, "ASIL", "QM")
            try:
                marker = e2e_pkg.create_system_signal("E2E_%s" % mn)
                elem = marker.element
                desc = elem.get_or_create_sub_element("DESC")
                l2 = desc.get_or_create_sub_element("L-2")
                l2.set_attribute("L", "EN")
                l2.character_data = (
                    "E2E Profile: DataID(4bit)+AliveCounter(4bit)+CRC8. "
                    "CAN ID: 0x%03X. ASIL: %s. Cycle: %sms."
                    % (msg.frame_id, asil, get_msg_attr_value(msg, "GenMsgCycleTime", "?"))
                )
            except Exception:
                pass

    # -- Software Components -----------------------------------------------

    def _create_swc_types(self):
        """Create ApplicationSwComponentType per SWC with ports, runnables, events."""
        if self.ecu_model is None:
            return

        for ecu_name, ecu_data in self.ecu_model.get("ecus", {}).items():
            eu = ecu_name.upper()
            swc_pkg = self.am.get_or_create_package("/Taktflow/SWCs/%s" % eu)

            tx_sigs = set(sn for _, sn, _ in self.ecu_tx_signals.get(eu, []))
            rx_sigs = set(sn for _, sn, _ in self.ecu_rx_signals.get(eu, []))

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
                except Exception as e:
                    print("  Warning: SWC %s/%s: %s" % (eu, swc_info["name"], e))

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
                    except Exception:
                        pass

        # Require ports (RX)
        for sn in rx_sigs:
            if self._sig_matches(sn, swc_low) and sn not in added:
                sr = self.sr_interfaces.get(sn)
                if sr:
                    try:
                        swc.create_r_port("RP_%s" % sn, sr)
                        self.port_count += 1
                        added.add(sn)
                    except Exception:
                        pass

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
            except Exception as e:
                print("  Warning: runnable %s: %s" % (func, e))

        # Init runnables
        for func in functions:
            if func.endswith("_Init") and func not in self.swc_runnables:
                try:
                    run = beh.create_runnable_entity(func)
                    self.swc_runnables[func] = run
                    self.runnable_count += 1
                    beh.create_init_event("IE_%s" % func, run)
                    self.init_event_count += 1
                except Exception:
                    pass

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
            a = get_msg_attr_value(msg, "ASIL", "QM")
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
    if len(sys.argv) < 3:
        print("Usage: %s <dbc-file> <output-dir> [ecu-model-json]" % sys.argv[0])
        sys.exit(1)

    dbc_path = sys.argv[1]
    output_dir = sys.argv[2]
    model_path = sys.argv[3] if len(sys.argv) > 3 else None

    if not os.path.isfile(dbc_path):
        print("Error: DBC file not found: %s" % dbc_path)
        sys.exit(1)

    print("Converting %s -> ARXML..." % dbc_path)
    if model_path:
        print("  ECU model: %s" % model_path)

    c = Dbc2Arxml(dbc_path, model_path)
    c.convert()
    c.report()
    c.write(output_dir)
    print("\nDone.")


if __name__ == "__main__":
    main()
