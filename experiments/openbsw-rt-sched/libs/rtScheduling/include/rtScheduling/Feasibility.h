// Copyright 2026 Taktflow.

/**
 * \file
 * \ingroup rtScheduling
 *
 * Feasibility analysis for a schedule manifest. All functions are
 * constexpr so the checks can live inside `static_assert` and catch
 * over-utilized schedules at compile time.
 *
 * What is checked here:
 *   - per-entity sanity (cycle/wcet/deadline invariants)
 *   - per-task CPU utilization
 *   - Liu & Layland rate-monotonic schedulability bound
 *   - hyperbolic bound (tighter than L&L for non-harmonic periods)
 *   - global hyperperiod (lcm of cycles) for bounded analysis
 *
 * Not (yet) checked:
 *   - response-time analysis (Audsley RTA) — planned follow-up
 *   - blocking / resource ceiling — would need a PIP/PCP model
 *   - multi-core allocation — openbsw is single-core in its async model
 */
#pragma once

#include "rtScheduling/Types.h"

#include <array>
#include <cstdint>

namespace rtSched
{

// ---------------------------------------------------------------------------
// Utilities

/// Greatest common divisor, Euclidean. constexpr since C++14.
constexpr Duration gcd_us(Duration a, Duration b) noexcept
{
    while (b != 0)
    {
        Duration const t = b;
        b                = a % b;
        a                = t;
    }
    return a;
}

/// Least common multiple. Wraps on overflow — caller checks hyperperiod_us
/// against a sane cap.
constexpr std::uint64_t lcm_u64(std::uint64_t a, std::uint64_t b) noexcept
{
    if (a == 0 || b == 0)
    {
        return 0;
    }
    return (a / gcd_us(static_cast<Duration>(a), static_cast<Duration>(b))) * b;
}

// ---------------------------------------------------------------------------
// Span-ish view. We stay self-contained, no std::span (C++20) needed.

struct DeclView
{
    EntityDecl const* data;
    std::size_t       size;

