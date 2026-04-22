// Copyright 2026 Taktflow.

/**
 * Tests for response-time analysis (Joseph-Pandya).
 *
 * The numeric expectations come from hand-worked examples and from the
 * classical textbook case:
 *   Burns & Wellings, "Real-Time Systems and Programming Languages",
 *   Chapter 13 — Response Time Analysis.
 *
 * Every test documents the hand calculation inline so a reviewer can
 * verify the library output against first principles without running
 * the tests.
 */

#include "rtScheduling/Feasibility.h"

#include <gtest/gtest.h>

using namespace rtSched;

namespace
{

constexpr ContextId CTX = 1;

constexpr EntityDecl mk(
    std::string_view name,
    Duration         T,
    Duration         C,
    Duration         D = 0)
{
    return EntityDecl{name, CTX, Timing{T, C, D, 0}, Criticality::QM};
}

} // namespace

// --------------------------------------------------------------------------
// Single task — R = C, trivially schedulable if C ≤ D.

TEST(Rta, SingleTask_R_equals_C)
{
    constexpr std::array<EntityDecl, 1> m{{mk("solo", 1000, 200)}};
    EXPECT_EQ(rta_response_time_us(view(m), 0), 200u);
    EXPECT_TRUE(rta_pass(view(m), CTX));
}

TEST(Rta, SingleTask_OverDeadline_IsInfeasible)
{
    // C > D via constrained deadline.
    EntityDecl e = mk("too-slow", 1000, 800);
    e.timing.deadline_us = 500; // D < C
    // entity_is_sane rejects this at the analyze() level, but RTA itself
    // also flags it: R=800 > D=500 on first evaluation.
    std::array<EntityDecl, 1> m{{e}};
    EXPECT_EQ(rta_response_time_us(view(m), 0), RTA_INFEASIBLE);
}

// --------------------------------------------------------------------------
// Two tasks, higher priority preempts lower.
//
//   T1 (hp): T=100, C=20, D=100
//   T2 (lp): T=200, C=60, D=200
//
// R_1 = 20
// R_2 initial = 60
//   iter 1: 60 + ⌈60/100⌉·20 = 60 + 20 = 80
//   iter 2: 60 + ⌈80/100⌉·20 = 60 + 20 = 80   ← converged
// Both R ≤ D, feasible.

TEST(Rta, TwoTasks_Converges_Feasible)
{
    constexpr std::array<EntityDecl, 2> m{{
        mk("hp", 100, 20),
        mk("lp", 200, 60),
    }};
    EXPECT_EQ(rta_response_time_us(view(m), 0), 20u);
    EXPECT_EQ(rta_response_time_us(view(m), 1), 80u);
    EXPECT_TRUE(rta_pass(view(m), CTX));
}

// --------------------------------------------------------------------------
// Three tasks, all harmonic — shows RTA succeeding where L&L fails.
//
//   T1: T=100, C=30, D=100
//   T2: T=200, C=60, D=200
//   T3: T=400, C=120, D=400
//
//   U = 0.30 + 0.30 + 0.30 = 0.90
//   L&L bound(3) ≈ 0.7798 → L&L FAILS
//
//   R_1 = 30
//   R_2 = 60 + ⌈60/100⌉·30 = 90
//         ⌈90/100⌉=1 → 60+30=90 (converged)
//   R_3 initial = 120 + 30 + 60 = 210
//     iter 1: 120 + ⌈210/100⌉·30 + ⌈210/200⌉·60 = 120 + 90 + 120 = 330
//     iter 2: 120 + ⌈330/100⌉·30 + ⌈330/200⌉·60 = 120 + 120 + 120 = 360
//     iter 3: 120 + ⌈360/100⌉·30 + ⌈360/200⌉·60 = 120 + 120 + 120 = 360 ✓
//   All ≤ D → RTA says FEASIBLE even though L&L says maybe-not.

TEST(Rta, HarmonicTriplet_RtaBeatsLiuLayland)
{
    constexpr std::array<EntityDecl, 3> m{{
        mk("T1", 100, 30),
        mk("T2", 200, 60),
        mk("T3", 400, 120),
    }};
    // L&L: sufficient test rejects this.
    EXPECT_FALSE(rm_sufficient(view(m), CTX));
    EXPECT_FALSE(hyperbolic_bound_met(view(m), CTX));

    // RTA: exact test accepts it.
    EXPECT_EQ(rta_response_time_us(view(m), 0),  30u);
    EXPECT_EQ(rta_response_time_us(view(m), 1),  90u);
    EXPECT_EQ(rta_response_time_us(view(m), 2), 360u);
    EXPECT_TRUE(rta_pass(view(m), CTX));
}

