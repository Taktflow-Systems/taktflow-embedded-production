"""
CanIf_Cfg generator — produces CanIf_Cfg_*.c for each ECU.

Generates TX PDU config table (CAN ID → upper-layer PDU ID),
RX PDU config table, and the aggregate CanIf_ConfigType struct.

CanIf PDU IDs mirror Com PDU IDs 1:1 — the mapping is implicit
via the ECU_COM_TX_*/ECU_COM_RX_* defines from Ecu_Cfg.h.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


name = "canif"


class CanIfCfgGenerator:
    """Generates CanIf_Cfg.c for each ECU."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render CanIf_Cfg.c content for one ECU.

        @return dict of {filename: rendered_content}
        """
        context = {
            "ecu": ecu,
            "filename": f"CanIf_Cfg_{ecu.prefix.capitalize()}.c",
            "brief": f"CanIf configuration for {ecu.prefix} — CAN ID to PDU routing",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
        }

        content = engine.render("canif/CanIf_Cfg.c.j2", context)
        return {"CanIf_Cfg.c": content}

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write CanIf_Cfg.c for one ECU."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            output_name = f"CanIf_Cfg_{ecu.prefix.capitalize()}.c"
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.cfg_dir,
                output_name,
            )
            result = engine.write_file(path, content)
            result.template = "canif/CanIf_Cfg.c.j2"
            results.append(result)

        return results
