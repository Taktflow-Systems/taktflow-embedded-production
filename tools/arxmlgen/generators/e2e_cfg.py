"""
E2E_Cfg generator — produces E2E_Cfg_*.c for each E2E-enabled ECU.

Generates per-PDU E2E protection config tables mapping:
  PDU ID → data_id, counter_bit, crc_bit

Runs for all ECUs with e2e enabled. SC (safety controller) must verify
E2E on incoming messages as part of its safety monitoring function.
The E2E data IDs come from the sidecar YAML via the ARXML reader.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


name = "e2e"


class E2ECfgGenerator:
    """Generates E2E_Cfg.c for each E2E-enabled ECU."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render E2E_Cfg.c content for one ECU.

        @return dict of {filename: rendered_content}
        """
        e2e_tx_pdus = [p for p in ecu.tx_pdus if p.e2e_protected]
        e2e_rx_pdus = [p for p in ecu.rx_pdus if p.e2e_protected]

        context = {
            "ecu": ecu,
            "filename": f"E2E_Cfg_{ecu.prefix.capitalize()}.c",
            "brief": f"E2E protection configuration for {ecu.prefix}",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
            "e2e_tx_pdus": e2e_tx_pdus,
            "e2e_rx_pdus": e2e_rx_pdus,
        }

        content = engine.render("e2e/E2E_Cfg.c.j2", context)
        return {"E2E_Cfg.c": content}

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write E2E_Cfg.c for one ECU."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            output_name = f"E2E_Cfg_{ecu.prefix.capitalize()}.c"
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.cfg_dir,
                output_name,
            )
            result = engine.write_file(path, content)
            result.template = "e2e/E2E_Cfg.c.j2"
            results.append(result)

        return results
