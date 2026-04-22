// Copyright 2026 Taktflow.

#include "demo/ControlSystem.h"

namespace demo
{

ControlSystem::ControlSystem(::async::ContextType context) : _context(context) {}

void ControlSystem::execute() { ++_tick_count; }

} // namespace demo
