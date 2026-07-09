# Public DBC Regression Corpus

Run the compatibility gate and rewrite the committed deterministic report:

```sh
python tools/ci/run_dbc_corpus.py --must-not-crash
```

Run conversion plus autosar-data strict load-back validation:

```sh
python tools/ci/run_dbc_corpus.py --strict
```

Include an explicitly cloned opendbc checkout without vendoring it:

```sh
python tools/ci/run_dbc_corpus.py --must-not-crash --opendbc ../opendbc
```

The harness never downloads inputs. Results use only source labels and paths
relative to their corpus root, so the report is deterministic and contains no
workstation paths.
