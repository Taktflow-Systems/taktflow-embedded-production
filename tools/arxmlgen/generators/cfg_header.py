"""
Ecu_Cfg.h generator — produces per-ECU master config header.

Generates RTE signal IDs, Com PDU IDs, DTC event IDs, E2E data IDs,
enums, and thresholds as #define constants.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


name = "cfg"


class CfgHeaderGenerator:
    """Generates Ecu_Cfg.h for each ECU."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render Ecu_Cfg.h content for one ECU.

        @return dict of {filename: rendered_content}
        """
        # Build ECU-specific signal list (ID >= 16) sorted by ID
        ecu_app_signals = sorted(
            [(name, sid) for name, sid in ecu.rte_signal_map.items() if sid >= 16],
            key=lambda x: x[1],
        )

        # Total signal count (16 BSW reserved + ECU-specific from map + internal)
        # rte_signal_map only contains app signals (ID >= 16)
        sig_count = 16 + len(ecu_app_signals) + ecu.rte_internal_signal_count

        # Count periodic runnables (exclude init-only) for RTE_MAX_RUNNABLES
        runnable_count = 0
        for swc in ecu.swcs:
            for r in swc.runnables:
                if not r.is_init and r.period_ms > 0:
                    runnable_count += 1

        # Total Com signal count (TX + RX signals across all PDUs)
        com_signal_count = sum(len(p.signals) for p in ecu.tx_pdus) + \
                           sum(len(p.signals) for p in ecu.rx_pdus)

        # Build internal signal defines: (name, id) starting after app signals
        internal_base = 16 + len(ecu_app_signals)
        rte_internal_signals = [
            (name, internal_base + i)
            for i, name in enumerate(ecu.rte_internal_signals)
        ]

        context = {
            "ecu": ecu,
            "filename": f"{ecu.prefix.capitalize()}_Cfg.h",
            "brief": f"{ecu.prefix} configuration — all {ecu.prefix}-specific ID definitions",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
            "ecu_app_signals": ecu_app_signals,
            "rte_internal_signals": rte_internal_signals,
            "sig_count": sig_count,
            "runnable_count": runnable_count,
            "com_signal_count": com_signal_count,
        }

        content = engine.render("cfg/Ecu_Cfg.h.j2", context)
        return {"Ecu_Cfg.h": content}

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write Ecu_Cfg.h for one ECU."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            output_name = f"{ecu.prefix.capitalize()}_Cfg.h"
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.header_dir,
                output_name,
            )
            result = engine.write_file(path, content)
            result.template = "cfg/Ecu_Cfg.h.j2"
            results.append(result)

        return results
