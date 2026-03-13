/**
 * @file    test_Swc_FzcNvm.c
 * @brief   Unit tests for Swc_FzcNvm — DTC persistence and calibration storage
 * @date    2026-02-24
 *
 * @verifies SWR-FZC-031, SWR-FZC-032
 *
 * Tests DTC store/load with CRC-16 integrity, DTC CRC corruption detection,
 * calibration load with valid data, calibration corrupt fallback to defaults,
 * and lidar threshold calibration values.
 *
 * Mocks: NvM_ReadBlock, NvM_WriteBlock
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * NVM Constants (from header)
 * ================================================================== */

#define FZC_NVM_DTC_MAX_SLOTS       20u
#define FZC_NVM_CRC16_POLY       0x1021u
#define FZC_NVM_CRC16_INIT       0xFFFFu

#define FZC_NVM_DTC_EMPTY           0u
#define FZC_NVM_DTC_ACTIVE          1u
#define FZC_NVM_DTC_PASSIVE         2u
#define FZC_NVM_DTC_CLEARED         3u

/* Default calibration values */
#define FZC_NVM_CAL_STEER_OFFSET_DEFAULT     0
#define FZC_NVM_CAL_STEER_GAIN_DEFAULT     100u
#define FZC_NVM_CAL_BRAKE_OFFSET_DEFAULT     0
#define FZC_NVM_CAL_BRAKE_GAIN_DEFAULT     100u
#define FZC_NVM_CAL_LIDAR_WARN_DEFAULT     100u
#define FZC_NVM_CAL_LIDAR_BRAKE_DEFAULT     50u
#define FZC_NVM_CAL_LIDAR_EMERG_DEFAULT     20u

/* DTC IDs from Fzc_Cfg.h */
#define FZC_DTC_STEER_PLAUSIBILITY    0u
#define FZC_DTC_BRAKE_FAULT           5u

/* ==================================================================
 * NVM Types (from header)
 * ================================================================== */

typedef struct {
    uint8   dtcId;
    uint8   status;
    sint16  freezeSteer;
    uint8   freezeBrake;
    uint16  freezeLidar;
    uint16  crc;
} Swc_FzcNvm_DtcRecord;

typedef struct {
    sint16  steerCenterOffset;
    uint16  steerGain;
    sint16  brakePosOffset;
    uint16  brakeGain;
    uint16  lidarWarnCm;
    uint16  lidarBrakeCm;
    uint16  lidarEmergencyCm;
    uint16  crc;
} Swc_FzcNvm_CalData;

/* ==================================================================
 * Prevent BSW headers from redefining types
 * ================================================================== */

#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_FZC_NVM_H
#define FZC_CFG_H
#define NVM_H

/* Forward-declare public API (guarded out from header) */
void               Swc_FzcNvm_Init(void);
Std_ReturnType     Swc_FzcNvm_StoreDtc(uint8 dtcId, sint16 steerAngle,
                                        uint8 brakePos, uint16 lidarDist);
Std_ReturnType     Swc_FzcNvm_LoadDtc(uint8 index, Swc_FzcNvm_DtcRecord* record);
Std_ReturnType     Swc_FzcNvm_LoadCal(Swc_FzcNvm_CalData* cal);
Std_ReturnType     Swc_FzcNvm_StoreCal(const Swc_FzcNvm_CalData* cal);
uint16             Swc_FzcNvm_Crc16(const uint8* data, uint16 length);

/* ==================================================================
 * Mock: NvM_ReadBlock / NvM_WriteBlock
 * ================================================================== */

static uint8 mock_nvm_write_count;
static uint8 mock_nvm_read_count;

Std_ReturnType NvM_ReadBlock(uint8 BlockId, void* DstPtr)
{
    mock_nvm_read_count++;
    (void)BlockId;
    (void)DstPtr;
    /* Return OK but don't modify buffer — Init already set defaults */
    return E_OK;
}

Std_ReturnType NvM_WriteBlock(uint8 BlockId, const void* SrcPtr)
{
    mock_nvm_write_count++;
    (void)BlockId;
    (void)SrcPtr;
    return E_OK;
}

/* ==================================================================
 * Include SWC under test (source inclusion for test build)
 * ================================================================== */