// --------------------------------------------------------------------------
// Three tasks non-harmonic — forces RTA to detect a deadline miss.
//
//   T1: T=3,   C=1,  D=3
//   T2: T=5,   C=2,  D=5
//   T3: T=15,  C=5,  D=15
//
//   U = 1/3 + 2/5 + 5/15 ≈ 1.067 → over 100%
//   R_1 = 1 ≤ 3
//   R_2 init = 2 + ⌈2/3⌉·1 = 2+1 = 3; iter 2: 2 + ⌈3/3⌉·1 = 3 (converged)
//   R_3 init = 5 + 1 + 2 = 8
//     iter 1: 5 + ⌈8/3⌉·1 + ⌈8/5⌉·2  = 5 + 3 + 4 = 12
//     iter 2: 5 + ⌈12/3⌉·1 + ⌈12/5⌉·2 = 5 + 4 + 6 = 15
//     iter 3: 5 + ⌈15/3⌉·1 + ⌈15/5⌉·2 = 5 + 5 + 6 = 16  > D=15  → MISS

TEST(Rta, NonHarmonicOverloaded_DetectsMiss)
{
    constexpr std::array<EntityDecl, 3> m{{
        mk("T1",  3, 1),
        mk("T2",  5, 2),
        mk("T3", 15, 5),
    }};
    EXPECT_EQ(rta_response_time_us(view(m), 0),  1u);
    EXPECT_EQ(rta_response_time_us(view(m), 1),  3u);
    EXPECT_EQ(rta_response_time_us(view(m), 2), RTA_INFEASIBLE);
    EXPECT_FALSE(rta_pass(view(m), CTX));
}

// --------------------------------------------------------------------------
// Constrained deadlines — D < T case. L&L is NOT sufficient here; RTA is
// the only honest check.
//
//   T1: T=100, C=20, D= 25   (deadline tighter than period)
//   T2: T=200, C=30, D=100
//
//   U = 0.20 + 0.15 = 0.35 — L&L says "feasible" with implicit deadlines
//   but with D=25 for T1, R_1 = 20 ≤ 25 OK. Tight but passes.
//   R_2 init = 30 + 20 = 50
//     iter 1: 30 + ⌈50/100⌉·20 = 30 + 20 = 50 (converged)
//   R_2 = 50 ≤ D=100 OK.
//   Overall feasible.

TEST(Rta, ConstrainedDeadline_TightButFeasible)
{
    EntityDecl e1 = mk("t1", 100, 20);
    e1.timing.deadline_us = 25;
    EntityDecl e2 = mk("t2", 200, 30);
    e2.timing.deadline_us = 100;
    std::array<EntityDecl, 2> m{{e1, e2}};

    EXPECT_EQ(rta_response_time_us(view(m), 0), 20u);
    EXPECT_EQ(rta_response_time_us(view(m), 1), 50u);
    EXPECT_TRUE(rta_pass(view(m), CTX));
}

TEST(Rta, ConstrainedDeadline_TooTight_IsInfeasible)
{
    // Same workload but D_1 = 15 < C_1 = 20 — instantly fails.
    EntityDecl e1 = mk("t1", 100, 20);
    e1.timing.deadline_us = 15;
    std::array<EntityDecl, 1> m{{e1}};
    EXPECT_EQ(rta_response_time_us(view(m), 0), RTA_INFEASIBLE);
}

// --------------------------------------------------------------------------
// Blocking term — optional PIP/PCP model. A task that would otherwise pass
// fails once blocking is added.
//
//   T1: T=100, C=20, D=25
//   Blocking B=10 on T1 pushes R_1 = 20 + 10 = 30 > D=25 → miss.

TEST(Rta, BlockingTerm_DeclFieldTakesEffect)
{
    EntityDecl e = mk("t1", 100, 20);
    e.timing.deadline_us = 25;
    // Without blocking, feasible: R = 20 ≤ 25.
    std::array<EntityDecl, 1> m1{{e}};
    EXPECT_EQ(rta_response_time_us(view(m1), 0), 20u);

    // With B=10 declared on the entity: R = 30 > 25 → miss.
    e.blocking_us = 10;
    std::array<EntityDecl, 1> m2{{e}};
    EXPECT_EQ(rta_response_time_us(view(m2), 0), RTA_INFEASIBLE);
}

TEST(Rta, BlockingTerm_OverrideFlag)
{
    EntityDecl e = mk("t1", 100, 20);
    e.timing.deadline_us = 25;
    e.blocking_us = 0;
    std::array<EntityDecl, 1> m{{e}};

    // Override to 10 even though the decl says 0.
    EXPECT_EQ(rta_response_time_us(view(m), 0, 10, true), RTA_INFEASIBLE);
    // Default call still uses the decl's 0.
    EXPECT_EQ(rta_response_time_us(view(m), 0), 20u);
}

// --------------------------------------------------------------------------
// Cross-context independence — tasks on different contexts do not interfere
// in the RTA model (openbsw assigns one RTOS thread per context; RTA within
// a context handles preemption inside that thread; across contexts, the OS
// scheduler handles it).

