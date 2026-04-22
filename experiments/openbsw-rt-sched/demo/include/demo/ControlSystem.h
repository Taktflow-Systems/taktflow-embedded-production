// Copyright 2026 Taktflow.

#pragma once

#include <async/IRunnable.h>
#include <async/Types.h>

namespace demo
{

/// A slower control loop. In the before version this scheduled itself in
/// `run()`; in the after version the declaration lives in manifest.cpp.
class ControlSystem : public ::async::IRunnable
{
public:
    explicit ControlSystem(::async::ContextType context);
    void execute() override;

    ::async::TimeoutType& timeout() { return _timeout; }
    ::async::ContextType  context() const { return _context; }

private:
    ::async::ContextType _context;
    ::async::TimeoutType _timeout{};
    std::uint32_t        _tick_count{0};
};

} // namespace demo
