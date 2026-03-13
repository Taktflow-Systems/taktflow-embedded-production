/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-24 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
=========================================================================

    TODO:HARDWARE — Replace this minimal stub with the full Unity v2.6.x source
    from https://github.com/ThrowTheSwitch/Unity once network access is available.
    Download unity.h, unity.c, unity_internals.h from the src/ directory.
    This stub provides enough functionality to compile and run BSW unit tests
    on x86 host. The full Unity adds: 64-bit support, float/double assertions,
    array comparisons, color output, command-line filtering, and more.
========================================================================= */

#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

/*-------------------------------------------------------
 * Basic Types
 *-------------------------------------------------------*/
#ifndef UNITY_INT
    typedef int UNITY_INT;
#endif
#ifndef UNITY_UINT
    typedef unsigned int UNITY_UINT;
#endif
typedef UNITY_INT  _UNITY_SINT;
typedef UNITY_UINT _UNITY_UINT;
typedef unsigned short UNITY_LINE_TYPE;
typedef unsigned short UNITY_COUNTER_TYPE;

/*-------------------------------------------------------
 * Internal Storage
 *-------------------------------------------------------*/
typedef struct _Unity {
    const char* TestFile;
    const char* CurrentTestName;
    UNITY_LINE_TYPE CurrentTestLineNumber;
    UNITY_COUNTER_TYPE NumberOfTests;
    UNITY_COUNTER_TYPE TestFailures;
    UNITY_COUNTER_TYPE TestIgnores;
    UNITY_COUNTER_TYPE CurrentTestFailed;
    UNITY_COUNTER_TYPE CurrentTestIgnored;
    jmp_buf AbortFrame;
} _Unity_t;

extern _Unity_t Unity;

/*-------------------------------------------------------
 * Error strings
 *-------------------------------------------------------*/
extern const char UnityStrErrShorthand[];

/*-------------------------------------------------------
 * Core Functions
 *-------------------------------------------------------*/
void UnityBegin(const char* filename);
int  UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(void (*func)(void), const char* name, int line_num);
void UnityMessage(const char* msg, UNITY_LINE_TYPE line);

/* Fail / Ignore / Abort */
void UnityFail(const char* msg, const UNITY_LINE_TYPE line);
void UnityIgnore(const char* msg, const UNITY_LINE_TYPE line);

/* Assertion backends */
void UnityAssertEqualNumber(const UNITY_INT expected, const UNITY_INT actual,
                            const char* msg, const UNITY_LINE_TYPE line,
                            const int style);
void UnityAssertBits(const UNITY_INT mask, const UNITY_INT expected,
                     const UNITY_INT actual, const char* msg,
                     const UNITY_LINE_TYPE line);
void UnityAssertNull(const void* pointer, const char* msg,
                     const UNITY_LINE_TYPE line);
void UnityAssertNotNull(const void* pointer, const char* msg,
                        const UNITY_LINE_TYPE line);
void UnityAssertEqualString(const char* expected, const char* actual,
                            const char* msg, const UNITY_LINE_TYPE line);
void UnityAssertEqualMemory(const void* expected, const void* actual,
                            const UNITY_UINT length, const char* msg,
                            const UNITY_LINE_TYPE line);

/*-------------------------------------------------------
 * Display Style (for number printing)
 *-------------------------------------------------------*/
#define UNITY_DISPLAY_STYLE_INT     0
#define UNITY_DISPLAY_STYLE_UINT    1
#define UNITY_DISPLAY_STYLE_HEX8    2
#define UNITY_DISPLAY_STYLE_HEX16   3
#define UNITY_DISPLAY_STYLE_HEX32   4
#define UNITY_DISPLAY_STYLE_CHAR    5

/*-------------------------------------------------------
 * Test Execution Macros
 *-------------------------------------------------------*/
#define UNITY_TEST_FAIL(line, message)              UnityFail((message), (UNITY_LINE_TYPE)(line))
#define UNITY_TEST_IGNORE(line, message)            UnityIgnore((message), (UNITY_LINE_TYPE)(line))

#define TEST_ABORT()                                do { longjmp(Unity.AbortFrame, 1); } while (0)
#define TEST_PROTECT()                              (setjmp(Unity.AbortFrame) == 0)

