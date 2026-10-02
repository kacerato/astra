// Spike S-08 (docs/plano-mestre/22-ROADMAP-FASES-E-GATES.md §1, doc 14): Luau 0.740 atende?
// Itens: bindings genérico (reflexão) e rápido (átomo/thunk) com custo medido; `vector` sem alocação; interrupt
// por prazo; limite duro de memória; ponto de parada com o "editor" seguindo responsivo; sandbox; codegen nativo.
//
// A mesma suíte roda como executável de console (PC, adb shell) e dentro do app Android (S-02, contexto real de
// um app, onde a política de memória executável difere da do shell).

#include "luau_suite.h"

#include <lua.h>
#include <luacode.h>
#include <luacodegen.h>
#include <Luau/CodeGen.h>
#include <lualib.h>

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace
{
using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

LuauSuiteLog gLog = nullptr;
int          gFailures = 0;

void logf(const char* fmt, ...)
{
    char    buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gLog(buf);
}

void check(bool cond, const char* what)
{
    if (!cond)
    {
        logf("S08 FALHA: %s", what);
        ++gFailures;
    }
}

// ---------------------------------------------------------------------------
// VM: alocador com contabilidade e limite duro (doc 14 §6)
// ---------------------------------------------------------------------------
struct VmMemory
{
    size_t   used = 0;
    size_t   limit = SIZE_MAX;
    uint64_t allocations = 0;
};

void* vmAlloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    VmMemory& m = *(VmMemory*)ud;
    if (nsize == 0)
    {
        free(ptr);
        m.used -= osize;
        return nullptr;
    }
    if (m.used - osize + nsize > m.limit)
        return nullptr; // o Luau converte em LUA_ERRMEM
    void* p = realloc(ptr, nsize);
    if (p)
    {
        m.used = m.used - osize + nsize;
        if (!ptr)
            ++m.allocations;
    }
    return p;
}

// Prazo do callback atual (interrupt). Zero = sem prazo.
Clock::time_point gDeadline;
bool              gDeadlineActive = false;

void onInterrupt(lua_State* L, int gc)
{
    if (gc >= 0 || !gDeadlineActive || Clock::now() < gDeadline)
        return; // durante o GC não é seguro lançar erro
    gDeadlineActive = false;
    lua_Debug ar = {};
    lua_getinfo(L, 0, "sl", &ar);
    luaL_error(L, "tempo excedido em %s:%d", ar.short_src, ar.currentline);
}

// Átomos de nomes de propriedade (caminho rápido): o Luau guarda um id por string interna.
enum Atom : int16_t
{
    kAtomNone = -1,
    kAtomPosition = 0,
    kAtomScale = 1,
};
int16_t onUserAtom(lua_State*, const char* s, size_t l)
{
    if (l == 8 && memcmp(s, "position", 8) == 0)
        return kAtomPosition;
    if (l == 5 && memcmp(s, "scale", 5) == 0)
        return kAtomScale;
    return kAtomNone;
}

lua_State* newVm(VmMemory& mem)
{
    lua_State* L = lua_newstate(vmAlloc, &mem);
    luaL_openlibs(L);
    lua_callbacks(L)->interrupt = onInterrupt;
    lua_callbacks(L)->useratom = onUserAtom;
    return L;
}

Luau::CodeGen::CompilationStats gLastNativeStats;

