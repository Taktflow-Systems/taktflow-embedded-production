// Copyright 2026 Taktflow.

#include "rtScheduling/Json.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace rtSched
{
namespace json
{

namespace
{

// Tiny buffered writer. The same logic drives both the std::string path
// (append to buffer then move) and the chunked path (flush buffer through
// the emit callback). Keeps the formatting code single-source.
struct Writer
{
    static constexpr std::size_t BUF_SIZE = 256;

    explicit Writer(std::string& sink) : _sink(&sink), _emit(nullptr), _user(nullptr) {}
    Writer(EmitFn emit, void* user) : _sink(nullptr), _emit(emit), _user(user) {}

    void write(char const* s, std::size_t n)
    {
        if (_sink) { _sink->append(s, n); return; }
        if (_emit) { _emit(s, n, _user); }
    }

    void write(char const* cstr)
    {
        std::size_t n = 0;
        while (cstr[n] != '\0') { ++n; }
        write(cstr, n);
    }

    void write_u32(std::uint32_t v)
    {
        char buf[16];
        int const n = std::snprintf(buf, sizeof(buf), "%u", v);
        if (n > 0) write(buf, static_cast<std::size_t>(n));
    }

    void write_u64(std::uint64_t v)
    {
        char buf[24];
        int const n = std::snprintf(buf, sizeof(buf), "%llu",
                                    static_cast<unsigned long long>(v));
        if (n > 0) write(buf, static_cast<std::size_t>(n));
    }

    // Escape and quote a string_view. Only \ and " need escaping for
    // ASCII input; we pass through everything else. Control chars in
    // entity names are not expected.
    void write_json_string(std::string_view s)
    {
        write("\"", 1);
        for (char c : s)
        {
            if (c == '"')       { write("\\\"", 2); }
            else if (c == '\\') { write("\\\\", 2); }
            else                { write(&c, 1); }
        }
        write("\"", 1);
    }

    void write_bool(bool b) { write(b ? "true" : "false"); }

private:
    std::string* _sink;
    EmitFn       _emit;
    void*        _user;
};

void write_stats_object(Writer& w, EntityStats const& s)
{
    // Sentinel min_observed_us (0xFFFFFFFF) means "never observed" — emit 0
    // so the GUI does not have to special-case a magic number.
    std::uint32_t const min_obs = (s.min_observed_us == 0xFFFFFFFFu)
                                  ? 0u : s.min_observed_us;
    w.write("{\"invocations\":");      w.write_u32(s.invocations);
    w.write(",\"overruns\":");         w.write_u32(s.overruns);
    w.write(",\"last_exec_us\":");     w.write_u32(s.last_exec_us);
    w.write(",\"max_observed_us\":");  w.write_u32(s.max_observed_us);
    w.write(",\"min_observed_us\":");  w.write_u32(min_obs);
    w.write("}");
}

void write_entity(
    Writer& w, EntityDecl const& e, EntityStats const* s,
    DeclView decls, std::size_t idx)
{
    EntityStats const empty{};
    EntityStats const& eff = s ? *s : empty;

    // Run RTA at JSON-build time. Blocking term left at 0 — callers with a
    // resource-sharing model can plug per-entity blocking once that's wired
    // through the manifest.
    Duration    const R      = rta_response_time_us(decls, idx, 0);
    bool        const rta_ok = (R != RTA_INFEASIBLE);

    w.write("{\"name\":");        w.write_json_string(e.name);
    w.write(",\"context\":");     w.write_u32(e.context);
    w.write(",\"cycle_us\":");    w.write_u32(e.timing.cycle_us);
    w.write(",\"wcet_us\":");     w.write_u32(e.timing.wcet_us);
    w.write(",\"deadline_us\":"); w.write_u32(e.timing.effective_deadline());
    w.write(",\"phase_us\":");    w.write_u32(e.timing.phase_us);
    w.write(",\"criticality\":\""); w.write(criticality_str(e.criticality)); w.write("\"");
    w.write(",\"overrun\":\"");     w.write(overrun_str(e.overrun));         w.write("\"");
    w.write(",\"priority_hint\":"); w.write_u32(e.priority_hint);
    w.write(",\"blocking_us\":");   w.write_u32(e.blocking_us);
    // Exact worst-case response time. When RTA proves a miss, response_time_us
    // is RTA_INFEASIBLE (0xFFFFFFFF) and rta_feasible is false — the GUI
    // treats this as a deadline-miss indicator without having to recompute.
    w.write(",\"response_time_us\":"); w.write_u32(R);
    w.write(",\"rta_feasible\":");     w.write_bool(rta_ok);
    w.write(",\"stats\":");       write_stats_object(w, eff);
    w.write("}");
}

// Collect distinct contexts in insertion order. 64 contexts is more than
// any real openbsw application will use — TASK_COUNT in the reference app
// is 8.
struct ContextList
{
    static constexpr std::size_t CAP = 64;
    std::array<ContextId, CAP> items{};
    std::size_t                n = 0;

    void add_if_new(ContextId c) noexcept
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            if (items[i] == c) return;
        }
        if (n < CAP) { items[n++] = c; }
    }
};

ContextList collect_contexts(DeclView decls)
{
    ContextList cl;
    for (std::size_t i = 0; i < decls.length(); ++i)
    {
        cl.add_if_new(decls[i].context);
    }
    return cl;
}

void write_context_block(Writer& w, DeclView decls, ContextId ctx)
{
    std::size_t const   n_tasks   = count_on_context(decls, ctx);
    std::uint32_t const util_ppm  = utilization_ppm(decls, ctx);
    std::uint32_t const ll_bound  = rm_bound_ppm(n_tasks);
    std::uint64_t const hyp_prod  = hyperbolic_product_ppm(decls, ctx);
    bool const          ll_pass   = util_ppm <= ll_bound;
    bool const          hyp_pass  = hyp_prod <= 2'000'000ULL;
    bool const          rta_ok    = rta_pass(decls, ctx);
    CriticalityUtilPpm const b    = utilization_by_criticality_ppm(decls, ctx);
    bool const          mixed     = context_is_mixed_criticality(decls, ctx);

    w.write("{\"id\":");                    w.write_u32(ctx);
    w.write(",\"n_tasks\":");               w.write_u32(static_cast<std::uint32_t>(n_tasks));
    w.write(",\"utilization_ppm\":");       w.write_u32(util_ppm);
    w.write(",\"ll_bound_ppm\":");          w.write_u32(ll_bound);
    w.write(",\"ll_pass\":");               w.write_bool(ll_pass);
    w.write(",\"hyperbolic_product_ppm\":"); w.write_u64(hyp_prod);
    w.write(",\"hyperbolic_pass\":");       w.write_bool(hyp_pass);
    // rta_pass is the authoritative per-context verdict (exact test).
    w.write(",\"rta_pass\":");              w.write_bool(rta_ok);
    w.write(",\"mixed_criticality\":");     w.write_bool(mixed);
    w.write(",\"criticality_util_ppm\":{");
    w.write("\"qm\":");     w.write_u32(b.qm_ppm);
    w.write(",\"asil_a\":"); w.write_u32(b.asil_a_ppm);
    w.write(",\"asil_b\":"); w.write_u32(b.asil_b_ppm);
    w.write(",\"asil_c\":"); w.write_u32(b.asil_c_ppm);
    w.write(",\"asil_d\":"); w.write_u32(b.asil_d_ppm);
    w.write("}}");
}

void write_report_core(Writer& w, DeclView decls, EntityStats const* stats)
{
    // Top-level verdict is the EXACT RTA-based result — matches what
    // Registry::arm() checks, so the GUI verdict pill and the firmware
    // arm outcome always agree.
    w.write("{\"schema\":\""); w.write(SCHEMA_TAG); w.write("\"");
    w.write(",\"verdict\":\""); w.write(verdict_str(analyze_with_rta(decls))); w.write("\"");
    // Sufficient-test verdict is also exposed for pedagogical display —
    // the GUI shows both so the reader sees when RTA saves a schedule that
    // L&L would reject.
    w.write(",\"verdict_sufficient\":\""); w.write(verdict_str(analyze(decls))); w.write("\"");
    w.write(",\"hyperperiod_us\":"); w.write_u64(hyperperiod_us(decls));

    w.write(",\"entities\":[");
    for (std::size_t i = 0; i < decls.length(); ++i)
    {
        if (i > 0) w.write(",");
        write_entity(w, decls[i], stats ? &stats[i] : nullptr, decls, i);
    }
    w.write("]");

    ContextList const cl = collect_contexts(decls);
    w.write(",\"contexts\":[");
    for (std::size_t i = 0; i < cl.n; ++i)
    {
        if (i > 0) w.write(",");
        write_context_block(w, decls, cl.items[i]);
    }
    w.write("]}");
}

} // namespace

