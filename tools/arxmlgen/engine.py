"""
arxmlgen template engine — Jinja2 rendering and file output.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass

import jinja2


@dataclass
class OutputFile:
    """Result of a single file generation."""

    path: str
    action: str  # "WRITE" | "CREATE" | "SKIP"
    template: str
    size: int = 0


class TemplateEngine:
    """Jinja2 template rendering and file output."""

    def __init__(self, config):
        self.config = config

        # Build template search path
        search_paths = []
        if config.template_search_path:
            search_paths.extend(config.template_search_path)

        # Built-in templates (always last fallback)
        builtin = os.path.join(os.path.dirname(__file__), "templates")
        search_paths.append(builtin)

        self.env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(search_paths),
            keep_trailing_newline=True,
            trim_blocks=True,
            lstrip_blocks=True,
            undefined=jinja2.StrictUndefined,
        )

        # Register custom filters
        self.env.filters["upper_snake"] = _filter_upper_snake
        self.env.filters["pascal_case"] = _filter_pascal_case
        self.env.filters["camel_case"] = _filter_camel_case
        self.env.filters["c_type"] = _filter_c_type
        self.env.filters["com_type"] = _filter_com_type
        self.env.filters["hex"] = _filter_hex
        self.env.filters["hex2"] = _filter_hex2
        self.env.filters["hex3"] = _filter_hex3
        self.env.filters["hex4"] = _filter_hex4
        self.env.filters["align"] = _filter_align
        self.env.filters["suffix_u"] = _filter_suffix_u

    def render(self, template_name: str, context: dict) -> str:
        """Render a Jinja2 template with the given context."""
        tmpl = self.env.get_template(template_name)
        return tmpl.render(**context)

    def write_file(
        self, path: str, content: str, overwrite: bool = True
    ) -> OutputFile:
        """
        Write generated content to file.

        @param path       Output file path
        @param content    Rendered content
        @param overwrite  If False, skip existing files (generate-if-absent)
        @return OutputFile with action taken
        """
        os.makedirs(os.path.dirname(path), exist_ok=True)

        if not overwrite and os.path.exists(path):
            return OutputFile(
                path=path, action="SKIP", template="", size=0
            )

        action = "CREATE" if not os.path.exists(path) else "WRITE"

        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)

        return OutputFile(
            path=path, action=action, template="", size=len(content)
        )


# ======================================================================
# Jinja2 custom filters
# ======================================================================


def _filter_upper_snake(value: str) -> str:
    """Convert to UPPER_SNAKE_CASE. 'PedalRaw1' → 'PEDAL_RAW_1'."""
    # Insert underscore before uppercase letters that follow lowercase
    s = re.sub(r"([a-z])([A-Z])", r"\1_\2", value)
    # Insert underscore before digits that follow letters
    s = re.sub(r"([a-zA-Z])(\d)", r"\1_\2", s)
    # Insert underscore before letters that follow digits
    s = re.sub(r"(\d)([a-zA-Z])", r"\1_\2", s)
    return s.upper()


def _filter_pascal_case(value: str) -> str:
    """Convert to PascalCase. 'pedal_raw' → 'PedalRaw'."""
    return "".join(w.capitalize() for w in re.split(r"[_\s]+", value))


def _filter_camel_case(value: str) -> str:
    """Convert to camelCase. 'pedal_raw' → 'pedalRaw'."""
    parts = re.split(r"[_\s]+", value)
    if not parts:
        return value
    return parts[0].lower() + "".join(w.capitalize() for w in parts[1:])


def _filter_c_type(signal) -> str:
    """Get C type from a Signal object or bit size int."""
    if isinstance(signal, int):
        bits = signal
    elif hasattr(signal, "bit_size"):
        bits = signal.bit_size
    elif hasattr(signal, "data_type"):
        return signal.data_type
    else:
        return "uint32_t"

    if bits <= 1:
        return "boolean"
    elif bits <= 8:
        return "uint8_t"
    elif bits <= 16:
        return "uint16_t"
    return "uint32_t"


def _filter_com_type(signal) -> str:
    """Get COM enum type from a Signal object."""
    if hasattr(signal, "bit_size"):
        bits = signal.bit_size
    elif isinstance(signal, int):
        bits = signal
    else:
        return "COM_UINT32"

    if bits <= 8:
        return "COM_UINT8"
    elif bits <= 16:
        return "COM_UINT16"
    return "COM_UINT32"


def _filter_hex(value: int) -> str:
    return f"0x{value:X}"


def _filter_hex2(value: int) -> str:
    return f"0x{value:02X}"


def _filter_hex3(value: int) -> str:
    return f"0x{value:03X}"


def _filter_hex4(value: int) -> str:
    return f"0x{value:04X}"


def _filter_align(value: str, width: int = 35) -> str:
    """Left-align string to given width."""
    return f"{value:<{width}}"


def _filter_suffix_u(value: int) -> str:
    """Add C unsigned suffix."""
    return f"{value}u"
