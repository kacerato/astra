// Spike S-06 (docs/plano-mestre/22-ROADMAP-FASES-E-GATES.md §1):
// O flecs 4.1 aguenta o editor? Reparent/reordenação de 10 mil, Multi<T>, mudanças estruturais adiadas,
// mundos de 100 mil entidades, ordem de filhos estável (OrderedChildren).
//
// Mede as duas formas de hierarquia do flecs 4.1: ChildOf (par, fragmenta tabelas por pai) + OrderedChildren, e
// Parent (componente, não fragmenta). Os orçamentos abaixo foram fixados ANTES da primeira medição; valem para o
// aparelho T3 (o host é só referência).
//
// Uso: astra_spike_s06 [repetições=3]. Saída: uma linha por medição + APROVADO/REPROVADO por orçamento.

#include <flecs.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

// Orçamentos (aparelho T3). Interativo = uma ação de arrastar no editor; lote = ação única sobre 10 mil itens.
constexpr double kBudgetSingleReparentP99Us = 200.0;
constexpr double kBudgetSingleReorderP99Us = 500.0; // pai com 1000 filhos
constexpr double kBudgetBatchReparentMs = 100.0;    // 10 mil
constexpr double kBudgetDestroySubtreeMs = 50.0;    // 10 mil
constexpr double kBudgetDeferredMergeMs = 50.0;     // 10 mil operações adiadas
constexpr double kBudgetMultiAddMs = 20.0;          // 10 mil instâncias
constexpr double kBudgetMultiIterateMs = 1.0;       // 10 mil instâncias
constexpr double kBudgetTraversalMs = 20.0;         // 100 mil em profundidade, na ordem

int gFailures = 0;

void report(const char* mode, const char* what, double value, const char* unit, double budget)
{
    const bool ok = value <= budget;
    if (!ok)
        ++gFailures;
    std::printf("%-8s %-46s %10.3f %-3s (orçamento %8.1f) %s\n", mode, what, value, unit, budget, ok ? "ok" : "ESTOUROU");
}

void check(bool cond, const char* what)
{
    if (!cond)
    {
        std::printf("FALHA DE CORRETUDE: %s\n", what);
        ++gFailures;
    }
}

struct Position
{
    float x, y, z;
};
struct Selected
{
};

// Multi<T>: várias instâncias do mesmo tipo na entidade (doc 05 §"componentes múltiplos"). Vetor pequeno com
// instanceId estável; o flecs vê um único componente.
struct BoxCollider
{
    float cx, cy, cz;
    float sx, sy, sz;
};
struct BoxColliders
{
    struct Item
    {
        uint32_t    instanceId;
        BoxCollider value;
    };
    std::vector<Item> items;
    uint32_t          nextId = 1;

    uint32_t add(const BoxCollider& c)
    {
        items.push_back({ nextId, c });
        return nextId++;
    }
    bool remove(uint32_t instanceId)
    {
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].instanceId == instanceId)
            {
                items.erase(items.begin() + (ptrdiff_t)i); // preserva a ordem (ordem do Inspector)
                return true;
            }
        return false;
    }
};

enum class Storage
{
    ChildOf,
    Parent
};

const char* name(Storage s) { return s == Storage::ChildOf ? "ChildOf" : "Parent"; }

ecs_entity_t newChild(flecs::world& w, Storage s, ecs_entity_t parent)
{
    if (s == Storage::ChildOf)
        return ecs_new_w_pair(w, EcsChildOf, parent);
    return ecs_new_w_parent(w, parent, nullptr);
}

void reparent(flecs::world& w, Storage s, ecs_entity_t e, ecs_entity_t parent)
{
    if (s == Storage::ChildOf)
        ecs_add_pair(w, e, EcsChildOf, parent);
    else
        flecs::entity(w, e).set<flecs::Parent>({ parent });
}

ecs_entity_t newParent(flecs::world& w)
{
    ecs_entity_t p = ecs_new(w);
    ecs_add_id(w, p, EcsOrderedChildren);
    return p;
}

std::vector<ecs_entity_t> orderedChildren(flecs::world& w, ecs_entity_t parent)
{
    ecs_entities_t c = ecs_get_ordered_children(w, parent);
    return std::vector<ecs_entity_t>(c.ids, c.ids + c.count);
}

