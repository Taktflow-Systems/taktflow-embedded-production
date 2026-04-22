// Copyright 2026 Taktflow.

/**
 * \file
 * \ingroup rtScheduling
 *
 * JSON export of a schedule manifest and its feasibility analysis. The
 * output is consumed by the single-page HTML schedule-audit tool (see
 * `gui/schedule-audit.html`) to render the math visibly, and by any
 * offline analyser a project wants to plug in (Python, spreadsheet, CI
 * gate).
 *
 * The format is stable and versioned via the top-level `schema` field.
 * Every math output is precomputed here so the viewer does not redo the
 * feasibility analysis — the viewer shows the numbers, not recomputes them.
 *
 * Output shape (v1):
 *
 * \code{.json}
 * {
 *   "schema": "rt-sched/1",
 *   "verdict": "ok",
 *   "hyperperiod_us": 100000,
 *   "entities": [
 *     {
 *       "name": "safety_tick", "context": 1,
 *       "cycle_us": 5000, "wcet_us": 700,
 *       "deadline_us": 5000, "phase_us": 0,
 *       "criticality": "asil_d", "overrun": "safety", "priority_hint": 0,
 *       "stats": {
 *         "invocations": 0, "overruns": 0,
 *         "last_exec_us": 0, "max_observed_us": 0, "min_observed_us": 0
 *       }
 *     }
 *   ],
 *   "contexts": [
 *     {
 *       "id": 1, "n_tasks": 1,
 *       "utilization_ppm": 140000,
 *       "ll_bound_ppm": 1000000,
 *       "ll_pass": true,
 *       "hyperbolic_product_ppm": 1140000,
 *       "hyperbolic_pass": true,
 *       "mixed_criticality": false,
 *       "criticality_util_ppm": {
 *         "qm": 0, "asil_a": 0, "asil_b": 0, "asil_c": 0, "asil_d": 140000
 *       }
 *     }
 *   ]
 * }
 * \endcode
 *
 * All percentages are parts-per-million integers (0..≈2'000'000) so no
 * floating-point appears on the wire; the GUI converts to fractions for
 * display.
 */
#pragma once

#include "rtScheduling/Feasibility.h"
#include "rtScheduling/Registry.h"
#include "rtScheduling/Types.h"

#include <cstdint>
#include <string>

namespace rtSched
{
namespace json
{

/// Schema tag. Bump when a breaking change lands.
inline constexpr char const* SCHEMA_TAG = "rt-sched/1";

/// Write the full report for an arbitrary manifest into `out` (appended,
/// not cleared). Per-entity stats are optional; if `stats` is null, all
/// stats blocks are emitted with zeros.
///
/// The function is allocation-only-through-std::string; no heap in the
/// feasibility math itself. Suitable for a posix or WSL host dump. For
/// embedded use, see `write_report_chunked` below.
void write_report(
    DeclView                 decls,
    EntityStats const* const stats, // parallel to decls; nullptr = all zero
    std::string&             out);

/// Callback-driven variant for embedded targets where building a large
/// std::string is undesirable. Calls `emit` with chunks of the JSON; the
/// callback may forward to UART, a ring buffer, or a file writer.
using EmitFn = void (*)(char const* data, std::size_t size, void* user);

void write_report_chunked(
    DeclView                 decls,
    EntityStats const* const stats,
    EmitFn                   emit,
    void*                    user);

/// Pull the manifest out of the Registry singleton and emit the report.
/// Uses each binding's own stats pointer (nullptr → zero stats for that
/// entity). The typical firmware path: console command calls this into a
/// UART writer.
void write_report_from_registry(std::string& out);

void write_report_from_registry_chunked(EmitFn emit, void* user);

// ---- Stringification helpers (exposed for tests and for custom consumers)

char const* criticality_str(Criticality c) noexcept;
char const* overrun_str(OverrunPolicy p) noexcept;
char const* verdict_str(Verdict v) noexcept;

} // namespace json
} // namespace rtSched
