"""
Rte_Cfg generator — produces Rte_Cfg_*.c and per-SWC Rte_Swc_*.h wrappers.

Generates signal config table (16 BSW + N ECU-specific), runnable config
table (sorted by priority descending), the aggregate Rte_ConfigType, and
typed inline wrappers for each SWC's ports.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


# BSW well-known signal names (slots 0-9, 10-15 reserved)
BSW_SIGNAL_NAMES = [
    "RTE_SIG_TORQUE_REQUEST",
    "RTE_SIG_STEERING_ANGLE",
    "RTE_SIG_VEHICLE_SPEED",
    "RTE_SIG_BRAKE_PRESSURE",
    "RTE_SIG_MOTOR_STATUS",
    "RTE_SIG_BATTERY_VOLTAGE",
    "RTE_SIG_BATTERY_CURRENT",
    "RTE_SIG_MOTOR_TEMP",
    "RTE_SIG_ESTOP_STATUS",
    "RTE_SIG_HEARTBEAT",
]

name = "rte"


class RteCfgGenerator:
    """Generates Rte_Cfg.c for each ECU."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render Rte_Cfg.c content for one ECU.

        @return dict of {filename: rendered_content}
        """
        # Build ECU-specific signal list (ID >= 16) sorted by ID
        ecu_app_signals = sorted(
            [(name, sid) for name, sid in ecu.rte_signal_map.items() if sid >= 16],
            key=lambda x: x[1],
        )

        # Collect periodic runnables (exclude init-only) and sort by priority desc
        all_runnables = []
        for swc in ecu.swcs:
            for r in swc.runnables:
                if not r.is_init and r.period_ms > 0:
                    all_runnables.append(r)
        sorted_runnables = sorted(all_runnables, key=lambda r: r.priority, reverse=True)

        context = {
            "ecu": ecu,
            "filename": f"Rte_Cfg_{ecu.prefix.capitalize()}.c",
            "brief": f"RTE configuration for {ecu.prefix} — signal table and runnable table",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
            "bsw_signal_names": BSW_SIGNAL_NAMES,
            "ecu_app_signals": ecu_app_signals,
            "sorted_runnables": sorted_runnables,
        }

        content = engine.render("rte/Rte_Cfg.c.j2", context)
        return {"Rte_Cfg.c": content}

    def render_wrappers(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render per-SWC typed wrapper headers for one ECU.

        @return dict of {"Rte_Swc_Pedal.h": content, ...}
        """
        results = {}
        for swc in ecu.swcs:
            swc_label = swc.short_name or swc.name
            # Strip ECU prefix if present (e.g., "CVC_Swc_Pedal" → "Swc_Pedal")
            clean_name = swc.name  # Already stripped by reader

            filename = f"Rte_{clean_name}.h"
            guard_name = filename.replace(".", "_").upper()

            read_ports = [p for p in swc.ports if p.direction == "REQUIRED"]
            write_ports = [p for p in swc.ports if p.direction == "PROVIDED"]

            context = {
                "ecu": ecu,
                "swc": swc,
                "filename": filename,
                "guard_name": guard_name,
                "brief": f"Typed RTE wrappers for {clean_name} ({ecu.prefix})",
                "tool_version": "1.0.0",
                "arxml_source": "TaktflowSystem.arxml",
                "read_ports": read_ports,
                "write_ports": write_ports,
            }

            content = engine.render("rte/Rte_Swc.h.j2", context)
            results[filename] = content

        return results

    def render_pbcfg(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> str:
        """
        Render Rte_PbCfg.h — per-ECU RTE buffer limits.

        @return rendered content string
        """
        # Build ECU-specific signal list to compute total count
        ecu_app_signals = sorted(
            [(name, sid) for name, sid in ecu.rte_signal_map.items() if sid >= 16],
            key=lambda x: x[1],
        )
        sig_count = 16 + len(ecu_app_signals) + ecu.rte_internal_signal_count

        # Count periodic runnables (exclude init-only)
        runnable_count = 0
        for swc in ecu.swcs:
            for r in swc.runnables:
                if not r.is_init and r.period_ms > 0:
                    runnable_count += 1

        context = {
            "ecu": ecu,
            "filename": "Rte_PbCfg.h",
            "brief": f"RTE buffer limits for {ecu.prefix} — sizes static arrays in Rte.c",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
            "sig_count": sig_count,
            "runnable_count": runnable_count,
        }

        return engine.render("rte/Rte_PbCfg.h.j2", context)

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write Rte_Cfg.c, Rte_PbCfg.h, and per-SWC wrappers for one ECU."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            output_name = f"Rte_Cfg_{ecu.prefix.capitalize()}.c"
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.cfg_dir,
                output_name,
            )
            result = engine.write_file(path, content)
            result.template = "rte/Rte_Cfg.c.j2"
            results.append(result)

        # Generate Rte_PbCfg.h (per-ECU RTE limits)
        pbcfg_content = self.render_pbcfg(ecu, gen_config, engine)
        pbcfg_path = os.path.join(
            engine.config.project_root,
            engine.config.output.base_dir,
            ecu.name,
            engine.config.output.header_dir,
            "Rte_PbCfg.h",
        )
        result = engine.write_file(pbcfg_path, pbcfg_content)
        result.template = "rte/Rte_PbCfg.h.j2"
        results.append(result)

        # Generate per-SWC typed wrapper headers
        wrappers = self.render_wrappers(ecu, gen_config, engine)
        for filename, content in wrappers.items():
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.header_dir,
                filename,
            )
            result = engine.write_file(path, content)
            result.template = "rte/Rte_Swc.h.j2"
            results.append(result)

        return results
