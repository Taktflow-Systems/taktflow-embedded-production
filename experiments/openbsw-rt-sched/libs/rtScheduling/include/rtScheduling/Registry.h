// Copyright 2026 Taktflow.

/**
 * \file
 * \ingroup rtScheduling
 *
 * The Registry is the single gateway through which periodic activities enter
 * the runtime. It replaces the scattered `async::scheduleAtFixedRate(...)`
 * calls that issue #370 flags as unobservable.
 *
 * Two usage modes are supported:
 *
 *   1. Static manifest (preferred for ASIL contexts):
 *      The app provides a compile-time `std::array<EntityDecl, N>`. Every
 *      feasibility check is run inside `static_assert`. The Registry merely
 *      walks the manifest at startup and invokes the async bridge.
 *
 *   2. Dynamic registration (for iterative development):
 *      Each SchedulableEntity registers itself at static-init time via a
 *      Binder RAII object. The Registry assembles the list at runtime and
 *      runs feasibility before arming.
 *
 * Only `arm()` calls the async layer; every other method is pure.
 */
#pragma once

#include "rtScheduling/Feasibility.h"
#include "rtScheduling/Types.h"

#include <array>
#include <cstddef>

namespace rtSched
{

/// Runtime binding: a declaration plus pointers to the user-owned runnable
/// object and timeout storage. The rtScheduling library never allocates
/// these; the app owns the lifetime.
struct Binding
{
    EntityDecl const* decl;
    void*             runnable_ptr; ///< erased ptr to async::RunnableType
    void*             timeout_ptr;  ///< erased ptr to async::TimeoutType
    EntityStats*      stats;        ///< optional stats sink (may be nullptr)
};

/// The bridge function type. The async-bridge header supplies a concrete
/// implementation; tests supply a mock. Erased via function pointer so the
/// core Registry has no async dependency.
using ArmBridge = bool (*)(Binding const&);

/// Default capacity for the dynamic-registration path. Compile-time
/// configurable by defining RT_SCHED_MAX_ENTITIES before including.
#ifndef RT_SCHED_MAX_ENTITIES
#define RT_SCHED_MAX_ENTITIES 64U
#endif

class Registry
{
public:
    static constexpr std::size_t Capacity = RT_SCHED_MAX_ENTITIES;

    enum class ArmResult : std::uint8_t
    {
        NotArmed,
        Ok,
        CapacityExceeded,
        InfeasibleOverUtilized, ///< sufficient test only — not used by default path
        InfeasibleDeadline,     ///< C > D (entity-level implausibility)
        InfeasibleSanity,       ///< cycle/wcet/phase invariant violation
        InfeasibleRtaMiss,      ///< exact RTA found R_i > D_i (ASIL-D gate)
        BridgeFailed,
        BridgeMissing,
    };

    static Registry& instance() noexcept;

    /// Install the async bridge. Typically called once from the
    /// async-bridge header's init function; tests pass a mock.
    void set_bridge(ArmBridge bridge) noexcept { _bridge = bridge; }

    /// Register a single binding. Safe to call during static init; callers
    /// must ensure Binding outlives the Registry (trivial for static storage).
    /// Returns false only if capacity is exceeded — a design error.
    bool add(Binding const& b) noexcept;

    /// Arm the entire manifest. Runs feasibility first; returns a
    /// diagnostic code. On Ok, every entity is now live on its context.
    ArmResult arm() noexcept;

    /// Read-only view of the registered bindings. Ordered by insertion.
    /// Intended for introspection (logging, console commands, audit dumps).
    std::size_t size() const noexcept { return _count; }
    Binding const& at(std::size_t i) const noexcept { return _bindings[i]; }

    /// View of all EntityDecls for pure analysis (no runtime pointers).
    /// Fills `out` with up to N entries and returns the count written.
    std::size_t collect_decls(
        EntityDecl* out, std::size_t max) const noexcept;

    /// Current arm state. Useful for console commands.
    ArmResult last_result() const noexcept { return _last; }

    /// Test-only: wipe state so RegistryTest can run multiple scenarios.
    /// NOT thread-safe; not for production use.
    void reset_for_test() noexcept;

private:
    Registry() = default;

    std::array<Binding, Capacity> _bindings{};
    std::size_t                   _count  = 0;
    ArmBridge                     _bridge = nullptr;
    ArmResult                     _last   = ArmResult::NotArmed;
};

// ---------------------------------------------------------------------------
// RAII binder — the convenience path for the dynamic-registration mode.
//
// Declare one of these next to a runnable and it self-registers:
//
//     class MyWork : public async::IRunnable { void execute() override; };
//     static MyWork mywork;
//     static async::TimeoutType mywork_timeout;
//     static constexpr rtSched::EntityDecl mywork_decl{
//         "MyWork", TASK_DEMO, {10'000, 1'500}, rtSched::Criticality::ASIL_B
//     };
//     static rtSched::EntityStats mywork_stats;
//     static rtSched::Binder mywork_binder{
//         mywork_decl, mywork, mywork_timeout, &mywork_stats
//     };
//
// For the static-manifest mode, skip Binder and call `arm_manifest()`
// directly with a `std::array<Binding, N>`.
template<class Runnable, class Timeout>
class Binder
{
public:
    Binder(EntityDecl const& decl,
           Runnable&         runnable,
           Timeout&          timeout,
           EntityStats*      stats = nullptr) noexcept
    {
        Binding b{&decl, &runnable, &timeout, stats};
        Registry::instance().add(b);
    }

    Binder(Binder const&)            = delete;
    Binder& operator=(Binder const&) = delete;
    Binder(Binder&&)                 = delete;
    Binder& operator=(Binder&&)      = delete;
};

// Deduction guide so users don't have to spell the template args.
template<class R, class T>
Binder(EntityDecl const&, R&, T&) -> Binder<R, T>;

template<class R, class T>
Binder(EntityDecl const&, R&, T&, EntityStats*) -> Binder<R, T>;

// ---------------------------------------------------------------------------
// Static-manifest helper. Call once at startup, typically from lifecycle
// level 0 before any runnable has been enabled. Equivalent to:
//    for (auto const& b : manifest) Registry::instance().add(b);
//    return Registry::instance().arm();
template<std::size_t N>
inline Registry::ArmResult arm_manifest(
    std::array<Binding, N> const& manifest) noexcept
{
    auto& r = Registry::instance();
    for (auto const& b : manifest) { (void)r.add(b); }
    return r.arm();
}

} // namespace rtSched
