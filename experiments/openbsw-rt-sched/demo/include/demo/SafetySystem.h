// Copyright 2026 Taktflow.

/**
 * \file
 *
 * Minimal shape of a safety-cyclic system, used by the demo to show how
 * the rtScheduling API replaces an in-line `scheduleAtFixedRate` call.
 *
 * Kept deliberately close to the pattern used in openbsw's referenceApp
 * (see `executables/referenceApp/application/src/systems/SafetySystem.cpp`)
 * so the diff between before/after is meaningful.
 */
#pragma once

#include <async/IRunnable.h>
#include <async/Types.h>

namespace demo
{

class SafetySystem : public ::async::IRunnable
{
public:
    explicit SafetySystem(::async::ContextType context);

    /// Cyclic body. Called by the openbsw async executor. In the
    /// rtScheduling world this is wrapped by MonitoredRunnable for WCET
    /// enforcement, but the shape is unchanged.
    void execute() override;

    /// Storage for the async timeout. Owned by SafetySystem so the manifest
    /// can reference it by address at link time.
    ::async::TimeoutType& timeout() { return _timeout; }

    /// Context this system runs on. Kept public so the manifest can fetch
    /// it without the system having to be instantiated before declaration.
    ::async::ContextType context() const { return _context; }

private:
    ::async::ContextType _context;
    ::async::TimeoutType _timeout{};
    std::uint32_t        _tick_count{0};
};

} // namespace demo
