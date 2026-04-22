// Copyright 2026 Taktflow.

#include "demo/TelemetrySystem.h"

namespace demo
{

TelemetrySystem::TelemetrySystem(::async::ContextType context) : _context(context) {}

void TelemetrySystem::execute() { ++_tick_count; }

} // namespace demo
