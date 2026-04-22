// Copyright 2026 Taktflow.

/**
 * \file
 *
 * THE WHOLE POINT OF ISSUE #370 — in one file.
 *
 * This is the single location where every periodic activity in the system is
 * declared. An auditor, a static analyser, and a future maintainer can all
 * read this file and know exactly what the schedule is. No hunting through
 * individual system `run()` methods to grep out `scheduleAtFixedRate` calls.
 *
 * Feasibility is validated at compile time via the static_assert below; any
 * change that pushes a task context over its rate-monotonic bound fails the
 * build, not the deployment.
 */

#include "demo/ControlSystem.h"
#include "demo/SafetySystem.h"
#include "demo/TelemetrySystem.h"
#include "demo/manifest.h"

#include <rtScheduling/AsyncBridge.h>
#include <rtScheduling/Feasibility.h>
#include <rtScheduling/Registry.h>

#include <array>

namespace demo
{

// ---- Task assignments (must match async::Config.h in the host app) -------
// These IDs mirror the referenceApp's enum. In a real project they'd be
// pulled in from <async/Config.h>. For the demo we hard-code them so the
// manifest reads like a spec.
constexpr rtSched::ContextId TASK_SAFETY     = 8;
constexpr rtSched::ContextId TASK_BACKGROUND = 1;
constexpr rtSched::ContextId TASK_SYSADMIN   = 7;

// ---- Entity declarations --------------------------------------------------
// Each decl names the entity, assigns a context, and states its timing
// contract. Criticality drives the overrun policy.

constexpr rtSched::EntityDecl DECL_SAFETY{
    /*name*/        "SafetySystem",
    /*context*/     TASK_SAFETY,
    /*timing*/      {/*cycle_us*/ 10'000, /*wcet_us*/ 1'500, /*deadline*/ 10'000, /*phase*/ 0},
    /*criticality*/ rtSched::Criticality::ASIL_D,
    /*overrun*/     rtSched::OverrunPolicy::Safety,
};

constexpr rtSched::EntityDecl DECL_CONTROL{
    /*name*/        "ControlSystem",
    /*context*/     TASK_BACKGROUND,
    /*timing*/      {/*cycle_us*/ 20'000, /*wcet_us*/ 3'000, /*deadline*/ 20'000, /*phase*/ 1'000},
    /*criticality*/ rtSched::Criticality::ASIL_B,
    /*overrun*/     rtSched::OverrunPolicy::LogAndCount,
};

constexpr rtSched::EntityDecl DECL_TELEMETRY{
    /*name*/        "TelemetrySystem",
    /*context*/     TASK_SYSADMIN,
    /*timing*/      {/*cycle_us*/ 100'000, /*wcet_us*/ 5'000, /*deadline*/ 100'000, /*phase*/ 0},
    /*criticality*/ rtSched::Criticality::QM,
    /*overrun*/     rtSched::OverrunPolicy::LogAndCount,
};

constexpr std::array<rtSched::EntityDecl, 3> SCHEDULE_DECLS{{
    DECL_SAFETY,
    DECL_CONTROL,
    DECL_TELEMETRY,
}};

// ---- Compile-time feasibility gate ----------------------------------------
// If any future edit pushes a context over its RM bound or violates sanity,
// the build fails here — before any binary is produced.
static_assert(rtSched::analyze(rtSched::view(SCHEDULE_DECLS)) == rtSched::Verdict::Ok,
              "demo schedule is infeasible — check WCET budgets");
static_assert(rtSched::hyperperiod_us(rtSched::view(SCHEDULE_DECLS)) == 100'000ULL,
              "demo hyperperiod is 100 ms");

// ---- Runtime stats storage -----------------------------------------------
rtSched::EntityStats g_stats_safety{};
rtSched::EntityStats g_stats_control{};
rtSched::EntityStats g_stats_telemetry{};

// ---- Manifest: decls + runnable/timeout bindings -------------------------
// The bindings are initialized at runtime (references to global objects are
// not constexpr), but the shape is one array, one file, one point of truth.
std::array<rtSched::Binding, 3> make_manifest(
    SafetySystem&    safety,
    ControlSystem&   control,
    TelemetrySystem& telemetry)
{
    return {{
        {&DECL_SAFETY,    &safety,    &safety.timeout(),    &g_stats_safety},
        {&DECL_CONTROL,   &control,   &control.timeout(),   &g_stats_control},
        {&DECL_TELEMETRY, &telemetry, &telemetry.timeout(), &g_stats_telemetry},
    }};
}

// ---- Bootstrap ------------------------------------------------------------
// Call once from app::startApp() at the right lifecycle level. Replaces
// every scattered `async::scheduleAtFixedRate` call in system `run()` bodies.
rtSched::Registry::ArmResult bootstrap_schedule(
    SafetySystem&    safety,
    ControlSystem&   control,
    TelemetrySystem& telemetry)
{
    rtSched::bridge::install();

    auto manifest = make_manifest(safety, control, telemetry);
    return rtSched::arm_manifest(manifest);
}

} // namespace demo
