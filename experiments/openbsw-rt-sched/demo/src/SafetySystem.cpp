// Copyright 2026 Taktflow.

#include "demo/SafetySystem.h"

namespace demo
{

SafetySystem::SafetySystem(::async::ContextType context) : _context(context) {}

void SafetySystem::execute()
{
    // Real safety cyclic work goes here. For the demo we just tick a counter
    // so a supervisor can observe the heartbeat.
    ++_tick_count;
}

} // namespace demo
