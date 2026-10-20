// Luau for Rudolf OS (1.2.5): run and check Luau programs in the page, as WebAssembly (wasm32-wasi, reactor).
// Built by iso/systools/mkluau.sh from Luau (MIT, github.com/luau-lang/luau). GPL-2.0-or-later (this file).
//   rl_alloc(n) -> buffer; rl_free(p)
//   rl_run(src, n, limit_ms, prelude, pn) -> 0 ok, 1 error, 2 time limit. The prelude (Pilot.lua's stand-in parts,
//     or nothing) runs first with writable globals; then the globals are frozen (luaL_sandbox) and the program runs.
//   rl_define(src, n, name) -> 0 ok, 1 error: add a definitions file (declare ...) to the type checker
//   rl_check(src, n, strict) -> number of problems; each problem is printed as "LINE:COL: message"
// Everything is printed to standard output (fd 1), which the page collects. One instance runs one program
// (the page makes a new instance for each run); the checker instance is kept for its definitions.
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#include "Luau/Frontend.h"
#include "Luau/BuiltinDefinitions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <memory>
#include <string>
#include <unordered_map>

static double now_ms() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
static double deadline = 0; static int timed_out = 0, limit_s = 10;

static void interrupt(lua_State* L, int gc)
{
    if (gc >= 0) return;
    if (deadline && now_ms() > deadline) {
        timed_out = 1; lua_callbacks(L)->interrupt = NULL;
        luaL_error(L, "time limit: the program ran for more than %d seconds", limit_s);
    }
}

// WebAssembly memory has no page protection: the checker's freeze() (a debugging aid that write-protects type
// pages) becomes a no-op
extern "C" int mprotect(void*, size_t, int) { return 0; }

extern "C" __attribute__((export_name("rl_alloc"))) char* rl_alloc(int n) { return (char*)malloc(n + 1); }
extern "C" __attribute__((export_name("rl_free"))) void rl_free(char* p) { free(p); }

// compile and run one chunk on L; prints the error and returns 1/2 on failure
static int chunk(lua_State* L, const char* name, const char* src, int len)
{
    size_t blen = 0;
    lua_CompileOptions opts = {};
    opts.optimizationLevel = 1; opts.debugLevel = 1;
    char* bc = luau_compile(src, len, &opts, &blen);
    int rc = 0;
    if (luau_load(L, name, bc, blen, 0) != 0) {
        printf("%s\n", lua_tostring(L, -1)); lua_pop(L, 1); rc = 1;
    } else if (lua_pcall(L, 0, 0, 0) != 0) {
        const char* m = lua_tostring(L, -1);
        printf("%s\n", m ? m : "error"); lua_pop(L, 1);
        rc = timed_out ? 2 : 1;
    }
    free(bc);
    return rc;
}

extern "C" __attribute__((export_name("rl_run"))) int rl_run(const char* src, int len, int limit_ms, const char* pre, int plen)
{
    timed_out = 0; limit_s = limit_ms > 0 ? (limit_ms + 999) / 1000 : 0;
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    deadline = limit_ms > 0 ? now_ms() + limit_ms : 0;
    lua_callbacks(L)->interrupt = interrupt;
    int rc = 0;
    if (pre && plen > 0)
        rc = chunk(L, "=parts", pre, plen);
    if (rc == 0) {
        luaL_sandbox(L);
        luaL_sandboxthread(L);
        rc = chunk(L, "=program", src, len);
    }
    fflush(stdout);
    lua_close(L); deadline = 0;
    return rc;
}

// ---- type checking (Luau's own checker, the old solver; strict with --!strict or strict=1) ----
struct MemFiles : Luau::FileResolver
{
    std::unordered_map<std::string, std::string> source;
    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        auto it = source.find(name);
        if (it == source.end()) return std::nullopt;
        return Luau::SourceCode{it->second, Luau::SourceCode::Module};
    }
    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo*, Luau::AstExpr*, const Luau::TypeCheckLimits&) override { return std::nullopt; }
    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override { return name; }
};
struct OneConfig : Luau::ConfigResolver
{
    Luau::Config config;
    const Luau::Config& getConfig(const Luau::ModuleName&, const Luau::TypeCheckLimits&) const override { return config; }
};

LUAU_FASTINT(LuauTarjanChildLimit)
LUAU_FASTINT(LuauTypeInferIterationLimit)
LUAU_FASTINT(LuauTypeInferTypePackLoopLimit)

static MemFiles files; static OneConfig cfg; static std::unique_ptr<Luau::Frontend> fe;

static Luau::Frontend& frontend()
{
    if (!fe) {
        // Pilot.lua's definitions are large (GetPart has an overload for each of about 200 parts): raise the limits
        // that would otherwise call them "too complex"
        FInt::LuauTarjanChildLimit.value = 1000000; FInt::LuauTypeInferIterationLimit.value = 2000000; FInt::LuauTypeInferTypePackLoopLimit.value = 100000;
        Luau::FrontendOptions o; o.runLintChecks = false;
        fe = std::make_unique<Luau::Frontend>(Luau::SolverMode::Old, &files, &cfg, o);
        Luau::unfreeze(fe->globals.globalTypes);
        Luau::registerBuiltinGlobals(*fe, fe->globals);
        Luau::freeze(fe->globals.globalTypes);
    }
    return *fe;
}

// ns: "" puts the file's types in the global scope; a name ("Enum") makes them a type namespace (Enum.Material),
// while the file's declared globals still become globals
extern "C" __attribute__((export_name("rl_define"))) int rl_define(const char* src, int len, const char* name, const char* ns)
{
    try {
        Luau::Frontend& f = frontend();
        Luau::ScopePtr g = f.globals.globalScope, target = g;
        if (ns && *ns) target = std::make_shared<Luau::Scope>(g);
        Luau::unfreeze(f.globals.globalTypes);
        Luau::LoadDefinitionFileResult r = f.loadDefinitionFile(f.globals, target, std::string_view(src, len), name, false);
        if (r.success && target != g) {
            g->importedTypeBindings[ns] = target->exportedTypeBindings;
            for (auto& [n, b] : target->bindings) g->bindings[n] = b;
        }
        Luau::freeze(f.globals.globalTypes);
        if (r.success) return 0;
        for (auto& e : r.parseResult.errors) printf("%s:%d: %s\n", name, e.getLocation().begin.line + 1, e.getMessage().c_str());
        if (r.module) for (auto& e : r.module->errors) printf("%s:%d: %s\n", name, e.location.begin.line + 1, Luau::toString(e).c_str());
    } catch (const std::exception& e) { printf("%s: %s\n", name, e.what()); }
    fflush(stdout);
    return 1;
}

extern "C" __attribute__((export_name("rl_check"))) int rl_check(const char* src, int len, int strict)
{
    int n = 0;
    try {
        Luau::Frontend& f = frontend();
        cfg.config.mode = strict ? Luau::Mode::Strict : Luau::Mode::Nonstrict;
        f.clear();
        files.source["program"] = std::string(src, len);
        Luau::CheckResult r = f.check("program");
        for (const Luau::TypeError& e : r.errors) {
            printf("%d:%d: %s\n", e.location.begin.line + 1, e.location.begin.column + 1, Luau::toString(e).c_str()); n++;
        }
    } catch (const std::exception& e) { printf("0:0: the checker stopped: %s\n", e.what()); n++; }
    fflush(stdout);
    return n;
}