    constexpr EntityDecl const& operator[](std::size_t i) const { return data[i]; }
    constexpr std::size_t       length() const { return size; }
};

template<std::size_t N>
constexpr DeclView view(std::array<EntityDecl, N> const& a) noexcept
{
    return {a.data(), N};
}

// ---------------------------------------------------------------------------
// Per-entity sanity

constexpr bool entity_is_sane(EntityDecl const& e) noexcept
{
    if (e.timing.cycle_us == 0) return false;
    if (e.timing.wcet_us  == 0) return false;
    if (e.timing.wcet_us  >  e.timing.effective_deadline()) return false;
    if (e.timing.effective_deadline() > e.timing.cycle_us) return false; // constrained-deadline
    if (e.timing.phase_us >= e.timing.cycle_us) return false;
    return true;
}

constexpr bool all_entities_sane(DeclView v) noexcept
{
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        if (!entity_is_sane(v[i])) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Per-task utilization
//
// Utilization is reported in parts-per-million (ppm) to keep everything
// in integer arithmetic. 1'000'000 ppm = 100% CPU.

constexpr std::uint32_t utilization_ppm(DeclView v, ContextId ctx) noexcept
{
    std::uint64_t u_ppm = 0;
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        EntityDecl const& e = v[i];
        if (e.context != ctx) continue;
        // u_i = C_i / T_i     → ppm = C_i * 1_000_000 / T_i
        u_ppm += (static_cast<std::uint64_t>(e.timing.wcet_us) * 1'000'000ULL)
               / e.timing.cycle_us;
    }
    return static_cast<std::uint32_t>(u_ppm);
}

/// Count distinct contexts in a manifest — needed for the L&L bound.
constexpr std::size_t count_on_context(DeclView v, ContextId ctx) noexcept
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        if (v[i].context == ctx) ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Rate-monotonic schedulability
//
// Liu & Layland 1973: U <= n * (2^(1/n) - 1) is SUFFICIENT for RM feasibility.
// We precompute the bound for small n (typical per-task entity counts are
// << 32). The bound converges to ln(2) ≈ 0.693147 as n → ∞; we return
// ln(2) * 1_000_000 rounded down for n >= 32, which is still sound.

constexpr std::uint32_t rm_bound_ppm(std::size_t n) noexcept
{
    // Precomputed floor(n*(2^(1/n)-1) * 1e6). Conservative: we round DOWN
    // so the test "U <= bound" is strict.
    constexpr std::uint32_t table[] = {
        1'000'000,  //  0 (degenerate)
        1'000'000,  //  1
          828'427,  //  2
          779'763,  //  3
          756'828,  //  4
          743'491,  //  5
          734'772,  //  6
          728'626,  //  7
          724'061,  //  8
          720'538,  //  9
          717'734,  // 10
          715'452,  // 11
          713'557,  // 12
          711'959,  // 13
          710'593,  // 14
          709'411,  // 15
          708'380,  // 16
          707'472,  // 17
          706'666,  // 18
          705'945,  // 19
          705'297,  // 20
          704'709,  // 21
          704'175,  // 22
          703'686,  // 23
          703'238,  // 24
          702'826,  // 25
          702'445,  // 26
          702'092,  // 27
          701'764,  // 28
          701'459,  // 29
          701'173,  // 30
          700'906,  // 31
    };
    if (n < sizeof(table) / sizeof(table[0]))
    {
        return table[n];
    }
    return 693'147; // ln(2)
}

/// Liu & Layland sufficient bound. If this returns true the task set is
/// guaranteed RM-schedulable on that context. Failure is INCONCLUSIVE, not
/// proof of infeasibility — use hyperbolic_bound_met or (future) RTA for a
/// tighter answer.
constexpr bool rm_sufficient(DeclView v, ContextId ctx) noexcept
{
    return utilization_ppm(v, ctx) <= rm_bound_ppm(count_on_context(v, ctx));
}

/// Hyperbolic product Π(uᵢ + 1) in parts-per-million (so 2.0 → 2'000'000).
/// Exposed so consumers (notably the JSON exporter) can show the working,
/// not just the pass/fail result. Does NOT early-out — callers get the exact
/// product even when it is > 2.
constexpr std::uint64_t hyperbolic_product_ppm(DeclView v, ContextId ctx) noexcept
{
    std::uint64_t prod_ppm = 1'000'000ULL; // 1.0 in ppm
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        EntityDecl const& e = v[i];
        if (e.context != ctx) continue;
        std::uint64_t const u_ppm
            = (static_cast<std::uint64_t>(e.timing.wcet_us) * 1'000'000ULL)
            / e.timing.cycle_us;
        prod_ppm = prod_ppm * (1'000'000ULL + u_ppm) / 1'000'000ULL;
    }
    return prod_ppm;
}

/// Hyperbolic bound (Bini, Buttazzo, Buttazzo 2003):
///   product_i (u_i + 1) <= 2  ⇔ RM-schedulable
///
/// Tighter than L&L. We compute in fixed-point parts-per-million.
constexpr bool hyperbolic_bound_met(DeclView v, ContextId ctx) noexcept
{
    return hyperbolic_product_ppm(v, ctx) <= 2'000'000ULL;
}

// ---------------------------------------------------------------------------
// Criticality breakdown — per context, per ASIL class. Used by the GUI to
// render the freedom-from-interference view (how much of a context's CPU
// budget is owed to each ASIL level).

struct CriticalityUtilPpm
{
    std::uint32_t qm_ppm     = 0;
    std::uint32_t asil_a_ppm = 0;
    std::uint32_t asil_b_ppm = 0;
    std::uint32_t asil_c_ppm = 0;
    std::uint32_t asil_d_ppm = 0;
};

constexpr CriticalityUtilPpm utilization_by_criticality_ppm(
    DeclView v, ContextId ctx) noexcept
{
    CriticalityUtilPpm out{};
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        EntityDecl const& e = v[i];
        if (e.context != ctx) continue;
        std::uint32_t const u_ppm = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(e.timing.wcet_us) * 1'000'000ULL)
            / e.timing.cycle_us);
        switch (e.criticality)
        {
        case Criticality::QM:     out.qm_ppm     += u_ppm; break;
        case Criticality::ASIL_A: out.asil_a_ppm += u_ppm; break;
        case Criticality::ASIL_B: out.asil_b_ppm += u_ppm; break;
        case Criticality::ASIL_C: out.asil_c_ppm += u_ppm; break;
        case Criticality::ASIL_D: out.asil_d_ppm += u_ppm; break;
        }
    }
    return out;
}

