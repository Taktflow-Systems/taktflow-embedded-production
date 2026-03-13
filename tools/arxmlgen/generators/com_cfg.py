"""
Com_Cfg generator — produces Com_Cfg_*.c for each ECU.

Generates shadow buffers, signal config table, TX/RX PDU config tables,
and the aggregate Com_ConfigType struct.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


# Generator metadata
name = "com"


class ComCfgGenerator:
    """Generates Com_Cfg.c for each ECU."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render Com_Cfg.c content for one ECU.

        @param ecu         ECU model data
        @param gen_config  Generator config from project.yaml
        @param engine      Jinja2 template engine
        @return dict of {filename: rendered_content}
        """
        context = {
            "ecu": ecu,
            "filename": f"Com_Cfg_{ecu.prefix.capitalize()}.c",
            "brief": f"Com module configuration for {ecu.prefix} — TX/RX PDUs and signal mappings",
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
        }

        content = engine.render("com/Com_Cfg.c.j2", context)
        return {"Com_Cfg.c": content}

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """
        Generate and write Com_Cfg.c for one ECU.

        @param ecu         ECU model data
        @param gen_config  Generator config from project.yaml
        @param engine      Jinja2 template engine
        @return list of OutputFile results
        """
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            output_name = f"Com_Cfg_{ecu.prefix.capitalize()}.c"
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.cfg_dir,
                output_name,
            )
            result = engine.write_file(path, content)
            result.template = "com/Com_Cfg.c.j2"
            results.append(result)

        return results
