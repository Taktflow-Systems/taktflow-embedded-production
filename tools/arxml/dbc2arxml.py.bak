#!/usr/bin/env python3
"""
DBC -> ARXML Converter for Taktflow Embedded Platform

Reads gateway/taktflow.dbc and generates AUTOSAR R22-11 compliant ARXML:
  - System topology:     ECU instances, CAN cluster, physical channel
  - Communication:       ISignalIPdus, ISignals, CAN frames, frame triggerings
  - Data types:          CompuMethods (enums + linear scales)
  - E2E annotations:     E2E protection metadata for protected messages

Usage:
    python tools/arxml/dbc2arxml.py <path-to-dbc> <output-dir>
"""

import sys
import os
import cantools
import autosar_data as asr
import autosar_data.abstraction as abst
from autosar_data.abstraction.communication import (
    CanAddressingMode,
    CanFrameType,
    CycleRepetition,
)
from autosar_data.abstraction.datatype import (
    CompuMethodContent_TextTable,
    CompuMethodContent_Linear,
    CompuScaleDirection,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def safe_name(name):
    """Sanitize a DBC name for ARXML SHORT-NAME (alphanum + underscore)."""
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
            if hasattr(attr, "value"):
                return attr.value
            return attr
    except (AttributeError, TypeError):
        pass
    return default


# ---------------------------------------------------------------------------
# Main converter
# ---------------------------------------------------------------------------

class Dbc2Arxml:
    """Convert a DBC file to ARXML using autosar-data abstraction layer."""

    def __init__(self, dbc_path):
        self.db = cantools.database.load_file(dbc_path)
        self.am = abst.AutosarModelAbstraction.create("AUTOSAR_00051")
        self.system = None
        self.cluster = None
        self.channel = None
        self.ecus = {}
        self.controllers = {}
        self.ipdus = {}
        self.isignals = {}
        self.frames = {}
        self.sys_signals = {}
        self.compu_methods = {}
        self.e2e_messages = []

    def convert(self):
        """Run the full conversion pipeline."""
        self._create_system()
        self._create_ecus()
        self._create_data_types()
        self._create_communication()
        self._create_e2e_annotations()

    def write(self, output_dir, output_name="TaktflowSystem.arxml"):
        """Write ARXML file to output directory."""
        os.makedirs(output_dir, exist_ok=True)
        files = self.am.model.serialize_files()
        for _filename, content in files.items():
            filepath = os.path.join(output_dir, output_name)
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(content)
            print("  Written: %s (%d bytes)" % (filepath, len(content)))

    # -- System topology ---------------------------------------------------

    def _create_system(self):
        """Create system, CAN cluster, and physical channel."""
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
        """Create ECU instances from DBC BU_ definitions."""
        ecu_pkg = self.am.get_or_create_package("/Taktflow/ECUs")

        # Add ECU descriptions from DBC comments
        ecu_comments = {}
        for node in self.db.nodes:
            if hasattr(node, "comment") and node.comment:
                ecu_comments[node.name] = node.comment

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

    # -- Data types (CompuMethods from DBC VAL_ and factor/offset) ---------

    def _create_data_types(self):
        """Create CompuMethods from DBC value tables and linear scales."""
        dt_pkg = self.am.get_or_create_package("/Taktflow/DataTypes")

        # Track which signal names already have a CompuMethod
        created = set()

        for msg in self.db.messages:
            for sig in msg.signals:
                cm_name = "CM_%s" % safe_name(sig.name)

                if cm_name in created:
                    continue

                if sig.choices:
                    self._create_enum_compu_method(dt_pkg, cm_name, sig)
                    created.add(cm_name)
                elif sig.scale != 1.0 or sig.offset != 0.0:
                    self._create_linear_compu_method(dt_pkg, cm_name, sig)
                    created.add(cm_name)

    def _create_enum_compu_method(self, pkg, name, sig):
        """Create a TEXTTABLE CompuMethod from DBC VAL_ choices."""
        try:
            tt = CompuMethodContent_TextTable(texts=[])
            cm = pkg.create_compu_method(name, tt)

            # Populate COMPU-INTERNAL-TO-PHYS > COMPU-SCALES manually
            elem = cm.element
            citp = elem.get_or_create_sub_element("COMPU-INTERNAL-TO-PHYS")
            scales = citp.get_or_create_sub_element("COMPU-SCALES")

            for value, label in sorted(sig.choices.items()):
                scale = scales.create_sub_element("COMPU-SCALE")
                ll = scale.get_or_create_sub_element("LOWER-LIMIT")
                ll.character_data = str(int(value))
                ul = scale.get_or_create_sub_element("UPPER-LIMIT")
                ul.character_data = str(int(value))
                cc = scale.get_or_create_sub_element("COMPU-CONST")
                vt = cc.get_or_create_sub_element("VT")
                vt.character_data = str(label)

            self.compu_methods[name] = cm
        except Exception as e:
            print("  Warning: enum CompuMethod %s: %s" % (name, e))

    def _create_linear_compu_method(self, pkg, name, sig):
        """Create a LINEAR CompuMethod from DBC factor/offset."""
        try:
            lin = CompuMethodContent_Linear(
                direction=CompuScaleDirection.IntToPhys,
                factor=float(sig.scale),
                offset=float(sig.offset),
                divisor=1.0,
            )
            cm = pkg.create_compu_method(name, lin)

            # Add physical range as COMPU-INTERNAL-TO-PHYS limits
            elem = cm.element
            citp = elem.get_or_create_sub_element("COMPU-INTERNAL-TO-PHYS")
            scales = citp.get_or_create_sub_element("COMPU-SCALES")
            scale = scales.get_or_create_sub_element("COMPU-SCALE")

            if sig.minimum is not None:
                ll = scale.get_or_create_sub_element("LOWER-LIMIT")
                ll.character_data = str(float(sig.minimum))
            if sig.maximum is not None:
                ul = scale.get_or_create_sub_element("UPPER-LIMIT")
                ul.character_data = str(float(sig.maximum))

            # Add unit annotation
            if sig.unit:
                try:
                    unit_ref = elem.get_or_create_sub_element("DISPLAY-FORMAT")
                    unit_ref.character_data = sig.unit
                except Exception:
                    pass

            self.compu_methods[name] = cm
        except Exception as e:
            print("  Warning: linear CompuMethod %s: %s" % (name, e))

    # -- Communication layer -----------------------------------------------

    def _create_communication(self):
        """Create ISignalIPdus, ISignals, CAN frames, and frame triggerings."""
        ipdu_pkg = self.am.get_or_create_package("/Taktflow/Communication/IPdus")
        sig_pkg = self.am.get_or_create_package("/Taktflow/Communication/Signals")
        syssig_pkg = self.am.get_or_create_package(
            "/Taktflow/Communication/SystemSignals"
        )
        frame_pkg = self.am.get_or_create_package("/Taktflow/Communication/Frames")

        for msg in self.db.messages:
            msg_name = safe_name(msg.name)

            # 1. Create ISignalIPdu
            ipdu = self.system.create_isignal_ipdu(msg_name, ipdu_pkg, msg.length)
            self.ipdus[msg_name] = ipdu

            # Set timing
            cycle_ms = get_msg_attr_value(msg, "GenMsgCycleTime", 0)
            if cycle_ms and int(cycle_ms) > 0:
                try:
                    ipdu.set_timing(
                        float(int(cycle_ms)) / 1000.0,
                        CycleRepetition.CycleRepetitionOf1,
                    )
                except Exception:
                    pass

            # 2. Create ISignals
            for sig in msg.signals:
                sig_name = "%s_%s" % (msg_name, safe_name(sig.name))

                try:
                    syssig = syssig_pkg.create_system_signal("SS_%s" % sig_name)
                    self.sys_signals[(msg_name, sig.name)] = syssig
                except Exception:
                    continue

                try:
                    isignal = self.system.create_isignal(
                        sig_name, sig_pkg, sig.length, syssig
                    )
                    self.isignals[(msg_name, sig.name)] = isignal
                except Exception as e:
                    print("  Warning: ISignal %s: %s" % (sig_name, e))
                    continue

                try:
                    byte_order = dbc_byte_order(sig)
                    ipdu.map_signal(isignal, sig.start, byte_order)
                except Exception as e:
                    print("  Warning: map signal %s: %s" % (sig_name, e))

            # 3. Create CAN Frame + triggering
            try:
                frame = self.system.create_can_frame(
                    "%s_Frame" % msg_name, frame_pkg, msg.length
                )
                self.frames[msg_name] = frame

                # Map IPdu to frame
                try:
                    frame.map_pdu(ipdu, 0, abst.ByteOrder.MostSignificantByteLast)
                except Exception:
                    pass

                # Trigger frame on physical channel with CAN ID
                try:
                    self.channel.trigger_frame(
                        frame,
                        msg.frame_id,
                        CanAddressingMode.Standard,
                        CanFrameType.Can20,
                    )
                except Exception as e:
                    print("  Warning: trigger frame %s: %s" % (msg_name, e))

            except Exception as e:
                print("  Warning: frame %s: %s" % (msg_name, e))

    # -- E2E annotations ---------------------------------------------------

    def _create_e2e_annotations(self):
        """Annotate E2E-protected messages.

        Identifies protected messages by E2E_DataID + E2E_AliveCounter + E2E_CRC8
        signal pattern and stores metadata in a dedicated package.
        """
        e2e_pkg = self.am.get_or_create_package("/Taktflow/E2E")

        for msg in self.db.messages:
            msg_name = safe_name(msg.name)
            sig_names = [s.name for s in msg.signals]

            has_e2e = (
                "E2E_DataID" in sig_names
                and "E2E_AliveCounter" in sig_names
                and "E2E_CRC8" in sig_names
            )
            if not has_e2e:
                continue

            self.e2e_messages.append(msg_name)

            # Get ASIL level
            asil = get_msg_attr_value(msg, "ASIL", "QM")

            # Create a system signal as documentation marker
            try:
                marker = e2e_pkg.create_system_signal(
                    "E2E_%s" % msg_name
                )
                # Add annotation with E2E details via element API
                elem = marker.element
                desc = elem.get_or_create_sub_element("DESC")
                l2 = desc.get_or_create_sub_element("L-2")
                l2.set_attribute("L", "EN")
                l2.character_data = (
                    "E2E Profile: DataID(4bit)+AliveCounter(4bit)+CRC8. "
                    "CAN ID: 0x%03X. ASIL: %s. Cycle: %sms."
                    % (
                        msg.frame_id,
                        asil,
                        get_msg_attr_value(msg, "GenMsgCycleTime", "?"),
                    )
                )
            except Exception:
                pass

    # -- Reporting ---------------------------------------------------------

    def report(self):
        """Print conversion summary."""
        print("")
        print("=" * 60)
        print("DBC -> ARXML Conversion Summary")
        print("=" * 60)
        print("  ECU instances:    %d" % len(self.ecus))
        print("  Messages (IPdus): %d" % len(self.ipdus))
        print("  Signals:          %d" % len(self.isignals))
        print("  CAN Frames:       %d" % len(self.frames))
        print("  CompuMethods:     %d" % len(self.compu_methods))
        print("  System Signals:   %d" % len(self.sys_signals))
        print("  E2E protected:    %d" % len(self.e2e_messages))

        asil_counts = {}
        for msg in self.db.messages:
            asil = get_msg_attr_value(msg, "ASIL", "QM")
            asil_counts[asil] = asil_counts.get(asil, 0) + 1
        print("  ASIL distribution: %s" % asil_counts)

        # ECU TX/RX summary
        print("")
        print("  ECU signal routing:")
        for node in self.db.nodes:
            tx_count = sum(
                1 for m in self.db.messages
                if hasattr(m, "senders") and node.name in m.senders
            )
            rx_count = sum(
                1
                for m in self.db.messages
                for s in m.signals
                if node.name in (s.receivers or [])
            )
            print("    %s: TX=%d msgs, RX=%d signals" % (node.name, tx_count, rx_count))

        print("=" * 60)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print("Usage: %s <dbc-file> <output-dir>" % sys.argv[0])
        print("Example: %s gateway/taktflow.dbc arxml/" % sys.argv[0])
        sys.exit(1)

    dbc_path = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.isfile(dbc_path):
        print("Error: DBC file not found: %s" % dbc_path)
        sys.exit(1)

    print("Converting %s -> ARXML..." % dbc_path)
    converter = Dbc2Arxml(dbc_path)
    converter.convert()
    converter.report()
    converter.write(output_dir)
    print("\nDone.")


if __name__ == "__main__":
    main()
