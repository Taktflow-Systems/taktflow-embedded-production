// Copyright 2026 Taktflow.

#pragma once

#include <rtScheduling/Registry.h>

namespace demo
{

class SafetySystem;
class ControlSystem;
class TelemetrySystem;

/// Single entry point called by the host application (e.g. inside
/// `app::startApp()` at the appropriate lifecycle level). Runs:
///   1. bridge install
///   2. feasibility analysis (at runtime; the compile-time gate has already
///      run in manifest.cpp via static_assert)
///   3. one `async::scheduleAtFixedRate` per entity
///
/// Returns a diagnostic code. In an ASIL-D context the caller should treat
/// anything other than Ok as a hard failure.
rtSched::Registry::ArmResult bootstrap_schedule(
    SafetySystem&    safety,
    ControlSystem&   control,
    TelemetrySystem& telemetry);

} // namespace demo
