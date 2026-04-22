// Copyright 2026 Taktflow.

/**
 * \file
 * \ingroup rtScheduling
 *
 * The only header in this library that depends on openbsw's `async` module.
 * Including it from app code both:
 *   (a) adapts Registry::Binding pointers to `async::scheduleAtFixedRate`,
 *   (b) installs the bridge on the Registry singleton via a static init.
 *
 * Keep this header out of unit tests — tests supply a mock via
 * `Registry::set_bridge` directly.
 */
#pragma once

#include "rtScheduling/Registry.h"
#include "rtScheduling/Types.h"

#include <async/Async.h>
#include <async/Types.h>

namespace rtSched
{
namespace bridge
{

/// Concrete arm callback that maps a Binding to async::scheduleAtFixedRate.
/// Returns true on success. Failure is currently only reachable if the
/// bridge has a null runnable/timeout pointer — which would mean the app
/// mis-registered the binding.
inline bool arm_one(Binding const& b) noexcept
{
    if (b.decl == nullptr || b.runnable_ptr == nullptr || b.timeout_ptr == nullptr)
    {
        return false;
    }

    auto& runnable = *static_cast<async::RunnableType*>(b.runnable_ptr);
    auto& timeout  = *static_cast<async::TimeoutType*>(b.timeout_ptr);

    async::scheduleAtFixedRate(
        static_cast<async::ContextType>(b.decl->context),
        runnable,
        timeout,
        b.decl->timing.cycle_us,
        async::TimeUnit::MICROSECONDS);
    return true;
}

/// Install the bridge on the global Registry. Call this exactly once before
/// Registry::instance().arm(). Safe to call multiple times (idempotent).
inline void install() noexcept
{
    Registry::instance().set_bridge(&arm_one);
}

/// One-shot static installer — declare a single
///   static const rtSched::bridge::Installer _install;
/// in the app's translation unit to get the bridge wired before main().
struct Installer
{
    Installer() noexcept { install(); }
};

} // namespace bridge
} // namespace rtSched
