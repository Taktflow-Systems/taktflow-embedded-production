"""CAN frame decoder using cantools + taktflow_vehicle.dbc."""
import os
import cantools

# E2E signal names to hide by default
E2E_SIGNAL_NAMES = {"E2E_DataID", "E2E_AliveCounter", "E2E_CRC8"}


class CanDecoder:
    def __init__(self, dbc_path=None):
        if dbc_path is None:
            # Default: find DBC relative to this file (core/ -> can-monitor/ -> tools/ -> repo root)
            base = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
            dbc_path = os.path.join(base, "gateway", "taktflow_vehicle.dbc")
        self.db = cantools.database.load_file(dbc_path)
        self._msg_cache = {}
        self._sig_ranges = {}  # frame_id -> {sig_name: (min, max)}
        self._asil_cache = {}
        self._cycle_cache = {}
        self._sender_cache = {}
        self._receiver_cache = {}
        for msg in self.db.messages:
            self._msg_cache[msg.frame_id] = msg
            ranges = {}
            for sig in msg.signals:
                if sig.minimum is not None and sig.maximum is not None:
                    ranges[sig.name] = (sig.minimum, sig.maximum)
            self._sig_ranges[msg.frame_id] = ranges
            # Extract DBC attributes
            attrs = {}
            try:
                attrs = msg.dbc.attributes
            except Exception:
                pass
            asil_attr = attrs.get("ASIL")
            self._asil_cache[msg.frame_id] = asil_attr.value if asil_attr else "QM"
            cycle_attr = attrs.get("GenMsgCycleTime")
            self._cycle_cache[msg.frame_id] = cycle_attr.value if cycle_attr else 0
            self._sender_cache[msg.frame_id] = msg.senders[0] if msg.senders else "?"
            receivers = set()
            for sig in msg.signals:
                for rx in (sig.receivers or []):
                    receivers.add(rx)
            if not receivers and getattr(msg, "receivers", None):
                receivers.update(msg.receivers)
            self._receiver_cache[msg.frame_id] = ", ".join(sorted(receivers)) if receivers else "-"

    def decode_raw(self, arb_id, data, decode_choices=True):
        """Decode raw CAN data (bytes) by arbitration ID.

        Returns dict with msg_name, signals, sender, asil, cycle_ms or None.
        """
        msg_def = self._msg_cache.get(arb_id)
        if msg_def is None:
            return None
        try:
            signals = msg_def.decode(data, decode_choices=decode_choices)
        except Exception:
            return None
        return {
            "msg_name": msg_def.name,
            "signals": signals,
            "sender": self._sender_cache.get(arb_id, "?"),
            "receivers": self._receiver_cache.get(arb_id, "-"),
            "asil": self._asil_cache.get(arb_id, "QM"),
            "cycle_ms": self._cycle_cache.get(arb_id, 0),
        }

    def signals_in_range(self, arb_id, signals):
        """Check all numeric signal values against DBC min/max ranges.

        Returns False if any signal exceeds its defined range (corrupted frame).
        """
        ranges = self._sig_ranges.get(arb_id)
        if not ranges:
            return True
        for name, value in signals.items():
            if not isinstance(value, (int, float)):
                continue
            bounds = ranges.get(name)
            if bounds and not (bounds[0] <= value <= bounds[1]):
                return False
        return True

    def get_message_name(self, arb_id):
        msg = self._msg_cache.get(arb_id)
        return msg.name if msg else None

    def get_signal_tree(self):
        """Return dict of {msg_name: {msg_id, sender, signals: [(name, unit, min, max), ...]}}.

        For building signal selector UI. Excludes E2E signals.
        """
        tree = {}
        for msg in self.db.messages:
            sigs = []
            for sig in msg.signals:
                if sig.name not in E2E_SIGNAL_NAMES:
                    sigs.append((sig.name, sig.unit or "", sig.minimum, sig.maximum))
            if sigs:
                tree[msg.name] = {
                    "msg_id": msg.frame_id,
                    "sender": msg.senders[0] if msg.senders else "?",
                    "signals": sigs,
                }
        return tree

    def format_signals(self, signals, hide_e2e=True):
        """Format signal dict to a display string."""
        parts = []
        for name, value in signals.items():
            if hide_e2e and name in E2E_SIGNAL_NAMES:
                continue
            if isinstance(value, float):
                parts.append(f"{name}={value:.2f}")
            else:
                parts.append(f"{name}={value}")
        return "  ".join(parts)