// ---- Public entry points -------------------------------------------------

void write_report(
    DeclView                 decls,
    EntityStats const* const stats,
    std::string&             out)
{
    Writer w{out};
    write_report_core(w, decls, stats);
}

void write_report_chunked(
    DeclView                 decls,
    EntityStats const* const stats,
    EmitFn                   emit,
    void*                    user)
{
    Writer w{emit, user};
    write_report_core(w, decls, stats);
}

void write_report_from_registry(std::string& out)
{
    auto&                   reg = Registry::instance();
    std::array<EntityDecl, Registry::Capacity>    decls_storage{};
    std::array<EntityStats, Registry::Capacity>   stats_storage{};
    std::size_t const n = reg.collect_decls(decls_storage.data(), Registry::Capacity);

    // Copy current stats snapshots. A stats pointer of nullptr in a binding
    // leaves the corresponding stats_storage entry at its default-constructed
    // zero state.
    for (std::size_t i = 0; i < n; ++i)
    {
        if (reg.at(i).stats != nullptr) { stats_storage[i] = *reg.at(i).stats; }
    }

    Writer w{out};
    DeclView const v{decls_storage.data(), n};
    write_report_core(w, v, stats_storage.data());
}

void write_report_from_registry_chunked(EmitFn emit, void* user)
{
    auto&                   reg = Registry::instance();
    std::array<EntityDecl, Registry::Capacity>    decls_storage{};
    std::array<EntityStats, Registry::Capacity>   stats_storage{};
    std::size_t const n = reg.collect_decls(decls_storage.data(), Registry::Capacity);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (reg.at(i).stats != nullptr) { stats_storage[i] = *reg.at(i).stats; }
    }

    Writer w{emit, user};
    DeclView const v{decls_storage.data(), n};
    write_report_core(w, v, stats_storage.data());
}

// ---- Stringifiers --------------------------------------------------------

char const* criticality_str(Criticality c) noexcept
{
    switch (c)
    {
    case Criticality::QM:     return "qm";
    case Criticality::ASIL_A: return "asil_a";
    case Criticality::ASIL_B: return "asil_b";
    case Criticality::ASIL_C: return "asil_c";
    case Criticality::ASIL_D: return "asil_d";
    }
    return "unknown";
}

char const* overrun_str(OverrunPolicy p) noexcept
{
    switch (p)
    {
    case OverrunPolicy::Ignore:      return "ignore";
    case OverrunPolicy::LogAndCount: return "log_count";
    case OverrunPolicy::Safety:      return "safety";
    }
    return "unknown";
}

char const* verdict_str(Verdict v) noexcept
{
    switch (v)
    {
    case Verdict::Ok:                  return "ok";
    case Verdict::EntityInsane:        return "entity_insane";
    case Verdict::OverUtilized:        return "over_utilized";
    case Verdict::DeadlineImplausible: return "deadline_implausible";
    case Verdict::RtaDeadlineMiss:     return "rta_deadline_miss";
    }
    return "unknown";
}

} // namespace json
} // namespace rtSched
