/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-24 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
=========================================================================

    TODO:HARDWARE — Replace this minimal implementation with the full Unity
    v2.6.x source from https://github.com/ThrowTheSwitch/Unity
    This stub provides: pass/fail/ignore, integer/string/memory assertions,
    and test runner. Missing: float/double, array-within-delta, color output,
    command-line filtering, printf support, detailed diff output.
========================================================================= */

#include "unity.h"
#include <stdio.h>
#include <string.h>

/*-------------------------------------------------------
 * Global State
 *-------------------------------------------------------*/
_Unity_t Unity;

const char UnityStrErrShorthand[] = "Unity Shorthand Support Disabled";

/*-------------------------------------------------------
 * Output Helpers
 *-------------------------------------------------------*/
static void UnityPrintChar(char c)
{
    putchar(c);
}

static void UnityPrint(const char* string)
{
    if (string != NULL)
    {
        while (*string)
        {
            UnityPrintChar(*string);
            string++;
        }
    }
}

static void UnityPrintNumber(UNITY_INT number)
{
    char buffer[32];
    int i = 0;
    int negative = 0;
    unsigned int n;

    if (number < 0)
    {
        negative = 1;
        n = (unsigned int)(-(number + 1)) + 1U;
    }
    else
    {
        n = (unsigned int)number;
    }

    do
    {
        buffer[i++] = (char)('0' + (int)(n % 10U));
        n /= 10U;
    } while (n > 0U);

    if (negative)
    {
        UnityPrintChar('-');
    }

    while (i > 0)
    {
        UnityPrintChar(buffer[--i]);
    }
}

static void UnityPrintNumberUnsigned(UNITY_UINT number)
{
    char buffer[32];
    int i = 0;

    do
    {
        buffer[i++] = (char)('0' + (int)(number % 10U));
        number /= 10U;
    } while (number > 0U);

    while (i > 0)
    {
        UnityPrintChar(buffer[--i]);
    }
}

static void UnityPrintNumberHex(UNITY_UINT number, int nibbles)
{
    const char hex[] = "0123456789ABCDEF";
    int i;

    UnityPrint("0x");
    for (i = nibbles - 1; i >= 0; i--)
    {
        UnityPrintChar(hex[(number >> (4 * (unsigned)i)) & 0x0FU]);
    }
}

/*-------------------------------------------------------
 * Core Functions
 *-------------------------------------------------------*/
void UnityBegin(const char* filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0U;
    Unity.NumberOfTests = 0U;
    Unity.TestFailures = 0U;
    Unity.TestIgnores = 0U;
    Unity.CurrentTestFailed = 0U;
    Unity.CurrentTestIgnored = 0U;
}

int UnityEnd(void)
{
    UnityPrint("\n-----------------------\n");
    UnityPrintNumberUnsigned(Unity.NumberOfTests);
    UnityPrint(" Tests ");
    UnityPrintNumberUnsigned(Unity.TestFailures);
    UnityPrint(" Failures ");
    UnityPrintNumberUnsigned(Unity.TestIgnores);
    UnityPrint(" Ignored\n");

    if (Unity.TestFailures == 0U)
    {
        UnityPrint("OK\n");
    }
    else
    {
        UnityPrint("FAIL\n");
    }

    return (int)Unity.TestFailures;
}

void UnityConcludeTest(void)
{
    if (Unity.CurrentTestIgnored)
    {
        Unity.TestIgnores++;
    }
    else if (Unity.CurrentTestFailed)
    {
        Unity.TestFailures++;
    }

    Unity.CurrentTestFailed = 0U;
    Unity.CurrentTestIgnored = 0U;
}

void UnityDefaultTestRun(void (*func)(void), const char* name, int line_num)
{
    Unity.CurrentTestName = name;
    Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE)line_num;
    Unity.NumberOfTests++;

    if (TEST_PROTECT())
    {
        setUp();
        func();
    }

    if (TEST_PROTECT())
    {
        tearDown();
    }

    UnityConcludeTest();
}

void UnityMessage(const char* msg, UNITY_LINE_TYPE line)
{
    UnityPrint(Unity.TestFile);
    UnityPrintChar(':');
    UnityPrintNumberUnsigned((UNITY_UINT)line);
    UnityPrint(":INFO: ");
    UnityPrint(msg);
    UnityPrintChar('\n');
}

/*-------------------------------------------------------
 * Fail / Ignore
 *-------------------------------------------------------*/
void UnityFail(const char* msg, const UNITY_LINE_TYPE line)
{
    UnityPrint(Unity.TestFile);
    UnityPrintChar(':');
    UnityPrintNumberUnsigned((UNITY_UINT)line);
    UnityPrintChar(':');
    UnityPrint(Unity.CurrentTestName);
    UnityPrint(":FAIL");

    if (msg != NULL)
    {
        UnityPrintChar(':');
        UnityPrint(msg);
    }

    UnityPrintChar('\n');
    Unity.CurrentTestFailed = 1U;
    TEST_ABORT();
}