/// True if a context carries entities of more than one criticality level.
/// Flagged by the GUI because freedom-from-interference requires timing
/// protection when mixed-criticality tasks share an execution context.
constexpr bool context_is_mixed_criticality(DeclView v, ContextId ctx) noexcept
{
    auto const b = utilization_by_criticality_ppm(v, ctx);
    int levels = 0;
    levels += (b.qm_ppm     > 0 ? 1 : 0);
    levels += (b.asil_a_ppm > 0 ? 1 : 0);
    levels += (b.asil_b_ppm > 0 ? 1 : 0);
    levels += (b.asil_c_ppm > 0 ? 1 : 0);
    levels += (b.asil_d_ppm > 0 ? 1 : 0);
    return levels > 1;
}

// ---------------------------------------------------------------------------
// Deadlines — simple check that C <= D for every entity. Actual deadline
// miss analysis requires RTA (follow-up work).

constexpr bool deadlines_plausible(DeclView v) noexcept
{
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        auto const& t = v[i].timing;
        if (t.wcet_us > t.effective_deadline()) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Hyperperiod (lcm of cycles). Bounded analysis window — a supervisor could
// use this to set the trace buffer size or the audit cadence.

constexpr std::uint64_t hyperperiod_us(DeclView v) noexcept
{
    std::uint64_t h = 1;
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        h = lcm_u64(h, v[i].timing.cycle_us);
    }
    return v.length() == 0 ? 0 : h;
}

// ---------------------------------------------------------------------------
// Response-time analysis (Joseph-Pandya fixed-point iteration).
//
// RTA is the exact test for rate-monotonic schedulability under the
// independent-task model (no blocking, implicit or constrained deadlines).
// Unlike L&L and hyperbolic, RTA is both sufficient AND necessary — if it
// says the schedule misses, there is a feasible arrival pattern that
// actually misses the deadline; if it says OK, the worst-case response time
// is at most R_i ≤ D_i for every task. This is what ASIL-D sign-off needs.
//
// Priority assignment: rate-monotonic, higher priority to strictly shorter
// periods. Ties in period are broken by declaration order (earlier index =
// higher priority) — this matches the FIFO-within-priority-level behaviour
// of every real RTOS we care about (FreeRTOS, ThreadX, OSEK) and is the
// standard Audsley RTA convention.
//
// Blocking term B_i is optional additive; pass 0 when no shared resources
// are modeled. PIP/PCP users can plug B_i per entity via the 3-arg form.
//
// All functions are constexpr so RTA results can drive static_assert-based
// build-time gates.

constexpr Duration RTA_INFEASIBLE = 0xFFFFFFFFu;

/// Maximum iterations for the fixed-point loop before giving up. With
/// integer ceilings and strictly increasing R, convergence is fast in
/// practice (log of the hyperperiod at most). 256 is safely above anything
/// a real ECU schedule would need.
inline constexpr int RTA_MAX_ITER = 256;

