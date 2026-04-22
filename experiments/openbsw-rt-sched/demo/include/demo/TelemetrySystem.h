// Copyright 2026 Taktflow.

#pragma once

#include <async/IRunnable.h>
#include <async/Types.h>

namespace demo
{

/// Low-criticality background telemetry. Its long cycle dominates the
/// hyperperiod, so analysis tooling picks this out when sizing trace buffers.
class TelemetrySystem : public ::async::IRunnable
{
public:
    explicit TelemetrySystem(::async::ContextType context);
    void execute() override;

    ::async::TimeoutType& timeout() { return _timeout; }
    ::async::ContextType  context() const { return _context; }

private:
    ::async::ContextType _context;
    ::async::TimeoutType _timeout{};
    std::uint32_t        _tick_count{0};
};

} // namespace demo
