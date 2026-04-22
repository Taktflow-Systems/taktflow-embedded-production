// Copyright 2026 Taktflow.

#include "rtScheduling/MonitoredRunnable.h"

#include <gtest/gtest.h>

using namespace rtSched;

namespace
{

// Fake clock with controllable return values. Global because NowFn is a
// function pointer; we keep a static sequence.
struct FakeClock
{
    static std::uint32_t values[8];
    static std::size_t   idx;
    static std::uint32_t now() { return values[idx++]; }
    static void reset(std::initializer_list<std::uint32_t> vs)
    {
        idx = 0;
        std::size_t i = 0;
        for (auto v : vs) { if (i < 8) values[i++] = v; }
    }
};
std::uint32_t FakeClock::values[8] = {};
std::size_t   FakeClock::idx       = 0;

struct InnerRunnable
{
    int ticks = 0;
    void execute() { ++ticks; }
};

struct OverrunCapture
{
    EntityDecl const* last_decl     = nullptr;
    Duration          last_observed = 0;
    int               count         = 0;
};
OverrunCapture* g_cap = nullptr;
void capture_overrun(EntityDecl const& d, Duration obs)
{
    g_cap->last_decl = &d; g_cap->last_observed = obs; ++g_cap->count;
}

} // namespace

TEST(MonitoredRunnable, UnderBudget_DoesNotFlagOverrun)
{
    static constexpr EntityDecl d{"x", 1, {1000, 500}, Criticality::QM};
    InnerRunnable      inner;
    EntityStats        stats;
    OverrunCapture     cap;
    g_cap = &cap;

    FakeClock::reset({100, 300}); // exec = 200us, under 500us WCET
    MonitoredRunnable<InnerRunnable> wrapper{
        inner, d, &stats, &FakeClock::now, &capture_overrun};

    wrapper.execute();
    EXPECT_EQ(inner.ticks, 1);
    EXPECT_EQ(stats.invocations, 1u);
    EXPECT_EQ(stats.overruns, 0u);
    EXPECT_EQ(stats.last_exec_us, 200u);
    EXPECT_EQ(stats.max_observed_us, 200u);
    EXPECT_EQ(cap.count, 0);
}

TEST(MonitoredRunnable, OverBudget_FlagsOverrunAndInvokesHandler)
{
    static constexpr EntityDecl d{"x", 1, {1000, 500}, Criticality::ASIL_D};
    InnerRunnable      inner;
    EntityStats        stats;
    OverrunCapture     cap;
    g_cap = &cap;

    FakeClock::reset({1000, 1800}); // exec = 800us, over 500us WCET
    MonitoredRunnable<InnerRunnable> wrapper{
        inner, d, &stats, &FakeClock::now, &capture_overrun};

    wrapper.execute();
    EXPECT_EQ(stats.invocations, 1u);
    EXPECT_EQ(stats.overruns, 1u);
    EXPECT_EQ(stats.last_exec_us, 800u);
    EXPECT_EQ(cap.count, 1);
    EXPECT_EQ(cap.last_decl, &d);
    EXPECT_EQ(cap.last_observed, 800u);
}

TEST(MonitoredRunnable, MinMax_TracksExtremesAcrossInvocations)
{
    static constexpr EntityDecl d{"x", 1, {1000, 900}, Criticality::QM};
    InnerRunnable      inner;
    EntityStats        stats;
    g_cap = nullptr;

    FakeClock::reset({0, 100, 100, 600, 600, 800});
    // exec1=100, exec2=500, exec3=200
    MonitoredRunnable<InnerRunnable> wrapper{
        inner, d, &stats, &FakeClock::now, nullptr};

    wrapper.execute();
    wrapper.execute();
    wrapper.execute();
    EXPECT_EQ(stats.invocations, 3u);
    EXPECT_EQ(stats.min_observed_us, 100u);
    EXPECT_EQ(stats.max_observed_us, 500u);
    EXPECT_EQ(stats.overruns, 0u);
}
