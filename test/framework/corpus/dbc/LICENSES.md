# DBC Corpus Provenance and Licenses

The corpus contains only public upstream test fixtures with an explicit
redistribution license. Files are renamed locally to make their source and
purpose visible. The unmodified upstream license texts are retained beside
this ledger.

## Vendored files

| Local file | Upstream source | Revision | Coverage | License |
|---|---|---|---|---|
| `cantools_signal_groups.dbc` | [cantools `sig_groups.dbc`](https://github.com/cantools/cantools/blob/1058d84a5c96791cee5f24a40933ad2833c017b3/tests/files/dbc/sig_groups.dbc) | `1058d84a5c96791cee5f24a40933ad2833c017b3` | `SIG_GROUP_` | MIT |
| `cantools_floating_point.dbc` | [cantools `floating_point.dbc`](https://github.com/cantools/cantools/blob/1058d84a5c96791cee5f24a40933ad2833c017b3/tests/files/dbc/floating_point.dbc) | `1058d84a5c96791cee5f24a40933ad2833c017b3` | 32-bit and 64-bit `SIG_VALTYPE_` floats | MIT |
| `cantools_extended_multiplexing.dbc` | [cantools `issue_184_extended_mux_cascaded.dbc`](https://github.com/cantools/cantools/blob/1058d84a5c96791cee5f24a40933ad2833c017b3/tests/files/dbc/issue_184_extended_mux_cascaded.dbc) | `1058d84a5c96791cee5f24a40933ad2833c017b3` | Cascaded extended multiplexing via `SG_MUL_VAL_` | MIT |
| `cantools_extended_ids.dbc` | [cantools `test_extended_id_dump.dbc`](https://github.com/cantools/cantools/blob/1058d84a5c96791cee5f24a40933ad2833c017b3/tests/files/dbc/test_extended_id_dump.dbc) | `1058d84a5c96791cee5f24a40933ad2833c017b3` | Standard and 29-bit extended CAN identifiers | MIT |
| `canmatrix_parser_stress.dbc` | [canmatrix `test_frame_decoding.dbc`](https://github.com/ebroecker/canmatrix/blob/c5708864235a17ae7f2b0efb83ee85466ae38f0b/tests/files/dbc/test_frame_decoding.dbc) | `c5708864235a17ae7f2b0efb83ee85466ae38f0b` | Overlap rejection and float signal parsing | BSD-2-Clause |

## License notices

- cantools is distributed under the [MIT License](https://github.com/cantools/cantools/blob/1058d84a5c96791cee5f24a40933ad2833c017b3/LICENSE).
  The required copyright and permission notice is retained in
  `LICENSE.cantools.txt`.
- canmatrix is distributed under the [BSD-2-Clause License](https://github.com/ebroecker/canmatrix/blob/c5708864235a17ae7f2b0efb83ee85466ae38f0b/LICENSE).
  The required copyright, conditions and disclaimer are retained in
  `LICENSE.canmatrix.txt`.

## Reviewed but not vendored

- [comma.ai opendbc](https://github.com/commaai/opendbc/tree/fe144714d18bdc11c99b4b8ac1d13a8195d35f66)
  was verified as MIT-licensed. It is intentionally optional because a full
  vehicle corpus is large and changes frequently. Supply a local checkout via
  `--opendbc`; the harness performs no implicit network access.
- [CSS Electronics OBD2 DBC](https://www.csselectronics.com/pages/obd2-dbc-file)
  and [J1939 demo material](https://www.csselectronics.com/pages/j1939-explained-simple-intro-tutorial)
  are advertised as free downloads, but the reviewed pages do not state an
  explicit redistribution license. No CSS Electronics file is vendored.
- Vector sample DBCs are excluded regardless of availability.