double percentile(std::vector<double> v, double p)
{
    std::sort(v.begin(), v.end());
    return v[(size_t)(p * (double)(v.size() - 1))];
}

void runStorage(Storage mode)
{
    const char*  m = name(mode);
    flecs::world w;
    std::mt19937 rng(1234);

    // Cena: raiz → 100 grupos × 1000 folhas (100.101 entidades), todos os pais com OrderedChildren.
    auto                      t0 = Clock::now();
    ecs_entity_t              root = newParent(w);
    std::vector<ecs_entity_t> groups, leaves;
    for (int g = 0; g < 100; ++g)
    {
        ecs_entity_t grp = newChild(w, mode, root);
        ecs_add_id(w, grp, EcsOrderedChildren);
        groups.push_back(grp);
        for (int i = 0; i < 1000; ++i)
        {
            ecs_entity_t leaf = newChild(w, mode, grp);
            flecs::entity(w, leaf).set<Position>({ (float)i, 0, 0 });
            leaves.push_back(leaf);
        }
    }
    std::printf("%-8s criar 100.101 entidades: %.1f ms\n", m, msSince(t0));

    // Ordem estável: mudança estrutural nos filhos (troca de tabela) não pode mexer na ordem.
    {
        auto before = orderedChildren(w, groups[0]);
        for (size_t i = 0; i < before.size(); i += 2)
            flecs::entity(w, before[i]).add<Selected>();
        check(orderedChildren(w, groups[0]) == before, "ordem dos filhos mudou após adicionar componente");
    }

    // Reparent interativo: 1000 amostras, um filho por vez para o fim de outro grupo.
    {
        std::vector<double> us;
        for (int i = 0; i < 1000; ++i)
        {
            ecs_entity_t e = leaves[rng() % leaves.size()];
            ecs_entity_t dst = groups[rng() % groups.size()];
            auto         t = Clock::now();
            reparent(w, mode, e, dst);
            us.push_back(msSince(t) * 1000.0);
            if (i == 0)
            {
                check(ecs_get_parent(w, e) == dst, "reparent não trocou o pai");
                auto c = orderedChildren(w, dst);
                check(!c.empty() && c.back() == e, "filho reparentado não foi para o fim da lista ordenada");
            }
        }
        report(m, "reparent interativo p99", percentile(us, 0.99), "us", kBudgetSingleReparentP99Us);
    }

    // Reordenação interativa: levar um filho para o início de um grupo de ~1000 (arrastar na hierarquia).
    {
        std::vector<double> us;
        for (int i = 0; i < 1000; ++i)
        {
            ecs_entity_t grp = groups[rng() % groups.size()];
            auto         c = orderedChildren(w, grp);
            const ptrdiff_t idx = (ptrdiff_t)(rng() % c.size()); // arrastar o filho idx para o topo do grupo
            std::rotate(c.begin(), c.begin() + idx, c.begin() + idx + 1);
            auto t = Clock::now();
            ecs_set_child_order(w, grp, c.data(), (int32_t)c.size());
            us.push_back(msSince(t) * 1000.0);
            if (i == 0)
                check(orderedChildren(w, grp) == c, "set_child_order não aplicou a ordem");
        }
        report(m, "reordenação interativa p99 (1000 filhos)", percentile(us, 0.99), "us", kBudgetSingleReorderP99Us);
    }

    // Reparent em lote: 10 mil entidades para um único novo pai.
    {
        ecs_entity_t dst = newChild(w, mode, root);
        ecs_add_id(w, dst, EcsOrderedChildren);
        std::vector<ecs_entity_t> pick(leaves.begin(), leaves.begin() + 10000);
        auto                      t = Clock::now();
        for (ecs_entity_t e : pick)
            reparent(w, mode, e, dst);
        report(m, "reparent em lote (10 mil)", msSince(t), "ms", kBudgetBatchReparentMs);
        check(orderedChildren(w, dst) == pick, "lote reparentado fora de ordem");
        groups.push_back(dst);
    }

    // Percorrer 100 mil na ordem (reconstrução completa do painel Hierarquia).
    {
        size_t visited = 0;
        auto   t = Clock::now();
        std::vector<ecs_entity_t> stack{ root };
        while (!stack.empty())
        {
            ecs_entity_t e = stack.back();
            stack.pop_back();
            ++visited;
            if (ecs_has_id(w, e, EcsOrderedChildren))
            {
                ecs_entities_t c = ecs_get_ordered_children(w, e);
                for (int32_t i = c.count - 1; i >= 0; --i)
                    stack.push_back(c.ids[i]);
            }
        }
        report(m, "percorrer hierarquia na ordem (100 mil)", msSince(t), "ms", kBudgetTraversalMs);
        check(visited >= 100000, "percurso não visitou todas as entidades");
    }

    // Mudanças estruturais adiadas: 10 mil operações dentro de defer_begin/defer_end (como em sistemas/scripts).
    {
        ecs_entity_t dst = groups[1];
        w.defer_begin();
        for (int i = 0; i < 10000; ++i)
        {
            ecs_entity_t e = leaves[10000 + (size_t)i];
            if (i % 2)
                flecs::entity(w, e).add<Selected>();
            else
                reparent(w, mode, e, dst);
        }
        auto t = Clock::now();
        w.defer_end();
        report(m, "aplicar 10 mil operações adiadas", msSince(t), "ms", kBudgetDeferredMergeMs);
        check(ecs_get_parent(w, leaves[10000]) == dst, "operação adiada não foi aplicada");
    }

    // Destruir uma subárvore de 10 mil (o lote reparentado acima).
    {
        ecs_entity_t sub = groups.back();
        auto         t = Clock::now();
        ecs_delete(w, sub);
        report(m, "destruir subárvore (10 mil)", msSince(t), "ms", kBudgetDestroySubtreeMs);
        check(!ecs_is_alive(w, leaves[0]), "filho continuou vivo após destruir o pai");
    }
}

