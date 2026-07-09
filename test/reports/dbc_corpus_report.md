# DBC Corpus Report

Mode: `must-not-crash`

| Source | File | Result | Reason |
|---|---|---|---|
| vendored | canmatrix_parser_stress.dbc | skipped-with-reason | cantools rejected input: UnsupportedDatabaseFormatError |
| vendored | cantools_extended_ids.dbc | converted | converter completed |
| vendored | cantools_extended_multiplexing.dbc | skipped-with-reason | DBC007: multiplexing is not represented in emitted ARXML |
| vendored | cantools_floating_point.dbc | converted | converter completed |
| vendored | cantools_signal_groups.dbc | converted | converter completed |

Converted: 3; skipped: 2; crashed: 0.