/// Exact worst-case response time for entity decls[idx] under RM on its
/// context. Uses the entity's own `blocking_us` from the EntityDecl; the
/// override parameter lets callers force a different value (e.g. tests, or
/// a supervisor exploring "what if this B_i were larger"). Returns
/// RTA_INFEASIBLE if the iteration diverges past the deadline or fails to
/// converge within RTA_MAX_ITER.
constexpr Duration rta_response_time_us(
    DeclView    v,
    std::size_t idx,
    Duration    blocking_override = 0,
    bool        use_override      = false) noexcept
{
    if (idx >= v.length()) { return RTA_INFEASIBLE; }

    EntityDecl const& me  = v[idx];
    Duration   const  Ci  = me.timing.wcet_us;
    Duration   const  Ti  = me.timing.cycle_us;
    Duration   const  Di  = me.timing.effective_deadline();
    ContextId  const  ctx = me.context;
    Duration   const  Bi  = use_override ? blocking_override : me.blocking_us;

    // Initial guess: own work + blocking. If already past deadline, done.
    Duration R = Ci + Bi;
    if (R > Di) { return RTA_INFEASIBLE; }

    for (int iter = 0; iter < RTA_MAX_ITER; ++iter)
    {
        // R_{n+1} = C_i + B_i + Σ_{j ∈ hp(i)} ⌈R_n / T_j⌉ · C_j
        //   where hp(i) = { j : same context, T_j < T_i }
        std::uint64_t sum = 0;
        for (std::size_t j = 0; j < v.length(); ++j)
        {
            if (j == idx) continue;
            EntityDecl const& other = v[j];
            if (other.context != ctx) continue;
            // hp(i) = { j : T_j < T_i }  ∪  { j : T_j == T_i  ∧  j < i }
            bool const strict_hp = other.timing.cycle_us < Ti;
            bool const tied_hp   = (other.timing.cycle_us == Ti) && (j < idx);
            if (!strict_hp && !tied_hp) continue;
            Duration const Tj = other.timing.cycle_us;
            Duration const Cj = other.timing.wcet_us;
            // ⌈R / Tj⌉ using integer math; R fits in Duration (uint32).
            std::uint64_t const ceil_div
                = (static_cast<std::uint64_t>(R) + Tj - 1ULL) / Tj;
            sum += ceil_div * Cj;
            if (sum > Di) { return RTA_INFEASIBLE; } // early out
        }
        std::uint64_t const R_new_u64
            = static_cast<std::uint64_t>(Ci) + Bi + sum;
        if (R_new_u64 > Di) { return RTA_INFEASIBLE; }

        Duration const R_new = static_cast<Duration>(R_new_u64);
        if (R_new == R) { return R; } // converged
        R = R_new;
    }
    return RTA_INFEASIBLE; // did not converge
}

/// True if every entity on `ctx` passes RTA (R_i ≤ D_i).
constexpr bool rta_pass(DeclView v, ContextId ctx) noexcept
{
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        if (v[i].context != ctx) continue;
        if (rta_response_time_us(v, i, 0) == RTA_INFEASIBLE) { return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Aggregate verdict for a manifest
//
// analyze() uses sufficient tests (sanity + L&L/hyperbolic). Fast, constexpr,
// suitable for quick feedback. Can be pessimistic — will reject some
// actually-feasible schedules.
//
// analyze_with_rta() uses the exact test (sanity + RTA). Slower (fixed-point
// iteration) but gives a definitive answer for the independent-task model.
// This is the check ASIL-D evidence should rely on.

enum class Verdict : std::uint8_t
{
    Ok,
    EntityInsane,        ///< some EntityDecl violates its invariants
    OverUtilized,        ///< hyperbolic bound failed on some context
    DeadlineImplausible, ///< C > D somewhere
    RtaDeadlineMiss,     ///< exact RTA found R_i > D_i on some entity
};

constexpr Verdict analyze(DeclView v) noexcept
{
    if (!all_entities_sane(v))   return Verdict::EntityInsane;
    if (!deadlines_plausible(v)) return Verdict::DeadlineImplausible;
    // check each distinct context
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        ContextId const ctx = v[i].context;
        // skip duplicates (only check each context once)
        bool seen = false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (v[j].context == ctx) { seen = true; break; }
        }
        if (seen) continue;
        if (!hyperbolic_bound_met(v, ctx)) return Verdict::OverUtilized;
    }
    return Verdict::Ok;
}

/// Exact feasibility analysis using RTA instead of the sufficient bounds.
/// Returns Verdict::Ok if every entity is RM-feasible under Joseph-Pandya
/// (no blocking modeled). Returns Verdict::RtaDeadlineMiss if any entity's
/// worst-case response time exceeds its deadline. Sanity + deadline
/// checks run first as in analyze().
constexpr Verdict analyze_with_rta(DeclView v) noexcept
{
    if (!all_entities_sane(v))   return Verdict::EntityInsane;
    if (!deadlines_plausible(v)) return Verdict::DeadlineImplausible;
    for (std::size_t i = 0; i < v.length(); ++i)
    {
        ContextId const ctx = v[i].context;
        bool seen = false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (v[j].context == ctx) { seen = true; break; }
        }
        if (seen) continue;
        if (!rta_pass(v, ctx)) return Verdict::RtaDeadlineMiss;
    }
    return Verdict::Ok;
}

} // namespace rtSched
