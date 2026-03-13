"""
SWC skeleton generator — produces Swc_*.c and Swc_*.h for each SWC.

Generates stub source files with runnable function bodies and headers
with function declarations. Uses generate-if-absent guard — never
overwrites existing application source files.

Each SWC source includes its typed RTE wrapper header for port access.
"""

from __future__ import annotations

import os

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu


name = "swc"


class SwcSkeletonGenerator:
    """Generates SWC skeleton .c and .h files for each ECU's SWCs."""

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """
        Render SWC skeleton files for all SWCs in one ECU.

        @return dict of {filename: rendered_content}
        """
        results = {}
        for swc in ecu.swcs:
            init_runnables = [r for r in swc.runnables if r.is_init]
            periodic_runnables = [r for r in swc.runnables if not r.is_init]

            context = {
                "ecu": ecu,
                "swc": swc,
                "init_runnables": init_runnables,
                "periodic_runnables": periodic_runnables,
            }

            # Render .h header
            h_context = {
                **context,
                "filename": f"{swc.name}.h",
                "brief": f"SWC header for {swc.name} ({ecu.prefix})",
                "tool_version": "1.0.0",
                "arxml_source": "TaktflowSystem.arxml",
                "guard_name": f"{swc.name}_H".upper(),
            }
            h_content = engine.render("swc/Swc_Skeleton.h.j2", h_context)
            results[f"{swc.name}.h"] = h_content

            # Render .c source
            c_context = {
                **context,
                "filename": f"{swc.name}.c",
                "brief": f"SWC implementation for {swc.name} ({ecu.prefix})",
                "tool_version": "1.0.0",
                "arxml_source": "TaktflowSystem.arxml",
            }
            c_content = engine.render("swc/Swc_Skeleton.c.j2", c_context)
            results[f"{swc.name}.c"] = c_content

        return results

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write SWC skeleton files (generate-if-absent)."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            if filename.endswith(".h"):
                subdir = engine.config.output.header_dir
            else:
                subdir = engine.config.output.src_dir

            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                subdir,
                filename,
            )

            # Generate-if-absent: never overwrite existing SWC source
            result = engine.write_file(path, content, overwrite=False)
            result.template = (
                "swc/Swc_Skeleton.h.j2" if filename.endswith(".h")
                else "swc/Swc_Skeleton.c.j2"
            )
            results.append(result)

        return results
