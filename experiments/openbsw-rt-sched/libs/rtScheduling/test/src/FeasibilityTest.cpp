// Copyright 2026 Taktflow.

#include "rtScheduling/Feasibility.h"

#include <gtest/gtest.h>

using namespace rtSched;

namespace
{

constexpr ContextId CTX_A = 1;
constexpr ContextId CTX_B = 2;

constexpr EntityDecl mk(
    std::string_view name,
    ContextId        ctx,
    Duration         cycle,
    Duration         wcet,
    Duration         deadline = 0)
{
    return EntityDecl{name, ctx, Timing{cycle, wcet, deadline, 0}, Criticality::QM};
}

} // namespace

TEST(Feasibility, EntitySanity_Basics)
{
    EXPECT_TRUE(entity_is_sane(mk("ok", CTX_A, 1000, 100)));
    EXPECT_FALSE(entity_is_sane(mk("zeroCycle", CTX_A, 0, 100)));
    EXPECT_FALSE(entity_is_sane(mk("zeroWcet", CTX_A, 1000, 0)));
    EXPECT_FALSE(entity_is_sane(mk("wcetGtDeadline", CTX_A, 1000, 500, 400)));
    EXPECT_FALSE(entity_is_sane(mk("deadlineGtCycle", CTX_A, 1000, 100, 2000)));
}

TEST(Feasibility, EntitySanity_PhaseLtCycle)
{
    EntityDecl e = mk("phaseOk", CTX_A, 1000, 100);
    e.timing.phase_us = 999;
    EXPECT_TRUE(entity_is_sane(e));
    e.timing.phase_us = 1000;
    EXPECT_FALSE(entity_is_sane(e));
}

