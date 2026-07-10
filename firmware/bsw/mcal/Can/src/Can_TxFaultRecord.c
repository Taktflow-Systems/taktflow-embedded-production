/**
 * @file    Can_TxFaultRecord.c
 * @brief   Retained one-shot CAN transmit failure record
 * @date    2026-07-10
 *
 * @details RZC-FDCAN-01. Capture is bounded and performs no I/O. Validity is
 *          committed by writing Magic last, after the payload and inverse.
 *
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Can_TxFaultRecord.h"

#define CAN_TX_FAULT_RECORD_MAGIC  0x54584644u /* 'TXFD' */

#if defined(PLATFORM_STM32)
static volatile Can_TxFaultRecordType can_tx_fault_record
    __attribute__((section(".noinit")));
#else
static volatile Can_TxFaultRecordType can_tx_fault_record;
#endif

boolean Can_TxFaultRecord_IsValid(void)
{
    return (boolean)((can_tx_fault_record.Magic == CAN_TX_FAULT_RECORD_MAGIC) &&
                     (can_tx_fault_record.MagicInverse ==
                      (uint32)~CAN_TX_FAULT_RECORD_MAGIC));
}

boolean Can_TxFaultRecord_Capture(const Can_TxFaultSnapshotType* Snapshot)
{
    uint32 sequence;

    if ((Snapshot == NULL_PTR) || (Can_TxFaultRecord_IsValid() == TRUE)) {
        return FALSE;
    }

    sequence = can_tx_fault_record.SequenceNumber + 1u;
    can_tx_fault_record.Magic = 0u;
    can_tx_fault_record.SequenceNumber = sequence;
    can_tx_fault_record.Snapshot = *Snapshot;
    can_tx_fault_record.MagicInverse = (uint32)~CAN_TX_FAULT_RECORD_MAGIC;

#if defined(PLATFORM_STM32) && (defined(__arm__) || defined(__thumb__))
    __asm volatile ("dmb" ::: "memory");
#endif
    can_tx_fault_record.Magic = CAN_TX_FAULT_RECORD_MAGIC;
    return TRUE;
}

void Can_TxFaultRecord_Clear(void)
{
    can_tx_fault_record.Magic = 0u;
    can_tx_fault_record.MagicInverse = 0u;
}

const volatile Can_TxFaultRecordType* Can_TxFaultRecord_Get(void)
{
    return &can_tx_fault_record;
}

#if defined(UNIT_TEST)
void Can_TxFaultRecord_TestSetSequence(uint32 SequenceNumber)
{
    Can_TxFaultRecord_Clear();
    can_tx_fault_record.SequenceNumber = SequenceNumber;
}
#endif

#if defined(PLATFORM_STM32)
extern void Dbg_Uart_Print(const char* str);

static void can_tx_fault_record_print_hex32(uint32 Value)
{
    static const char digits[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    char buf[11];
    uint8 idx;

    buf[0] = '0';
    buf[1] = 'x';
    for (idx = 0u; idx < 8u; idx++) {
        buf[2u + idx] = digits[(Value >> (28u - (4u * idx))) & 0xFu];
    }
    buf[10] = '\0';
    Dbg_Uart_Print(buf);
}

static void can_tx_fault_record_print_pair(
    const char* Label, uint32 First, const char* SecondLabel, uint32 Second)
{
    Dbg_Uart_Print(Label);
    can_tx_fault_record_print_hex32(First);
    Dbg_Uart_Print(SecondLabel);
    can_tx_fault_record_print_hex32(Second);
    Dbg_Uart_Print("\r\n");
}

void Can_TxFaultRecord_BootReport(void)
{
    const volatile Can_TxFaultRecordType* record = &can_tx_fault_record;

    if (Can_TxFaultRecord_IsValid() == FALSE) {
        Dbg_Uart_Print("[CAN-TX] no retained failure record\r\n");
        return;
    }

    Dbg_Uart_Print("[CAN-TX] RETAINED FAILURE seq=");
    can_tx_fault_record_print_hex32(record->SequenceNumber);
    Dbg_Uart_Print("\r\n");
    can_tx_fault_record_print_pair(
        "[CAN-TX] ID=", record->Snapshot.FailedCanId,
        " PATH=", record->Snapshot.ReturnPath);
    can_tx_fault_record_print_pair(
        "[CAN-TX] CCCR=", record->Snapshot.Cccr,
        " ECR=", record->Snapshot.Ecr);
    can_tx_fault_record_print_pair(
        "[CAN-TX] PSR=", record->Snapshot.Psr,
        " IR=", record->Snapshot.Ir);
    can_tx_fault_record_print_pair(
        "[CAN-TX] TXFQS=", record->Snapshot.Txfqs,
        " HAL_STATE=", record->Snapshot.HalState);
    can_tx_fault_record_print_pair(
        "[CAN-TX] HAL_LOCK=", record->Snapshot.HalLock,
        " HAL_ERROR=", record->Snapshot.HalErrorCode);
    can_tx_fault_record_print_pair(
        "[CAN-TX] Q_HEAD=", record->Snapshot.QueueHead,
        " Q_TAIL=", record->Snapshot.QueueTail);
    can_tx_fault_record_print_pair(
        "[CAN-TX] Q_HWM=", record->Snapshot.QueueHighWater,
        " CLEARED=", 1u);

    Can_TxFaultRecord_Clear();
}
#else
void Can_TxFaultRecord_BootReport(void)
{
    /* Host builds have no retained target UART report. */
}
#endif
