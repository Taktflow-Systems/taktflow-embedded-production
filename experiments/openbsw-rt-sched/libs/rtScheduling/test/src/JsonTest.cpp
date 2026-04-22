// Copyright 2026 Taktflow.

#include "rtScheduling/Json.h"
#include "rtScheduling/Registry.h"

#include <gtest/gtest.h>

#include <array>
#include <string>

using namespace rtSched;

namespace
{

bool contains(std::string const& hay, std::string_view needle)
{
    return hay.find(needle) != std::string::npos;
}

// Locate the integer value of a "key":N pair. Returns -1 if not found.
// Primitive but sufficient for structural assertions on known keys.
long long find_int_value(std::string const& s, std::string const& key)
{
    std::string const pattern = "\"" + key + "\":";
    auto pos = s.find(pattern);
    if (pos == std::string::npos) return -1;
    pos += pattern.size();
    // Skip optional whitespace (we don't emit any but be robust).
    while (pos < s.size() && (s[pos] == ' ')) ++pos;
    long long out = 0;
    bool      any = false;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
    {
        out = out * 10 + (s[pos] - '0');
        ++pos;
        any = true;
    }
    return any ? out : -1;
}

} // namespace

TEST(Json, EmptyManifest_ProducesWellShapedSkeleton)
{
    std::string out;
    DeclView    empty{nullptr, 0};
    json::write_report(empty, nullptr, out);

    EXPECT_TRUE(contains(out, "\"schema\":\"rt-sched/1\""));
    EXPECT_TRUE(contains(out, "\"verdict\":\"ok\""));
    EXPECT_TRUE(contains(out, "\"hyperperiod_us\":0"));
    EXPECT_TRUE(contains(out, "\"entities\":[]"));
    EXPECT_TRUE(contains(out, "\"contexts\":[]"));
}

TEST(Json, SingleEntity_EmitsAllFieldsAndOneContext)
{
    constexpr std::array<EntityDecl, 1> m{{
        EntityDecl{"safety_tick", 1, {5000, 700, 5000, 0},
                   Criticality::ASIL_D, OverrunPolicy::Safety},
    }};
    std::string out;
    json::write_report(view(m), nullptr, out);

    EXPECT_TRUE(contains(out, "\"name\":\"safety_tick\""));
    EXPECT_TRUE(contains(out, "\"context\":1"));
    EXPECT_TRUE(contains(out, "\"cycle_us\":5000"));
    EXPECT_TRUE(contains(out, "\"wcet_us\":700"));
    EXPECT_TRUE(contains(out, "\"criticality\":\"asil_d\""));
    EXPECT_TRUE(contains(out, "\"overrun\":\"safety\""));
    // 700 / 5000 = 0.14 → 140000 ppm
    EXPECT_TRUE(contains(out, "\"utilization_ppm\":140000"));
    // n=1 → L&L bound = 1.0 → 1000000 ppm
    EXPECT_TRUE(contains(out, "\"ll_bound_ppm\":1000000"));
    EXPECT_TRUE(contains(out, "\"ll_pass\":true"));
    EXPECT_TRUE(contains(out, "\"hyperbolic_pass\":true"));
    EXPECT_TRUE(contains(out, "\"rta_pass\":true"));
    EXPECT_TRUE(contains(out, "\"mixed_criticality\":false"));
    EXPECT_TRUE(contains(out, "\"asil_d\":140000"));
    EXPECT_TRUE(contains(out, "\"qm\":0"));
    // RTA: R = C = 700 for a single task.
    EXPECT_EQ(find_int_value(out, "response_time_us"), 700);
    EXPECT_TRUE(contains(out, "\"rta_feasible\":true"));
}

TEST(Json, MixedCriticality_FlaggedPerContext)
{
    constexpr std::array<EntityDecl, 2> m{{
        EntityDecl{"asil",   1, {10000, 1000}, Criticality::ASIL_B,
                   OverrunPolicy::LogAndCount},
        EntityDecl{"qm",     1, {20000, 500},  Criticality::QM,
                   OverrunPolicy::Ignore},
    }};
    std::string out;
    json::write_report(view(m), nullptr, out);

    EXPECT_TRUE(contains(out, "\"mixed_criticality\":true"));
    // 1000/10000 = 100_000 ppm ASIL_B
    // 500/20000  =  25_000 ppm QM
    EXPECT_TRUE(contains(out, "\"asil_b\":100000"));
    EXPECT_TRUE(contains(out, "\"qm\":25000"));
}

TEST(Json, OverUtilized_VerdictPropagatesAndLlPassFalse)
{
    // Two tasks at 70% each on same context → 140% utilization.
    // Top-level verdict is rta_deadline_miss (exact RTA).
    // verdict_sufficient stays over_utilized (L&L / hyperbolic).
    constexpr std::array<EntityDecl, 2> m{{
        EntityDecl{"a", 1, {1000, 700}, Criticality::QM},
        EntityDecl{"b", 1, {1000, 700}, Criticality::QM},
    }};
    std::string out;
    json::write_report(view(m), nullptr, out);

    EXPECT_TRUE(contains(out, "\"verdict\":\"rta_deadline_miss\""));
    EXPECT_TRUE(contains(out, "\"verdict_sufficient\":\"over_utilized\""));
    // L&L bound for n=2 is 828427 ppm; 1_400_000 utilization fails.
    EXPECT_TRUE(contains(out, "\"utilization_ppm\":1400000"));
    EXPECT_TRUE(contains(out, "\"ll_pass\":false"));
    EXPECT_TRUE(contains(out, "\"hyperbolic_pass\":false"));
    EXPECT_TRUE(contains(out, "\"rta_pass\":false"));
    // First entity still feasible alone (R=700); second sees first as tied
    // higher priority → R=1400 > D=1000 → infeasible.
    EXPECT_TRUE(contains(out, "\"rta_feasible\":true"));
    EXPECT_TRUE(contains(out, "\"rta_feasible\":false"));
}

