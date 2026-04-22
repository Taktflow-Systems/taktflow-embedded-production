// Copyright 2026 Taktflow.

#include "rtScheduling/Registry.h"

#include <gtest/gtest.h>

using namespace rtSched;

namespace
{

struct FakeRunnable
{
    int ticks = 0;
    void execute() { ++ticks; }
};

struct FakeTimeout
{
    bool armed = false;
};

// Mock bridge: records which bindings were armed. We cannot use globals
// across tests safely because gtest runs tests in-process, so we reset on
// each setup via Registry::reset_for_test().
struct BridgeTrace
{
    std::vector<EntityDecl const*> armed;
};

BridgeTrace* g_trace = nullptr;

bool mock_bridge(Binding const& b) { g_trace->armed.push_back(b.decl); return true; }
bool failing_bridge(Binding const&) { return false; }

class RegistryFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Registry::instance().reset_for_test();
        _trace   = {};
        g_trace  = &_trace;
    }

    BridgeTrace _trace;
};

} // namespace

TEST_F(RegistryFixture, AddAndArm_EmptyManifest_IsOk)
{
    Registry::instance().set_bridge(&mock_bridge);
    EXPECT_EQ(Registry::instance().arm(), Registry::ArmResult::Ok);
    EXPECT_TRUE(_trace.armed.empty());
}

TEST_F(RegistryFixture, AddAndArm_MissingBridge_Fails)
{
    static constexpr EntityDecl d{"e", 1, {1000, 100}, Criticality::QM};
    FakeRunnable r;
    FakeTimeout  t;
    Binding      b{&d, &r, &t, nullptr};

    EXPECT_TRUE(Registry::instance().add(b));
    EXPECT_EQ(Registry::instance().arm(), Registry::ArmResult::BridgeMissing);
}

TEST_F(RegistryFixture, AddAndArm_Feasible_CallsBridgeOnce)
{
    static constexpr EntityDecl d1{"a", 1, {1000, 100}, Criticality::QM};
    static constexpr EntityDecl d2{"b", 2, {5000, 200}, Criticality::QM};
    FakeRunnable r1, r2;
    FakeTimeout  t1, t2;
    Binding      b1{&d1, &r1, &t1, nullptr};
    Binding      b2{&d2, &r2, &t2, nullptr};

    Registry::instance().set_bridge(&mock_bridge);
    EXPECT_TRUE(Registry::instance().add(b1));
    EXPECT_TRUE(Registry::instance().add(b2));
    EXPECT_EQ(Registry::instance().arm(), Registry::ArmResult::Ok);
    ASSERT_EQ(_trace.armed.size(), 2u);
    EXPECT_EQ(_trace.armed[0], &d1);
    EXPECT_EQ(_trace.armed[1], &d2);
}

TEST_F(RegistryFixture, AddAndArm_Infeasible_DoesNotArmAnything)
{
    // Both entities on same context, 70% utilization each — sum 140%.
    // Registry uses exact RTA (ASIL-D gate), so the result is the RTA
    // miss code, not the sufficient-test OverUtilized code.
    static constexpr EntityDecl dA{"heavyA", 1, {1000, 700}, Criticality::QM};
    static constexpr EntityDecl dB{"heavyB", 1, {1000, 700}, Criticality::QM};
    FakeRunnable r1, r2;
    FakeTimeout  t1, t2;
    Binding      b1{&dA, &r1, &t1, nullptr};
    Binding      b2{&dB, &r2, &t2, nullptr};

    Registry::instance().set_bridge(&mock_bridge);
    Registry::instance().add(b1);
    Registry::instance().add(b2);
    EXPECT_EQ(Registry::instance().arm(),
              Registry::ArmResult::InfeasibleRtaMiss);
    EXPECT_TRUE(_trace.armed.empty());
}

TEST_F(RegistryFixture, AddAndArm_BridgeFailurePropagates)
{
    static constexpr EntityDecl d{"e", 1, {1000, 100}, Criticality::QM};
    FakeRunnable r;
    FakeTimeout  t;
    Binding      b{&d, &r, &t, nullptr};
    Registry::instance().set_bridge(&failing_bridge);
    Registry::instance().add(b);
    EXPECT_EQ(Registry::instance().arm(), Registry::ArmResult::BridgeFailed);
}

TEST_F(RegistryFixture, CollectDecls_ReturnsRegistered)
{
    static constexpr EntityDecl d1{"a", 1, {1000, 100}, Criticality::ASIL_B};
    static constexpr EntityDecl d2{"b", 2, {5000, 200}, Criticality::QM};
    FakeRunnable r1, r2;
    FakeTimeout  t1, t2;
    Binding      b1{&d1, &r1, &t1, nullptr};
    Binding      b2{&d2, &r2, &t2, nullptr};
    Registry::instance().add(b1);
    Registry::instance().add(b2);

    EntityDecl out[4]{};
    std::size_t const n = Registry::instance().collect_decls(out, 4);
    EXPECT_EQ(n, 2u);
    EXPECT_EQ(out[0].name, std::string_view{"a"});
    EXPECT_EQ(out[1].name, std::string_view{"b"});
    EXPECT_EQ(out[0].criticality, Criticality::ASIL_B);
}

TEST_F(RegistryFixture, ArmManifest_StaticArrayPath)
{
    static constexpr EntityDecl dA{"a", 1, {10'000, 1'000}, Criticality::ASIL_D};
    static constexpr EntityDecl dB{"b", 2, {20'000, 3'000}, Criticality::ASIL_B};
    FakeRunnable rA, rB;
    FakeTimeout  tA, tB;
    Registry::instance().set_bridge(&mock_bridge);

    std::array<Binding, 2> manifest{{
        {&dA, &rA, &tA, nullptr},
        {&dB, &rB, &tB, nullptr},
    }};
    EXPECT_EQ(arm_manifest(manifest), Registry::ArmResult::Ok);
    EXPECT_EQ(_trace.armed.size(), 2u);
}
