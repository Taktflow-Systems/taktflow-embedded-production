// Copyright 2026 Taktflow.

/**
 * \file
 * \ingroup rtScheduling
 *
 * MonitoredRunnable wraps a user-supplied IRunnable-shaped object and
 * measures its execute() latency. On WCET overrun the configured policy
 * fires (log / safety-trip). This is the piece that turns a declared WCET
 * from a comment into a runtime invariant.
 *
 * The wrapper is header-only and templated on the inner runnable type so
 * the async dependency stays optional — for tests we instantiate it over
 * a mock runnable.
 */
#pragma once

#include "rtScheduling/Types.h"

#include <cstdint>

namespace rtSched
{

/// Clock source abstraction. Default points to a function supplied by the
/// platform (openbsw's getSystemTimeUs32Bit() on real targets, a mock in
/// tests). We take this as a function pointer rather than a template
/// parameter so that replacing the clock at runtime is cheap.
using NowFn = std::uint32_t (*)();

/// Called when exec_us > wcet_us. Defaults to nullptr (silent, counter only).
/// Policy dispatch lives in the app — we just inform.
using OverrunHandler = void (*)(EntityDecl const&, Duration observed_us);

template<class Inner>
class MonitoredRunnable
{
public:
    /// \param inner   The user's runnable. Must stay alive for the lifetime
    ///                of the MonitoredRunnable.
    /// \param decl    The declaration this runnable is bound to. Used for
    ///                the WCET threshold and for overrun reporting.
    /// \param stats   Where to accumulate stats. May be nullptr.
    /// \param now     Clock source.
    /// \param onOver  Called on WCET overrun. May be nullptr.
    constexpr MonitoredRunnable(
        Inner&            inner,
        EntityDecl const& decl,
        EntityStats*      stats,
        NowFn             now,
        OverrunHandler    onOver = nullptr) noexcept
    : _inner{inner}, _decl{decl}, _stats{stats}, _now{now}, _onOver{onOver}
    {
    }

    /// Shape-matches async::IRunnable::execute(). Not virtual — the user
    /// passes *this to scheduleAtFixedRate only via an adapter that knows
    /// the type (see AsyncBridge.h).
    void execute() noexcept
    {
        std::uint32_t const t0 = _now ? _now() : 0U;
        _inner.execute();
        std::uint32_t const t1 = _now ? _now() : 0U;

        Duration const dt = t1 - t0; // wraparound-safe for 32-bit us counter

        if (_stats)
        {
            ++_stats->invocations;
            _stats->last_exec_us = dt;
            if (dt > _stats->max_observed_us) _stats->max_observed_us = dt;
            if (dt < _stats->min_observed_us) _stats->min_observed_us = dt;
        }

        if (dt > _decl.timing.wcet_us)
        {
            if (_stats) ++_stats->overruns;
            if (_onOver) _onOver(_decl, dt);
        }
    }

    EntityDecl const& decl()  const noexcept { return _decl; }
    EntityStats*      stats() const noexcept { return _stats; }

private:
    Inner&            _inner;
    EntityDecl const& _decl;
    EntityStats*      _stats;
    NowFn             _now;
    OverrunHandler    _onOver;
};

} // namespace rtSched