TEST(Rta, CrossContextTasks_DoNotInterfere)
{
    constexpr std::array<EntityDecl, 2> m{{
        EntityDecl{"ctx1_short", 1, {100, 80}, Criticality::QM},
        EntityDecl{"ctx2_short", 2, {100, 80}, Criticality::QM},
    }};
    // If the RTA wrongly treated both as interfering, R would go to 160,
    // violating the deadline. Correct behaviour: each sees only itself.
    EXPECT_EQ(rta_response_time_us(view(m), 0), 80u);
    EXPECT_EQ(rta_response_time_us(view(m), 1), 80u);
    EXPECT_TRUE(rta_pass(view(m), 1));
    EXPECT_TRUE(rta_pass(view(m), 2));
}

// --------------------------------------------------------------------------
// Tied periods on same context — tie-break by declaration order.
//
// Two tasks with T=1000, C=700 each. Total 140% utilization. Under strict
// `<` hp set they'd neither preempt each other — obviously wrong, FIFO
// execution means the second finishes at t=1400 > D=1000.
//
// With declaration-order tie-break:
//   RTA(idx=0): no hp → R=700 ≤ 1000 OK
//   RTA(idx=1): hp={0} → init 700; iter 1: 700 + ⌈700/1000⌉·700 = 1400 > D
//               → INFEASIBLE  ✓

TEST(Rta, TiedPeriodsOnSameContext_TieBreakByDeclarationOrder)
{
    constexpr std::array<EntityDecl, 2> m{{
        mk("first",  1000, 700),
        mk("second", 1000, 700),
    }};
    EXPECT_EQ(rta_response_time_us(view(m), 0),  700u);
    EXPECT_EQ(rta_response_time_us(view(m), 1),  RTA_INFEASIBLE);
    EXPECT_FALSE(rta_pass(view(m), CTX));
}

TEST(Rta, TiedPeriodsOnSameContext_BothFit)
{
    // Both at 30% utilization, tied period → second sees first as hp.
    //   RTA(0): R=300 ≤ 1000 OK
    //   RTA(1): init 300; iter: 300 + ⌈300/1000⌉·300 = 600 (converged)
    //           600 ≤ 1000 OK
    constexpr std::array<EntityDecl, 2> m{{
        mk("first",  1000, 300),
        mk("second", 1000, 300),
    }};
    EXPECT_EQ(rta_response_time_us(view(m), 0), 300u);
    EXPECT_EQ(rta_response_time_us(view(m), 1), 600u);
    EXPECT_TRUE(rta_pass(view(m), CTX));
}

// --------------------------------------------------------------------------
// analyze_with_rta() — aggregate verdict.

TEST(Rta, AnalyzeWithRta_OkPath)
{
    constexpr std::array<EntityDecl, 3> m{{
        mk("a", 100, 30),
        mk("b", 200, 60),
        mk("c", 400, 120),
    }};
    EXPECT_EQ(analyze_with_rta(view(m)), Verdict::Ok);
    // analyze() sufficient test rejects the same manifest — demonstrates
    // that analyze_with_rta is strictly tighter.
    EXPECT_EQ(analyze(view(m)), Verdict::OverUtilized);
}

TEST(Rta, AnalyzeWithRta_DetectsMissWhereHyperbolicFails)
{
    constexpr std::array<EntityDecl, 3> m{{
        mk("T1",  3, 1),
        mk("T2",  5, 2),
        mk("T3", 15, 5),
    }};
    EXPECT_EQ(analyze_with_rta(view(m)), Verdict::RtaDeadlineMiss);
}

TEST(Rta, AnalyzeWithRta_CatchesSanityAndDeadlineBeforeRta)
{
    // wcet=0 → entity insane, must not reach RTA path
    constexpr std::array<EntityDecl, 1> m{{mk("bad", 1000, 0)}};
    EXPECT_EQ(analyze_with_rta(view(m)), Verdict::EntityInsane);
}

// --------------------------------------------------------------------------
// Compile-time RTA gate — the headline feature for ASIL-D builds.
// These run at build time; a failure here fails the compile, not the
// deployment.

namespace compile_time_rta
{
constexpr std::array<EntityDecl, 3> FEASIBLE{{
    EntityDecl{"a", 1, {100, 30}, Criticality::ASIL_D},
    EntityDecl{"b", 1, {200, 60}, Criticality::ASIL_B},
    EntityDecl{"c", 1, {400, 120}, Criticality::QM},
}};
static_assert(rta_response_time_us(view(FEASIBLE), 2) == 360u,
              "harmonic textbook case: R_3 must be 360us");
static_assert(analyze_with_rta(view(FEASIBLE)) == Verdict::Ok,
              "harmonic triplet is RTA-feasible");

constexpr std::array<EntityDecl, 3> INFEASIBLE{{
    EntityDecl{"T1", 1, {3, 1},  Criticality::QM},
    EntityDecl{"T2", 1, {5, 2},  Criticality::QM},
    EntityDecl{"T3", 1, {15, 5}, Criticality::QM},
}};
static_assert(analyze_with_rta(view(INFEASIBLE)) == Verdict::RtaDeadlineMiss,
              "over-utilized triplet must fail RTA at compile time");
} // namespace compile_time_rta
