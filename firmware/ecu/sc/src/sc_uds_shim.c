/**
 * @file    sc_uds_shim.c
 * @brief   Minimal HIL-only UDS shim for the Phase 5 SC diagnostic alias
 * @date    2026-04-20
 *
 * @details Answers a tiny single-frame subset of UDS directly on the TMS570
 *          so the Pi CAN-to-DoIP proxy can prove live SC routing without
 *          porting the full TCU diagnostic stack to the SC image.
 *
 *          Supported services:
 *            - 0x10 DiagnosticSessionControl (default, extended)
 *            - 0x14 ClearDiagnosticInformation
 *            - 0x19 ReadDTCInformation (subfunctions 0x01, 0x02)
 *            - 0x3E TesterPresent
 *
 *          The shim is intentionally HIL-only and single-frame only.
 */
#include "sc_uds_shim.h"

#include "Sc_Hw_Cfg.h"
#include "sc_can.h"
#include "sc_types.h"

#ifdef PLATFORM_HIL

#define UDS_FRAME_TYPE_SINGLE              0x00u
#define UDS_SVC_DIAGNOSTIC_SESSION_CONTROL 0x10u
#define UDS_SVC_CLEAR_DIAGNOSTIC_INFO      0x14u
#define UDS_SVC_READ_DTC_INFO              0x19u
#define UDS_SVC_TESTER_PRESENT             0x3Eu

#define UDS_POSITIVE_RESPONSE_OFFSET       0x40u
#define UDS_NEGATIVE_RESPONSE_SID          0x7Fu

#define UDS_NRC_SERVICE_NOT_SUPPORTED      0x11u
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED  0x12u
#define UDS_NRC_INCORRECT_LENGTH           0x13u

#define UDS_DTC_AVAILABILITY_MASK          0xFFu
#define UDS_DTC_FORMAT_ISO_14229           0x01u

static void sc_uds_send_single_frame(const uint8* payload, uint8 payload_len)
{
    uint8 frame[SC_CAN_DLC];
    uint8 i;

    if ((payload == NULL_PTR) || (payload_len == 0u) || (payload_len > 7u)) {
        return;
    }

    for (i = 0u; i < SC_CAN_DLC; i++) {
        frame[i] = 0u;
    }
    frame[0] = payload_len;
    for (i = 0u; i < payload_len; i++) {
        frame[i + 1u] = payload[i];
    }

    SC_CAN_TransmitDiagResponse(frame, SC_CAN_DLC);
}

static void sc_uds_send_nrc(uint8 request_sid, uint8 nrc)
{
    const uint8 payload[3] = {
        UDS_NEGATIVE_RESPONSE_SID,
        request_sid,
        nrc
    };
    sc_uds_send_single_frame(payload, 3u);
}

void SC_UdsShim_Init(void)
{
    /* No state to initialize yet. */
}

void SC_UdsShim_Poll(void)
{
    uint8 frame[SC_CAN_DLC];
    uint8 dlc = 0u;
    uint8 uds_len;
    uint8 sid;

    if (SC_CAN_GetDiagRequest(frame, &dlc) == FALSE) {
        return;
    }

    if ((dlc < 2u) || ((frame[0] & 0xF0u) != UDS_FRAME_TYPE_SINGLE)) {
        return;
    }

    uds_len = (uint8)(frame[0] & 0x0Fu);
    if ((uds_len == 0u) || (uds_len > 7u) || ((uint8)(uds_len + 1u) > dlc)) {
        return;
    }

    sid = frame[1];
    switch (sid) {
    case UDS_SVC_DIAGNOSTIC_SESSION_CONTROL:
        if (uds_len < 2u) {
            sc_uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH);
        } else if ((frame[2] != 0x01u) && (frame[2] != 0x03u)) {
            sc_uds_send_nrc(sid, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        } else {
            const uint8 payload[6] = {
                (uint8)(sid + UDS_POSITIVE_RESPONSE_OFFSET),
                frame[2],
                0x00u,
                0x32u,
                0x01u,
                0xF4u
            };
            sc_uds_send_single_frame(payload, 6u);
        }
        break;

    case UDS_SVC_TESTER_PRESENT:
        if (uds_len < 2u) {
            sc_uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH);
        } else if ((frame[2] & 0x7Fu) != 0x00u) {
            sc_uds_send_nrc(sid, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        } else if ((frame[2] & 0x80u) == 0u) {
            const uint8 payload[2] = {
                (uint8)(sid + UDS_POSITIVE_RESPONSE_OFFSET),
                0x00u
            };
            sc_uds_send_single_frame(payload, 2u);
        } else {
            /* 0x3E80 suppresses the positive response. */
        }
        break;

    case UDS_SVC_READ_DTC_INFO:
        if (uds_len < 3u) {
            sc_uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH);
        } else if (frame[2] == 0x01u) {
            const uint8 payload[6] = {
                (uint8)(sid + UDS_POSITIVE_RESPONSE_OFFSET),
                0x01u,
                UDS_DTC_AVAILABILITY_MASK,
                UDS_DTC_FORMAT_ISO_14229,
                0x00u,
                0x00u
            };
            sc_uds_send_single_frame(payload, 6u);
        } else if (frame[2] == 0x02u) {
            const uint8 payload[3] = {
                (uint8)(sid + UDS_POSITIVE_RESPONSE_OFFSET),
                0x02u,
                UDS_DTC_AVAILABILITY_MASK
            };
            sc_uds_send_single_frame(payload, 3u);
        } else {
            sc_uds_send_nrc(sid, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        break;

    case UDS_SVC_CLEAR_DIAGNOSTIC_INFO:
        if (uds_len < 4u) {
            sc_uds_send_nrc(sid, UDS_NRC_INCORRECT_LENGTH);
        } else {
            const uint8 payload[1] = {
                (uint8)(sid + UDS_POSITIVE_RESPONSE_OFFSET)
            };
            sc_uds_send_single_frame(payload, 1u);
        }
        break;

    default:
        sc_uds_send_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

#else

void SC_UdsShim_Init(void)
{
}

void SC_UdsShim_Poll(void)
{
}

#endif /* PLATFORM_HIL */