#define UNITY_BEGIN()                               UnityBegin(__FILE__)
#define UNITY_END()                                 UnityEnd()

#define RUN_TEST(func)                              UnityDefaultTestRun(func, #func, __LINE__)

/*-------------------------------------------------------
 * Assertion Macros — Boolean
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT(condition, line, message) \
    do { if (!(condition)) { UnityFail((message), (UNITY_LINE_TYPE)(line)); } } while (0)

#define UNITY_TEST_ASSERT_NULL(pointer, line, message) \
    UnityAssertNull((const void*)(pointer), (message), (UNITY_LINE_TYPE)(line))

#define UNITY_TEST_ASSERT_NOT_NULL(pointer, line, message) \
    UnityAssertNotNull((const void*)(pointer), (message), (UNITY_LINE_TYPE)(line))

#define UNITY_TEST_ASSERT_EMPTY(pointer, line, message) \
    do { UNITY_TEST_ASSERT_NOT_NULL((pointer), (line), (message)); \
         UNITY_TEST_ASSERT(((const char*)(pointer))[0] == '\0', (line), (message)); } while (0)

#define UNITY_TEST_ASSERT_NOT_EMPTY(pointer, line, message) \
    do { UNITY_TEST_ASSERT_NOT_NULL((pointer), (line), (message)); \
         UNITY_TEST_ASSERT(((const char*)(pointer))[0] != '\0', (line), (message)); } while (0)

/*-------------------------------------------------------
 * Assertion Macros — Integer Equal
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT_EQUAL_INT(expected, actual, line, message) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_INT)

#define UNITY_TEST_ASSERT_EQUAL_INT8(expected, actual, line, message)   UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_INT16(expected, actual, line, message)  UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_INT32(expected, actual, line, message)  UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_INT64(expected, actual, line, message)  UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), (line), (message))

#define UNITY_TEST_ASSERT_EQUAL_UINT(expected, actual, line, message) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_UINT)

#define UNITY_TEST_ASSERT_EQUAL_UINT8(expected, actual, line, message)  UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_UINT16(expected, actual, line, message) UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_UINT32(expected, actual, line, message) UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_EQUAL_UINT64(expected, actual, line, message) UNITY_TEST_ASSERT_EQUAL_UINT((expected), (actual), (line), (message))

#define UNITY_TEST_ASSERT_EQUAL_HEX8(expected, actual, line, message) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_EQUAL_HEX16(expected, actual, line, message) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_EQUAL_HEX32(expected, actual, line, message) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_HEX32)
#define UNITY_TEST_ASSERT_EQUAL_HEX64(expected, actual, line, message) \
    UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), (line), (message))

#define UNITY_TEST_ASSERT_EQUAL_CHAR(expected, actual, line, message) \
    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), UNITY_DISPLAY_STYLE_CHAR)

#define UNITY_TEST_ASSERT_BITS(mask, expected, actual, line, message) \
    UnityAssertBits((UNITY_INT)(mask), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line))

/*-------------------------------------------------------
 * Assertion Macros — Integer Not Equal
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT_NOT_EQUAL_INT(threshold, actual, line, message) \
    UNITY_TEST_ASSERT(((UNITY_INT)(threshold) != (UNITY_INT)(actual)), (line), (message) ? (message) : " Expected Not-Equal")

#define UNITY_TEST_ASSERT_NOT_EQUAL_INT8(t, a, l, m)   UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_INT16(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_INT32(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_INT64(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_UINT(t, a, l, m)   UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_UINT8(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_UINT16(t, a, l, m) UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_UINT32(t, a, l, m) UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_UINT64(t, a, l, m) UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_HEX8(t, a, l, m)   UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_HEX16(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_HEX32(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_HEX64(t, a, l, m)  UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))
#define UNITY_TEST_ASSERT_NOT_EQUAL_CHAR(t, a, l, m)   UNITY_TEST_ASSERT_NOT_EQUAL_INT((t), (a), (l), (m))

/*-------------------------------------------------------
 * Assertion Macros — Greater / Less / Within
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT_GREATER_THAN_INT(threshold, actual, line, message) \
    UNITY_TEST_ASSERT(((UNITY_INT)(actual) > (UNITY_INT)(threshold)), (line), (message) ? (message) : " Expected Greater Than")

#define UNITY_TEST_ASSERT_GREATER_THAN_INT8(t,a,l,m)   UNITY_TEST_ASSERT_GREATER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_INT16(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_INT32(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_INT64(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT(t,a,l,m)   UNITY_TEST_ASSERT(((UNITY_UINT)(actual) > (UNITY_UINT)(threshold)), (l), (m) ? (m) : " Expected Greater Than")
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT8(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT16(t,a,l,m) UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT32(t,a,l,m) UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT64(t,a,l,m) UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX8(t,a,l,m)   UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX16(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX32(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX64(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_THAN_CHAR(t,a,l,m)   UNITY_TEST_ASSERT_GREATER_THAN_INT((t),(a),(l),(m))

#define UNITY_TEST_ASSERT_SMALLER_THAN_INT(threshold, actual, line, message) \
    UNITY_TEST_ASSERT(((UNITY_INT)(actual) < (UNITY_INT)(threshold)), (line), (message) ? (message) : " Expected Less Than")

#define UNITY_TEST_ASSERT_SMALLER_THAN_INT8(t,a,l,m)   UNITY_TEST_ASSERT_SMALLER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT16(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT32(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT64(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT(t,a,l,m)   UNITY_TEST_ASSERT(((UNITY_UINT)(actual) < (UNITY_UINT)(threshold)), (l), (m) ? (m) : " Expected Less Than")
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT8(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT16(t,a,l,m) UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT32(t,a,l,m) UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT64(t,a,l,m) UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX8(t,a,l,m)   UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX16(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX32(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX64(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_THAN_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_THAN_CHAR(t,a,l,m)   UNITY_TEST_ASSERT_SMALLER_THAN_INT((t),(a),(l),(m))

#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT(threshold, actual, line, message) \
    UNITY_TEST_ASSERT(((UNITY_INT)(actual) >= (UNITY_INT)(threshold)), (line), (message) ? (message) : " Expected Greater Or Equal")

#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT8(t,a,l,m)   UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT16(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT32(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT64(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT(t,a,l,m)   UNITY_TEST_ASSERT(((UNITY_UINT)(actual) >= (UNITY_UINT)(threshold)), (l), (m) ? (m) : " Expected Greater Or Equal")
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT8(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT16(t,a,l,m) UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT32(t,a,l,m) UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT64(t,a,l,m) UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX8(t,a,l,m)   UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX16(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX32(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX64(t,a,l,m)  UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_CHAR(t,a,l,m)   UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((t),(a),(l),(m))

#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT(threshold, actual, line, message) \
    UNITY_TEST_ASSERT(((UNITY_INT)(actual) <= (UNITY_INT)(threshold)), (line), (message) ? (message) : " Expected Less Or Equal")

#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT8(t,a,l,m)   UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT16(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT32(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT64(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT(t,a,l,m)   UNITY_TEST_ASSERT(((UNITY_UINT)(actual) <= (UNITY_UINT)(threshold)), (l), (m) ? (m) : " Expected Less Or Equal")
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT8(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT16(t,a,l,m) UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT32(t,a,l,m) UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT64(t,a,l,m) UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX8(t,a,l,m)   UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX16(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX32(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX64(t,a,l,m)  UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((t),(a),(l),(m))
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_CHAR(t,a,l,m)   UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((t),(a),(l),(m))

/*-------------------------------------------------------
 * Assertion Macros — Within (delta range)
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT_INT_WITHIN(delta, expected, actual, line, message) \
    UNITY_TEST_ASSERT((((UNITY_INT)(actual) - (UNITY_INT)(expected)) <= (UNITY_INT)(delta)) && \
                      (((UNITY_INT)(expected) - (UNITY_INT)(actual)) <= (UNITY_INT)(delta)), (line), \
                      (message) ? (message) : " Expected Within Delta")

#define UNITY_TEST_ASSERT_INT8_WITHIN(d,e,a,l,m)   UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_INT16_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_INT32_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_INT64_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_UINT_WITHIN(d,e,a,l,m)   UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_UINT8_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_UINT16_WITHIN(d,e,a,l,m) UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_UINT32_WITHIN(d,e,a,l,m) UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_UINT64_WITHIN(d,e,a,l,m) UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_HEX8_WITHIN(d,e,a,l,m)   UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_HEX16_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_HEX32_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_HEX64_WITHIN(d,e,a,l,m)  UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))
#define UNITY_TEST_ASSERT_CHAR_WITHIN(d,e,a,l,m)   UNITY_TEST_ASSERT_INT_WITHIN((d),(e),(a),(l),(m))

/*-------------------------------------------------------
 * Assertion Macros — String / Memory / Pointer
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT_EQUAL_STRING(expected, actual, line, message) \
    UnityAssertEqualString((expected), (actual), (message), (UNITY_LINE_TYPE)(line))

#define UNITY_TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len, line, message) \
    do { if (strncmp((expected), (actual), (len)) != 0) UnityFail((message), (UNITY_LINE_TYPE)(line)); } while (0)

#define UNITY_TEST_ASSERT_EQUAL_MEMORY(expected, actual, len, line, message) \
    UnityAssertEqualMemory((expected), (actual), (UNITY_UINT)(len), (message), (UNITY_LINE_TYPE)(line))

#define UNITY_TEST_ASSERT_EQUAL_PTR(expected, actual, line, message) \
    UNITY_TEST_ASSERT(((const void*)(expected) == (const void*)(actual)), (line), (message) ? (message) : " Expected Equal Pointer")

/*-------------------------------------------------------
 * Assertion Macros — Float/Double stubs (disabled by default)
 * Enable with UNITY_INCLUDE_FLOAT / UNITY_INCLUDE_DOUBLE
 *-------------------------------------------------------*/
