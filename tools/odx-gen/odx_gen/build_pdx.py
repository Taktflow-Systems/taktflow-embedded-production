"""Build a PDX archive from an EcuDiagnosticModel using odxtools.

This module owns the odxtools wiring. It mirrors the structural pattern of
`examples/somersaultecu.py` but takes all data from an `EcuDiagnosticModel`
that was populated by parsing firmware C sources. There is NO hardcoded ECU
data in this module — only the shapes required by odxtools.
"""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterable

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

from .model import EcuDiagnosticModel


# UDS protocol constants — these are NOT ECU data. They are fixed by
# ISO 14229: a positive response to service 0xNN has SID 0xNN + 0x40.
_POSITIVE_RESPONSE_OFFSET = 0x40
_READ_DID_SID = 0x22  # ISO 14229 ReadDataByIdentifier service id


def _safe_short_name(name: str) -> str:
    """Sanitise a string for use as an ODX short name."""
    if not name:
        return "unnamed"
    s = re.sub(r"[^A-Za-z0-9_]+", "_", name)
    s = s.strip("_")
    if not s:
        return "unnamed"
    if not s[0].isalpha() and s[0] != "_":
        s = "n_" + s
    return s


def build_database(model: EcuDiagnosticModel) -> Database:
    """Construct an odxtools `Database` from a parsed EcuDiagnosticModel."""
    ecu = _safe_short_name(model.ecu_name).lower()
    dlc_short_name = ecu

    doc_frags = (OdxDocFragment(dlc_short_name, DocType.CONTAINER),)

    # --- diag coded types & compu methods ---------------------------------
    uint8_type = StandardLengthType(
        base_data_type=DataType.A_UINT32,
        bit_length=8,
    )
    uint16_type = StandardLengthType(
        base_data_type=DataType.A_UINT32,
        bit_length=16,
    )
    uint_passthrough = IdenticalCompuMethod(
        category=CompuCategory.IDENTICAL,
        internal_type=DataType.A_UINT32,
        physical_type=DataType.A_UINT32,
    )
    uint8_phys = PhysicalType(base_data_type=DataType.A_UINT32)

    # --- one DOP for raw byte payloads (used by every DID) ----------------
    dop_uint8 = DataObjectProperty(
        odx_id=OdxLinkId(f"{ecu}.DOP.uint8", doc_frags),
        short_name="uint8",
        diag_coded_type=uint8_type,
        physical_type=uint8_phys,
        compu_method=uint_passthrough,
    )

    # --- containers ------------------------------------------------------
    requests: list[Request] = []
    pos_responses: list[Response] = []
    neg_responses: list[Response] = []
    services: list[DiagService] = []

    # --- DID-based ReadDataByIdentifier services -------------------------
    for did in model.dids:
        sn = _safe_short_name(did.name) if did.name else f"did_{did.did_id:04x}"
        sn = f"read_{sn}_{did.did_id:04x}".lower()
        rq_id = OdxLinkId(f"{ecu}.RQ.{sn}", doc_frags)
        pr_id = OdxLinkId(f"{ecu}.PR.{sn}", doc_frags)
        svc_id = OdxLinkId(f"{ecu}.service.{sn}", doc_frags)

        rq = Request(
            odx_id=rq_id,
            short_name=sn,
            long_name=did.name or sn,
            parameters=NamedItemList([
                CodedConstParameter(
                    short_name="sid",
                    diag_coded_type=uint8_type,
                    byte_position=0,
                    coded_value_raw=str(_READ_DID_SID),
                ),
                CodedConstParameter(
                    short_name="did_high",
                    diag_coded_type=uint8_type,
                    byte_position=1,
                    coded_value_raw=str((did.did_id >> 8) & 0xFF),
                ),
                CodedConstParameter(
                    short_name="did_low",
                    diag_coded_type=uint8_type,
                    byte_position=2,
                    coded_value_raw=str(did.did_id & 0xFF),
                ),
            ]),
        )
        requests.append(rq)

        # Positive response: 0x62, did_high, did_low, then `length_bytes` payload
        pr_params = [
            CodedConstParameter(
                short_name="sid",
                diag_coded_type=uint8_type,
                byte_position=0,
                coded_value_raw=str(_READ_DID_SID + _POSITIVE_RESPONSE_OFFSET),
            ),
            CodedConstParameter(
                short_name="did_high",
                diag_coded_type=uint8_type,
                byte_position=1,
                coded_value_raw=str((did.did_id >> 8) & 0xFF),
            ),
            CodedConstParameter(
                short_name="did_low",
                diag_coded_type=uint8_type,
                byte_position=2,
                coded_value_raw=str(did.did_id & 0xFF),
            ),
        ]
        for byte_idx in range(did.length_bytes):
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

    # --- non-DID UDS service stubs ---------------------------------------
    # For services like SessionControl, ECUReset, SecurityAccess, TesterPresent,
    # we emit a minimal Request/Response pair with just the SID so the PDX
    # advertises the service surface. Skip ReadDataByIdentifier (covered above).
    for svc in model.services:
        if svc.sid == _READ_DID_SID:
            continue
        sn = _safe_short_name(svc.name).lower()
        rq_id = OdxLinkId(f"{ecu}.RQ.{sn}", doc_frags)
        pr_id = OdxLinkId(f"{ecu}.PR.{sn}", doc_frags)
        svc_id = OdxLinkId(f"{ecu}.service.{sn}", doc_frags)

        rq = Request(
            odx_id=rq_id,
            short_name=sn,
            long_name=svc.name,
            parameters=NamedItemList([
                CodedConstParameter(
                    short_name="sid",
                    diag_coded_type=uint8_type,
                    byte_position=0,
                    coded_value_raw=str(svc.sid),
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
                    coded_value_raw=str(svc.sid + _POSITIVE_RESPONSE_OFFSET),
                ),
            ]),
        )
        pos_responses.append(pr)

        services.append(
            DiagService(
                odx_id=svc_id,
                short_name=sn,
                semantic="FUNCTION",
                request_ref=OdxLinkRef.from_id(rq.odx_id),
                pos_response_refs=[OdxLinkRef.from_id(pr.odx_id)],
                neg_response_refs=[],
            )
        )

    # --- base variant ----------------------------------------------------
    ddds = DiagDataDictionarySpec(
        data_object_props=NamedItemList([dop_uint8]),
    )

    bv_short_name = f"{ecu}_base"
    bv_raw = BaseVariantRaw(
        variant_type=DiagLayerType.BASE_VARIANT,
        odx_id=OdxLinkId(f"{ecu}.base_variant", doc_frags),
        short_name=bv_short_name,
        long_name=f"{model.ecu_name.upper()} base variant",
        diag_data_dictionary_spec=ddds,
        diag_comms_raw=list(services),
        requests=NamedItemList(requests),
        positive_responses=NamedItemList(pos_responses),
        negative_responses=NamedItemList(neg_responses),
    )
    base_variant = BaseVariant(diag_layer_raw=bv_raw)

    # --- diag layer container --------------------------------------------
    dlc = DiagLayerContainer(
        odx_id=OdxLinkId(f"DLC.{ecu}", doc_frags),
        short_name=dlc_short_name,
        long_name=f"{model.ecu_name.upper()} diagnostic layer container",
        base_variants=NamedItemList([base_variant]),
        ecu_variants=NamedItemList([]),
        protocols=NamedItemList([]),
    )

    db = Database()
    db.short_name = f"{ecu}_database"
    db._diag_layer_containers = NamedItemList([dlc])
    # Resolve all OdxLinkRef -> object pointers so that the Jinja serializer
    # can detect DiagService objects via `hasattr(dc, "request")`.
    db.refresh()
    return db


def write_pdx(model: EcuDiagnosticModel, output_path: Path | str) -> Path:
    """Build a Database from `model` and write it as a PDX to `output_path`."""
    out = Path(output_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    db = build_database(model)
    odxtools.write_pdx_file(str(out), db)
    return out