void UnityIgnore(const char* msg, const UNITY_LINE_TYPE line)
{
    UnityPrint(Unity.TestFile);
    UnityPrintChar(':');
    UnityPrintNumberUnsigned((UNITY_UINT)line);
    UnityPrintChar(':');
    UnityPrint(Unity.CurrentTestName);
    UnityPrint(":IGNORE");

    if (msg != NULL)
    {
        UnityPrintChar(':');
        UnityPrint(msg);
    }

    UnityPrintChar('\n');
    Unity.CurrentTestIgnored = 1U;
    TEST_ABORT();
}

/*-------------------------------------------------------
 * Assertion Backends
 *-------------------------------------------------------*/
void UnityAssertEqualNumber(const UNITY_INT expected, const UNITY_INT actual,
                            const char* msg, const UNITY_LINE_TYPE line,
                            const int style)
{
    if (expected != actual)
    {
        UnityPrint(Unity.TestFile);
        UnityPrintChar(':');
        UnityPrintNumberUnsigned((UNITY_UINT)line);
        UnityPrintChar(':');
        UnityPrint(Unity.CurrentTestName);
        UnityPrint(":FAIL: Expected ");

        switch (style)
        {
            case UNITY_DISPLAY_STYLE_HEX8:
                UnityPrintNumberHex((UNITY_UINT)expected, 2);
                UnityPrint(" Was ");
                UnityPrintNumberHex((UNITY_UINT)actual, 2);
                break;
            case UNITY_DISPLAY_STYLE_HEX16:
                UnityPrintNumberHex((UNITY_UINT)expected, 4);
                UnityPrint(" Was ");
                UnityPrintNumberHex((UNITY_UINT)actual, 4);
                break;
            case UNITY_DISPLAY_STYLE_HEX32:
                UnityPrintNumberHex((UNITY_UINT)expected, 8);
                UnityPrint(" Was ");
                UnityPrintNumberHex((UNITY_UINT)actual, 8);
                break;
            case UNITY_DISPLAY_STYLE_UINT:
                UnityPrintNumberUnsigned((UNITY_UINT)expected);
                UnityPrint(" Was ");
                UnityPrintNumberUnsigned((UNITY_UINT)actual);
                break;
            case UNITY_DISPLAY_STYLE_CHAR:
                UnityPrintChar('\'');
                UnityPrintChar((char)expected);
                UnityPrint("' Was '");
                UnityPrintChar((char)actual);
                UnityPrintChar('\'');
                break;
            default: /* UNITY_DISPLAY_STYLE_INT */
                UnityPrintNumber(expected);
                UnityPrint(" Was ");
                UnityPrintNumber(actual);
                break;
        }

        if (msg != NULL)
        {
            UnityPrintChar(' ');
            UnityPrint(msg);
        }

        UnityPrintChar('\n');
        Unity.CurrentTestFailed = 1U;
        TEST_ABORT();
    }
}

void UnityAssertBits(const UNITY_INT mask, const UNITY_INT expected,
                     const UNITY_INT actual, const char* msg,
                     const UNITY_LINE_TYPE line)
{
    if ((mask & expected) != (mask & actual))
    {
        UnityFail(msg, line);
    }
}

void UnityAssertNull(const void* pointer, const char* msg,
                     const UNITY_LINE_TYPE line)
{
    if (pointer != NULL)
    {
        UnityFail(msg, line);
    }
}

void UnityAssertNotNull(const void* pointer, const char* msg,
                        const UNITY_LINE_TYPE line)
{
    if (pointer == NULL)
    {
        UnityFail(msg, line);
    }
}

void UnityAssertEqualString(const char* expected, const char* actual,
                            const char* msg, const UNITY_LINE_TYPE line)
{
    if (expected == NULL && actual == NULL)
    {
        return;
    }

    if (expected == NULL || actual == NULL)
    {
        UnityFail(msg, line);
        return;
    }

    if (strcmp(expected, actual) != 0)
    {
        UnityPrint(Unity.TestFile);
        UnityPrintChar(':');
        UnityPrintNumberUnsigned((UNITY_UINT)line);
        UnityPrintChar(':');
        UnityPrint(Unity.CurrentTestName);
        UnityPrint(":FAIL: Expected '");
        UnityPrint(expected);
        UnityPrint("' Was '");
        UnityPrint(actual);
        UnityPrintChar('\'');

        if (msg != NULL)
        {
            UnityPrintChar(' ');
            UnityPrint(msg);
        }

        UnityPrintChar('\n');
        Unity.CurrentTestFailed = 1U;
        TEST_ABORT();
    }
}

void UnityAssertEqualMemory(const void* expected, const void* actual,
                            const UNITY_UINT length, const char* msg,
                            const UNITY_LINE_TYPE line)
{
    if (expected == NULL && actual == NULL)
    {
        return;
    }

    if (expected == NULL || actual == NULL)
    {
        UnityFail(msg, line);
        return;
    }

    if (memcmp(expected, actual, (size_t)length) != 0)
    {
        UnityFail(msg, line);
    }
}