TEST(Json, Stats_EmittedWhenProvided)
{
    constexpr std::array<EntityDecl, 1> m{{
        EntityDecl{"x", 1, {1000, 100}, Criticality::QM},
    }};
    EntityStats stats{};
    stats.invocations     = 42;
    stats.overruns        = 1;
    stats.last_exec_us    = 90;
    stats.max_observed_us = 120;
    stats.min_observed_us = 55;
    std::array<EntityStats, 1> stats_arr{stats};

    std::string out;
    json::write_report(view(m), stats_arr.data(), out);

    EXPECT_EQ(find_int_value(out, "invocations"),     42);
    EXPECT_EQ(find_int_value(out, "overruns"),        1);
    EXPECT_EQ(find_int_value(out, "last_exec_us"),    90);
    EXPECT_EQ(find_int_value(out, "max_observed_us"), 120);
    EXPECT_EQ(find_int_value(out, "min_observed_us"), 55);
}

TEST(Json, Stats_MinSentinelNormalizedToZero)
{
    constexpr std::array<EntityDecl, 1> m{{
        EntityDecl{"x", 1, {1000, 100}, Criticality::QM},
    }};
    EntityStats stats{};  // default: min_observed_us = 0xFFFFFFFF
    std::array<EntityStats, 1> stats_arr{stats};

    std::string out;
    json::write_report(view(m), stats_arr.data(), out);
    EXPECT_EQ(find_int_value(out, "min_observed_us"), 0);
}

TEST(Json, ChunkedEmitter_ProducesSameBytes)
{
    constexpr std::array<EntityDecl, 2> m{{
        EntityDecl{"a", 1, {1000, 200}, Criticality::ASIL_D},
        EntityDecl{"b", 2, {2000, 500}, Criticality::QM},
    }};

    std::string buffered;
    json::write_report(view(m), nullptr, buffered);

    std::string captured;
    auto sink = +[](char const* data, std::size_t n, void* user) {
        static_cast<std::string*>(user)->append(data, n);
    };
    json::write_report_chunked(view(m), nullptr, sink, &captured);

    EXPECT_EQ(buffered, captured);
}

TEST(Json, NameWithSpecialChars_IsEscaped)
{
    // std::string_view over a literal containing backslash and quote.
    static constexpr EntityDecl weird{
        "he said \"hi\" \\ done", 1, {1000, 100}, Criticality::QM};
    std::array<EntityDecl, 1> m{{weird}};

    std::string out;
    json::write_report(DeclView{m.data(), 1}, nullptr, out);

    EXPECT_TRUE(contains(out, "\\\"hi\\\""));
    EXPECT_TRUE(contains(out, "\\\\"));
}

TEST(Json, RtaBeatsLiuLayland_VerdictsDiverge)
{
    // Harmonic triplet at 90% utilization — L&L fails, RTA feasible.
    // The JSON must show the split: top verdict ok, verdict_sufficient
    // over_utilized.
    constexpr std::array<EntityDecl, 3> m{{
        EntityDecl{"a", 1, {100, 30}, Criticality::ASIL_D},
        EntityDecl{"b", 1, {200, 60}, Criticality::ASIL_B},
        EntityDecl{"c", 1, {400, 120}, Criticality::QM},
    }};
    std::string out;
    json::write_report(view(m), nullptr, out);

    EXPECT_TRUE(contains(out, "\"verdict\":\"ok\""));
    EXPECT_TRUE(contains(out, "\"verdict_sufficient\":\"over_utilized\""));
    EXPECT_TRUE(contains(out, "\"ll_pass\":false"));
    EXPECT_TRUE(contains(out, "\"hyperbolic_pass\":false"));
    EXPECT_TRUE(contains(out, "\"rta_pass\":true"));
    // Hand-worked response times: 30, 90, 360
    EXPECT_TRUE(contains(out, "\"response_time_us\":30,"));
    EXPECT_TRUE(contains(out, "\"response_time_us\":90,"));
    EXPECT_TRUE(contains(out, "\"response_time_us\":360,"));
}

TEST(Json, FromRegistry_EmitsRegisteredEntities)
{
    Registry::instance().reset_for_test();

    static constexpr EntityDecl d1{"a", 1, {1000, 100}, Criticality::ASIL_D};
    static constexpr EntityDecl d2{"b", 2, {5000, 200}, Criticality::QM};
    int    r1 = 0, r2 = 0; // dummies; bridge not installed
    int    t1 = 0, t2 = 0;
    EntityStats s1{}, s2{};
    s1.invocations = 7;

    Binding b1{&d1, &r1, &t1, &s1};
    Binding b2{&d2, &r2, &t2, &s2};
    Registry::instance().add(b1);
    Registry::instance().add(b2);

    std::string out;
    json::write_report_from_registry(out);

    EXPECT_TRUE(contains(out, "\"name\":\"a\""));
    EXPECT_TRUE(contains(out, "\"name\":\"b\""));
    EXPECT_EQ(find_int_value(out, "invocations"), 7);
}