TEST(Feasibility, Utilization_SingleTask)
{
    // 100us out of 1000us → 100_000 ppm (10%)
    constexpr std::array<EntityDecl, 1> m{{mk("a", CTX_A, 1000, 100)}};
    EXPECT_EQ(utilization_ppm(view(m), CTX_A), 100'000u);
    EXPECT_EQ(utilization_ppm(view(m), CTX_B), 0u);
}

TEST(Feasibility, Utilization_MultipleOnSameContext)
{
    constexpr std::array<EntityDecl, 3> m{{
        mk("a", CTX_A, 1000, 100), // 10%
        mk("b", CTX_A, 5000, 500), // 10%
        mk("c", CTX_A, 2000, 100), //  5%
    }};
    EXPECT_EQ(utilization_ppm(view(m), CTX_A), 250'000u);
}

TEST(Feasibility, Utilization_IgnoresOtherContexts)
{
    constexpr std::array<EntityDecl, 2> m{{
        mk("a", CTX_A, 1000, 300), // 30% on A
        mk("b", CTX_B, 1000, 600), // 60% on B
    }};
    EXPECT_EQ(utilization_ppm(view(m), CTX_A), 300'000u);
    EXPECT_EQ(utilization_ppm(view(m), CTX_B), 600'000u);
}

TEST(Feasibility, RmBound_MatchesKnownValues)
{
    // Liu & Layland table spot checks. Floor values: the bound decreases as
    // n increases, approaching ln(2).
    EXPECT_EQ(rm_bound_ppm(1), 1'000'000u);
    EXPECT_EQ(rm_bound_ppm(2),   828'427u);
    EXPECT_EQ(rm_bound_ppm(3),   779'763u);
    EXPECT_EQ(rm_bound_ppm(10),  717'734u);
    EXPECT_EQ(rm_bound_ppm(100), 693'147u); // beyond table → asymptotic
}

TEST(Feasibility, RmSufficient_SimpleFeasible)
{
    // Two tasks at 40% each → U=80%. L&L bound for n=2 is ~82.8%. Feasible.
    constexpr std::array<EntityDecl, 2> m{{
        mk("a", CTX_A, 1000, 400),
        mk("b", CTX_A, 5000, 2000),
    }};
    EXPECT_TRUE(rm_sufficient(view(m), CTX_A));
}

TEST(Feasibility, RmSufficient_OverBound)
{
    // Three tasks at 30% each → U=90%. L&L bound for n=3 is ~77.9%. Fails L&L.
    // This is inconclusive (not proof of infeasibility) but rm_sufficient
    // should return false.
    constexpr std::array<EntityDecl, 3> m{{
        mk("a", CTX_A, 1000, 300),
        mk("b", CTX_A, 2000, 600),
        mk("c", CTX_A, 5000, 1500),
    }};
    EXPECT_FALSE(rm_sufficient(view(m), CTX_A));
}

TEST(Feasibility, HyperbolicBound_TighterThanLiuLayland)
{
    // 3 tasks at 30%+30%+30%=90% fails L&L (bound ~77.9%).
    // Hyperbolic: (1.3)^3 = 2.197 > 2 → also fails. Expected.
    constexpr std::array<EntityDecl, 3> m{{
        mk("a", CTX_A, 1000, 300),
        mk("b", CTX_A, 2000, 600),
        mk("c", CTX_A, 5000, 1500),
    }};
    EXPECT_FALSE(hyperbolic_bound_met(view(m), CTX_A));

    // 3 tasks at 25%+25%+25%=75% fails L&L (bound for n=3 is ~77.9% so this
    // passes L&L actually). Hyperbolic: (1.25)^3 = 1.953 < 2 → passes.
    constexpr std::array<EntityDecl, 3> m2{{
        mk("a", CTX_A, 1000, 250),
        mk("b", CTX_A, 2000, 500),
        mk("c", CTX_A, 5000, 1250),
    }};
    EXPECT_TRUE(hyperbolic_bound_met(view(m2), CTX_A));
}

TEST(Feasibility, Hyperperiod_HarmonicCycles)
{
    constexpr std::array<EntityDecl, 3> m{{
        mk("a", CTX_A, 1000, 100),
        mk("b", CTX_A, 2000, 100),
        mk("c", CTX_A, 4000, 100),
    }};
    EXPECT_EQ(hyperperiod_us(view(m)), 4000u);
}

TEST(Feasibility, Hyperperiod_NonHarmonic)
{
    // 3,5,7 → lcm 105
    constexpr std::array<EntityDecl, 3> m{{
        mk("a", CTX_A, 3, 1),
        mk("b", CTX_A, 5, 1),
        mk("c", CTX_A, 7, 1),
    }};
    EXPECT_EQ(hyperperiod_us(view(m)), 105u);
}

TEST(Feasibility, Hyperperiod_Empty)
{
    DeclView const v{nullptr, 0};
    EXPECT_EQ(hyperperiod_us(v), 0u);
}

TEST(Feasibility, Analyze_Verdicts)
{
    // Infeasible sanity: wcet=0
    {
        std::array<EntityDecl, 1> m{{mk("bad", CTX_A, 1000, 0)}};
        EXPECT_EQ(analyze(view(m)), Verdict::EntityInsane);
    }
    // Infeasible deadline: wcet > deadline
    {
        EntityDecl e = mk("bad", CTX_A, 1000, 900);
        e.timing.deadline_us = 500;
        std::array<EntityDecl, 1> m{{e}};
        EXPECT_EQ(analyze(view(m)), Verdict::EntityInsane); // caught by sanity first
    }
    // Over-utilized
    {
        std::array<EntityDecl, 2> m{{
            mk("a", CTX_A, 1000, 600),
            mk("b", CTX_A, 1000, 600),
        }};
        EXPECT_EQ(analyze(view(m)), Verdict::OverUtilized);
    }
    // OK
    {
        std::array<EntityDecl, 2> m{{
            mk("a", CTX_A, 1000, 100),
            mk("b", CTX_B, 5000, 500),
        }};
        EXPECT_EQ(analyze(view(m)), Verdict::Ok);
    }
}

// --- Compile-time checks: these run at build time. If they fail the build
// fails. They are the "hard gate" that the API promises for ASIL contexts.
namespace compile_time_checks
{
constexpr std::array<EntityDecl, 3> OK_MANIFEST{{
    EntityDecl{"safety",   1, {10'000, 1'000}, Criticality::ASIL_D},
    EntityDecl{"control",  2, {20'000, 3'000}, Criticality::ASIL_B},
    EntityDecl{"telemetry",3, {100'000, 5'000}, Criticality::QM},
}};
static_assert(analyze(view(OK_MANIFEST)) == Verdict::Ok,
              "reference manifest should be feasible");
static_assert(hyperperiod_us(view(OK_MANIFEST)) == 100'000ULL,
              "hyperperiod of {10,20,100} ms is 100 ms");

constexpr std::array<EntityDecl, 2> BAD_MANIFEST{{
    EntityDecl{"heavyA", 1, {1'000, 700}, Criticality::QM},
    EntityDecl{"heavyB", 1, {1'000, 700}, Criticality::QM},
}};
static_assert(analyze(view(BAD_MANIFEST)) == Verdict::OverUtilized,
              "140% utilization should be caught at compile time");
} // namespace compile_time_checks