void runMulti()
{
    flecs::world w;
    w.component<BoxColliders>();
    std::vector<flecs::entity> ents;
    for (int i = 0; i < 5000; ++i)
        ents.push_back(w.entity());

    auto t = Clock::now();
    for (int i = 0; i < 10000; ++i)
    {
        flecs::entity e = ents[(size_t)(i % 5000)];
        BoxColliders& c = e.ensure<BoxColliders>(); // 1º add cria o componente; os seguintes só crescem o vetor
        c.add({ 0, 0, 0, 1, 1, 1 });
        e.modified<BoxColliders>();
    }
    report("Multi<T>", "adicionar 10 mil colisores (5 mil entidades)", msSince(t), "ms", kBudgetMultiAddMs);

    t = Clock::now();
    float  sum = 0;
    size_t count = 0;
    w.each([&](const BoxColliders& c) {
        for (const auto& it : c.items)
        {
            sum += it.value.sx;
            ++count;
        }
    });
    report("Multi<T>", "iterar 10 mil colisores", msSince(t), "ms", kBudgetMultiIterateMs);
    check(count == 10000 && sum == 10000.0f, "iteração Multi<T> com contagem errada");

    // Remover a 1ª instância de cada entidade: ids estáveis para as demais.
    t = Clock::now();
    for (flecs::entity e : ents)
    {
        BoxColliders& c = e.ensure<BoxColliders>();
        const uint32_t keep = c.items.back().instanceId;
        c.remove(c.items.front().instanceId);
        check(c.items.back().instanceId == keep, "instanceId mudou após remover outra instância");
    }
    std::printf("Multi<T> remover 5 mil instâncias: %.3f ms\n", msSince(t));
}
} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const int reps = argc > 1 ? std::atoi(argv[1]) : 3;
    std::printf("S-06 flecs %d.%d.%d, %d repetições\n", FLECS_VERSION_MAJOR, FLECS_VERSION_MINOR, FLECS_VERSION_PATCH, reps);
    for (int r = 0; r < reps; ++r)
    {
        std::printf("--- repetição %d\n", r + 1);
        runStorage(Storage::ChildOf);
        runStorage(Storage::Parent);
        runMulti();
    }
    std::printf("%s (%d falhas)\n", gFailures == 0 ? "APROVADO" : "REPROVADO", gFailures);
    return gFailures == 0 ? 0 : 1;
}
