// Copyright 2026 Taktflow.

#include "rtScheduling/Registry.h"

#include "rtScheduling/Feasibility.h"

namespace rtSched
{

Registry& Registry::instance() noexcept
{
    static Registry inst;
    return inst;
}

bool Registry::add(Binding const& b) noexcept
{
    if (_count >= Capacity) { return false; }
    _bindings[_count++] = b;
    return true;
}

std::size_t Registry::collect_decls(
    EntityDecl* out, std::size_t max) const noexcept
{
    std::size_t const n = _count < max ? _count : max;
    for (std::size_t i = 0; i < n; ++i)
    {
        out[i] = *_bindings[i].decl;
    }
    return n;
}

void Registry::reset_for_test() noexcept
{
    for (auto& b : _bindings) { b = {}; }
    _count  = 0;
    _bridge = nullptr;
    _last   = ArmResult::NotArmed;
}

Registry::ArmResult Registry::arm() noexcept
{
    if (_count > Capacity)
    {
        _last = ArmResult::CapacityExceeded;
        return _last;
    }

    // Feasibility analysis — build a local decl view and run analyze().
    // We stash decls on the stack; capacity is small enough (64) that this
    // is cheap. No heap allocation.
    std::array<EntityDecl, Capacity> decls{};
    for (std::size_t i = 0; i < _count; ++i) { decls[i] = *_bindings[i].decl; }
    DeclView const v{decls.data(), _count};

    // Use exact RTA instead of the sufficient bounds — we want the arm-time
    // gate to be as tight as possible for ASIL-D usage. The sufficient tests
    // (L&L, hyperbolic) remain available via analyze() for fast preview.
    switch (analyze_with_rta(v))
    {
    case Verdict::EntityInsane:
        _last = ArmResult::InfeasibleSanity;
        return _last;
    case Verdict::DeadlineImplausible:
        _last = ArmResult::InfeasibleDeadline;
        return _last;
    case Verdict::OverUtilized:
        // analyze_with_rta never returns this; handled for completeness.
        _last = ArmResult::InfeasibleOverUtilized;
        return _last;
    case Verdict::RtaDeadlineMiss:
        _last = ArmResult::InfeasibleRtaMiss;
        return _last;
    case Verdict::Ok:
        break;
    }

    if (_bridge == nullptr)
    {
        _last = ArmResult::BridgeMissing;
        return _last;
    }

    for (std::size_t i = 0; i < _count; ++i)
    {
        if (!_bridge(_bindings[i]))
        {
            _last = ArmResult::BridgeFailed;
            return _last;
        }
    }

    _last = ArmResult::Ok;
    return _last;
}

} // namespace rtSched