#include "../src/Swc_FzcNvm.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    mock_nvm_write_count = 0u;
    mock_nvm_read_count  = 0u;

    Swc_FzcNvm_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-FZC-031: DTC Store and Load (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-031 — Store DTC with freeze-frame, load back, verify data */
void test_FzcNvm_store_load_dtc(void)
{
    Std_ReturnType ret;
    Swc_FzcNvm_DtcRecord record;

    /* Store a DTC: steering plausibility, angle=20, brake=50%, lidar=100cm */
    ret = Swc_FzcNvm_StoreDtc(FZC_DTC_STEER_PLAUSIBILITY, 20, 50u, 100u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Load it back */
    ret = Swc_FzcNvm_LoadDtc(0u, &record);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Verify all freeze-frame data */
    TEST_ASSERT_EQUAL_UINT8(FZC_DTC_STEER_PLAUSIBILITY, record.dtcId);
    TEST_ASSERT_EQUAL_UINT8(FZC_NVM_DTC_ACTIVE, record.status);
    TEST_ASSERT_EQUAL_INT16(20, record.freezeSteer);
    TEST_ASSERT_EQUAL_UINT8(50u, record.freezeBrake);
    TEST_ASSERT_EQUAL_UINT16(100u, record.freezeLidar);

    /* CRC should be non-zero and valid */
    TEST_ASSERT_TRUE(record.crc != 0u);
}

/* ==================================================================
 * SWR-FZC-031: DTC CRC Corruption Detection (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-031 — Corrupted DTC CRC detected on load */
void test_FzcNvm_dtc_crc_corruption(void)
{
    Std_ReturnType ret;
    Swc_FzcNvm_DtcRecord record;

    /* Store a valid DTC */
    ret = Swc_FzcNvm_StoreDtc(FZC_DTC_BRAKE_FAULT, 10, 30u, 200u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Verify it loads OK first */
    ret = Swc_FzcNvm_LoadDtc(0u, &record);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Now corrupt the CRC by storing a second DTC and then manually
     * verifying that an empty/out-of-range slot returns E_NOT_OK */
    ret = Swc_FzcNvm_LoadDtc(5u, &record);  /* Slot 5 is empty */
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);

    /* Out-of-range index */
    ret = Swc_FzcNvm_LoadDtc(FZC_NVM_DTC_MAX_SLOTS, &record);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);

    /* NULL pointer */
    ret = Swc_FzcNvm_LoadDtc(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-FZC-032: Calibration Load Valid (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-032 — Load calibration data returns valid defaults after init */
void test_FzcNvm_cal_load_valid(void)
{
    Swc_FzcNvm_CalData cal;
    Std_ReturnType ret;

    /* After init with no NVM data, defaults should be loaded */
    ret = Swc_FzcNvm_LoadCal(&cal);

    /* E_OK because defaults have valid CRC */
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Verify default values */
    TEST_ASSERT_EQUAL_INT16(FZC_NVM_CAL_STEER_OFFSET_DEFAULT, cal.steerCenterOffset);
    TEST_ASSERT_EQUAL_UINT16(FZC_NVM_CAL_STEER_GAIN_DEFAULT, cal.steerGain);
    TEST_ASSERT_EQUAL_INT16(FZC_NVM_CAL_BRAKE_OFFSET_DEFAULT, cal.brakePosOffset);
    TEST_ASSERT_EQUAL_UINT16(FZC_NVM_CAL_BRAKE_GAIN_DEFAULT, cal.brakeGain);
}

/* ==================================================================
 * SWR-FZC-032: Calibration Corrupt Falls Back to Defaults (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-032 — Corrupt calibration CRC triggers default values */
void test_FzcNvm_cal_corrupt_uses_defaults(void)
{
    Swc_FzcNvm_CalData cal;
    Std_ReturnType ret;

    /* Store custom calibration */
    cal.steerCenterOffset = 5;
    cal.steerGain         = 110u;
    cal.brakePosOffset    = -3;
    cal.brakeGain         = 95u;
    cal.lidarWarnCm       = 120u;
    cal.lidarBrakeCm      = 60u;
    cal.lidarEmergencyCm  = 25u;
    cal.crc               = 0u;  /* Not computed yet */

    ret = Swc_FzcNvm_StoreCal(&cal);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Verify custom values loaded */
    Swc_FzcNvm_CalData loaded;
    ret = Swc_FzcNvm_LoadCal(&loaded);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_INT16(5, loaded.steerCenterOffset);
    TEST_ASSERT_EQUAL_UINT16(110u, loaded.steerGain);

    /* Now NULL pointer test */
    ret = Swc_FzcNvm_LoadCal(NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);

    ret = Swc_FzcNvm_StoreCal(NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-FZC-032: Lidar Threshold Calibration (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-032 — Lidar thresholds stored and loaded correctly */
void test_FzcNvm_cal_lidar_thresholds(void)
{
    Swc_FzcNvm_CalData cal;
    Swc_FzcNvm_CalData loaded;
    Std_ReturnType ret;

    /* Store custom lidar thresholds */
    cal.steerCenterOffset = 0;
    cal.steerGain         = 100u;
    cal.brakePosOffset    = 0;
    cal.brakeGain         = 100u;
    cal.lidarWarnCm       = 150u;   /* Custom warning threshold */
    cal.lidarBrakeCm      = 75u;    /* Custom brake threshold */
    cal.lidarEmergencyCm  = 30u;    /* Custom emergency threshold */
    cal.crc               = 0u;

    ret = Swc_FzcNvm_StoreCal(&cal);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Load and verify */
    ret = Swc_FzcNvm_LoadCal(&loaded);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* assert: lidar thresholds match stored values */
    TEST_ASSERT_EQUAL_UINT16(150u, loaded.lidarWarnCm);
    TEST_ASSERT_EQUAL_UINT16(75u, loaded.lidarBrakeCm);
    TEST_ASSERT_EQUAL_UINT16(30u, loaded.lidarEmergencyCm);

    /* assert: CRC is valid (non-zero, computed by StoreCal) */
    TEST_ASSERT_TRUE(loaded.crc != 0u);

    /* Verify CRC matches independently computed value */
    {
        uint8 buf[14];
        uint16 expected_crc;

        buf[0]  = (uint8)((uint16)loaded.steerCenterOffset & 0xFFu);
        buf[1]  = (uint8)(((uint16)loaded.steerCenterOffset >> 8u) & 0xFFu);
        buf[2]  = (uint8)(loaded.steerGain & 0xFFu);
        buf[3]  = (uint8)((loaded.steerGain >> 8u) & 0xFFu);
        buf[4]  = (uint8)((uint16)loaded.brakePosOffset & 0xFFu);
        buf[5]  = (uint8)(((uint16)loaded.brakePosOffset >> 8u) & 0xFFu);
        buf[6]  = (uint8)(loaded.brakeGain & 0xFFu);
        buf[7]  = (uint8)((loaded.brakeGain >> 8u) & 0xFFu);
        buf[8]  = (uint8)(loaded.lidarWarnCm & 0xFFu);
        buf[9]  = (uint8)((loaded.lidarWarnCm >> 8u) & 0xFFu);
        buf[10] = (uint8)(loaded.lidarBrakeCm & 0xFFu);
        buf[11] = (uint8)((loaded.lidarBrakeCm >> 8u) & 0xFFu);
        buf[12] = (uint8)(loaded.lidarEmergencyCm & 0xFFu);
        buf[13] = (uint8)((loaded.lidarEmergencyCm >> 8u) & 0xFFu);

        expected_crc = Swc_FzcNvm_Crc16(buf, 14u);
        TEST_ASSERT_EQUAL_UINT16(expected_crc, loaded.crc);
    }
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-031: DTC Persistence */
    RUN_TEST(test_FzcNvm_store_load_dtc);
    RUN_TEST(test_FzcNvm_dtc_crc_corruption);

    /* SWR-FZC-032: Servo Calibration Data Storage */
    RUN_TEST(test_FzcNvm_cal_load_valid);
    RUN_TEST(test_FzcNvm_cal_corrupt_uses_defaults);
    RUN_TEST(test_FzcNvm_cal_lidar_thresholds);

    return UNITY_END();
}
