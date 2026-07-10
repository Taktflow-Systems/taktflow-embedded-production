/**
 * @file    test_Can_TxFaultRecord_asild.c
 * @brief   Host tests for the retained RZC FDCAN TX failure record
 * @date    2026-07-10
 *
 * @verifies RZC-FDCAN-01 one-shot capture and sequence wrap
 */
#include "unity.h"
#include "Can_TxFaultRecord.h"

static Can_TxFaultSnapshotType snapshot(uint32 seed)
{
    Can_TxFaultSnapshotType value;

    value.Cccr = seed + 1u;
    value.Ecr = seed + 2u;
    value.Psr = seed + 3u;
    value.Ir = seed + 4u;
    value.Txfqs = seed + 5u;
    value.HalState = seed + 6u;
    value.HalLock = seed + 7u;
    value.HalErrorCode = seed + 8u;
    value.QueueHead = seed + 9u;
    value.QueueTail = seed + 10u;
    value.QueueHighWater = seed + 11u;
    value.FailedCanId = seed + 12u;
    value.ReturnPath = seed + 13u;
    return value;
}

void setUp(void)
{
    Can_TxFaultRecord_TestSetSequence(0u);
}

void tearDown(void)
{
}

void test_capture_records_complete_snapshot_once(void)
{
    Can_TxFaultSnapshotType first = snapshot(0x100u);
    Can_TxFaultSnapshotType second = snapshot(0x200u);
    const volatile Can_TxFaultRecordType* record;

    TEST_ASSERT_TRUE(Can_TxFaultRecord_Capture(&first));
    TEST_ASSERT_FALSE(Can_TxFaultRecord_Capture(&second));
    TEST_ASSERT_TRUE(Can_TxFaultRecord_IsValid());

    record = Can_TxFaultRecord_Get();
    TEST_ASSERT_EQUAL_HEX32(1u, record->SequenceNumber);
    TEST_ASSERT_EQUAL_HEX32(first.Cccr, record->Snapshot.Cccr);
    TEST_ASSERT_EQUAL_HEX32(first.Ecr, record->Snapshot.Ecr);
    TEST_ASSERT_EQUAL_HEX32(first.Psr, record->Snapshot.Psr);
    TEST_ASSERT_EQUAL_HEX32(first.Ir, record->Snapshot.Ir);
    TEST_ASSERT_EQUAL_HEX32(first.Txfqs, record->Snapshot.Txfqs);
    TEST_ASSERT_EQUAL_HEX32(first.HalState, record->Snapshot.HalState);
    TEST_ASSERT_EQUAL_HEX32(first.HalLock, record->Snapshot.HalLock);
    TEST_ASSERT_EQUAL_HEX32(first.HalErrorCode, record->Snapshot.HalErrorCode);
    TEST_ASSERT_EQUAL_HEX32(first.QueueHead, record->Snapshot.QueueHead);
    TEST_ASSERT_EQUAL_HEX32(first.QueueTail, record->Snapshot.QueueTail);
    TEST_ASSERT_EQUAL_HEX32(first.QueueHighWater, record->Snapshot.QueueHighWater);
    TEST_ASSERT_EQUAL_HEX32(first.FailedCanId, record->Snapshot.FailedCanId);
    TEST_ASSERT_EQUAL_HEX32(first.ReturnPath, record->Snapshot.ReturnPath);
}

void test_clear_preserves_sequence_for_next_capture(void)
{
    Can_TxFaultSnapshotType first = snapshot(1u);
    Can_TxFaultSnapshotType second = snapshot(2u);

    TEST_ASSERT_TRUE(Can_TxFaultRecord_Capture(&first));
    Can_TxFaultRecord_Clear();
    TEST_ASSERT_FALSE(Can_TxFaultRecord_IsValid());
    TEST_ASSERT_TRUE(Can_TxFaultRecord_Capture(&second));
    TEST_ASSERT_EQUAL_HEX32(2u, Can_TxFaultRecord_Get()->SequenceNumber);
}

void test_sequence_wrap_is_defined(void)
{
    Can_TxFaultSnapshotType value = snapshot(3u);

    Can_TxFaultRecord_TestSetSequence(0xFFFFFFFFu);
    TEST_ASSERT_TRUE(Can_TxFaultRecord_Capture(&value));
    TEST_ASSERT_EQUAL_HEX32(0u, Can_TxFaultRecord_Get()->SequenceNumber);
}

void test_null_snapshot_is_rejected_without_valid_record(void)
{
    TEST_ASSERT_FALSE(Can_TxFaultRecord_Capture(NULL_PTR));
    TEST_ASSERT_FALSE(Can_TxFaultRecord_IsValid());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_capture_records_complete_snapshot_once);
    RUN_TEST(test_clear_preserves_sequence_for_next_capture);
    RUN_TEST(test_sequence_wrap_is_defined);
    RUN_TEST(test_null_snapshot_is_rejected_without_valid_record);
    return UNITY_END();
}
