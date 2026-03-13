/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-24 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
=========================================================================

    TODO:HARDWARE — Replace this minimal stub with the full Unity v2.6.x source
    from https://github.com/ThrowTheSwitch/Unity once network access is available.
    This stub is API-compatible with Unity v2.6.2. The full version adds:
    64-bit integer support, float/double assertions, array within-delta,
    color output, command-line test filtering, and printf support.
========================================================================= */

#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H
#define UNITY

#define UNITY_VERSION_MAJOR    2
#define UNITY_VERSION_MINOR    6
#define UNITY_VERSION_BUILD    0
#define UNITY_VERSION          ((UNITY_VERSION_MAJOR << 16) | (UNITY_VERSION_MINOR << 8) | UNITY_VERSION_BUILD)

#ifdef __cplusplus
extern "C"
{
#endif

#include "unity_internals.h"

/*-------------------------------------------------------
 * Test Setup / Teardown
 *-------------------------------------------------------*/

void setUp(void);
void tearDown(void);

/*-------------------------------------------------------
 * Configuration: See unity_internals.h for details.
 * All options should be passed as compiler flags.
 *-------------------------------------------------------*/

/*-------------------------------------------------------
 * Basic Fail and Ignore
 *-------------------------------------------------------*/

#define TEST_FAIL_MESSAGE(message)              UNITY_TEST_FAIL(__LINE__, (message))
#define TEST_FAIL()                             UNITY_TEST_FAIL(__LINE__, NULL)
#define TEST_IGNORE_MESSAGE(message)            UNITY_TEST_IGNORE(__LINE__, (message))
#define TEST_IGNORE()                           UNITY_TEST_IGNORE(__LINE__, NULL)
#define TEST_MESSAGE(message)                   UnityMessage((message), __LINE__)
#define TEST_ONLY()

#define TEST_PASS()                             TEST_ABORT()
#define TEST_PASS_MESSAGE(message)              do { UnityMessage((message), __LINE__); TEST_ABORT(); } while (0)

/* Build directives (no-ops, for Ceedling compatibility) */
#define TEST_SOURCE_FILE(a)
#define TEST_INCLUDE_PATH(a)

/*-------------------------------------------------------
 * Test Asserts (simple)
 *-------------------------------------------------------*/

/* Boolean */
#define TEST_ASSERT(condition)                  UNITY_TEST_ASSERT(       (condition), __LINE__, " Expression Evaluated To FALSE")
#define TEST_ASSERT_TRUE(condition)             UNITY_TEST_ASSERT(       (condition), __LINE__, " Expected TRUE Was FALSE")
#define TEST_ASSERT_UNLESS(condition)           UNITY_TEST_ASSERT(      !(condition), __LINE__, " Expression Evaluated To TRUE")
#define TEST_ASSERT_FALSE(condition)            UNITY_TEST_ASSERT(      !(condition), __LINE__, " Expected FALSE Was TRUE")
#define TEST_ASSERT_NULL(pointer)               UNITY_TEST_ASSERT_NULL(    (pointer), __LINE__, " Expected NULL")
#define TEST_ASSERT_NOT_NULL(pointer)           UNITY_TEST_ASSERT_NOT_NULL((pointer), __LINE__, " Expected Non-NULL")

/* Integers (of all sizes) */
#define TEST_ASSERT_EQUAL_INT(expected, actual)     UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL(expected, actual)         TEST_ASSERT_EQUAL_INT((expected), (actual))
#define TEST_ASSERT_EQUAL_INT8(expected, actual)    UNITY_TEST_ASSERT_EQUAL_INT8((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_INT16(expected, actual)   UNITY_TEST_ASSERT_EQUAL_INT16((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_INT32(expected, actual)   UNITY_TEST_ASSERT_EQUAL_INT32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT(expected, actual)    UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT8(expected, actual)   UNITY_TEST_ASSERT_EQUAL_UINT8((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT16(expected, actual)  UNITY_TEST_ASSERT_EQUAL_UINT16((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual)  UNITY_TEST_ASSERT_EQUAL_UINT32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX(expected, actual)     UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX8(expected, actual)    UNITY_TEST_ASSERT_EQUAL_HEX8((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX16(expected, actual)   UNITY_TEST_ASSERT_EQUAL_HEX16((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX32(expected, actual)   UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_CHAR(expected, actual)    UNITY_TEST_ASSERT_EQUAL_CHAR((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_BITS(mask, expected, actual)    UNITY_TEST_ASSERT_BITS((mask), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_BITS_HIGH(mask, actual)         UNITY_TEST_ASSERT_BITS((mask), (UNITY_UINT)(-1), (actual), __LINE__, NULL)
#define TEST_ASSERT_BITS_LOW(mask, actual)          UNITY_TEST_ASSERT_BITS((mask), (UNITY_UINT)(0), (actual), __LINE__, NULL)
#define TEST_ASSERT_BIT_HIGH(bit, actual)           UNITY_TEST_ASSERT_BITS(((UNITY_UINT)1 << (bit)), (UNITY_UINT)(-1), (actual), __LINE__, NULL)
#define TEST_ASSERT_BIT_LOW(bit, actual)            UNITY_TEST_ASSERT_BITS(((UNITY_UINT)1 << (bit)), (UNITY_UINT)(0), (actual), __LINE__, NULL)

/* Integer Not Equal To (of all sizes) */
#define TEST_ASSERT_NOT_EQUAL_INT(threshold, actual)    UNITY_TEST_ASSERT_NOT_EQUAL_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_NOT_EQUAL_UINT(threshold, actual)   UNITY_TEST_ASSERT_NOT_EQUAL_UINT((threshold), (actual), __LINE__, NULL)

/* Integer Greater Than / Less Than */
#define TEST_ASSERT_GREATER_THAN(threshold, actual)         UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_GREATER_THAN_INT(threshold, actual)     UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_LESS_THAN(threshold, actual)            UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_LESS_THAN_INT(threshold, actual)        UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((threshold), (actual), __LINE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((threshold), (actual), __LINE__, NULL)

/* Integer Ranges */
#define TEST_ASSERT_INT_WITHIN(delta, expected, actual)     UNITY_TEST_ASSERT_INT_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_UINT_WITHIN(delta, expected, actual)    UNITY_TEST_ASSERT_UINT_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_HEX_WITHIN(delta, expected, actual)     UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_HEX8_WITHIN(delta, expected, actual)    UNITY_TEST_ASSERT_HEX8_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_HEX16_WITHIN(delta, expected, actual)   UNITY_TEST_ASSERT_HEX16_WITHIN((delta), (expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_HEX32_WITHIN(delta, expected, actual)   UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, NULL)

/* Structs and Strings */
#define TEST_ASSERT_EQUAL_PTR(expected, actual)                 UNITY_TEST_ASSERT_EQUAL_PTR((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_STRING(expected, actual)              UNITY_TEST_ASSERT_EQUAL_STRING((expected), (actual), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len)     UNITY_TEST_ASSERT_EQUAL_STRING_LEN((expected), (actual), (len), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)         UNITY_TEST_ASSERT_EQUAL_MEMORY((expected), (actual), (len), __LINE__, NULL)

/* Arrays */
#define TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)    UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((expected), (actual), (num_elements), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements)  UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY((expected), (actual), (num_elements), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, num_elements) UNITY_TEST_ASSERT_EQUAL_UINT16_ARRAY((expected), (actual), (num_elements), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, actual, num_elements) UNITY_TEST_ASSERT_EQUAL_UINT32_ARRAY((expected), (actual), (num_elements), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, num_elements)   UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY((expected), (actual), (num_elements), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, actual, num_elements)  UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY((expected), (actual), (num_elements), __LINE__, NULL)
#define TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num)     UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY((expected), (actual), (len), (num), __LINE__, NULL)

/*-------------------------------------------------------
 * Test Asserts (with additional messages)
 *-------------------------------------------------------*/

/* Boolean with message */
#define TEST_ASSERT_MESSAGE(condition, message)              UNITY_TEST_ASSERT(       (condition), __LINE__, (message))
#define TEST_ASSERT_TRUE_MESSAGE(condition, message)         UNITY_TEST_ASSERT(       (condition), __LINE__, (message))
#define TEST_ASSERT_FALSE_MESSAGE(condition, message)        UNITY_TEST_ASSERT(      !(condition), __LINE__, (message))
#define TEST_ASSERT_NULL_MESSAGE(pointer, message)           UNITY_TEST_ASSERT_NULL(    (pointer), __LINE__, (message))
#define TEST_ASSERT_NOT_NULL_MESSAGE(pointer, message)       UNITY_TEST_ASSERT_NOT_NULL((pointer), __LINE__, (message))

/* Integers with message */
#define TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, message)     UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_UINT_MESSAGE(expected, actual, message)    UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected, actual, message)   UNITY_TEST_ASSERT_EQUAL_UINT8((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected, actual, message)  UNITY_TEST_ASSERT_EQUAL_UINT16((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, actual, message)  UNITY_TEST_ASSERT_EQUAL_UINT32((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_HEX_MESSAGE(expected, actual, message)     UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected, actual, message)    UNITY_TEST_ASSERT_EQUAL_HEX8((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_HEX16_MESSAGE(expected, actual, message)   UNITY_TEST_ASSERT_EQUAL_HEX16((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_HEX32_MESSAGE(expected, actual, message)   UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, (message))

/* Strings / Memory with message */
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message)   UNITY_TEST_ASSERT_EQUAL_STRING((expected), (actual), __LINE__, (message))
#define TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expected, actual, len, message) UNITY_TEST_ASSERT_EQUAL_MEMORY((expected), (actual), (len), __LINE__, (message))
#define TEST_ASSERT_EQUAL_PTR_MESSAGE(expected, actual, message)      UNITY_TEST_ASSERT_EQUAL_PTR((expected), (actual), __LINE__, (message))

/* end of UNITY_FRAMEWORK_H */
#ifdef __cplusplus
}
#endif
#endif
