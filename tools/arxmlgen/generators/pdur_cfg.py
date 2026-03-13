"""
PduR_Cfg generator — produces PduR_Cfg_*.c for each ECU.

Generates routing tables that map CanIf PDUs ↔ Com PDUs:
  RX path: CanIf RX → Com RX  (PDUR_MOD_CANIF → PDUR_MOD_COM)
  TX path: Com TX → CanIf TX  (PDUR_MOD_COM → PDUR_MOD_CANIF)

Since CanIf PDU IDs mirror Com PDU IDs 1:1, the routing is a
straight pass-through — but the config table is still required
by the BSW PduR module for module-level abstraction.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


name = "pdur"


class PduRCfgGenerator:
    """Generates PduR_Cfg.c for each ECU."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render PduR_Cfg.c content for one ECU.

        @return dict of {filename: rendered_content}
        """
        context = {
            "ecu": ecu,
            "filename": f"PduR_Cfg_{ecu.prefix.capitalize()}.c",
            "brief": f"PduR configuration for {ecu.prefix} — CanIf ↔ Com routing",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
        }

        content = engine.render("pdur/PduR_Cfg.c.j2", context)
        return {"PduR_Cfg.c": content}

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write PduR_Cfg.c for one ECU."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            output_name = f"PduR_Cfg_{ecu.prefix.capitalize()}.c"
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.cfg_dir,
                output_name,
            )
            result = engine.write_file(path, content)
            result.template = "pdur/PduR_Cfg.c.j2"
            results.append(result)

        return results
