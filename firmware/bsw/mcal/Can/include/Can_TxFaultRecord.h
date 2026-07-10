/**
 * @file    Can_TxFaultRecord.h
 * @brief   Retained one-shot CAN transmit failure record
 * @date    2026-07-10
 *
 * @details RZC-FDCAN-01 captures the first persistent STM32G4 FDCAN enqueue
 *          failure without a debugger or failure-path UART traffic. The
 *          record survives a warm reset in .noinit and is reported once at
 *          the next boot.
 *
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef CAN_TXFAULTRECORD_H
#define CAN_TXFAULTRECORD_H

#include "Std_Types.h"

#define CAN_TX_FAILURE_PATH_DIRECT_ENQUEUE  1u
#define CAN_TX_FAILURE_PATH_QUEUE_OVERFLOW  2u
#define CAN_TX_FAILURE_PATH_QUEUE_DRAIN     3u

typedef struct {
    uint32 Cccr;
    uint32 Ecr;
    uint32 Psr;
    uint32 Ir;
    uint32 Txfqs;
    uint32 HalState;
    uint32 HalLock;
    uint32 HalErrorCode;
    uint32 QueueHead;
    uint32 QueueTail;
    uint32 QueueHighWater;
    uint32 FailedCanId;
    uint32 ReturnPath;
} Can_TxFaultSnapshotType;

typedef struct {
    uint32 Magic;
    uint32 SequenceNumber;
    Can_TxFaultSnapshotType Snapshot;
    uint32 MagicInverse;
} Can_TxFaultRecordType;

/** Store the first snapshot only. Returns FALSE while a valid record exists. */
boolean Can_TxFaultRecord_Capture(const Can_TxFaultSnapshotType* Snapshot);

/** TRUE when the retained record has a valid magic pair. */
boolean Can_TxFaultRecord_IsValid(void);

/** Invalidate the record while retaining its sequence counter. */
void Can_TxFaultRecord_Clear(void);

/** Read-only access for boot reporting and host verification. */
const volatile Can_TxFaultRecordType* Can_TxFaultRecord_Get(void);

/** Print and clear a retained record. Task context only, after UART init. */
void Can_TxFaultRecord_BootReport(void);

/**
 * Platform hook: gather hardware state and commit a retained snapshot.
 * The generic weak implementation is a no-op; RZC OSEK overrides it.
 */
void Can_Hw_CaptureTxFailure(
    uint32 FailedCanId,
    uint8 ReturnPath,
    uint8 QueueHead,
    uint8 QueueTail,
    uint32 QueueHighWater);

#if defined(UNIT_TEST)
/** Host-test seam for sequence wrap verification. */
void Can_TxFaultRecord_TestSetSequence(uint32 SequenceNumber);
#endif

#endif /* CAN_TXFAULTRECORD_H */
