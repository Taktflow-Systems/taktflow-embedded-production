/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * test_wire_fault_record.c — round-trip the WireFaultRecord encoder
 * through the local C decoder for every row in
 * libs/fault_lib/testdata/wire_records.csv.
 *
 * This file contains NO hardcoded fault codes, severities, component
 * names, timestamps, or meta payloads. Every value loaded into the
 * encoder comes from the CSV side file.
 *
 * Also covers error-path cases (truncated buffer, oversized meta,
 * zero-length component) that are structural, not value-based.
 */

#include "unity.h"

#include "fault_lib.h"
#include "wire_fault_record.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Path to the CSV test data, computed at build time from the source
 * location of THIS file. The caller cd's to repo root before
 * invoking the test binary; falling back to a relative path keeps
 * the test runnable from either working directory. */
#ifndef FAULT_LIB_TESTDATA_CSV
#define FAULT_LIB_TESTDATA_CSV "libs/fault_lib/testdata/wire_records.csv"
#endif

typedef struct csv_row_s {
    char     component[FAULT_LIB_MAX_COMPONENT_LEN + 1U];
    size_t   component_len;
    uint32_t id;
    uint8_t  severity_code;
    uint64_t timestamp_ms;
    char     meta[FAULT_LIB_MAX_META_JSON_LEN + 1U];
    size_t   meta_len;
    bool     meta_present;
} csv_row_t;

/* Split `line` in place on commas. Populates field pointers into
 * the original string. Returns field count. */