#ifdef UNITY_INCLUDE_FLOAT
#define UNITY_TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual, line, message) \
    UNITY_TEST_ASSERT((((actual) - (expected)) <= (delta)) && (((expected) - (actual)) <= (delta)), (line), (message) ? (message) : " Float Not Within Delta")
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(expected, actual, line, message) \
    UNITY_TEST_ASSERT_FLOAT_WITHIN(0.00001f, (expected), (actual), (line), (message))
#define UNITY_TEST_ASSERT_FLOAT_IS_INF(actual, line, message) \
    UNITY_TEST_ASSERT(((actual) == (1.0f/0.0f)), (line), (message) ? (message) : " Expected Inf")
#define UNITY_TEST_ASSERT_FLOAT_IS_NAN(actual, line, message) \
    UNITY_TEST_ASSERT(((actual) != (actual)), (line), (message) ? (message) : " Expected NaN")
#endif

#ifdef UNITY_INCLUDE_DOUBLE
#define UNITY_TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual, line, message) \
    UNITY_TEST_ASSERT((((actual) - (expected)) <= (delta)) && (((expected) - (actual)) <= (delta)), (line), (message) ? (message) : " Double Not Within Delta")
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE(expected, actual, line, message) \
    UNITY_TEST_ASSERT_DOUBLE_WITHIN(0.00001, (expected), (actual), (line), (message))
#endif

/*-------------------------------------------------------
 * Array assertion stubs — enough for BSW testing
 *-------------------------------------------------------*/
#define UNITY_TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements, line, message) \
    do { unsigned int _i; for (_i = 0; _i < (unsigned int)(num_elements); _i++) { \
        if ((expected)[_i] != (actual)[_i]) { UnityFail((message), (UNITY_LINE_TYPE)(line)); break; } \
    } } while (0)

#define UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY(e,a,n,l,m)  UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_INT16_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_INT32_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY(e,a,n,l,m)  UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_UINT16_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_UINT32_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY(e,a,n,l,m)  UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_HEX16_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY(e,a,n,l,m) UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((e),(a),(n),(l),(m))
#define UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY(e,a,len,n,l,m) UNITY_TEST_ASSERT_EQUAL_MEMORY((e),(a),((len)*(n)),(l),(m))

#endif /* UNITY_INTERNALS_H */
