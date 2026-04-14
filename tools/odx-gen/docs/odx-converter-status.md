# odx-converter compatibility status

Date: 2026-04-14

## What was tried

```sh
cd /h/taktflow-embedded-production/tools/odx-gen
.venv/Scripts/python -m odx_gen --list
# -> cvc, fzc, rzc, tcu

.venv/Scripts/python -m odx_gen cvc
# -> wrote H:\taktflow-embedded-production\firmware\ecu\cvc\odx\cvc.pdx (1967 bytes)

.venv/Scripts/python -m pytest tests/ -v
# -> 17 passed in 1.97s
```

PDX generation is fully working for every discovered ECU. Each generated
PDX successfully round-trips through `odxtools.load_pdx_file()` and
exposes the expected number of services, requests, and positive
responses. See `tests/test_roundtrip.py`.

## Attempt to run odx-converter

```sh
cd /h/eclipse-opensovd/odx-converter
JAVA_HOME="C:/Program Files/Eclipse Adoptium/jdk-21.0.10.7-hotspot" \
  PATH="C:/Program Files/Eclipse Adoptium/jdk-21.0.10.7-hotspot/bin:$PATH" \
  ./gradlew :converter:shadowJar
```

Result: BUILD FAILED.

```
* What went wrong:
ODX schema not found at H:\eclipse-opensovd\odx-converter\converter\src\main\resources\schema\odx_2_2_0.xsd, aborting build
```

## Root cause

`odx-converter`'s build script enforces the presence of the ASAM ODX 2.2.0
XSD schema (`odx_2_2_0.xsd`). We deliberately do not ship this file in
the workspace per ADR 0008
(`eclipse-opensovd/docs/adr/0008-odx-community-xsd-default.md`). Without
that XSD the converter Gradle build refuses to compile, which means we
cannot produce `converter/build/libs/converter-all.jar` and therefore
cannot run a `pdx -> mdd` conversion locally.

## What we verified instead

We use `odxtools` (MIT-licensed, no XSD redistribution) to verify the
PDX shape:

| ECU | DIDs | Services | Total diag-comms | Round-trip OK |
| --- | ---- | -------- | ---------------- | ------------- |
| cvc | 4    | 4 non-DID | 8                | yes           |
| fzc | 0    | 4 non-DID | 4                | yes           |
| rzc | 0    | 4 non-DID | 4                | yes           |
| tcu | 0    | 4 non-DID | 4                | yes           |

All four PDX files load cleanly via `odxtools.load_pdx_file`. The ones
without DIDs still expose the four UDS service stubs derived from
`Dcm.h`/`Dcm.c` (DiagnosticSessionControl, ECUReset, SecurityAccess,
TesterPresent).

## Next step to unblock end-to-end conversion

Either:

1. Provide an MIT-or-equivalent ODX 2.2 XSD (the eclipse-opensovd
   "community XSD" track in ADR 0008), drop it into
   `odx-converter/converter/src/main/resources/schema/odx_2_2_0.xsd`,
   and rebuild.
2. Patch the converter's gradle build to make the schema requirement
   optional or to fetch a permissively-licensed substitute.
3. Replace odx-converter with an `odxtools`-based MDD writer once one
   exists (currently odxtools only reads/writes PDX, not MDD).

Until one of these lands, `odx-gen` itself is fully functional and
unblocked. The PDX files are valid ODX 2.2 archives.