static size_t split_csv_line(char *line, char *fields[], size_t max_fields)
{
    size_t count = 0U;
    char  *p = line;
    fields[count++] = p;
    while (*p != '\0') {
        if (*p == ',') {
            *p = '\0';
            if (count >= max_fields) {
                return count;
            }
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

static bool parse_row(char *line, csv_row_t *row)
{
    char  *fields[8];
    size_t count;
    size_t i;

    /* Strip trailing newline. */
    size_t line_len = strlen(line);
    while ((line_len > 0U) &&
           ((line[line_len - 1U] == '\n') || (line[line_len - 1U] == '\r'))) {
        line[line_len - 1U] = '\0';
        line_len--;
    }
    if (line_len == 0U) {
        return false;
    }
    if (line[0] == '#') {
        return false;
    }

    count = split_csv_line(line, fields, 8U);
    if (count != 5U) {
        return false;
    }
    /* Skip the header row. */
    if (strcmp(fields[0], "component") == 0) {
        return false;
    }

    (void)memset(row, 0, sizeof(*row));
    row->component_len = strlen(fields[0]);
    if ((row->component_len == 0U) ||
        (row->component_len > FAULT_LIB_MAX_COMPONENT_LEN)) {
        return false;
    }
    (void)memcpy(row->component, fields[0], row->component_len);

    row->id = (uint32_t)strtoul(fields[1], NULL, 10);
    row->severity_code = (uint8_t)strtoul(fields[2], NULL, 10);
    row->timestamp_ms = strtoull(fields[3], NULL, 10);

    row->meta_len = strlen(fields[4]);
    if (row->meta_len == 0U) {
        row->meta_present = false;
    } else {
        if (row->meta_len > FAULT_LIB_MAX_META_JSON_LEN) {
            return false;
        }
        (void)memcpy(row->meta, fields[4], row->meta_len);
        row->meta_present = true;
    }
    i = row->component_len;
    (void)i;
    return true;
}

static void roundtrip_row(const csv_row_t *row)
{
    uint8_t frame[FAULT_LIB_MAX_FRAME_LEN + 32U];
    size_t  frame_len = 0U;
    wire_fault_record_decoded_t dec;
    fault_lib_record_t r;
    int rc;

    (void)memset(&r, 0, sizeof(r));
    r.component = row->component;
    r.component_len = row->component_len;
    r.id = row->id;
    r.severity = (fault_lib_severity_t)row->severity_code;
    r.timestamp_ms = row->timestamp_ms;
    if (row->meta_present) {
        r.meta.json = row->meta;
        r.meta.len = row->meta_len;
    } else {
        r.meta.json = NULL;
        r.meta.len = 0U;
    }

    rc = wire_fault_record_encode_frame(frame, sizeof(frame), &r, &frame_len);
    TEST_ASSERT_EQUAL_INT(WIRE_FAULT_RECORD_OK, rc);
    TEST_ASSERT_TRUE(frame_len >= 4U);

    rc = wire_fault_record_decode_frame(frame, frame_len, &dec);
    TEST_ASSERT_EQUAL_INT(WIRE_FAULT_RECORD_OK, rc);

    TEST_ASSERT_EQUAL_UINT(row->component_len, dec.component_len);
    TEST_ASSERT_EQUAL_MEMORY(row->component, dec.component, row->component_len);
    TEST_ASSERT_EQUAL_UINT32(row->id, dec.id);
    TEST_ASSERT_EQUAL_UINT8(row->severity_code, dec.severity_raw);
    /* Unity stub lacks UINT64; compare bytes. */
    TEST_ASSERT_EQUAL_MEMORY(&row->timestamp_ms, &dec.timestamp_ms,
                             sizeof(row->timestamp_ms));
    if (row->meta_present) {
        TEST_ASSERT_TRUE(dec.meta_present);
        TEST_ASSERT_EQUAL_UINT(row->meta_len, dec.meta_json_len);
        TEST_ASSERT_EQUAL_MEMORY(row->meta, dec.meta_json, row->meta_len);
    } else {
        TEST_ASSERT_FALSE(dec.meta_present);
    }
}

static void test_roundtrip_all_csv_rows(void)
{
    FILE *fp;
    char  line[4096];
    int   rows_tested = 0;

    fp = fopen(FAULT_LIB_TESTDATA_CSV, "r");
    if (fp == NULL) {
        /* Try sibling path when test binary is invoked from libs/fault_lib/. */
        fp = fopen("../testdata/wire_records.csv", "r");
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, "wire_records.csv not found");

    while (fgets(line, sizeof(line), fp) != NULL) {
        csv_row_t row;
        if (!parse_row(line, &row)) {
            continue;
        }
        roundtrip_row(&row);
        rows_tested++;
    }
    (void)fclose(fp);

    /* The CSV must contain at least one row or the regression has
     * silently emptied itself. */
    TEST_ASSERT_TRUE_MESSAGE(rows_tested >= 1, "no CSV rows were tested");
}

static void test_truncated_buffer_returns_overflow(void)
{
    fault_lib_record_t r = {
        .component = "tiny",
        .component_len = 4U,
        .id = 1U,
        .severity = FAULT_LIB_SEVERITY_ERROR,
        .timestamp_ms = 0U,
        .meta = { NULL, 0U }
    };
    uint8_t tiny_buf[6];
    size_t frame_len = 0U;
    int rc = wire_fault_record_encode_frame(tiny_buf, sizeof(tiny_buf), &r, &frame_len);
    TEST_ASSERT_EQUAL_INT(WIRE_FAULT_RECORD_OVERFLOW, rc);
}

static void test_zero_length_component_rejected(void)
{
    fault_lib_record_t r = {
        .component = "",
        .component_len = 0U,
        .id = 1U,
        .severity = FAULT_LIB_SEVERITY_ERROR,
        .timestamp_ms = 0U,
        .meta = { NULL, 0U }
    };
    uint8_t buf[128];
    size_t frame_len = 0U;
    int rc = wire_fault_record_encode_frame(buf, sizeof(buf), &r, &frame_len);
    TEST_ASSERT_EQUAL_INT(WIRE_FAULT_RECORD_INVAL, rc);
}

static void test_invalid_severity_rejected(void)
{
    fault_lib_record_t r = {
        .component = "x",
        .component_len = 1U,
        .id = 1U,
        .severity = (fault_lib_severity_t)99,
        .timestamp_ms = 0U,
        .meta = { NULL, 0U }
    };
    uint8_t buf[128];
    size_t frame_len = 0U;
    int rc = wire_fault_record_encode_frame(buf, sizeof(buf), &r, &frame_len);
    TEST_ASSERT_EQUAL_INT(WIRE_FAULT_RECORD_INVAL, rc);
}

static void test_oversized_meta_rejected(void)
{
    static char big[FAULT_LIB_MAX_META_JSON_LEN + 4U];
    fault_lib_record_t r;
    uint8_t buf[FAULT_LIB_MAX_FRAME_LEN + 32U];
    size_t frame_len = 0U;
    int rc;
    (void)memset(big, 'x', sizeof(big));
    (void)memset(&r, 0, sizeof(r));
    r.component = "x";
    r.component_len = 1U;
    r.id = 1U;
    r.severity = FAULT_LIB_SEVERITY_INFO;
    r.timestamp_ms = 0U;
    r.meta.json = big;
    r.meta.len = sizeof(big);
    rc = wire_fault_record_encode_frame(buf, sizeof(buf), &r, &frame_len);
    TEST_ASSERT_EQUAL_INT(WIRE_FAULT_RECORD_INVAL, rc);
}

static void test_version_string_non_null(void)
{
    const char *v = fault_lib_version();
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_TRUE(strlen(v) > 0U);
}

static void test_report_without_init_fails(void)
{
    fault_lib_record_t r = {
        .component = "x",
        .component_len = 1U,
        .id = 1U,
        .severity = FAULT_LIB_SEVERITY_INFO,
        .timestamp_ms = 0U,
        .meta = { NULL, 0U }
    };
    int rc = fault_lib_report(&r);
    TEST_ASSERT_TRUE((rc == FAULT_LIB_ERR_NOT_CONNECTED) ||
                     (rc == FAULT_LIB_ERR_SOCKET));
}

static void test_init_null_path_rejected(void)
{
    int rc = fault_lib_init(NULL);
    TEST_ASSERT_TRUE((rc == FAULT_LIB_ERR_INVAL) ||
                     (rc == FAULT_LIB_ERR_SOCKET));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_all_csv_rows);
    RUN_TEST(test_truncated_buffer_returns_overflow);
    RUN_TEST(test_zero_length_component_rejected);
    RUN_TEST(test_invalid_severity_rejected);
    RUN_TEST(test_oversized_meta_rejected);
    RUN_TEST(test_version_string_non_null);
    RUN_TEST(test_report_without_init_fails);
    RUN_TEST(test_init_null_path_rejected);
    return UNITY_END();
}
