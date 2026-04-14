"""Build a PDX archive from a DBC file + the ISO 15031-6 DTC table.

This is the DBC ingestion path, parallel to `build_pdx.py` (which builds
from parsed Dcm_Cfg_*.c sources). It is used to produce "real-shaped"
ODX test data from public opendbc DBCs plus the generic OBD-II DTC
namespace.

Design rules (enforced by the hardcoded-literal regression test):

  * No DID numbers, DTC codes, service IDs, or signal names live in
    this module. Everything comes from YAML:
        - odx_gen/config/dbc_to_odx_mapping.yaml  (DID range, services)
        - odx_gen/data/iso_15031_6_dtcs.yaml      (DTC list)
        - the input DBC file itself                (signals, messages)

  * The only bare numeric constants permitted here are ISO 14229
    protocol offsets that are *mathematically true* and not ECU data
    (e.g. the positive-response SID offset "+ 0x40"). Those live in
    `build_pdx.py` and are re-imported here so there is a single home
    for UDS protocol arithmetic.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

import odxtools
from odxtools.compumethods.compucategory import CompuCategory
from odxtools.compumethods.identicalcompumethod import IdenticalCompuMethod
from odxtools.database import Database
from odxtools.dataobjectproperty import DataObjectProperty
from odxtools.diagdatadictionaryspec import DiagDataDictionarySpec
from odxtools.diaglayercontainer import DiagLayerContainer
from odxtools.diaglayers.basevariant import BaseVariant
from odxtools.diaglayers.basevariantraw import BaseVariantRaw
from odxtools.diaglayers.diaglayertype import DiagLayerType
from odxtools.diagservice import DiagService
from odxtools.nameditemlist import NamedItemList
from odxtools.odxlink import DocType, OdxDocFragment, OdxLinkId, OdxLinkRef
from odxtools.odxtypes import DataType
from odxtools.parameters.codedconstparameter import CodedConstParameter
from odxtools.parameters.valueparameter import ValueParameter
from odxtools.physicaltype import PhysicalType
from odxtools.request import Request
from odxtools.response import Response, ResponseType
from odxtools.standardlengthtype import StandardLengthType

from .build_pdx import _POSITIVE_RESPONSE_OFFSET  # UDS arithmetic, not ECU data
from .iso_15031_6_dtcs import Iso15031DtcTable, load_iso_15031_6_dtcs
from .parsers.dbc import DbcDatabase, DbcSignal, parse_dbc


_DEFAULT_MAPPING_RELPATH = ("config", "dbc_to_odx_mapping.yaml")


# --- config loading ---------------------------------------------------------


@dataclass
class DbcToOdxMapping:
    """In-memory view of `dbc_to_odx_mapping.yaml`."""

    did_start: int
    did_end: int
    signal_filter_mode: str
    min_length_bits: int
    max_length_bits: int
    uds_services: list[dict[str, Any]]
    source_path: str


def _default_mapping_path() -> Path:
    return Path(__file__).resolve().parent.joinpath(*_DEFAULT_MAPPING_RELPATH)


def load_mapping(path: str | Path | None = None) -> DbcToOdxMapping:
    """Load `dbc_to_odx_mapping.yaml` from disk."""
    p = Path(path) if path is not None else _default_mapping_path()
    if not p.is_file():
        raise FileNotFoundError(f"DBC->ODX mapping file not found: {p}")
    with p.open("r", encoding="utf-8") as fh:
        doc = yaml.safe_load(fh) or {}

    did_range = doc.get("did_range") or {}
    sig_filter = doc.get("signal_filter") or {}
    services = list(doc.get("uds_services") or [])

    return DbcToOdxMapping(
        did_start=int(did_range.get("start")),
        did_end=int(did_range.get("end")),
        signal_filter_mode=str(sig_filter.get("mode", "all")),
        min_length_bits=int(sig_filter.get("min_length_bits", 1)),
        max_length_bits=int(sig_filter.get("max_length_bits", 32)),
        uds_services=services,
        source_path=str(p),
    )


# --- helpers ----------------------------------------------------------------


def _safe_short_name(name: str) -> str:
    if not name:
        return "unnamed"
    s = re.sub(r"[^A-Za-z0-9_]+", "_", name)
    s = s.strip("_")
    if not s:
        return "unnamed"
    if not s[0].isalpha() and s[0] != "_":
        s = "n_" + s
    return s


def _eligible_signals(
    dbc: DbcDatabase, mapping: DbcToOdxMapping
) -> list[tuple[DbcSignal, str]]:
    """Return (signal, qualified_name) pairs that pass the mapping filter."""
    out: list[tuple[DbcSignal, str]] = []
    for msg in dbc.messages:
        for sig in msg.signals:
            if sig.length_bits < mapping.min_length_bits:
                continue
            if sig.length_bits > mapping.max_length_bits:
                continue
            qualified = f"{msg.name}_{sig.name}"
            out.append((sig, qualified))
    return out


def _length_bytes_from_bits(bits: int) -> int:
    return max(1, (bits + 7) // 8)


# --- main builder -----------------------------------------------------------


def build_database_from_dbc(
    dbc_path: str | Path,
    ecu_name: str,
    mapping_path: str | Path | None = None,
    dtc_table_path: str | Path | None = None,
) -> Database:
    """Construct an odxtools `Database` from a DBC file plus ISO 15031-6 DTCs.

    Args:
        dbc_path: Absolute path to a `.dbc` file.
        ecu_name: Short ECU name used for ODX short-names. Vendor prefix
            belongs in the caller.
        mapping_path: Override for the mapping YAML.
        dtc_table_path: Override for the ISO 15031-6 YAML.
    """
    dbc = parse_dbc(dbc_path)
    mapping = load_mapping(mapping_path)
    dtc_table: Iso15031DtcTable = load_iso_15031_6_dtcs(dtc_table_path)

    ecu = _safe_short_name(ecu_name).lower()
    dlc_short_name = ecu
    doc_frags = (OdxDocFragment(dlc_short_name, DocType.CONTAINER),)

    uint8_type = StandardLengthType(
        base_data_type=DataType.A_UINT32,
        bit_length=8,
    )
    uint_passthrough = IdenticalCompuMethod(
        category=CompuCategory.IDENTICAL,
        internal_type=DataType.A_UINT32,
        physical_type=DataType.A_UINT32,
    )
    uint8_phys = PhysicalType(base_data_type=DataType.A_UINT32)

    dop_uint8 = DataObjectProperty(
        odx_id=OdxLinkId(f"{ecu}.DOP.uint8", doc_frags),
        short_name="uint8",
        diag_coded_type=uint8_type,
        physical_type=uint8_phys,
        compu_method=uint_passthrough,
    )

    requests: list[Request] = []
    pos_responses: list[Response] = []
    neg_responses: list[Response] = []
    services: list[DiagService] = []

    # --- resolve read-did service id from mapping -------------------------
    read_did_sid: int | None = None
    for svc in mapping.uds_services:
        nm = str(svc.get("name", "")).lower()
        if nm == "readdatabyidentifier":
            read_did_sid = int(svc["sid"])
            break
    if read_did_sid is None:
        # No ReadDataByIdentifier service means no DIDs — but we still
        # build the other service stubs.
        eligible: list[tuple[DbcSignal, str]] = []
    else:
        eligible = _eligible_signals(dbc, mapping)

    # --- per-signal DIDs under the ReadDataByIdentifier service -----------
    next_did = mapping.did_start
    emitted_did_count = 0
    for sig, qualified in eligible:
        if next_did > mapping.did_end:
            break
        did_id = next_did
        next_did += 1
        emitted_did_count += 1

        sn = _safe_short_name(qualified).lower()
        sn = f"read_{sn}_{did_id:04x}"
        rq_id = OdxLinkId(f"{ecu}.RQ.{sn}", doc_frags)
        pr_id = OdxLinkId(f"{ecu}.PR.{sn}", doc_frags)
        svc_id = OdxLinkId(f"{ecu}.service.{sn}", doc_frags)

        rq = Request(
            odx_id=rq_id,
            short_name=sn,
            long_name=qualified,
            parameters=NamedItemList([
                CodedConstParameter(
                    short_name="sid",
                    diag_coded_type=uint8_type,
                    byte_position=0,
                    coded_value_raw=str(read_did_sid),
                ),
                CodedConstParameter(
                    short_name="did_high",
                    diag_coded_type=uint8_type,
                    byte_position=1,
                    coded_value_raw=str((did_id >> 8) & 0xFF),
                ),
                CodedConstParameter(
                    short_name="did_low",
                    diag_coded_type=uint8_type,
                    byte_position=2,
                    coded_value_raw=str(did_id & 0xFF),
                ),
            ]),
        )
        requests.append(rq)

        payload_bytes = _length_bytes_from_bits(sig.length_bits)
        pr_params: list = [
            CodedConstParameter(
                short_name="sid",
                diag_coded_type=uint8_type,
                byte_position=0,
                coded_value_raw=str(read_did_sid + _POSITIVE_RESPONSE_OFFSET),
            ),
            CodedConstParameter(
                short_name="did_high",
                diag_coded_type=uint8_type,
                byte_position=1,
                coded_value_raw=str((did_id >> 8) & 0xFF),
            ),
            CodedConstParameter(
                short_name="did_low",
                diag_coded_type=uint8_type,
                byte_position=2,
                coded_value_raw=str(did_id & 0xFF),
            ),
        ]
        for byte_idx in range(payload_bytes):
            pr_params.append(
                ValueParameter(
                    short_name=f"byte_{byte_idx}",
                    byte_position=3 + byte_idx,
                    dop_ref=OdxLinkRef.from_id(dop_uint8.odx_id),
                )
            )
        pr = Response(
            odx_id=pr_id,
            short_name=f"{sn}_response",
            response_type=ResponseType.POSITIVE,
            parameters=NamedItemList(pr_params),
        )
        pos_responses.append(pr)

        services.append(
            DiagService(
                odx_id=svc_id,
                short_name=sn,
                semantic="DATA",
                request_ref=OdxLinkRef.from_id(rq.odx_id),
                pos_response_refs=[OdxLinkRef.from_id(pr.odx_id)],
                neg_response_refs=[],
            )
        )

    # --- non-DID UDS service stubs from mapping ---------------------------
    for svc_def in mapping.uds_services:
        sid = int(svc_def["sid"])
        if sid == read_did_sid:
            continue
        name = str(svc_def.get("name", "")).strip() or f"svc_{sid}"
        semantic = str(svc_def.get("semantic", "FUNCTION")).strip().upper()

        sn = _safe_short_name(name).lower()
        rq_id = OdxLinkId(f"{ecu}.RQ.{sn}", doc_frags)
        pr_id = OdxLinkId(f"{ecu}.PR.{sn}", doc_frags)
        svc_id = OdxLinkId(f"{ecu}.service.{sn}", doc_frags)

        rq = Request(
            odx_id=rq_id,
            short_name=sn,
            long_name=name,
            parameters=NamedItemList([
                CodedConstParameter(
                    short_name="sid",
                    diag_coded_type=uint8_type,
                    byte_position=0,
                    coded_value_raw=str(sid),
                ),
            ]),
        )
        requests.append(rq)

        pr = Response(
            odx_id=pr_id,
            short_name=f"{sn}_response",
            response_type=ResponseType.POSITIVE,
            parameters=NamedItemList([
                CodedConstParameter(
                    short_name="sid",
                    diag_coded_type=uint8_type,
                    byte_position=0,
                    coded_value_raw=str(sid + _POSITIVE_RESPONSE_OFFSET),
                ),
            ]),
        )
        pos_responses.append(pr)

        services.append(
            DiagService(
                odx_id=svc_id,
                short_name=sn,
                semantic=semantic,
                request_ref=OdxLinkRef.from_id(rq.odx_id),
                pos_response_refs=[OdxLinkRef.from_id(pr.odx_id)],
                neg_response_refs=[],
            )
        )

    # --- DTC list is informational metadata -------------------------------
    # odxtools' full DTC-DOP pipeline is out of scope for this ingestion
    # step — we attach the DTC table to the container via a sidecar list
    # on the returned database. Consumers that need full DTC-DOP wiring
    # should extend this in a follow-up.
    ddds = DiagDataDictionarySpec(
        data_object_props=NamedItemList([dop_uint8]),
    )

    bv_raw = BaseVariantRaw(
        variant_type=DiagLayerType.BASE_VARIANT,
        odx_id=OdxLinkId(f"{ecu}.base_variant", doc_frags),
        short_name=f"{ecu}_base",
        long_name=f"{ecu_name.upper()} base variant (from DBC)",
        diag_data_dictionary_spec=ddds,
        diag_comms_raw=list(services),
        requests=NamedItemList(requests),
        positive_responses=NamedItemList(pos_responses),
        negative_responses=NamedItemList(neg_responses),
    )
    base_variant = BaseVariant(diag_layer_raw=bv_raw)

    dlc = DiagLayerContainer(
        odx_id=OdxLinkId(f"DLC.{ecu}", doc_frags),
        short_name=dlc_short_name,
        long_name=f"{ecu_name.upper()} diagnostic layer container (DBC)",
        base_variants=NamedItemList([base_variant]),
        ecu_variants=NamedItemList([]),
        protocols=NamedItemList([]),
    )

    db = Database()
    db.short_name = f"{ecu}_database"
    db._diag_layer_containers = NamedItemList([dlc])
    db.refresh()

    # Attach a sidecar with the DTC and stat info so tests / callers can
    # inspect what the builder decided without re-parsing the PDX.
    db.dbc_ingestion_stats = {  # type: ignore[attr-defined]
        "source_dbc": dbc.source_path,
        "mapping_source": mapping.source_path,
        "dtc_table_source": dtc_table.source_path,
        "dtc_count": len(dtc_table.dtcs),
        "dtc_codes": [d.code for d in dtc_table.dtcs],
        "did_count": emitted_did_count,
        "service_count": len(services),
        "eligible_signal_count": len(eligible),
    }

    return db


def write_pdx_from_dbc(
    dbc_path: str | Path,
    ecu_name: str,
    output_path: str | Path,
    mapping_path: str | Path | None = None,
    dtc_table_path: str | Path | None = None,
) -> Path:
    """Build a database from a DBC and write it to `output_path` as PDX."""
    out = Path(output_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    db = build_database_from_dbc(
        dbc_path=dbc_path,
        ecu_name=ecu_name,
        mapping_path=mapping_path,
        dtc_table_path=dtc_table_path,
    )
    odxtools.write_pdx_file(str(out), db)
    return out