// debugLevel 2 guarda nomes de locais e upvalues (necessário para o depurador; o editor compila assim os scripts
// em depuração). O player usa 1.
bool loadChunk(lua_State* L, const char* name, const std::string& src, bool native = false, int debugLevel = 1)
{
    lua_CompileOptions opts = {};
    opts.optimizationLevel = 1;
    opts.debugLevel = debugLevel;
    size_t size = 0;
    char*  bytecode = luau_compile(src.c_str(), src.size(), &opts, &size);
    int    rc = luau_load(L, name, bytecode, size, 0);
    free(bytecode);
    if (rc != 0)
    {
        logf("S08: erro ao carregar %s: %s", name, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    if (native)
    {
        gLastNativeStats = {};
        Luau::CodeGen::CompilationOptions copts;
        Luau::CodeGen::CompilationResult  r = Luau::CodeGen::compile(L, -1, copts, &gLastNativeStats);
        if (r.hasErrors())
            logf("S08: codegen falhou (resultado %d, %zu funções com falha)", (int)r.result, r.protoFailures.size());
    }
    return true;
}

// Executa a função no topo protegida; devolve o código e deixa a mensagem de erro (se houver) em *err.
int runProtected(lua_State* L, int nresults, std::string* err = nullptr)
{
    int rc = lua_pcall(L, 0, nresults, 0);
    if (rc != LUA_OK)
    {
        if (err)
            *err = lua_tostring(L, -1) ? lua_tostring(L, -1) : "?";
        lua_pop(L, 1);
    }
    return rc;
}

// ---------------------------------------------------------------------------
// Bindings: Transform com propriedade `position` (vector) e `scale` (number)
// ---------------------------------------------------------------------------
struct Transform
{
    float  position[3];
    double scale;
};
constexpr int kTagTransformFast = 1;
constexpr int kTagTransformGeneric = 2;

// Caminho genérico: busca por nome numa tabela de propriedades com valor "Variant" (como o TypeRegistry).
struct Variant
{
    enum Kind
    {
        Number,
        Vec3
    } kind;
    double n;
    float  v[3];
};
struct PropertyInfo
{
    Variant (*get)(const Transform&);
    void (*set)(Transform&, const Variant&);
};
const std::unordered_map<std::string, PropertyInfo>& properties()
{
    static const std::unordered_map<std::string, PropertyInfo> props = {
        { "position",
          { [](const Transform& t) { return Variant{ Variant::Vec3, 0, { t.position[0], t.position[1], t.position[2] } }; },
            [](Transform& t, const Variant& v) { memcpy(t.position, v.v, sizeof(t.position)); } } },
        { "scale", { [](const Transform& t) { return Variant{ Variant::Number, t.scale, {} }; },
                     [](Transform& t, const Variant& v) { t.scale = v.n; } } },
    };
    return props;
}

void pushVariant(lua_State* L, const Variant& v)
{
    if (v.kind == Variant::Vec3)
        lua_pushvector(L, v.v[0], v.v[1], v.v[2]);
    else
        lua_pushnumber(L, v.n);
}
Variant toVariant(lua_State* L, int idx)
{
    if (const float* v = lua_tovector(L, idx))
        return Variant{ Variant::Vec3, 0, { v[0], v[1], v[2] } };
    return Variant{ Variant::Number, luaL_checknumber(L, idx), {} };
}

int genericIndex(lua_State* L)
{
    Transform*  t = (Transform*)lua_touserdatatagged(L, 1, kTagTransformGeneric);
    const char* key = luaL_checkstring(L, 2);
    auto        it = properties().find(key);
    if (!t || it == properties().end())
        luaL_error(L, "Transform não tem a propriedade '%s'", key);
    pushVariant(L, it->second.get(*t));
    return 1;
}
int genericNewIndex(lua_State* L)
{
    Transform*  t = (Transform*)lua_touserdatatagged(L, 1, kTagTransformGeneric);
    const char* key = luaL_checkstring(L, 2);
    auto        it = properties().find(key);
    if (!t || it == properties().end())
        luaL_error(L, "Transform não tem a propriedade '%s'", key);
    it->second.set(*t, toVariant(L, 3));
    return 0;
}

// Caminho rápido (o que o gerador de thunks produzirá para tipos quentes): átomo → switch, sem busca por string.
int fastIndex(lua_State* L)
{
    Transform* t = (Transform*)lua_touserdatatagged(L, 1, kTagTransformFast);
    int        atom = kAtomNone;
    lua_tostringatom(L, 2, &atom);
    switch (atom)
    {
    case kAtomPosition:
        lua_pushvector(L, t->position[0], t->position[1], t->position[2]);
        return 1;
    case kAtomScale:
        lua_pushnumber(L, t->scale);
        return 1;
    default:
        luaL_error(L, "Transform não tem a propriedade '%s'", lua_tostring(L, 2));
    }
}
int fastNewIndex(lua_State* L)
{
    Transform* t = (Transform*)lua_touserdatatagged(L, 1, kTagTransformFast);
    int        atom = kAtomNone;
    lua_tostringatom(L, 2, &atom);
    switch (atom)
    {
    case kAtomPosition:
        memcpy(t->position, luaL_checkvector(L, 3), sizeof(t->position));
        return 0;
    case kAtomScale:
        t->scale = luaL_checknumber(L, 3);
        return 0;
    default:
        luaL_error(L, "Transform não tem a propriedade '%s'", lua_tostring(L, 2));
    }
}

Transform* pushTransform(lua_State* L, int tag, lua_CFunction index, lua_CFunction newindex)
{
    Transform* t = (Transform*)lua_newuserdatatagged(L, sizeof(Transform), tag);
    *t = Transform{ { 0, 0, 0 }, 1.0 };
    lua_newtable(L);
    lua_pushcfunction(L, index, "__index");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, newindex, "__newindex");
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    return t;
}

int emptyCFunction(lua_State*) { return 0; }

// ---------------------------------------------------------------------------
// Itens
// ---------------------------------------------------------------------------
void testBindings()
{
    VmMemory   mem;
    lua_State* L = newVm(mem);
    const int  N = 1000000;

    const std::string bench = "local t, n = ...\n"
                              "for i = 1, n do\n"
                              "  local p = t.position\n"
                              "  t.position = p + vector.create(1, 0, 0)\n"
                              "end\n"
                              "return t.position.x\n";
    double perIter[2] = {};
    for (int fast = 0; fast < 2; ++fast)
    {
        loadChunk(L, "=bindings", bench);
        Transform* t = fast ? pushTransform(L, kTagTransformFast, fastIndex, fastNewIndex)
                            : pushTransform(L, kTagTransformGeneric, genericIndex, genericNewIndex);
        lua_pushinteger(L, N);
        auto t0 = Clock::now();
        int  rc = lua_pcall(L, 2, 1, 0);
        perIter[fast] = msSince(t0) * 1e6 / N; // ns por iteração (1 get + 1 set + 1 vector)
        check(rc == LUA_OK && lua_tonumber(L, -1) == N && t->position[0] == (float)N, "binding não atualizou a posição");
        lua_pop(L, 1);
    }
    logf("S08: binding get+set de vector: genérico %.1f ns/iter, rápido (átomo) %.1f ns/iter", perIter[0], perIter[1]);

    // Custo de chamada nos dois sentidos.
    loadChunk(L, "=vazio", "return function() end");
    runProtected(L, 1);
    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i)
    {
        lua_pushvalue(L, -1);
        lua_call(L, 0, 0);
    }
    const double cppToLuau = msSince(t0) * 1e6 / N;
    lua_pop(L, 1);

    lua_pushcfunction(L, emptyCFunction, "vazio");
    lua_setglobal(L, "vazioC");
    loadChunk(L, "=chamadaC", "local f, n = vazioC, ... for i = 1, n do f() end");
    lua_pushinteger(L, N);
    t0 = Clock::now();
    lua_pcall(L, 1, 0, 0);
    const double luauToCpp = msSince(t0) * 1e6 / N;
    logf("S08: chamada C++->Luau (função vazia) %.1f ns; Luau->C++ %.1f ns", cppToLuau, luauToCpp);

    // Erro claro para propriedade inexistente (base do erro de referência destruída, doc 14 §5.3).
    loadChunk(L, "=erro", "local t = ... return t.inexistente");
    pushTransform(L, kTagTransformFast, fastIndex, fastNewIndex);
    std::string err;
    if (lua_pcall(L, 1, 0, 0) != LUA_OK)
    {
        err = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    check(err.find("inexistente") != std::string::npos, "erro de propriedade inexistente sem o nome");
    logf("S08: erro de propriedade: %s", err.c_str());
    lua_close(L);
}

void testVectorGc()
{
    VmMemory   mem;
    lua_State* L = newVm(mem);
    loadChunk(L, "=vec",
              "local a, b = vector.create(1, 2, 3), vector.create(0.5, 0.25, 0.125)\n"
              "for i = 1, 100000 do a = a * 0.999 + b; a = vector.normalize(a) end\n"
              "return a.x");
    lua_gc(L, LUA_GCCOLLECT, 0);
    const uint64_t before = mem.allocations;
    runProtected(L, 1);
    lua_pop(L, 1);
    const uint64_t vecAllocs = mem.allocations - before;
    logf("S08: 100 mil operações de vector: %llu alocações", (unsigned long long)vecAllocs);
    check(vecAllocs < 10, "vector alocou memória no laço");
    lua_close(L);
}

void testInterrupt()
{
    VmMemory   mem;
    lua_State* L = newVm(mem);
    loadChunk(L, "@Porta.luau", "local x = 0\nwhile true do\n  x = x + 1\nend\n");
    gDeadline = Clock::now() + std::chrono::milliseconds(100);
    gDeadlineActive = true;
    auto        t0 = Clock::now();
    std::string err;
    const int   rc = runProtected(L, 0, &err);
    const double ms = msSince(t0);
    logf("S08: laço infinito interrompido em %.1f ms (limite 100 ms): %s", ms, err.c_str());
    check(rc == LUA_ERRRUN && err.find("tempo excedido") != std::string::npos, "interrupt não parou o laço");
    check(err.find("Porta.luau:") != std::string::npos, "erro do interrupt sem arquivo:linha");
    check(ms < 150.0, "interrupt demorou mais que 150 ms");

    loadChunk(L, "=depois", "return 2 + 2");
    check(runProtected(L, 1) == LUA_OK && lua_tonumber(L, -1) == 4, "VM inutilizável após interrupt");
    lua_close(L);
}

void testMemoryLimit()
{
    VmMemory   mem;
    lua_State* L = newVm(mem);
    mem.limit = mem.used + 8 * 1024 * 1024; // limite duro: 8 MB além do estado inicial
    loadChunk(L, "=guloso", "local t = {} for i = 1, 1e8 do t[i] = {i} end");
    std::string err;
    const int   rc = runProtected(L, 0, &err);
    logf("S08: limite de memória: código %d, uso no erro %.1f MB, limite %.1f MB", rc, mem.used / 1048576.0, mem.limit / 1048576.0);
    check(rc == LUA_ERRMEM, "estouro de memória não virou LUA_ERRMEM");
    lua_gc(L, LUA_GCCOLLECT, 0);
    loadChunk(L, "=depois", "return #'ok'");
    check(runProtected(L, 1) == LUA_OK && lua_tonumber(L, -1) == 2, "VM inutilizável após estouro de memória");
    logf("S08: após coleta, uso %.2f MB", mem.used / 1048576.0);
    lua_close(L);
}

int  gBreakHits = 0;
bool gStepOffBreak = false; // retomada: a VM reexecuta a instrução BREAK; esta passagem deve seguir adiante
void onDebugBreak(lua_State* L, lua_Debug* ar)
{
    ++gBreakHits;
    (void)ar;
    if (gStepOffBreak)
    {
        gStepOffBreak = false;
        return;
    }
    lua_break(L); // suspende a coroutine; quem chamou lua_resume recebe LUA_BREAK
}

void testBreakpoint()
{
    VmMemory   mem;
    lua_State* L = newVm(mem);
    lua_callbacks(L)->debugbreak = onDebugBreak;

    // Callback de script rodando como coroutine (doc 14 §9).
    lua_State* co = lua_newthread(L);
    loadChunk(co, "@Contador.luau",
              "local total = 0\n"          // 1
              "for i = 1, 5 do\n"          // 2
              "  total = total + i\n"      // 3
              "end\n"                      // 4
              "local marcador = total * 2\n" // 5  <- ponto de parada
              "return marcador\n",         // 6
              false, 2);
    const int bpLine = lua_breakpoint(co, -1, 5, 1);
    check(bpLine == 5, "ponto de parada não caiu na linha 5");

    int status = lua_resume(co, L, 0);
    check(status == LUA_BREAK, "lua_resume não parou no ponto de parada");

    // "Editor" segue rodando enquanto o script está parado: quadros simulados + inspeção de locais.
    int framesWhilePaused = 0;
    for (int i = 0; i < 120; ++i)
        ++framesWhilePaused;
    // Inspeção sob demanda enquanto pausado: lua_callhook roda um hook na thread em estado de pausa, com o quadro
    // válido (consultar lua_getinfo/lua_getlocal direto de fora devolve a linha de entrada e nenhum local).
    struct Inspection
    {
        int         line = -1;
        std::string source;
        std::string locals;
    } insp;
    lua_callhook(
        co,
        [](lua_State* T, lua_Debug* d) {
            Inspection& out = *(Inspection*)d->userdata;
            lua_Debug   ar = {};
            lua_getinfo(T, 0, "sl", &ar);
            out.line = ar.currentline;
            out.source = ar.short_src;
            for (int n = 1;; ++n)
            {
                const char* name = lua_getlocal(T, 0, n);
                if (!name)
                    break;
                char buf[64];
                snprintf(buf, sizeof(buf), "%s=%g ", name, lua_tonumber(T, -1));
                out.locals += buf;
                lua_pop(T, 1);
            }
        },
        &insp);
    logf("S08: parado em %s:%d, locais: %s; %d quadros do editor rodaram durante a pausa", insp.source.c_str(), insp.line,
         insp.locals.c_str(), framesWhilePaused);
    check(insp.line == 5 && insp.locals.find("total=15") != std::string::npos, "locais errados no ponto de parada");

    gStepOffBreak = true;
    status = lua_resume(co, L, 0);
    check(status == LUA_OK && lua_tonumber(co, -1) == 30, "retomada após o ponto de parada não terminou certo");
    logf("S08: retomado; resultado %g (esperado 30), paradas %d", lua_tonumber(co, -1), gBreakHits);
    lua_close(L);
}

void testSandbox()
{
    VmMemory   mem;
    lua_State* L = newVm(mem);
    luaL_sandbox(L);
    loadChunk(L, "=sandbox", "math.pi = 3");
    std::string err;
    check(runProtected(L, 0, &err) == LUA_ERRRUN, "sandbox deixou alterar biblioteca padrão");
    logf("S08: sandbox: %s; io=%s os.execute=%s", err.c_str(), "ausente", "ausente");
    lua_close(L);
}

void testCodegen()
{
    const bool supported = luau_codegen_supported() != 0;
    logf("S08: codegen nativo suportado nesta plataforma: %s", supported ? "sim" : "não");
    if (!supported)
        return;

    // Laços dominados pela VM (aritmética e array), onde o codegen atua; builtins como math.sqrt já são fastcalls.
    const std::string src = "--!native\n"
                            "local function aritmetica(n: number): number\n"
                            "  local s = 0\n"
                            "  for i = 1, n do s = s + i * 0.5 - i / 3 + (i // 7) end\n"
                            "  return s\n"
                            "end\n"
                            "local function arrays(n: number): number\n"
                            "  local t = table.create(1000, 1)\n"
                            "  local s = 0\n"
                            "  for r = 1, n do for i = 1, #t do s = s + t[i] * r end end\n"
                            "  return s\n"
                            "end\n"
                            "return aritmetica(20000000) + arrays(20000)\n";
    double ms[2] = {};
    double result[2] = {};
    for (int native = 0; native < 2; ++native)
    {
        VmMemory   mem;
        lua_State* L = newVm(mem);
        if (native)
            luau_codegen_create(L);
        loadChunk(L, "=bench", src, native != 0);
        if (native)
            logf("S08: codegen: %u de %u funções compiladas, %zu B de código nativo", gLastNativeStats.functionsCompiled,
                 gLastNativeStats.functionsTotal, gLastNativeStats.nativeCodeSizeBytes);
        auto t0 = Clock::now();
        runProtected(L, 1);
        ms[native] = msSince(t0);
        result[native] = lua_tonumber(L, -1);
        lua_close(L);
    }
    logf("S08: laços (20M aritmética + 20M acessos a array): interpretador %.1f ms, nativo %.1f ms (%.2fx)", ms[0], ms[1],
         ms[0] / ms[1]);
    check(result[0] == result[1], "codegen nativo deu resultado diferente do interpretador");
}
} // namespace

int runLuauSuite(LuauSuiteLog log)
{
    gLog = log;
    gFailures = 0;
    logf("S08: Luau 0.740 (%s, %d bits)", sizeof(void*) == 8 ? "64" : "32", (int)(sizeof(void*) * 8));
    testBindings();
    testVectorGc();
    testInterrupt();
    testMemoryLimit();
    testBreakpoint();
    testSandbox();
    testCodegen();
    logf("S08: %s (%d falhas)", gFailures == 0 ? "APROVADO" : "REPROVADO", gFailures);
    return gFailures;
}
