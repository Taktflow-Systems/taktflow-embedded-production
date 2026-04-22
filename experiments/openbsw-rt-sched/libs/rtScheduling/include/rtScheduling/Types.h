// Copyright 2026 Taktflow.

/**
 * \file
 * \ingroup rtScheduling
 *
 * Data types for declaring schedulable entities at design time.
 *
 * This header has no dependency on openbsw's async module — deliberately.
 * The feasibility math and the registry can be unit-tested standalone, and
 * only the async-bridge header pulls in the openbsw public API.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rtSched
{

using ContextId = std::uint8_t;  // mirrors async::ContextType without the coupling
using Duration  = std::uint32_t; // microseconds unless otherwise noted

/// Criticality classification per ISO 26262. Used for priority hints
/// and for failure-reaction policy when a WCET overrun is detected.
enum class Criticality : std::uint8_t
{
    QM,
    ASIL_A,
    ASIL_B,
    ASIL_C,
    ASIL_D,
};

/// When WCET is exceeded, what should happen.
/// Declared per entity so critical tasks can escalate while QM tasks only log.
enum class OverrunPolicy : std::uint8_t
{
    Ignore,       ///< only bump the counter
    LogAndCount,  ///< emit a one-shot log, bump the counter
    Safety,       ///< invoke the system safety handler (transition to safe state)
};

/// Timing parameters of a schedulable entity. All durations are microseconds.
///
/// Invariants (validated by Feasibility checks):
///   - cycle_us > 0
///   - wcet_us > 0
///   - wcet_us <= deadline_us <= cycle_us  (constrained-deadline task model)
///   - phase_us < cycle_us
struct Timing
{
    Duration cycle_us    = 0;  ///< period T
    Duration wcet_us     = 0;  ///< worst-case execution time C
    Duration deadline_us = 0;  ///< relative deadline D (0 means "same as cycle")
    Duration phase_us    = 0;  ///< initial offset from t=0

    constexpr Duration effective_deadline() const noexcept
    {
        return deadline_us == 0 ? cycle_us : deadline_us;
    }
};

/// A pure, compile-time-describable declaration of one periodic activity.
///
/// This is the thing issue #370 wants to make visible: a named entity with
/// known cycle, known WCET, known task assignment, captured in one place.
/// Every field is constexpr-constructible; a system-wide manifest is just a
/// `std::array<EntityDecl, N>` that the compiler and a future auditor can
/// inspect in a single glance.
struct EntityDecl
{
    std::string_view name;
    ContextId        context;
    Timing           timing;
    Criticality      criticality = Criticality::QM;
    OverrunPolicy    overrun     = OverrunPolicy::LogAndCount;

    /// Explicit priority hint. 0 means "derive from rate-monotonic ordering".
    /// Non-zero lets the app override RM when it knows better (e.g. a
    /// criticality-aware DMS assignment).
    std::uint8_t priority_hint = 0;

    /// Worst-case blocking time for this entity (µs). Additive term in the
    /// RTA equation: R_i = C_i + B_i + ∑ ⌈R_i/T_j⌉·C_j. Zero for the
    /// independent-task model (no shared resources). Non-zero when
    /// priority inversion protection (PIP/PCP) bounds the time this entity
    /// can be held up by a lower-priority task that grabbed a shared
    /// resource first. Deriving B_i is a separate static-analysis problem;
    /// carry it here so RTA can use it.
    Duration blocking_us = 0;
};

/// Observed runtime statistics for one entity. Written by the monitored
/// wrapper; read by introspection tooling.
struct EntityStats
{
    std::uint32_t invocations     = 0;
    std::uint32_t overruns        = 0;  ///< times exec_us exceeded wcet_us
    Duration      last_exec_us    = 0;
    Duration      max_observed_us = 0;
    Duration      min_observed_us = 0xFFFFFFFFu;
};

} // namespace rtSched
