/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * tests/interop/producer.c — thin C bridge used by the interop test.
 *
 * Modes
 * -----
 *
 *   producer --dump-frames <csv-path>
 *     Reads wire_records.csv-shaped test vectors from <csv-path> and
 *     writes one framed postcard WireFaultRecord per row to stdout,
 *     with a line per row of the form:
 *
 *       ROW <index> <hex_bytes>
 *
 *     where <hex_bytes> is the full frame (4-byte LE length prefix +
 *     postcard body) lowercased. The Python interop test compares
 *     these lines against the matching rows emitted by the Rust
 *     golden dumper.
 *
 *   producer --send <socket-path> <csv-path>
 *     Connects to <socket-path>, reads the CSV, and calls
 *     fault_lib_init / fault_lib_report / fault_lib_shutdown for each
 *     row. Used by the optional live Rust consumer variant of the
 *     interop gate.
 *
 * This file has NO hardcoded fault codes. Every value comes from the
 * CSV passed on the command line.
 */

#include "fault_lib.h"
#include "wire_fault_record.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE  4096U
#define MAX_FIELD  512U

typedef struct row_s {
    char     component[FAULT_LIB_MAX_COMPONENT_LEN + 1U];
    size_t   component_len;
    uint32_t id;
    uint8_t  severity_code;
    uint64_t timestamp_ms;
    char     meta[FAULT_LIB_MAX_META_JSON_LEN + 1U];
    size_t   meta_len;
    bool     meta_present;
} row_t;

static bool parse_row(char *line, row_t *row)
{
    char *fields[8];
    size_t count = 0U;
    char *p = line;
    size_t line_len;

    line_len = strlen(line);
    while ((line_len > 0U) &&
           ((line[line_len - 1U] == '\n') || (line[line_len - 1U] == '\r'))) {
        line[line_len - 1U] = '\0';
        line_len--;
    }
    if ((line_len == 0U) || (line[0] == '#')) {
        return false;
    }
    fields[count++] = p;
    while ((*p != '\0') && (count < 8U)) {
        if (*p == ',') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    if (count != 5U) {
        return false;
    }
    if (strcmp(fields[0], "component") == 0) {
        return false;
    }

    memset(row, 0, sizeof(*row));
    row->component_len = strlen(fields[0]);
    if ((row->component_len == 0U) ||
        (row->component_len > FAULT_LIB_MAX_COMPONENT_LEN)) {
        return false;
    }
    memcpy(row->component, fields[0], row->component_len);
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
        memcpy(row->meta, fields[4], row->meta_len);
        row->meta_present = true;
    }
    return true;
}

static int row_to_record(const row_t *row, fault_lib_record_t *r)
{
    memset(r, 0, sizeof(*r));
    r->component = row->component;
    r->component_len = row->component_len;
    r->id = row->id;
    r->severity = (fault_lib_severity_t)row->severity_code;
    r->timestamp_ms = row->timestamp_ms;
    if (row->meta_present) {
        r->meta.json = row->meta;
        r->meta.len = row->meta_len;
    } else {
        r->meta.json = NULL;
        r->meta.len = 0U;
    }
    return 0;
}

static int cmd_dump_frames(const char *csv_path)
{
    FILE  *fp;
    char   line[MAX_LINE];
    int    idx = 0;

    fp = fopen(csv_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "producer: cannot open %s\n", csv_path);
        return 2;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        row_t row;
        fault_lib_record_t r;
        uint8_t frame[FAULT_LIB_MAX_FRAME_LEN + 32U];
        size_t  frame_len = 0U;
        int     rc;
        size_t  i;

        if (!parse_row(line, &row)) {
            continue;
        }
        (void)row_to_record(&row, &r);
        rc = wire_fault_record_encode_frame(frame, sizeof(frame), &r, &frame_len);
        if (rc != WIRE_FAULT_RECORD_OK) {
            fprintf(stderr, "producer: encode failed rc=%d\n", rc);
            (void)fclose(fp);
            return 3;
        }
        printf("ROW %d ", idx);
        for (i = 0U; i < frame_len; ++i) {
            printf("%02x", frame[i]);
        }
        printf("\n");
        idx++;
    }
    (void)fclose(fp);
    return 0;
}

static int cmd_send(const char *socket_path, const char *csv_path)
{
    FILE *fp;
    char  line[MAX_LINE];
    int   rc;

    rc = fault_lib_init(socket_path);
    if (rc != FAULT_LIB_OK) {
        fprintf(stderr, "producer: init rc=%d\n", rc);
        return 4;
    }
    fp = fopen(csv_path, "r");
    if (fp == NULL) {
        (void)fault_lib_shutdown();
        fprintf(stderr, "producer: cannot open %s\n", csv_path);
        return 2;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        row_t row;
        fault_lib_record_t r;
        if (!parse_row(line, &row)) {
            continue;
        }
        (void)row_to_record(&row, &r);
        rc = fault_lib_report(&r);
        if (rc != FAULT_LIB_OK) {
            fprintf(stderr, "producer: report rc=%d\n", rc);
            (void)fclose(fp);
            (void)fault_lib_shutdown();
            return 5;
        }
    }
    (void)fclose(fp);
    (void)fault_lib_shutdown();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage:\n"
                        "  %s --dump-frames <csv>\n"
                        "  %s --send <socket-path> <csv>\n",
                argv[0], argv[0]);
        return 1;
    }
    if ((strcmp(argv[1], "--dump-frames") == 0) && (argc == 3)) {
        return cmd_dump_frames(argv[2]);
    }
    if ((strcmp(argv[1], "--send") == 0) && (argc == 4)) {
        return cmd_send(argv[2], argv[3]);
    }
    fprintf(stderr, "producer: unrecognised arguments\n");
    return 1;
}
