#include "souffle/problog/Pipeline.h"

#include "souffle/Derivation.h"
#include "souffle/cli/Cli.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/GraphAnalyzer.h"
#include "souffle/problog/GraphRewriter.h"
#include "souffle/problog/ImplicitSplitRewrite.h"
#include "souffle/problog/QueryManager.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/debug/Debugger.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/SddManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "ImplicitSplitRewrite.cpp"

namespace souffle::problog {

namespace {
bool fullOnlyMode = false;

struct RuleAppInventory {
    std::size_t headTuples = 0;
    std::size_t nullRuleSets = 0;
    std::size_t totalRuleApps = 0;
    std::size_t totalBindings = 0;
    std::size_t maxRuleAppsPerHead = 0;
    std::size_t uniqueRuleIds = 0;
};

static bool envFlagDisabled(const char* name) {
    const char* value = std::getenv(name);
    if (!value) {
        return false;
    }
    return std::string(value) == "1" || std::string(value) == "true" ||
            std::string(value) == "TRUE";
}

static std::size_t countInitialInputFacts() {
    return inputFactSet.size();
}

static RuleAppInventory collectRuleAppInventory() {
    RuleAppInventory inv;
    std::unordered_set<souffle::RamDomain> uniqueRuleIds;
    uniqueRuleIds.reserve(DerivationManager::untypedTuple2RuleApplications.size());
    for (const auto& [tuple, ruleSetPtr] : DerivationManager::untypedTuple2RuleApplications) {
        (void)tuple;
        ++inv.headTuples;
        if (ruleSetPtr == nullptr) {
            ++inv.nullRuleSets;
            continue;
        }
        inv.maxRuleAppsPerHead = std::max(inv.maxRuleAppsPerHead, ruleSetPtr->size());
        inv.totalRuleApps += ruleSetPtr->size();
        for (const auto& ruleApp : *ruleSetPtr) {
            inv.totalBindings += ruleApp.varValuesPure.size();
            uniqueRuleIds.insert(ruleApp.ruleId);
        }
    }
    inv.uniqueRuleIds = uniqueRuleIds.size();
    return inv;
}

static void writeJsonEscapedString(std::ostream& out, const std::string& value) {
    out.put('"');
    for (unsigned char c : value) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out << buf;
                } else {
                    out.put(static_cast<char>(c));
                }
        }
    }
    out.put('"');
}

static bool isFactLikeRule(const Rule* rule) {
    return rule != nullptr && (rule->isFact() || rule->getBodyAtoms().empty());
}

struct CachedRuleDumpInfo {
    const Rule* rule = nullptr;
    std::vector<std::string> vars;
};

static const CachedRuleDumpInfo& getCachedRuleDumpInfo(
        std::unordered_map<souffle::RamDomain, CachedRuleDumpInfo>& cache,
        const RuleManager& ruleManager,
        souffle::RamDomain ruleId) {
    auto it = cache.find(ruleId);
    if (it != cache.end()) {
        return it->second;
    }
    CachedRuleDumpInfo info;
    info.rule = ruleManager.getRule(ruleId);
    if (info.rule != nullptr) {
        info.vars = info.rule->getVars();
    }
    auto [insertedIt, _] = cache.emplace(ruleId, std::move(info));
    return insertedIt->second;
}

static void dumpRuleAppsBeforeGraphJson(
        const CmdOptions& opt,
        const RuleManager& ruleManager,
        const std::unordered_map<UntypedTuple, double>& factProb) {
    const std::string outputPath = makeOutputPath(opt, "derivation-before-graph.json");
    std::ofstream out(outputPath);
    if (!out.good()) {
        throw std::runtime_error("failed to open pre-graph dump output: " + outputPath);
    }

    out << std::setprecision(17);
    std::unordered_map<souffle::RamDomain, CachedRuleDumpInfo> ruleCache;
    ruleCache.reserve(ruleManager.size());

    out << "{\"facts\":[";
    bool firstFact = true;
    auto emitFact = [&](const UntypedTuple& tuple, double probability) {
        if (!firstFact) {
            out << ',';
        }
        firstFact = false;
        out << "{\"name\":";
        writeJsonEscapedString(out, tuple.toString());
        out << ",\"probability\":" << probability << "}";
    };

    for (const auto& [tuple, probability] : factProb) {
        emitFact(tuple, probability);
    }

    std::unordered_set<UntypedTuple> emittedDerivedFacts;
    for (const auto& [headTuple, ruleSetPtr] : DerivationManager::untypedTuple2RuleApplications) {
        if (ruleSetPtr == nullptr) {
            continue;
        }
        for (const auto& ruleApp : *ruleSetPtr) {
            const auto& cached = getCachedRuleDumpInfo(ruleCache, ruleManager, ruleApp.ruleId);
            if (!isFactLikeRule(cached.rule)) {
                continue;
            }
            if (factProb.find(headTuple) != factProb.end()) {
                continue;
            }
            if (!emittedDerivedFacts.insert(headTuple).second) {
                continue;
            }
            emitFact(headTuple, cached.rule ? cached.rule->getProbability() : 1.0);
        }
    }

    out << "],\"rules\":[";
    bool firstRule = true;
    for (const auto& [headTuple, ruleSetPtr] : DerivationManager::untypedTuple2RuleApplications) {
        if (ruleSetPtr == nullptr) {
            continue;
        }
        for (const auto& ruleApp : *ruleSetPtr) {
            const auto& cached = getCachedRuleDumpInfo(ruleCache, ruleManager, ruleApp.ruleId);
            const Rule* rule = cached.rule;
            if (rule == nullptr || isFactLikeRule(rule)) {
                continue;
            }
            if (!firstRule) {
                out << ',';
            }
            firstRule = false;
            out << "{\"head\":";
            writeJsonEscapedString(out, headTuple.toString());
            out << ",\"probability\":" << rule->getProbability();
            out << ",\"rule_id\":" << ruleApp.ruleId;
            out << ",\"mapping\":[";
            for (std::size_t i = 0; i < ruleApp.varValuesPure.size(); ++i) {
                if (i != 0) {
                    out << ',';
                }
                out << ruleApp.varValuesPure[i];
            }
            out << "],\"bodies\":[";
            bool firstBody = true;
            for (const auto& bodyAtom : rule->getBodyAtoms()) {
                if (!firstBody) {
                    out << ',';
                }
                firstBody = false;
                const UntypedTuple bodyTuple{
                        bodyAtom.getRelation(),
                        bodyAtom.instantiatedFields(cached.vars, ruleApp.varValuesPure)};
                out << "{\"negation\":" << (bodyAtom.isNegatedAtom() ? "true" : "false")
                    << ",\"name\":";
                writeJsonEscapedString(out, bodyTuple.toString());
                out << "}";
            }
            out << "]}";
        }
    }
    out << "]}";
    out.close();
    std::cout << "[pipeline] wrote pre-graph ruleapp JSON to " << outputPath << std::endl;
}

static bool needsMaterializedGraph(const CmdOptions& opt) {
    if (!opt.isDerivationOnly()) {
        return true;
    }
    if (opt.isDumpJsonEnabled() || opt.isDumpJsonBeforePruneEnabled() || opt.isDumpDotEnabled() ||
            opt.isDumpStatEnabled()) {
        return true;
    }
    if (opt.isRewriteEnabled() || opt.isImplicitRewriteEnabled() ||
            opt.isImplicitIterateSplitRewriteEnabled()) {
        return true;
    }
    return false;
}

static std::size_t estimateBddVarCount(const SubgraphView& view) {
    auto isSemanticRandomProb = [](double p) {
        return p > 0.0 && p < 1.0;
    };
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (node->isFact && isSemanticRandomProb(node->getProbability())) {
            ++count;
        }
    }
    for (const auto& edge : view.getEdges()) {
        if (isSemanticRandomProb(edge->getProbability())) {
            ++count;
        }
    }
    return count;
}

static IncSubgraphView buildFullIncViewLocal(IncrementalDerivationGraph& graph) {
    return IncSubgraphView(graph.getNodes(), graph.getEdges(), {}, {}, {}, {});
}

static IncSubgraphView buildFullIncViewLocal(
        const std::unordered_set<NodePtr>& nodes, const std::unordered_set<EdgePtr>& edges) {
    return IncSubgraphView(nodes, edges, {}, {}, {}, {});
}

static std::size_t precomputeIsolatedOutputFactsLocal(IncSubgraphView& view) {
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (!node || !node->needOutput || !node->isFact || node->hasEvidence()) {
            continue;
        }
        if (!view.getIncomingEdges(node).empty() || !view.getOutgoingEdges(node).empty()) {
            continue;
        }
        if (precomputedProbResult.count(node)) {
            continue;
        }
        precomputedProbResult[node] = node->getProbability();
        node->needOutput = false;
        node->isQuery = false;
        ++count;
    }
    return count;
}

static ImplicitSplitMode resolveImplicitSplitMode(const std::string& splitMode) {
    if (splitMode == "no-split") {
        return ImplicitSplitMode::None;
    }
    if (splitMode == "complete-split") {
        return ImplicitSplitMode::Complete;
    }
    return ImplicitSplitMode::Naive;
}

static bool isSemanticRandomProb(double p) {
    return p > 0.0 && p < 1.0;
}

enum class NegationPostPassMode {
    RuleApp,
    Graph,
    Off,
};

static NegationPostPassMode resolveNegationPostPassMode() {
    const char* env = std::getenv("SOUFFLE_NEGATION_POSTPASS_MODE");
    if (!env || *env == '\0') {
        return NegationPostPassMode::Off;
    }
    std::string mode(env);
    if (mode == "off" || mode == "none") {
        return NegationPostPassMode::Off;
    }
    if (mode == "graph" || mode == "after-graph") {
        return NegationPostPassMode::Graph;
    }
    return NegationPostPassMode::RuleApp;
}

static const char* negationPostPassModeLabel(NegationPostPassMode mode) {
    switch (mode) {
        case NegationPostPassMode::RuleApp:
            return "ruleapp";
        case NegationPostPassMode::Graph:
            return "graph";
        case NegationPostPassMode::Off:
            return "off";
    }
    return "ruleapp";
}

static std::string normalizeLogicalRelationName(std::string rel) {
    if (rel.rfind("@magic.", 0) == 0 || rel.rfind("@neglabel.", 0) == 0) {
        return rel;
    }

    for (;;) {
        bool changed = false;
        auto strip = [&](const std::string& prefix) {
            if (rel.rfind(prefix, 0) == 0) {
                rel = rel.substr(prefix.size());
                changed = true;
            }
        };
        strip("@split_in.");
        strip("@interm_in.");
        strip("@interm_out.");
        if (rel.rfind("@poscopy_", 0) == 0) {
            auto dot = rel.find('.');
            if (dot != std::string::npos) {
                rel = rel.substr(dot + 1);
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }

    if (!rel.empty()) {
        auto dot = rel.rfind('.');
        if (dot != std::string::npos) {
            auto last = rel.substr(dot + 1);
            if (last.size() >= 2 && last.front() == '{' && last.back() == '}') {
                bool ok = true;
                for (std::size_t i = 1; i + 1 < last.size(); ++i) {
                    if (last[i] != 'b' && last[i] != 'f') {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    rel = rel.substr(0, dot);
                }
            }
        }
    }

    return rel;
}

static std::unordered_map<std::string, bool> computeProbabilisticRelations(
        const SouffleProgram& program,
        RuleManager& ruleManager,
        const std::unordered_map<UntypedTuple, double>& factProb) {
    std::unordered_map<std::string, bool> probabilistic;

    for (const auto* rel : program.getAllRelations()) {
        if (!rel) continue;
        probabilistic.try_emplace(normalizeLogicalRelationName(rel->getName()), false);
    }
    for (const auto& [tuple, prob] : factProb) {
        auto rel = normalizeLogicalRelationName(tuple.relation_name);
        probabilistic.try_emplace(rel, false);
        if (isSemanticRandomProb(prob)) {
            probabilistic[rel] = true;
        }
    }

    const auto rules = ruleManager.getAllRules();
    for (const auto* rule : rules) {
        if (!rule) continue;
        probabilistic.try_emplace(normalizeLogicalRelationName(rule->getHead().getRelation()), false);
        for (const auto& bodyAtom : rule->getBodyAtoms()) {
            probabilistic.try_emplace(normalizeLogicalRelationName(bodyAtom.getRelation()), false);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto* rule : rules) {
            if (!rule) continue;
            const std::string headRel = normalizeLogicalRelationName(rule->getHead().getRelation());
            bool ruleIsProb = isSemanticRandomProb(rule->getProbability());
            if (!ruleIsProb) {
                for (const auto& bodyAtom : rule->getBodyAtoms()) {
                    const std::string bodyRel = normalizeLogicalRelationName(bodyAtom.getRelation());
                    auto it = probabilistic.find(bodyRel);
                    if (it != probabilistic.end() && it->second) {
                        ruleIsProb = true;
                        break;
                    }
                }
            }
            if (ruleIsProb && !probabilistic[headRel]) {
                probabilistic[headRel] = true;
                changed = true;
            }
        }
    }

    return probabilistic;
}

static bool isDeterministicRelation(
        const std::unordered_map<std::string, bool>& probabilisticRelations,
        const std::string& relation) {
    auto it = probabilisticRelations.find(normalizeLogicalRelationName(relation));
    return it == probabilisticRelations.end() || !it->second;
}

static bool resolveFieldValue(
        const SymbolicField& field,
        const std::vector<std::string>& vars,
        const std::vector<souffle::RamDomain>& values,
        souffle::RamDomain& out,
        bool& wildcard) {
    wildcard = false;
    if (std::holds_alternative<IntegerField>(field.field)) {
        out = std::get<IntegerField>(field.field).value;
        return true;
    }
    if (std::holds_alternative<FloatField>(field.field)) {
        out = souffle::ramBitCast<souffle::RamDomain>(
                static_cast<souffle::RamFloat>(std::get<FloatField>(field.field).value));
        return true;
    }
    if (std::holds_alternative<VariableField>(field.field)) {
        const auto& name = std::get<VariableField>(field.field).name;
        if (name == "_") {
            wildcard = true;
            return true;
        }
        for (std::size_t i = 0; i < vars.size() && i < values.size(); ++i) {
            if (vars[i] == name) {
                out = values[i];
                return true;
            }
        }
        wildcard = true;
        return true;
    }
    if (std::holds_alternative<std::shared_ptr<ExprField>>(field.field)) {
        out = std::get<std::shared_ptr<ExprField>>(field.field)->evaluate(vars, values);
        return true;
    }
    return false;
}

static bool tupleMatchesAtom(
        const UntypedTuple& tuple,
        const Atom& atom,
        const std::vector<std::string>& vars,
        const std::vector<souffle::RamDomain>& values) {
    if (tuple.relation_name != atom.getRelation()) {
        return false;
    }
    const auto& atomFields = atom.getFields();
    if (tuple.fields.size() != atomFields.size()) {
        return false;
    }

    for (std::size_t i = 0; i < atomFields.size(); ++i) {
        souffle::RamDomain expected = 0;
        bool wildcard = false;
        if (!resolveFieldValue(atomFields[i], vars, values, expected, wildcard)) {
            return false;
        }
        if (!wildcard && tuple.fields[i] != expected) {
            return false;
        }
    }
    return true;
}

static bool instantiateExactAtomTuple(
        const Atom& atom,
        const std::vector<std::string>& vars,
        const std::vector<souffle::RamDomain>& values,
        UntypedTuple& outTuple) {
    outTuple.relation_name = atom.getRelation();
    outTuple.fields.clear();
    outTuple.fields.reserve(atom.getFields().size());
    for (const auto& field : atom.getFields()) {
        souffle::RamDomain value = 0;
        bool wildcard = false;
        if (!resolveFieldValue(field, vars, values, value, wildcard) || wildcard) {
            outTuple.fields.clear();
            return false;
        }
        outTuple.fields.push_back(value);
    }
    return true;
}

static bool atomHasLiveMatch(
        const Atom& atom,
        const std::vector<std::string>& vars,
        const std::vector<souffle::RamDomain>& values,
        const std::unordered_map<std::string, std::vector<const UntypedTuple*>>& liveByRelation) {
    auto it = liveByRelation.find(atom.getRelation());
    if (it == liveByRelation.end()) {
        return false;
    }
    for (const UntypedTuple* tuple : it->second) {
        if (tuple && tupleMatchesAtom(*tuple, atom, vars, values)) {
            return true;
        }
    }
    return false;
}

static bool isSeedFactTuple(
        const UntypedTuple& tuple,
        const std::unordered_set<UntypedTuple>& seedFacts) {
    return seedFacts.find(tuple) != seedFacts.end();
}

static void rewriteProgramRelations(
        SouffleProgram& program,
        const std::unordered_set<UntypedTuple>& liveTuples) {
    std::unordered_map<std::string, std::vector<const UntypedTuple*>> tuplesByRelation;
    tuplesByRelation.reserve(liveTuples.size());
    for (const auto& tuple : liveTuples) {
        tuplesByRelation[tuple.relation_name].push_back(&tuple);
    }

    for (auto* rel : program.getAllRelations()) {
        if (!rel) continue;
        const std::string rawName = rel->getName();
        const std::string normalizedName = normalizeLogicalRelationName(rawName);
        rel->purge();

        auto it = tuplesByRelation.find(rawName);
        if (it == tuplesByRelation.end() && normalizedName != rawName) {
            it = tuplesByRelation.find(normalizedName);
        }
        if (it == tuplesByRelation.end()) {
            continue;
        }

        for (const UntypedTuple* tuplePtr : it->second) {
            if (!tuplePtr) continue;
            const auto& fields = tuplePtr->fields;
            if (fields.size() != rel->getArity()) {
                continue;
            }
            souffle::tuple typed(rel);
            for (std::size_t i = 0; i < fields.size(); ++i) {
                typed[i] = fields[i];
            }
            rel->insert(typed);
        }
    }
}

static std::size_t stripDeterministicRuleApps(
        const std::unordered_map<std::string, bool>& probabilisticRelations) {
    std::size_t removed = 0;
    for (auto it = DerivationManager::untypedTuple2RuleApplications.begin();
            it != DerivationManager::untypedTuple2RuleApplications.end();) {
        const std::string rel = it->first.relation_name;
        const bool isProb = probabilisticRelations.find(rel) != probabilisticRelations.end() &&
                probabilisticRelations.at(rel);
        if (!isProb) {
            if (it->second != nullptr) {
                removed += it->second->size();
                delete it->second;
            }
            it = DerivationManager::untypedTuple2RuleApplications.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
}

struct NegationRuleAppState {
    UntypedTuple head;
    RuleApplication ruleApp;
    const Rule* rule = nullptr;
    std::vector<UntypedTuple> positiveBodies;
    std::vector<UntypedTuple> deterministicNegatedBodies;
    bool live = true;
};

static std::unordered_map<std::string, std::vector<const UntypedTuple*>> buildLiveByRelation(
        const std::unordered_set<UntypedTuple>& liveTuples) {
    std::unordered_map<std::string, std::vector<const UntypedTuple*>> liveByRelation;
    liveByRelation.reserve(liveTuples.size());
    for (const auto& tuple : liveTuples) {
        liveByRelation[tuple.relation_name].push_back(&tuple);
    }
    return liveByRelation;
}

static std::unordered_set<UntypedTuple> buildSeedFactTuples(
        const std::unordered_map<UntypedTuple, double>& factProb,
        RuleManager& ruleManager) {
    std::unordered_set<UntypedTuple> seedFacts;
    seedFacts.reserve(factProb.size() + inputFactSet.size());
    for (const auto& [tuple, _] : factProb) {
        seedFacts.insert(tuple);
    }
    seedFacts.insert(inputFactSet.begin(), inputFactSet.end());
    for (const auto& [headTuple, ruleSetPtr] : DerivationManager::untypedTuple2RuleApplications) {
        if (ruleSetPtr == nullptr) {
            continue;
        }
        for (const auto& ruleApp : *ruleSetPtr) {
            const Rule* rule = ruleManager.getRule(ruleApp.ruleId);
            if (rule != nullptr && rule->isFact()) {
                seedFacts.insert(headTuple);
                break;
            }
        }
    }
    return seedFacts;
}

static bool runRuleAppNegationPostPass(
        SouffleProgram& program,
        RuleManager& ruleManager,
        const std::unordered_map<std::string, bool>& probabilisticRelations) {
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();

    std::unordered_set<UntypedTuple> seedFacts;
    seedFacts.reserve(fact_prob.size() + inputFactSet.size());
    for (const auto& [tuple, _] : fact_prob) {
        seedFacts.insert(tuple);
    }
    seedFacts.insert(inputFactSet.begin(), inputFactSet.end());

    std::vector<NegationRuleAppState> states;
    std::unordered_map<UntypedTuple, std::vector<std::size_t>> positiveConsumers;
    std::unordered_map<UntypedTuple, std::size_t> supportCount;
    std::unordered_set<UntypedTuple> liveTuples = seedFacts;
    std::vector<std::size_t> slowStates;

    std::size_t totalRuleApps = 0;
    for (const auto& [headTuple, ruleSetPtr] : DerivationManager::untypedTuple2RuleApplications) {
        liveTuples.insert(headTuple);
        if (ruleSetPtr) {
            totalRuleApps += ruleSetPtr->size();
        }
    }
    states.reserve(totalRuleApps);
    positiveConsumers.reserve(totalRuleApps);
    supportCount.reserve(DerivationManager::untypedTuple2RuleApplications.size());

    {
        auto liveByRelation = buildLiveByRelation(liveTuples);
        for (const auto& [headTuple, ruleSetPtr] : DerivationManager::untypedTuple2RuleApplications) {
            if (ruleSetPtr == nullptr) {
                continue;
            }
            for (const auto& ruleApp : *ruleSetPtr) {
                NegationRuleAppState state;
                state.head = headTuple;
                state.ruleApp = ruleApp;
                state.rule = ruleManager.getRule(ruleApp.ruleId);
                bool valid = state.rule != nullptr;
                bool exactOnly = true;
                if (valid) {
                    const auto vars = state.rule->getVars();
                    if (!tupleMatchesAtom(headTuple, state.rule->getHead(), vars, ruleApp.varValuesPure)) {
                        valid = false;
                    } else {
                        for (const auto& bodyAtom : state.rule->getBodyAtoms()) {
                            UntypedTuple bodyTuple;
                            const bool exact = instantiateExactAtomTuple(
                                    bodyAtom, vars, ruleApp.varValuesPure, bodyTuple);
                            if (bodyAtom.isNegatedAtom()) {
                                if (isDeterministicRelation(probabilisticRelations, bodyAtom.getRelation())) {
                                    if (exact) {
                                        state.deterministicNegatedBodies.push_back(bodyTuple);
                                        if (liveTuples.count(bodyTuple)) {
                                            valid = false;
                                            break;
                                        }
                                    } else {
                                        exactOnly = false;
                                        if (atomHasLiveMatch(bodyAtom, vars, ruleApp.varValuesPure, liveByRelation)) {
                                            valid = false;
                                            break;
                                        }
                                    }
                                }
                            } else if (exact) {
                                state.positiveBodies.push_back(bodyTuple);
                            } else {
                                exactOnly = false;
                            }
                        }
                    }
                }

                const std::size_t idx = states.size();
                state.live = valid;
                states.push_back(std::move(state));
                if (!exactOnly) {
                    slowStates.push_back(idx);
                }
                if (valid) {
                    supportCount[headTuple] += 1;
                    for (const auto& bodyTuple : states[idx].positiveBodies) {
                        positiveConsumers[bodyTuple].push_back(idx);
                    }
                }
            }
        }
    }

    std::queue<UntypedTuple> deadTuples;
    std::size_t removedRuleApps = 0;
    std::size_t removedTuples = 0;
    bool changedAny = false;

    auto maybeDeleteHead = [&](const UntypedTuple& head) {
        if (seedFacts.count(head)) {
            return;
        }
        auto it = supportCount.find(head);
        const bool noSupport = (it == supportCount.end()) || (it->second == 0);
        if (noSupport && liveTuples.erase(head) > 0) {
            deadTuples.push(head);
            ++removedTuples;
            changedAny = true;
        }
    };

    auto invalidateState = [&](std::size_t idx) {
        auto& state = states[idx];
        if (!state.live) {
            return;
        }
        state.live = false;
        ++removedRuleApps;
        changedAny = true;
        auto it = supportCount.find(state.head);
        if (it != supportCount.end() && it->second > 0) {
            it->second -= 1;
            if (it->second == 0) {
                maybeDeleteHead(state.head);
            }
        }
    };

    for (const auto& [headTuple, _] : DerivationManager::untypedTuple2RuleApplications) {
        maybeDeleteHead(headTuple);
    }

    while (!deadTuples.empty()) {
        const UntypedTuple tuple = deadTuples.front();
        deadTuples.pop();

        auto depIt = positiveConsumers.find(tuple);
        if (depIt != positiveConsumers.end()) {
            for (std::size_t idx : depIt->second) {
                invalidateState(idx);
            }
        }

        if (!slowStates.empty()) {
            auto liveByRelation = buildLiveByRelation(liveTuples);
            for (std::size_t idx : slowStates) {
                auto& state = states[idx];
                if (!state.live || state.rule == nullptr) {
                    continue;
                }
                const auto vars = state.rule->getVars();
                bool valid = true;
                for (const auto& bodyAtom : state.rule->getBodyAtoms()) {
                    if (bodyAtom.isNegatedAtom()) {
                        if (isDeterministicRelation(probabilisticRelations, bodyAtom.getRelation()) &&
                                atomHasLiveMatch(bodyAtom, vars, state.ruleApp.varValuesPure, liveByRelation)) {
                            valid = false;
                            break;
                        }
                    } else if (!atomHasLiveMatch(bodyAtom, vars, state.ruleApp.varValuesPure, liveByRelation)) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) {
                    invalidateState(idx);
                }
            }
        }
    }

    for (auto it = DerivationManager::untypedTuple2RuleApplications.begin();
            it != DerivationManager::untypedTuple2RuleApplications.end();) {
        const UntypedTuple head = it->first;
        auto* ruleSet = it->second;
        if (!liveTuples.count(head) && !seedFacts.count(head)) {
            if (ruleSet != nullptr) {
                delete ruleSet;
            }
            it = DerivationManager::untypedTuple2RuleApplications.erase(it);
            continue;
        }
        if (ruleSet != nullptr) {
            std::unordered_set<RuleApplication> filtered;
            filtered.reserve(ruleSet->size());
            for (const auto& state : states) {
                if (state.live && state.head == head) {
                    filtered.insert(state.ruleApp);
                }
            }
            *ruleSet = std::move(filtered);
        }
        ++it;
    }

    std::size_t strippedDetRuleApps = 0;
    if (detOptEnabled) {
        strippedDetRuleApps = stripDeterministicRuleApps(probabilisticRelations);
    }

    if (changedAny) {
        rewriteProgramRelations(program, liveTuples);
    }

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    std::cout << "[negation-post-pass] mode=ruleapp"
              << " ruleapps=" << totalRuleApps
              << " removed_ruleapps=" << removedRuleApps
              << " removed_tuples=" << removedTuples
              << " slow_states=" << slowStates.size()
              << " stripped_det_ruleapps=" << strippedDetRuleApps
              << " time_ms=" << elapsedMs
              << std::endl;

    return changedAny;
}

static IncSubgraphView runGraphNegationPostPass(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        const std::unordered_map<std::string, bool>& probabilisticRelations,
        const std::unordered_set<UntypedTuple>& seedTuples,
        IncrementalDerivationGraph& graph,
        bool& changedAny) {
    // This path is intentionally quarantined. The current graph-mode negation
    // post-pass does more than delete edges blocked by deterministic negation:
    // it rebuilds a live subgraph and can perturb programs that should be
    // semantic no-ops. Keep the implementation below for reference while the
    // mode stays disabled by default, but fail hard if it is re-enabled
    // accidentally before a simpler deletion-driven redesign lands.
    assert(false && "graph-mode negation post-pass is intentionally disabled");
    fatal("graph-mode negation post-pass is intentionally disabled until redesign");
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    const bool hardPruneAllNegations = opt.isDerivationOnly();

    struct CandidateEdgeState {
        EdgePtr edge;
        NodePtr head;
        std::size_t pending_same_stratum_positive = 0;
        bool blocked = false;
        bool active = false;
    };

    const IncSubgraphView fullView = buildFullIncViewLocal(graph);

    std::unordered_map<std::string, std::size_t> relationStrata;
    relationStrata.reserve(ruleManager.getAllRules().size() * 2 + fullView.getNodes().size());
    struct RelationDep {
        std::string head;
        std::string body;
        bool negated = false;
    };
    std::vector<RelationDep> deps;
    deps.reserve(ruleManager.getAllRules().size() * 4);

    for (const auto* rule : ruleManager.getAllRules()) {
        if (!rule) {
            continue;
        }
        const std::string headRel = normalizeLogicalRelationName(rule->getHead().getRelation());
        relationStrata.try_emplace(headRel, 0);
        for (const auto& bodyAtom : rule->getBodyAtoms()) {
            const std::string bodyRel = normalizeLogicalRelationName(bodyAtom.getRelation());
            relationStrata.try_emplace(bodyRel, 0);
            deps.push_back({headRel, bodyRel,
                    bodyAtom.isNegatedAtom() &&
                            (hardPruneAllNegations ||
                                    isDeterministicRelation(
                                            probabilisticRelations, bodyAtom.getRelation()))});
        }
    }
    for (const auto& node : fullView.getNodes()) {
        if (!node) {
            continue;
        }
        relationStrata.try_emplace(normalizeLogicalRelationName(node->getTuple().relation_name), 0);
    }

    const std::size_t maxIterations =
            std::max<std::size_t>(1, relationStrata.size()) * std::max<std::size_t>(1, deps.size() + 1);
    bool strataChanged = true;
    for (std::size_t iter = 0; iter < maxIterations && strataChanged; ++iter) {
        strataChanged = false;
        for (const auto& dep : deps) {
            const std::size_t required = relationStrata[dep.body] + (dep.negated ? 1 : 0);
            if (relationStrata[dep.head] < required) {
                relationStrata[dep.head] = required;
                strataChanged = true;
            }
        }
    }
    if (strataChanged) {
        throw std::runtime_error(
                "negation post-pass graph mode detected non-stratified negation while building the graph");
    }

    auto relationStratumOf = [&](const std::string& relation) -> std::size_t {
        const std::string normalized = normalizeLogicalRelationName(relation);
        auto it = relationStrata.find(normalized);
        return it == relationStrata.end() ? 0 : it->second;
    };
    const char* debugHeadEnv = std::getenv("SOUFFLE_NEGATION_DEBUG_HEAD");
    const std::string debugHead = debugHeadEnv ? std::string(debugHeadEnv) : std::string();

    std::size_t maxStratum = 0;
    for (const auto& [_, stratum] : relationStrata) {
        maxStratum = std::max(maxStratum, stratum);
    }

    std::unordered_set<NodePtr> liveNodes;
    std::unordered_set<UntypedTuple> liveTuples;
    liveNodes.reserve(fullView.getNodes().size());
    liveTuples.reserve(fullView.getNodes().size());
    for (const auto& node : fullView.getNodes()) {
        if (node && seedTuples.count(node->getTuple()) > 0) {
            liveNodes.insert(node);
            liveTuples.insert(node->getTuple());
        }
    }

    std::vector<std::vector<EdgePtr>> edgesByStratum(maxStratum + 1);
    for (const auto& edge : fullView.getEdges()) {
        NodePtr head = fullView.getOutput(edge);
        if (!head) {
            continue;
        }
        edgesByStratum[relationStratumOf(head->getTuple().relation_name)].push_back(edge);
    }

    std::unordered_set<EdgePtr> activeEdges;
    activeEdges.reserve(fullView.getEdges().size());

    auto activateEdge = [&](CandidateEdgeState& state, std::queue<NodePtr>& workQueue) {
        if (state.active || state.blocked) {
            return;
        }
        state.active = true;
        activeEdges.insert(state.edge);
        if (liveNodes.insert(state.head).second) {
            liveTuples.insert(state.head->getTuple());
            workQueue.push(state.head);
        }
    };

    for (std::size_t currentStratum = 0; currentStratum <= maxStratum; ++currentStratum) {
        const auto& stratumEdges = edgesByStratum[currentStratum];
        std::vector<CandidateEdgeState> states;
        states.reserve(stratumEdges.size());
        std::unordered_map<NodePtr, std::vector<std::size_t>> sameStratumPositiveConsumers;
        sameStratumPositiveConsumers.reserve(stratumEdges.size());
        std::queue<NodePtr> workQueue;

        for (const auto& edge : stratumEdges) {
            NodePtr head = fullView.getOutput(edge);
            if (!head) {
                continue;
            }
            CandidateEdgeState state{edge, head, 0, false, false};
            const auto inputs = fullView.getInputs(edge);
            const auto negs = fullView.getBodyNegations(edge);

            for (std::size_t i = 0; i < inputs.size(); ++i) {
                NodePtr input = inputs[i];
                if (!input) {
                    state.blocked = true;
                    break;
                }
                const bool isNegated = i < negs.size() && negs[i];
                const std::size_t inputStratum = relationStratumOf(input->getTuple().relation_name);

                if (isNegated) {
                    if (hardPruneAllNegations ||
                            isDeterministicRelation(
                                    probabilisticRelations, input->getTuple().relation_name)) {
                        if (inputStratum >= currentStratum) {
                            throw std::runtime_error(
                                    "negation post-pass graph mode expected negated relation to be in a lower stratum");
                        }
                        if (liveNodes.count(input) > 0) {
                            state.blocked = true;
                            break;
                        }
                    }
                    // Outside derivation-only mode, probabilistic negation is preserved in the
                    // derivation graph and handled later by the probability pipeline; graph
                    // pruning should not treat it as a positive dependency.
                    continue;
                }

                if (liveNodes.count(input) > 0) {
                    continue;
                }
                if (inputStratum == currentStratum) {
                    sameStratumPositiveConsumers[input].push_back(states.size());
                    state.pending_same_stratum_positive += 1;
                } else {
                    state.blocked = true;
                    break;
                }
            }

            if (!debugHead.empty() && head->getTuple().relation_name == debugHead) {
                std::cerr << "[neg-debug] head=" << head->getTuple().toString()
                          << " blocked=" << state.blocked
                          << " pending_same=" << state.pending_same_stratum_positive
                          << std::endl;
                for (std::size_t i = 0; i < inputs.size(); ++i) {
                    const bool isNegated = i < negs.size() && negs[i];
                    std::cerr << "  input=" << inputs[i]->getTuple().toString()
                              << " neg=" << isNegated
                              << " live=" << (liveNodes.count(inputs[i]) > 0)
                              << " det=" << isDeterministicRelation(
                                         probabilisticRelations, inputs[i]->getTuple().relation_name)
                              << " stratum=" << relationStratumOf(inputs[i]->getTuple().relation_name)
                              << std::endl;
                }
            }

            states.push_back(std::move(state));
        }

        for (auto& state : states) {
            if (!state.blocked && state.pending_same_stratum_positive == 0) {
                activateEdge(state, workQueue);
            }
        }

        while (!workQueue.empty()) {
            NodePtr node = workQueue.front();
            workQueue.pop();
            auto consumerIt = sameStratumPositiveConsumers.find(node);
            if (consumerIt == sameStratumPositiveConsumers.end()) {
                continue;
            }
            for (std::size_t idx : consumerIt->second) {
                auto& state = states[idx];
                if (state.blocked || state.active || state.pending_same_stratum_positive == 0) {
                    continue;
                }
                state.pending_same_stratum_positive -= 1;
                if (state.pending_same_stratum_positive == 0) {
                    activateEdge(state, workQueue);
                }
            }
        }
    }

    std::unordered_set<NodePtr> filteredNodes = liveNodes;
    for (const auto& edge : activeEdges) {
        NodePtr head = fullView.getOutput(edge);
        if (head) {
            filteredNodes.insert(head);
        }
        for (const auto& input : fullView.getInputs(edge)) {
            if (input && liveNodes.count(input) > 0) {
                filteredNodes.insert(input);
            }
        }
    }

    changedAny = filteredNodes.size() != fullView.getNodes().size() ||
            activeEdges.size() != fullView.getEdges().size();

    if (changedAny) {
        rewriteProgramRelations(program, liveTuples);
        if (opt.getOutputFileDir() != "-") {
            program.printAll(opt.getOutputFileDir());
        } else {
            std::cerr << "[negation-post-pass] mode=graph output-dir is '-', skipping sanitized re-print"
                      << std::endl;
        }
    }

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    std::cout << "[negation-post-pass] mode=graph"
              << " stage=create-graph"
              << " nodes=" << fullView.getNodes().size()
              << " edges=" << fullView.getEdges().size()
              << " kept_nodes=" << filteredNodes.size()
              << " kept_edges=" << activeEdges.size()
              << " max_stratum=" << maxStratum
              << " time_ms=" << elapsedMs
              << std::endl;

    return IncSubgraphView(std::move(filteredNodes), std::move(activeEdges), {}, {}, {}, {});
}

static IncSubgraphView pruneFilteredIncView(
        IncSubgraphView view, const std::vector<souffle::Relation*>& outputRelations) {
    std::unordered_set<std::string> outputRelationNames;
    outputRelationNames.reserve(outputRelations.size() * 2 + 4);
    for (const auto* rel : outputRelations) {
        if (!rel) {
            continue;
        }
        outputRelationNames.insert(rel->getName());
        outputRelationNames.insert(normalizeLogicalRelationName(rel->getName()));
    }

    std::unordered_set<NodePtr> reachableNodes;
    std::unordered_set<EdgePtr> reachableEdges;
    reachableNodes.reserve(view.getNodes().size());
    reachableEdges.reserve(view.getEdges().size());
    std::queue<NodePtr> workQueue;

    for (const auto& node : view.getNodes()) {
        if (!node) {
            continue;
        }
        const std::string rel = node->getTuple().relation_name;
        if (node->isQueryNode() || outputRelationNames.count(rel) > 0 ||
                outputRelationNames.count(normalizeLogicalRelationName(rel)) > 0) {
            if (reachableNodes.insert(node).second) {
                workQueue.push(node);
            }
            node->setQuery();
        }
        if (node->hasEvidence() && reachableNodes.insert(node).second) {
            workQueue.push(node);
        }
    }

    while (!workQueue.empty()) {
        NodePtr current = workQueue.front();
        workQueue.pop();
        if (!current || current->isFact) {
            continue;
        }
        for (const auto& edge : view.getIncomingEdges(current)) {
            if (edge->hasSelfDependency()) {
                continue;
            }
            reachableEdges.insert(edge);
            for (const auto& inputNode : view.getInputs(edge)) {
                if (inputNode && reachableNodes.insert(inputNode).second) {
                    workQueue.push(inputNode);
                }
            }
        }
    }

    return IncSubgraphView(std::move(reachableNodes), std::move(reachableEdges), {}, {}, {}, {});
}

static void markGraphPrunedFlags(IncrementalDerivationGraph& graph, const IncSubgraphView& view) {
    const auto& liveNodes = view.getNodes();
    const auto& liveEdges = view.getEdges();
    for (const auto& node : graph.getNodes()) {
        if (node) {
            node->pruned = liveNodes.count(node) == 0;
        }
    }
    for (const auto& edge : graph.getEdges()) {
        if (edge) {
            edge->pruned = liveEdges.count(edge) == 0;
        }
    }
}

struct GraphSummary {
    std::size_t nodes = 0;
    std::size_t edges = 0;
    std::size_t factNodes = 0;
    std::size_t derivedNodes = 0;
    std::size_t queryNodes = 0;
    std::size_t outputNodes = 0;
    std::size_t evidenceNodes = 0;
    std::size_t shadowNodes = 0;
    std::size_t probabilisticFactNodes = 0;
    std::size_t probabilisticEdges = 0;
    std::size_t randomVariables = 0;
    std::size_t disjunctionNodes = 0;
    std::size_t maxInDegree = 0;
    std::size_t maxOutDegree = 0;
    std::size_t maxHyperedgeInputs = 0;
};

static GraphSummary summarizeGraphLight(const DerivationGraphViewInterface& view) {
    GraphSummary s;
    const auto& nodes = view.getNodes();
    const auto& edges = view.getEdges();
    s.nodes = nodes.size();
    s.edges = edges.size();

    std::unordered_map<NodePtr, std::size_t> indeg;
    std::unordered_map<NodePtr, std::size_t> outdeg;
    indeg.reserve(nodes.size());
    outdeg.reserve(nodes.size());
    for (const auto& n : nodes) {
        indeg.emplace(n, 0);
        outdeg.emplace(n, 0);
    }

    for (const auto& e : edges) {
        const auto inputs = view.getInputs(e);
        s.maxHyperedgeInputs = std::max(s.maxHyperedgeInputs, inputs.size());
        NodePtr out = view.getOutput(e);
        if (out) {
            auto it = indeg.find(out);
            if (it != indeg.end()) {
                ++it->second;
            }
        }
        for (const auto& in : inputs) {
            auto it = outdeg.find(in);
            if (it != outdeg.end()) {
                ++it->second;
            }
        }
        if (!e->isDeterministic()) {
            ++s.probabilisticEdges;
        }
    }

    for (const auto& n : nodes) {
        if (n->isFact) {
            ++s.factNodes;
            if (n->getProbability() < 1.0) {
                ++s.probabilisticFactNodes;
            }
        } else {
            ++s.derivedNodes;
        }
        if (n->isQuery) {
            ++s.queryNodes;
        }
        if (n->needOutput) {
            ++s.outputNodes;
        }
        if (n->hasEvidence()) {
            ++s.evidenceNodes;
        }
        if (n->isShadow) {
            ++s.shadowNodes;
        }
        const auto inIt = indeg.find(n);
        const auto outIt = outdeg.find(n);
        const std::size_t in = (inIt == indeg.end()) ? 0 : inIt->second;
        const std::size_t out = (outIt == outdeg.end()) ? 0 : outIt->second;
        s.maxInDegree = std::max(s.maxInDegree, in);
        s.maxOutDegree = std::max(s.maxOutDegree, out);
        if ((!n->isFact && in > 1) || (n->isFact && in > 0)) {
            ++s.disjunctionNodes;
        }
    }
    s.randomVariables = s.probabilisticFactNodes + s.probabilisticEdges;
    return s;
}

static void addGraphSummaryInfo(
        Debugger& debugger, const std::string& prefix, const GraphSummary& s) {
    auto add = [&](const std::string& key, const std::size_t value) {
        debugger.addInfo(prefix + key, std::to_string(value));
    };
    add("nodes", s.nodes);
    add("edges", s.edges);
    add("fact_nodes", s.factNodes);
    add("derived_nodes", s.derivedNodes);
    add("query_nodes", s.queryNodes);
    add("output_nodes", s.outputNodes);
    add("evidence_nodes", s.evidenceNodes);
    add("shadow_nodes", s.shadowNodes);
    add("prob_fact_nodes", s.probabilisticFactNodes);
    add("prob_rule_edges", s.probabilisticEdges);
    add("random_variables", s.randomVariables);
    add("disjunction_nodes", s.disjunctionNodes);
    add("max_in_degree", s.maxInDegree);
    add("max_out_degree", s.maxOutDegree);
    add("max_hyperedge_inputs", s.maxHyperedgeInputs);
}

static void recordFcHeartbeat(
        Debugger& debugger,
        StageInfo* stage,
        const FcHeartbeatSnapshot& hb,
        const std::string& mode,
        std::size_t slowDone = 0,
        std::size_t slowTotal = 0,
        std::size_t componentId = std::numeric_limits<std::size_t>::max(),
        std::size_t liveNodes = 0) {
    debugger.addInfo("fc_heartbeat_mode", mode);
    debugger.addInfo("fc_heartbeat_elapsed_ms", std::to_string(hb.elapsedMs));
    debugger.addInfo("fc_heartbeat_round", std::to_string(hb.round));
    debugger.addInfo("fc_heartbeat_cycle_id", std::to_string(hb.currentCycleId));
    debugger.addInfo("fc_heartbeat_cycles_done", std::to_string(hb.completedCycles));
    debugger.addInfo("fc_heartbeat_cycles_total", std::to_string(hb.totalCycles));
    debugger.addInfo("fc_heartbeat_worklist_size", std::to_string(hb.worklistSize));
    debugger.addInfo("fc_heartbeat_ready_queue_size", std::to_string(hb.readyQueueSize));
    debugger.addInfo("fc_heartbeat_node_formulas", std::to_string(hb.nodeFormulaCount));
    debugger.addInfo("fc_heartbeat_edge_formulas", std::to_string(hb.edgeFormulaCount));
    debugger.addInfo("fc_heartbeat_live_nodes", std::to_string(liveNodes));
    if (slowTotal > 0) {
        debugger.addInfo("fc_heartbeat_slow_components_done", std::to_string(slowDone));
        debugger.addInfo("fc_heartbeat_slow_components_total", std::to_string(slowTotal));
    }
    if (componentId != std::numeric_limits<std::size_t>::max()) {
        debugger.addInfo("fc_heartbeat_component_id", std::to_string(componentId));
    }
    if (stage) {
        std::string msg = "heartbeat mode=" + mode + " elapsed_ms=" + std::to_string(hb.elapsedMs) +
                " round=" + std::to_string(hb.round) + " cycle=" + std::to_string(hb.currentCycleId) +
                " cycles=" + std::to_string(hb.completedCycles) + "/" + std::to_string(hb.totalCycles) +
                " worklist=" + std::to_string(hb.worklistSize) +
                " ready=" + std::to_string(hb.readyQueueSize) +
                " node_formulas=" + std::to_string(hb.nodeFormulaCount) +
                " edge_formulas=" + std::to_string(hb.edgeFormulaCount) +
                " live_nodes=" + std::to_string(liveNodes);
        if (componentId != std::numeric_limits<std::size_t>::max()) {
            msg += " component=" + std::to_string(componentId);
        }
        if (slowTotal > 0) {
            msg += " slow_components=" + std::to_string(slowDone) + "/" + std::to_string(slowTotal);
        }
        stage->logMessage(Level::INFO, msg);
    }
    debugger.dumpReportJsonToFile();
}

static WeightedBDDManager::InitConfig makeCuddInitConfig(std::size_t varCount) {
    WeightedBDDManager::InitConfig cfg;
    const auto maxVars = std::numeric_limits<unsigned int>::max();
    const auto doubledVars = varCount > maxVars / 2 ? maxVars : static_cast<unsigned int>(varCount * 2);
    cfg.numVars = doubledVars;
    cfg.numSlots = 512;
    // Smaller graphs downscale cache/memory; large graphs keep the default (largest) config.
    if (varCount <= 256) {
        cfg.cacheSize = 1u << 18;
        cfg.maxMemory = 1UL << 30;
    } else if (varCount <= 1024) {
        cfg.cacheSize = 1u << 20;
        cfg.maxMemory = 4UL << 30;
    } else if (varCount <= 4096) {
        cfg.cacheSize = 1u << 22;
        cfg.maxMemory = 8UL << 30;
    } else if (varCount > 10000) {
        cfg.cacheSize = 1u << 26;
    }
    return cfg;
}

static std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += sep;
        }
        out += parts[i];
    }
    return out;
}

static std::vector<std::vector<std::pair<NodePtr, bool>>> groupEvidencesByComponent(
        const DerivationGraphViewInterface& view,
        const std::vector<std::pair<NodePtr, bool>>& evidences) {
    auto& depGraph = view.getCycleDependencyGraph();
    const size_t componentCount = depGraph.getComponentCount();
    std::vector<std::vector<std::pair<NodePtr, bool>>> byComponent(componentCount);
    std::vector<std::unordered_map<NodePtr, bool>> seen(componentCount);

    for (const auto& ev : evidences) {
        const NodePtr& node = ev.first;
        if (!node || view.getNodes().count(node) == 0) {
            throw std::runtime_error("Evidence node not found in view: " +
                    (node ? node->toString() : std::string("null")));
        }
        size_t cid = depGraph.getComponentId(node);
        auto& seenMap = seen[cid];
        auto it = seenMap.find(node);
        if (it != seenMap.end()) {
            if (it->second != ev.second) {
                throw std::runtime_error("Conflicting evidence for node: " + node->toString());
            }
            continue;
        }
        seenMap.emplace(node, ev.second);
        byComponent[cid].push_back(ev);
    }
    return byComponent;
}

struct FastComponentEval {
    ComponentSubgraph comp;
    SingleRandVarInfo var;
    std::unordered_map<NodePtr, bool> valuesTrue;
    std::unordered_map<NodePtr, bool> valuesFalse;
    long long evalMsTrue = 0;
    long long evalMsFalse = 0;
};

struct ConjComponentEval {
    ComponentSubgraph comp;
    std::unordered_map<NodePtr, double> probabilities;
    long long evalMs = 0;
};

struct ComponentAnalysis {
    ComponentSubgraph comp;
    SingleRandVarInfo singleRand;
    std::size_t randVars = 0;
    bool hasNegation = false;
    bool hasOr = false;
    bool hasCycle = false;
};

struct SlowComponentEval {
    ComponentSubgraph comp;
    std::size_t randVars = 0;
};

struct FastComponentStats {
    size_t candidates = 0;
    size_t used = 0;
    size_t skipped = 0;
    long long evalMs = 0;
};

struct ConjFastStats {
    size_t candidates = 0;
    size_t used = 0;
    size_t skipped = 0;
    long long evalMs = 0;
};

struct ComponentDecision {
    size_t id = 0;
    size_t nodes = 0;
    size_t edges = 0;
    size_t randVars = 0;
    bool hasEvidence = false;
    bool hasNegation = false;
    bool hasOr = false;
    bool hasCycle = false;
    std::string mode;
    std::string reason;
};

static bool evaluateZeroRandConjComponent(
        const ComponentSubgraph& comp,
        std::unordered_map<NodePtr, double>& nodeProbs,
        long long* evalMs = nullptr) {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    SubgraphView subview(comp.nodes, comp.edges);
    std::unordered_map<NodePtr, std::size_t> indegree;
    indegree.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        if (node) {
            indegree.emplace(node, 0);
        }
    }
    for (const auto& edge : comp.edges) {
        if (!edge) {
            continue;
        }
        NodePtr out = subview.getOutput(edge);
        if (out) {
            ++indegree[out];
        }
    }

    std::queue<NodePtr> ready;
    std::vector<NodePtr> topo;
    topo.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        if (node && indegree[node] == 0) {
            ready.push(node);
        }
    }
    while (!ready.empty()) {
        NodePtr node = ready.front();
        ready.pop();
        topo.push_back(node);
        for (const auto& edge : subview.getOutgoingEdges(node)) {
            NodePtr out = subview.getOutput(edge);
            if (!out) {
                continue;
            }
            auto it = indegree.find(out);
            if (it == indegree.end()) {
                continue;
            }
            if (it->second == 0) {
                continue;
            }
            --it->second;
            if (it->second == 0) {
                ready.push(out);
            }
        }
    }
    if (topo.size() != comp.nodes.size()) {
        return false;
    }

    nodeProbs.clear();
    nodeProbs.reserve(comp.nodes.size());
    for (const auto& node : topo) {
        if (!node) {
            continue;
        }
        const auto& incoming = subview.getIncomingEdges(node);
        if (incoming.empty()) {
            nodeProbs[node] = node->isFact ? node->getProbability() : 0.0;
            continue;
        }
        if (incoming.size() != 1) {
            return false;
        }
        const auto& edge = incoming.front();
        double prob = edge->isDeterministic() ? 1.0 : edge->getProbability();
        const auto& inputs = subview.getInputs(edge);
        const auto& negs = subview.getBodyNegations(edge);
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            auto it = nodeProbs.find(inputs[i]);
            if (it == nodeProbs.end()) {
                return false;
            }
            double inputProb = it->second;
            if (i < negs.size() && negs[i]) {
                inputProb = 1.0 - inputProb;
            }
            prob *= inputProb;
        }
        nodeProbs[node] = prob;
    }
    if (evalMs) {
        *evalMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
    }
    return true;
}

static std::vector<ComponentAnalysis> analyzeComponents(
        const DerivationGraphViewInterface& view,
        std::vector<ComponentSubgraph> components) {
    auto isSemanticRandomProb = [](double p) {
        return p > 0.0 && p < 1.0;
    };
    auto& depGraph = view.getCycleDependencyGraph();
    std::vector<ComponentAnalysis> analyses;
    analyses.reserve(components.size());

    for (auto& comp : components) {
        ComponentAnalysis analysis;
        analysis.comp = std::move(comp);
        analysis.singleRand = SingleRandVarInfo{};
        std::unordered_map<NodePtr, size_t> incomingCounts;
        incomingCounts.reserve(analysis.comp.nodes.size());

        for (const auto& node : analysis.comp.nodes) {
            if (node->isFact && isSemanticRandomProb(node->getProbability())) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node = node;
                    analysis.singleRand.edge.reset();
                    analysis.singleRand.probability = node->getProbability();
                }
            }
            auto it = depGraph.nodeToCycleIndex.find(node);
            if (it != depGraph.nodeToCycleIndex.end()) {
                if (depGraph.nodeCycles[it->second].size() > 1) {
                    analysis.hasCycle = true;
                }
            }
        }

        for (const auto& edge : analysis.comp.edges) {
            if (isSemanticRandomProb(edge->getProbability())) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node.reset();
                    analysis.singleRand.edge = edge;
                    analysis.singleRand.probability = edge->getProbability();
                }
            }
            auto negs = view.getBodyNegations(edge);
            for (bool neg : negs) {
                if (neg) {
                    analysis.hasNegation = true;
                    break;
                }
            }
            NodePtr out = view.getOutput(edge);
            if (out) {
                incomingCounts[out]++;
                for (const auto& in : view.getInputs(edge)) {
                    if (in == out) {
                        analysis.hasCycle = true;
                        break;
                    }
                }
            }
        }

        for (const auto& node : analysis.comp.nodes) {
            auto it = incomingCounts.find(node);
            if (it != incomingCounts.end()) {
                if (it->second > 1) {
                    analysis.hasOr = true;
                }
                // A fact with derived support is only an OR-source if the base fact itself is probabilistic.
                if (node->isFact && it->second > 0 && isSemanticRandomProb(node->getProbability())) {
                    analysis.hasOr = true;
                }
            }
        }

        analyses.push_back(std::move(analysis));
    }

    return analyses;
}

} // namespace

void setFullOnlyMode(bool enabled) {
    fullOnlyMode = enabled;
}

bool isFullOnlyMode() {
    return fullOnlyMode;
}

std::string makeOutputPath(const CmdOptions& opt, const std::string& filename) {
    return joinOutputPath(opt.getOutputFileDir(), filename);
}

static void dumpDeterministicProbabilities(const CmdOptions& opt, SouffleProgram& program) {
    std::vector<std::string> outputTuples;
    for (auto* rel : program.getOutputRelations()) {
        if (rel == nullptr) {
            continue;
        }
        for (auto& tup : *rel) {
            outputTuples.push_back(tup.toString());
        }
    }
    std::sort(outputTuples.begin(), outputTuples.end());
    const std::string outPath = makeOutputPath(opt, "facts.prob");
    std::ofstream outputFile(outPath);
    outputFile << std::setprecision(8);
    for (const auto& tupleStr : outputTuples) {
        outputFile << tupleStr << " : 1.0\n";
    }
    std::cout << "[det-force] dumpProbabilities outputs=" << outputTuples.size()
              << " file=" << outPath << std::endl;
}

static std::vector<std::pair<NodePtr, bool>> applyEvidence(
        IncrementalDerivationGraph& graph,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences) {
    std::vector<std::pair<NodePtr, bool>> resolved;
    resolved.reserve(evidences.size());

    for (const auto& [tup, val] : evidences) {
        NodePtr node = graph.findNode(tup);
        if (!node) {
            throw std::runtime_error("Evidence " + tup.toString() + " is not found in the graph.");
        }

        resolved.emplace_back(node, val);
    }
    return resolved;
}

static void dumpSisoRegions(const DerivationGraphViewInterface& view) {
    auto start = std::chrono::steady_clock::now();
    auto regions = GraphAnalyzer::detectAllSISOStrictFromExit(view);
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    size_t singleHyperedgeCount = 0;
    size_t linearTwoEdgeCount = 0;
    size_t parallelTwoEdgeCount = 0;
    size_t allFactsToSOCount = 0;
    size_t generalCount = 0;
    for (const auto& r : regions) {
        switch (r.kind) {
        case SISORegionKind::SingleHyperedge:
            ++singleHyperedgeCount;
            break;
        case SISORegionKind::LinearTwoEdge:
            ++linearTwoEdgeCount;
            break;
        case SISORegionKind::ParallelEdge:
            ++parallelTwoEdgeCount;
            break;
        case SISORegionKind::AllFactsToSO:
            ++allFactsToSOCount;
            break;
        case SISORegionKind::General:
            ++generalCount;
            break;
        default:
            break;
        }
    }
    std::cout << "Found " << regions.size() << " SISO regions"
              << " (single-hyperedge=" << singleHyperedgeCount
              << ", linear-two-edge=" << linearTwoEdgeCount
              << ", parallel-two-edge=" << parallelTwoEdgeCount
              << ", all-facts=" << allFactsToSOCount
              << ", general=" << generalCount << ")" << std::endl;
    std::cout << "[pipeline] SISO detection took " << dur << " ms\n";
    for (const auto& r : regions) {
        GraphAnalyzer::printSISOInfo(view, r);
    }
    auto dotStart = std::chrono::steady_clock::now();
    GraphAnalyzer::dumpAllRegionsAsDot(view, regions, "siso_regions.dot");
    auto dotMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - dotStart)
                         .count();
    std::cout << "[pipeline] dumpAllRegionsAsDot took " << dotMs << " ms\n";
}

static bool tryRunScbfBdd(
        const CmdOptions& opt,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        const std::unordered_set<NodePtr>& seedTrueNodes,
        std::map<NodePtr, BddNodeRef>& nodeFormulas,
        std::map<EdgePtr, BddNodeRef>& edgeFormulas,
        std::unique_ptr<WeightedBDDManager>& bddManager,
        StageInfo* rewriteHybridStage) {
    if (!opt.isScbfEnabled()) {
        return false;
    }
    if (!evidences.empty()) {
        std::cout << "[pipeline] --scbf fallback: evidence-conditioned path not enabled yet; use default FC/WMC"
                  << std::endl;
        return false;
    }

    Debugger& debugger = Debugger::getInstance();
    debugger.addInfo("scbf_mode", "1");

    if (rewriteHybridStage == nullptr) {
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
    }

    const auto varEstimate = estimateBddVarCount(view);
    debugger.addInfo("rand_vars", std::to_string(varEstimate));
    auto initConfig = makeCuddInitConfig(varEstimate);
    debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
    debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
    debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
    debugger.addInfo("manager_init_maxmem_mb",
            std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));

    auto initStart = std::chrono::steady_clock::now();
    bddManager = std::make_unique<WeightedBDDManager>(initConfig);
    auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - initStart)
                          .count();
    debugger.addInfo("manager_init_ms", std::to_string(initMs));

    probResult.clear();
    auto t0 = std::chrono::steady_clock::now();
    ScbfCyclewiseStats scbfStats;
    buildFormulasCyclewiseScbf(view, *bddManager, nodeFormulas, edgeFormulas, seedTrueNodes, &scbfStats);
    auto t1 = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto wmcMs = static_cast<long long>(scbfStats.outputWmcMs);
    auto fcMs = totalMs > wmcMs ? (totalMs - wmcMs) : 0;
    debugger.addInfo("fc_build_ms", std::to_string(fcMs));
    debugger.addInfo("wmc_ms", std::to_string(wmcMs));
    std::cout << "[pipeline] SCBF(BDD) cyclewise total=" << totalMs << " ms"
              << " (fc=" << fcMs << " ms, wmc=" << wmcMs << " ms)\n";

    for (const auto& [node, prob] : precomputedProbResult) {
        probResult.emplace(node, prob);
    }

    debugger.endStage();
    debugger.startStage(StageKind::IO_DUMP_FULL);
    auto tDumpStart = std::chrono::steady_clock::now();
    dumpProbabilities(probResult, opt.getOutputFileDir());
    auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - tDumpStart)
                           .count();
    std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
    debugger.endStage();
    return true;
}

static bool tryRunScbfSdd(
        const CmdOptions& opt,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        const std::unordered_set<NodePtr>& seedTrueNodes,
        std::map<NodePtr, SddNodeRef>& nodeFormulas,
        std::map<EdgePtr, SddNodeRef>& edgeFormulas,
        std::unique_ptr<SddFormulaManager>& sddManager,
        StageInfo* rewriteHybridStage) {
    if (!opt.isScbfEnabled()) {
        return false;
    }
    if (!evidences.empty()) {
        std::cout << "[pipeline] --scbf fallback: evidence-conditioned path not enabled yet; use default FC/WMC"
                  << std::endl;
        return false;
    }

    Debugger& debugger = Debugger::getInstance();
    debugger.addInfo("scbf_mode", "1");
    if (rewriteHybridStage == nullptr) {
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
    }

    auto initStart = std::chrono::steady_clock::now();
    sddManager = std::make_unique<SddFormulaManager>();
    auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - initStart)
                          .count();
    debugger.addInfo("manager_init_ms", std::to_string(initMs));

    probResult.clear();
    auto t0 = std::chrono::steady_clock::now();
    ScbfCyclewiseStats scbfStats;
    buildFormulasCyclewiseScbf(view, *sddManager, nodeFormulas, edgeFormulas, seedTrueNodes, &scbfStats);
    auto t1 = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto wmcMs = static_cast<long long>(scbfStats.outputWmcMs);
    auto fcMs = totalMs > wmcMs ? (totalMs - wmcMs) : 0;
    debugger.addInfo("fc_build_ms", std::to_string(fcMs));
    debugger.addInfo("wmc_ms", std::to_string(wmcMs));
    std::cout << "[pipeline] SCBF(SDD) cyclewise total=" << totalMs << " ms"
              << " (fc=" << fcMs << " ms, wmc=" << wmcMs << " ms)\n";

    for (const auto& [node, prob] : precomputedProbResult) {
        probResult.emplace(node, prob);
    }

    debugger.endStage();
    debugger.startStage(StageKind::IO_DUMP_FULL);
    auto tDumpStart = std::chrono::steady_clock::now();
    dumpProbabilities(probResult, opt.getOutputFileDir());
    auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - tDumpStart)
                           .count();
    std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
    debugger.endStage();
    return true;
}

static void runBddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        std::unique_ptr<IncrementalDerivationGraph>& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli,
        StageInfo* rewriteHybridStage) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
    std::unique_ptr<WeightedBDDManager> bddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();
    std::unordered_set<NodePtr> seedTrueNodes;
    seedTrueNodes.reserve(view.getNodes().size());
    for (const auto& n : view.getNodes()) {
        const std::string s = n->getTuple().toString();
        if (s.rfind("@magic.", 0) != 0) continue;

        if (view.getIncomingEdges(n).empty()) {
            seedTrueNodes.insert(n);
        }
    }
    //std::cout << "[dbg] seedTrueNodes size = " << seedTrueNodes.size() << "\

    if (computeProbabilities) {
        if (tryRunScbfBdd(
                    opt, view, evidences, seedTrueNodes, nodeFormulas, edgeFormulas, bddManager,
                    rewriteHybridStage)) {
            // SCBF path handled FC/WMC and dump.
        } else if (opt.isRewriteEnabled()) {
            auto* hybridStage = rewriteHybridStage;
            if (!hybridStage) {
                hybridStage = debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
            }
            auto varEstimate = estimateBddVarCount(view);
            debugger.addInfo("rand_vars", std::to_string(varEstimate));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
            }

            auto components = buildComponentSubgraphs(view);
            auto analyses = analyzeComponents(view, std::move(components));
            long long initMsTotal = 0;
            long long initMsMax = 0;
            long long buildMs = 0;
            WeightedBDDManager::InitConfig initConfig;
            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(*graph, evidences);
            auto t3 = std::chrono::steady_clock::now();
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            long long fastPathMs = 0;
            long long liveNodesSum = 0;
            const bool wmcProfile = opt.isWmcProfileEnabled();
            using Clock = std::chrono::steady_clock;
            auto toMs = [](Clock::time_point start) {
                return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            };
            double evidenceMakeAndMs = 0.0;
            double evidenceWmcComputeMs = 0.0;
            double nodeMakeAndMs = 0.0;
            double nodeWmcComputeMs = 0.0;
            std::size_t evidenceWmcCalls = 0;
            std::size_t nodeWmcCalls = 0;
            std::size_t evidenceMakeAndCalls = 0;
            std::size_t nodeMakeAndCalls = 0;
            auto makeAndProfile = [&](const BddNodeRef& lhs, const BddNodeRef& rhs,
                                      double& ms, std::size_t& calls) {
                if (!wmcProfile) {
                    return bddManager->makeAnd(lhs, rhs);
                }
                auto andStart = Clock::now();
                auto res = bddManager->makeAnd(lhs, rhs);
                ms += toMs(andStart);
                calls++;
                return res;
            };
            auto computeWmcProfile = [&](const BddNodeRef& node, double& ms, std::size_t& calls) {
                calls++;
                if (!wmcProfile) {
                    return bddManager->computeWeightedModelCount(node);
                }
                auto wmcStart = Clock::now();
                double res = bddManager->computeWeightedModelCount(node);
                ms += toMs(wmcStart);
                return res;
            };

            FastComponentStats fastStats;
            ConjFastStats conjStats;
            std::vector<FastComponentEval> fastComponents;
            std::vector<ConjComponentEval> conjComponents;
            std::vector<SlowComponentEval> slowEvals;
            const bool logFastReasons = opt.isDumpDotEnabled();
            std::vector<ComponentDecision> decisions;
            decisions.reserve(analyses.size());

            probResult.clear();
            const bool enableFast = opt.isSingleRandFastEnabled();
            for (auto& analysis : analyses) {
                const auto& comp = analysis.comp;
                const auto& compEvs = evidencesByComponent[comp.id];
                const bool hasEvidence = !compEvs.empty();
                const bool singleCandidate = analysis.randVars == 1;
                const bool conjCandidate = !singleCandidate && !analysis.hasNegation &&
                        !analysis.hasOr && !analysis.hasCycle;  // fast path excludes cycles/OR/negation
                ComponentDecision decision{
                        comp.id,
                        comp.nodes.size(),
                        comp.edges.size(),
                        analysis.randVars,
                        hasEvidence,
                        analysis.hasNegation,
                        analysis.hasOr,
                        analysis.hasCycle,
                        "",
                        "",
                };

                if (enableFast) {
                    if (singleCandidate) fastStats.candidates++;
                    if (conjCandidate) conjStats.candidates++;
                }
                if (logFastReasons) {
                    std::vector<std::string> singleReasons;
                    if (!enableFast) singleReasons.emplace_back("fast_disabled");
                    if (!singleCandidate) singleReasons.emplace_back("randvars!=1");
                    if (hasEvidence) singleReasons.emplace_back("has_evidence");
                    if (!singleReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " single_skip=" << join(singleReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                    std::vector<std::string> conjReasons;
                    if (!enableFast) conjReasons.emplace_back("fast_disabled");
                    if (singleCandidate) conjReasons.emplace_back("single_randvar");
                    if (analysis.hasNegation) conjReasons.emplace_back("negation");
                    if (analysis.hasOr) conjReasons.emplace_back("or");
                    if (analysis.hasCycle) conjReasons.emplace_back("cycle");
                    if (hasEvidence) conjReasons.emplace_back("has_evidence");
                    if (!conjReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " conj_skip=" << join(conjReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                }

                if (!enableFast || hasEvidence) {
                    if (enableFast && singleCandidate) fastStats.skipped++;
                    if (enableFast && conjCandidate) conjStats.skipped++;
                    decision.mode = "slow";
                    decision.reason = !enableFast ? "fast_disabled" : "has_evidence";
                    decisions.push_back(decision);
                    SlowComponentEval slow;
                    slow.randVars = analysis.randVars;
                    slow.comp = std::move(analysis.comp);
                    slowEvals.push_back(std::move(slow));
                    continue;
                }

                if (singleCandidate) {
                    FastComponentEval eval;
                    eval.var = analysis.singleRand;
                    if (!evaluateSingleRandComponent(
                                analysis.comp, eval.var, true, eval.valuesTrue, &eval.evalMsTrue) ||
                            !evaluateSingleRandComponent(
                                    analysis.comp, eval.var, false, eval.valuesFalse, &eval.evalMsFalse)) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " single_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        fastStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        SlowComponentEval slow;
                        slow.randVars = analysis.randVars;
                        slow.comp = std::move(analysis.comp);
                        slowEvals.push_back(std::move(slow));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    fastStats.used++;
                    fastStats.evalMs += eval.evalMsTrue + eval.evalMsFalse;
                    decision.mode = "fast_single";
                    decisions.push_back(decision);
                    std::cout << "[fc-component] id=" << eval.comp.id
                              << " fast_path=1"
                              << " nodes=" << eval.comp.nodes.size()
                              << " edges=" << eval.comp.edges.size()
                              << " rand_vars=" << analysis.randVars
                              << " eval_ms_true=" << eval.evalMsTrue
                              << " eval_ms_false=" << eval.evalMsFalse
                              << " total_ms=" << (eval.evalMsTrue + eval.evalMsFalse)
                              << std::endl;
                    if (hybridStage) {
                        hybridStage->logMessage(Level::INFO,
                                "component id=" + std::to_string(eval.comp.id) +
                                        " fast_path=single" +
                                        " nodes=" + std::to_string(eval.comp.nodes.size()) +
                                        " edges=" + std::to_string(eval.comp.edges.size()) +
                                        " rand_vars=" + std::to_string(analysis.randVars) +
                                        " eval_ms_true=" + std::to_string(eval.evalMsTrue) +
                                        " eval_ms_false=" + std::to_string(eval.evalMsFalse) +
                                        " total_ms=" + std::to_string(eval.evalMsTrue + eval.evalMsFalse));
                    }
                    fastComponents.push_back(std::move(eval));
                    continue;
                }

                if (conjCandidate) {
                    ConjComponentEval eval;
                    const bool zeroRandConj = (analysis.randVars == 0);
                    const bool conjOk = zeroRandConj
                            ? evaluateZeroRandConjComponent(analysis.comp, eval.probabilities, &eval.evalMs)
                            : evaluateConjComponent(analysis.comp, eval.probabilities, &eval.evalMs);
                    if (!conjOk) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " conj_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        conjStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        SlowComponentEval slow;
                        slow.randVars = analysis.randVars;
                        slow.comp = std::move(analysis.comp);
                        slowEvals.push_back(std::move(slow));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    conjStats.used++;
                    conjStats.evalMs += eval.evalMs;
                    decision.mode = "fast_conj";
                    decisions.push_back(decision);
                    std::cout << "[fc-component] id=" << eval.comp.id
                              << " fast_path=conj"
                              << " nodes=" << eval.comp.nodes.size()
                              << " edges=" << eval.comp.edges.size()
                              << " rand_vars=" << analysis.randVars
                              << " eval_ms=" << eval.evalMs
                              << " total_ms=" << eval.evalMs
                              << std::endl;
                    if (hybridStage) {
                        hybridStage->logMessage(Level::INFO,
                                "component id=" + std::to_string(eval.comp.id) +
                                        " fast_path=conj" +
                                        " nodes=" + std::to_string(eval.comp.nodes.size()) +
                                        " edges=" + std::to_string(eval.comp.edges.size()) +
                                        " rand_vars=" + std::to_string(analysis.randVars) +
                                        " eval_ms=" + std::to_string(eval.evalMs));
                    }
                    conjComponents.push_back(std::move(eval));
                    continue;
                }

                SlowComponentEval slow;
                slow.randVars = analysis.randVars;
                decision.mode = "slow";
                if (analysis.hasOr) {
                    decision.reason = "or";
                } else if (analysis.hasNegation) {
                    decision.reason = "negation";
                } else if (analysis.hasCycle) {
                    decision.reason = "cycle";
                } else {
                    decision.reason = "other";
                }
                decisions.push_back(decision);
                slow.comp = std::move(analysis.comp);
                slowEvals.push_back(std::move(slow));
            }

            std::sort(slowEvals.begin(), slowEvals.end(),
                    [](const SlowComponentEval& a, const SlowComponentEval& b) {
                        return a.randVars > b.randVars;
                    });

            std::size_t maxRandVars = 0;
            for (const auto& slow : slowEvals) {
                maxRandVars = std::max(maxRandVars, slow.randVars);
            }
            if (!slowEvals.empty()) {
                initConfig = makeCuddInitConfig(maxRandVars);
                auto initStart = std::chrono::steady_clock::now();
                bddManager = std::make_unique<WeightedBDDManager>(initConfig);
                initMsTotal = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - initStart)
                                      .count();
                initMsMax = initMsTotal;
            } else {
                initConfig.numVars = 0;
                initConfig.numVarsZ = 0;
                initConfig.numSlots = 0;
                initConfig.cacheSize = 0;
                initConfig.maxMemory = 0;
            }

            debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
            debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
            debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
            debugger.addInfo("manager_init_maxmem_mb",
                    std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            debugger.addInfo("manager_init_ms", std::to_string(initMsTotal));
            debugger.addInfo("manager_init_ms_max", std::to_string(initMsMax));
            debugger.addInfo("manager_init_components", std::to_string(slowEvals.empty() ? 0 : 1));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "manager_init_vars=" + std::to_string(initConfig.numVars));
                hybridStage->logMessage(Level::INFO, "manager_init_slots=" + std::to_string(initConfig.numSlots));
                hybridStage->logMessage(Level::INFO, "manager_init_cache=" + std::to_string(initConfig.cacheSize));
                hybridStage->logMessage(Level::INFO, "manager_init_maxmem_mb=" +
                        std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
                hybridStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMsTotal));
                hybridStage->logMessage(Level::INFO, "manager_init_ms_max=" + std::to_string(initMsMax));
                hybridStage->logMessage(Level::INFO, "manager_init_components=" +
                        std::to_string(slowEvals.empty() ? 0 : 1));
            }

            debugger.addInfo("slow_components", std::to_string(slowEvals.size()));
            debugger.addInfo("fastpath_components", std::to_string(fastStats.used));
            debugger.addInfo("fastpath_candidates", std::to_string(fastStats.candidates));
            debugger.addInfo("fastpath_skipped", std::to_string(fastStats.skipped));
            debugger.addInfo("fastpath_eval_ms", std::to_string(fastStats.evalMs));
            debugger.addInfo("fastpath_conj_components", std::to_string(conjStats.used));
            debugger.addInfo("fastpath_conj_candidates", std::to_string(conjStats.candidates));
            debugger.addInfo("fastpath_conj_skipped", std::to_string(conjStats.skipped));
            debugger.addInfo("fastpath_conj_eval_ms", std::to_string(conjStats.evalMs));

            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "slow_components=" +
                        std::to_string(slowEvals.size()));
                hybridStage->logMessage(Level::INFO, "fastpath_components=" +
                        std::to_string(fastStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_candidates=" +
                        std::to_string(fastStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_skipped=" +
                        std::to_string(fastStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_eval_ms=" +
                        std::to_string(fastStats.evalMs));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_components=" +
                        std::to_string(conjStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_candidates=" +
                        std::to_string(conjStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_skipped=" +
                        std::to_string(conjStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_eval_ms=" +
                        std::to_string(conjStats.evalMs));
            }
            if (logFastReasons) {
                std::size_t fastSingle = 0;
                std::size_t fastConj = 0;
                std::size_t slowCount = 0;
                for (const auto& decision : decisions) {
                    if (decision.mode == "fast_single") {
                        fastSingle++;
                    } else if (decision.mode == "fast_conj") {
                        fastConj++;
                    } else {
                        slowCount++;
                    }
                }
                std::cout << "[fc-component-info] total=" << decisions.size()
                          << " fast_single=" << fastSingle
                          << " fast_conj=" << fastConj
                          << " slow=" << slowCount
                          << std::endl;
                for (const auto& decision : decisions) {
                    std::cout << "[fc-component-info] id=" << decision.id
                              << " nodes=" << decision.nodes
                              << " edges=" << decision.edges
                              << " rand_vars=" << decision.randVars
                              << " has_negation=" << decision.hasNegation
                              << " has_or=" << decision.hasOr
                              << " has_cycle=" << decision.hasCycle
                              << " has_evidence=" << decision.hasEvidence
                              << " mode=" << decision.mode
                              << " reason=" << decision.reason
                              << std::endl;
                }
            }

            if (!fastComponents.empty()) {
                for (const auto& fast : fastComponents) {
                    double p = fast.var.probability;
                    for (const auto& node : fast.comp.nodes) {
                        if (!node->needOutput) {
                            continue;
                        }
                        auto itTrue = fast.valuesTrue.find(node);
                        auto itFalse = fast.valuesFalse.find(node);
                        if (itTrue == fast.valuesTrue.end() && itFalse == fast.valuesFalse.end()) {
                            continue;
                        }
                        bool vTrue = (itTrue != fast.valuesTrue.end()) ? itTrue->second : false;
                        bool vFalse = (itFalse != fast.valuesFalse.end()) ? itFalse->second : false;
                        double numerator = (vTrue ? p : 0.0) + (vFalse ? (1.0 - p) : 0.0);
                        probResult[node] = numerator;
                    }
                }
            }
            if (!conjComponents.empty()) {
                for (const auto& conj : conjComponents) {
                    for (const auto& [node, prob] : conj.probabilities) {
                        if (!node->needOutput) {
                            continue;
                        }
                        probResult[node] = prob;
                    }
                }
            }
            fastPathMs = fastStats.evalMs + conjStats.evalMs;
            const std::size_t slowTotal = slowEvals.size();
            std::size_t slowDone = 0;
            for (auto& slow : slowEvals) {
                if (!bddManager) {
                    throw std::runtime_error("Missing BDD manager for slow components.");
                }
                auto compId = slow.comp.id;
                auto nodeCount = slow.comp.nodes.size();
                auto edgeCount = slow.comp.edges.size();
                auto randVars = slow.randVars;
                auto compStart = std::chrono::steady_clock::now();

                bddManager->reset();
                std::map<NodePtr, BddNodeRef> compNodeFormulas;
                std::map<EdgePtr, BddNodeRef> compEdgeFormulas;
                SubgraphView subview(std::move(slow.comp.nodes), std::move(slow.comp.edges));

                auto buildStart = std::chrono::steady_clock::now();
                auto heartbeatCallback = [&](const FcHeartbeatSnapshot& hb) {
                    const std::size_t liveNodes = static_cast<std::size_t>(
                            Cudd_ReadNodeCount(bddManager->getManager()));
                    recordFcHeartbeat(debugger, hybridStage, hb, "rewrite", slowDone, slowTotal, compId,
                            liveNodes);
                };
                buildFormulasCyclewise(subview, *bddManager, compNodeFormulas, compEdgeFormulas, {}, nullptr,
                        true, true, heartbeatCallback);
                auto buildMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - buildStart)
                                           .count();
                buildMs += buildMsComp;
                liveNodesSum += static_cast<long long>(
                        Cudd_ReadNodeCount(bddManager->getManager()));
                slowDone++;

                const auto& componentEvs = evidencesByComponent[compId];
                auto evidenceBuildStart = std::chrono::steady_clock::now();
                auto evidenceBdd = bddManager->getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = compNodeFormulas.find(eNode);
                    if (it == compNodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = bddManager->makeNot(lit);
                    }
                    evidenceBdd = makeAndProfile(evidenceBdd, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                }
                auto evidenceBuildMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::steady_clock::now() - evidenceBuildStart)
                                                   .count();
                evidenceBuildMs += evidenceBuildMsComp;

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = computeWmcProfile(evidenceBdd, evidenceWmcComputeMs, evidenceWmcCalls);
                }
                auto evidenceWmcMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now() - wmcStart)
                                                 .count();
                evidenceWmcMs += evidenceWmcMsComp;

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& node : subview.getNodes()) {
                    if (!node->needOutput) {
                        continue;
                    }
                    auto it = compNodeFormulas.find(node);
                    if (it == compNodeFormulas.end()) {
                        continue;
                    }
                    const auto& bdd = it->second;
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = computeWmcProfile(bdd, nodeWmcComputeMs, nodeWmcCalls);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = makeAndProfile(bdd, evidenceBdd, nodeMakeAndMs, nodeMakeAndCalls);
                        double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                auto perNodeWmcMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now() - perNodeStart)
                                                .count();
                perNodeWmcMs += perNodeWmcMsComp;

                auto compTotalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - compStart)
                                           .count();
                std::cout << "[fc-component] id=" << compId
                          << " fast_path=0"
                          << " nodes=" << nodeCount
                          << " edges=" << edgeCount
                          << " rand_vars=" << randVars
                          << " build_ms=" << buildMsComp
                          << " evidence_build_ms=" << evidenceBuildMsComp
                          << " evidence_wmc_ms=" << evidenceWmcMsComp
                          << " per_node_wmc_ms=" << perNodeWmcMsComp
                          << " total_ms=" << compTotalMs
                          << std::endl;
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            debugger.addInfo("live_nodes", std::to_string(liveNodesSum));
            debugger.addInfo("fc_build_ms", std::to_string(buildMs));
            debugger.addInfo("wmc_ms", std::to_string(evidenceBuildMs + evidenceWmcMs + perNodeWmcMs + fastPathMs));
            debugger.addInfo("evidence_build_ms", std::to_string(evidenceBuildMs));
            debugger.addInfo("evidence_wmc_ms", std::to_string(evidenceWmcMs));
            debugger.addInfo("per_node_wmc_ms", std::to_string(perNodeWmcMs));
            debugger.addInfo("fastpath_wmc_ms", std::to_string(fastPathMs));
            std::cout << "[pipeline] BDD formula build took " << buildMs << " ms\n";
            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] evidence BDD build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took " << perNodeWmcMs << " ms\n";
            if (fastStats.used > 0 || conjStats.used > 0) {
                std::cout << "[pipeline] fastpath WMC took " << fastPathMs << " ms\n";
            }
            if (wmcProfile) {
                std::cout << "[wmc-profile] stage=FULL"
                          << " mode=full-rewrite"
                          << " total_ms=" << (evidenceBuildMs + evidenceWmcMs + perNodeWmcMs + fastPathMs)
                          << " components=" << analyses.size()
                          << " nodes=" << view.getNodes().size()
                          << " evidence_build_ms=" << evidenceBuildMs
                          << " evidence_make_and_calls=" << evidenceMakeAndCalls
                          << " evidence_make_and_ms=" << evidenceMakeAndMs
                          << " evidence_wmc_calls=" << evidenceWmcCalls
                          << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                          << " node_make_and_calls=" << nodeMakeAndCalls
                          << " node_make_and_ms=" << nodeMakeAndMs
                          << " node_wmc_calls=" << nodeWmcCalls
                          << " node_wmc_compute_ms=" << nodeWmcComputeMs
                          << " fastpath_ms=" << fastPathMs
                          << " live_nodes=" << liveNodesSum
                          << std::endl;
            }

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        } else {
            auto* fcStage = debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
            auto varEstimate = estimateBddVarCount(view);
            debugger.addInfo("rand_vars", std::to_string(varEstimate));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
            }

            auto initConfig = makeCuddInitConfig(varEstimate);
            debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
            debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
            debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
            debugger.addInfo("manager_init_maxmem_mb",
                    std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "manager_init_vars=" + std::to_string(initConfig.numVars));
                fcStage->logMessage(Level::INFO, "manager_init_slots=" + std::to_string(initConfig.numSlots));
                fcStage->logMessage(Level::INFO, "manager_init_cache=" + std::to_string(initConfig.cacheSize));
                fcStage->logMessage(Level::INFO, "manager_init_maxmem_mb=" +
                        std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            }
            auto initStart = std::chrono::steady_clock::now();
            bddManager = std::make_unique<WeightedBDDManager>(initConfig);
            auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - initStart)
                                  .count();
            debugger.addInfo("manager_init_ms", std::to_string(initMs));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMs));
            }
            auto t0 = std::chrono::steady_clock::now();
            auto heartbeatCallback = [&](const FcHeartbeatSnapshot& hb) {
                std::size_t liveNodes = 0;
                if (bddManager) {
                    liveNodes = bddManager->getLiveNodeCount();
                }
                recordFcHeartbeat(debugger, fcStage, hb, "no-rewrite", 0, 0,
                        std::numeric_limits<std::size_t>::max(), liveNodes);
            };
            buildFormulasCyclewise(view, *bddManager, nodeFormulas, edgeFormulas, seedTrueNodes, nullptr, true,
                    true, heartbeatCallback);
            auto t1 = std::chrono::steady_clock::now();
            std::cout << "[pipeline] BDD formula build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                      << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(*graph, evidences);
            auto t3 = std::chrono::steady_clock::now();

            auto components = buildComponentSubgraphs(view);
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            const bool wmcProfile = opt.isWmcProfileEnabled();
            using Clock = std::chrono::steady_clock;
            auto toMs = [](Clock::time_point start) {
                return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            };
            double evidenceMakeAndMs = 0.0;
            double evidenceWmcComputeMs = 0.0;
            double nodeMakeAndMs = 0.0;
            double nodeWmcComputeMs = 0.0;
            std::size_t evidenceWmcCalls = 0;
            std::size_t nodeWmcCalls = 0;
            std::size_t evidenceMakeAndCalls = 0;
            std::size_t nodeMakeAndCalls = 0;
            auto makeAndProfile = [&](const BddNodeRef& lhs, const BddNodeRef& rhs,
                                      double& ms, std::size_t& calls) {
                if (!wmcProfile) {
                    return bddManager->makeAnd(lhs, rhs);
                }
                auto andStart = Clock::now();
                auto res = bddManager->makeAnd(lhs, rhs);
                ms += toMs(andStart);
                calls++;
                return res;
            };
            auto computeWmcProfile = [&](const BddNodeRef& node, double& ms, std::size_t& calls) {
                calls++;
                if (!wmcProfile) {
                    return bddManager->computeWeightedModelCount(node);
                }
                auto wmcStart = Clock::now();
                double res = bddManager->computeWeightedModelCount(node);
                ms += toMs(wmcStart);
                return res;
            };

            probResult.clear();
            for (const auto& comp : components) {
                const auto& componentEvs = evidencesByComponent[comp.id];
                auto evidenceBuildStart = std::chrono::steady_clock::now();
                auto evidenceBdd = bddManager->getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = nodeFormulas.find(eNode);
                    if (it == nodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = bddManager->makeNot(lit);
                    }
                    evidenceBdd = makeAndProfile(evidenceBdd, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                }
                evidenceBuildMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - evidenceBuildStart)
                                           .count();

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = computeWmcProfile(evidenceBdd, evidenceWmcComputeMs, evidenceWmcCalls);
                }
                evidenceWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - wmcStart)
                                         .count();

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& node : comp.nodes) {
                    if (!node->needOutput) {
                        continue;
                    }
                    auto it = nodeFormulas.find(node);
                    if (it == nodeFormulas.end()) {
                        continue;
                    }
                    const auto& bdd = it->second;
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = computeWmcProfile(bdd, nodeWmcComputeMs, nodeWmcCalls);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = makeAndProfile(bdd, evidenceBdd, nodeMakeAndMs, nodeMakeAndCalls);
                        double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                perNodeWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - perNodeStart)
                                        .count();
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] component evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took "
                      << perNodeWmcMs << " ms\n";
            if (wmcProfile) {
                std::cout << "[wmc-profile] stage=FULL"
                          << " mode=full"
                          << " total_ms=" << (evidenceBuildMs + evidenceWmcMs + perNodeWmcMs)
                          << " components=" << components.size()
                          << " nodes=" << view.getNodes().size()
                          << " evidence_build_ms=" << evidenceBuildMs
                          << " evidence_make_and_calls=" << evidenceMakeAndCalls
                          << " evidence_make_and_ms=" << evidenceMakeAndMs
                          << " evidence_wmc_calls=" << evidenceWmcCalls
                          << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                          << " node_make_and_calls=" << nodeMakeAndCalls
                          << " node_make_and_ms=" << nodeMakeAndMs
                          << " node_wmc_calls=" << nodeWmcCalls
                          << " node_wmc_compute_ms=" << nodeWmcComputeMs
                          << " live_nodes=" << bddManager->getLiveNodeCount()
                          << std::endl;
            }

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        }
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");

    if (enableOnlineCli) {
        IncrementalCLI<BddNodeRef> cli(
                &program, graph.get(), &graph, &ruleManager, &queryManager, bddManager.get(), &nodeFormulas,
                &edgeFormulas);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

static void runSddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        std::unique_ptr<IncrementalDerivationGraph>& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli,
        StageInfo* rewriteHybridStage) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, SddNodeRef> nodeFormulas;
    std::map<EdgePtr, SddNodeRef> edgeFormulas;
    std::unique_ptr<SddFormulaManager> sddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();
    std::unordered_set<NodePtr> seedTrueNodes;
    seedTrueNodes.reserve(view.getNodes().size());
    for (const auto& n : view.getNodes()) {
        const std::string s = n->getTuple().toString();
        if (s.rfind("@magic.", 0) != 0) continue;

        if (view.getIncomingEdges(n).empty()) {
            seedTrueNodes.insert(n);
        }
    }
    //std::cout << "[dbg] seedTrueNodes size = " << seedTrueNodes.size() << "\

    if (computeProbabilities) {
        if (tryRunScbfSdd(
                    opt, view, evidences, seedTrueNodes, nodeFormulas, edgeFormulas, sddManager,
                    rewriteHybridStage)) {
            // SCBF path handled FC/WMC and dump.
        } else if (opt.isRewriteEnabled()) {
            auto* hybridStage = rewriteHybridStage;
            if (!hybridStage) {
                hybridStage = debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
            }
            auto varEstimate = estimateBddVarCount(view);
            debugger.addInfo("rand_vars", std::to_string(varEstimate));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
            }

            auto components = buildComponentSubgraphs(view);
            auto analyses = analyzeComponents(view, std::move(components));

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(*graph, evidences);
            auto t3 = std::chrono::steady_clock::now();
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);

            std::vector<FastComponentEval> fastComponents;
            std::vector<ConjComponentEval> conjComponents;
            std::vector<ComponentSubgraph> slowComponents;
            FastComponentStats fastStats;
            ConjFastStats conjStats;
            const bool logFastReasons = opt.isDumpDotEnabled();
            std::vector<ComponentDecision> decisions;
            decisions.reserve(analyses.size());

            const bool enableFast = opt.isSingleRandFastEnabled();
            for (auto& analysis : analyses) {
                const auto& comp = analysis.comp;
                const auto& compEvs = evidencesByComponent[comp.id];
                const bool hasEvidence = !compEvs.empty();
                const bool singleCandidate = analysis.randVars == 1;
                const bool conjCandidate = !singleCandidate && !analysis.hasNegation &&
                        !analysis.hasOr && !analysis.hasCycle;  // fast path excludes cycles/OR/negation
                ComponentDecision decision{
                        comp.id,
                        comp.nodes.size(),
                        comp.edges.size(),
                        analysis.randVars,
                        hasEvidence,
                        analysis.hasNegation,
                        analysis.hasOr,
                        analysis.hasCycle,
                        "",
                        "",
                };

                if (enableFast) {
                    if (singleCandidate) fastStats.candidates++;
                    if (conjCandidate) conjStats.candidates++;
                }
                if (logFastReasons) {
                    std::vector<std::string> singleReasons;
                    if (!enableFast) singleReasons.emplace_back("fast_disabled");
                    if (!singleCandidate) singleReasons.emplace_back("randvars!=1");
                    if (hasEvidence) singleReasons.emplace_back("has_evidence");
                    if (!singleReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " single_skip=" << join(singleReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                    std::vector<std::string> conjReasons;
                    if (!enableFast) conjReasons.emplace_back("fast_disabled");
                    if (singleCandidate) conjReasons.emplace_back("single_randvar");
                    if (analysis.hasNegation) conjReasons.emplace_back("negation");
                    if (analysis.hasOr) conjReasons.emplace_back("or");
                    if (analysis.hasCycle) conjReasons.emplace_back("cycle");
                    if (hasEvidence) conjReasons.emplace_back("has_evidence");
                    if (!conjReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " conj_skip=" << join(conjReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                }

                if (!enableFast || hasEvidence) {
                    if (enableFast && singleCandidate) fastStats.skipped++;
                    if (enableFast && conjCandidate) conjStats.skipped++;
                    decision.mode = "slow";
                    decision.reason = !enableFast ? "fast_disabled" : "has_evidence";
                    decisions.push_back(decision);
                    slowComponents.push_back(std::move(analysis.comp));
                    continue;
                }

                if (singleCandidate) {
                    FastComponentEval eval;
                    eval.var = analysis.singleRand;
                    if (!evaluateSingleRandComponent(
                                analysis.comp, eval.var, true, eval.valuesTrue, &eval.evalMsTrue) ||
                            !evaluateSingleRandComponent(
                                    analysis.comp, eval.var, false, eval.valuesFalse, &eval.evalMsFalse)) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " single_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        fastStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        slowComponents.push_back(std::move(analysis.comp));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    fastStats.used++;
                    fastStats.evalMs += eval.evalMsTrue + eval.evalMsFalse;
                    decision.mode = "fast_single";
                    decisions.push_back(decision);
                    fastComponents.push_back(std::move(eval));
                    continue;
                }

                if (conjCandidate) {
                    ConjComponentEval eval;
                    if (!evaluateConjComponent(analysis.comp, eval.probabilities, &eval.evalMs)) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " conj_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        conjStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        slowComponents.push_back(std::move(analysis.comp));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    conjStats.used++;
                    conjStats.evalMs += eval.evalMs;
                    decision.mode = "fast_conj";
                    decisions.push_back(decision);
                    conjComponents.push_back(std::move(eval));
                    continue;
                }

                decision.mode = "slow";
                if (analysis.hasOr) {
                    decision.reason = "or";
                } else if (analysis.hasNegation) {
                    decision.reason = "negation";
                } else if (analysis.hasCycle) {
                    decision.reason = "cycle";
                } else {
                    decision.reason = "other";
                }
                decisions.push_back(decision);
                slowComponents.push_back(std::move(analysis.comp));
            }

            long long initMsTotal = 0;
            long long initMsMax = 0;
            auto makeManager = [&](SubgraphView&) {
                return std::make_unique<SddFormulaManager>();
            };

            std::vector<ComponentFormulaBundle<SddFormulaManager, SddNodeRef>> bundles;
            long long buildMs = 0;
            if (!slowComponents.empty()) {
                auto t0 = std::chrono::steady_clock::now();
                bundles = buildFormulasCyclewiseByComponentList<SddFormulaManager, SddNodeRef>(
                        std::move(slowComponents), makeManager, &initMsTotal, &initMsMax);
                auto t1 = std::chrono::steady_clock::now();
                auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                buildMs = totalMs - initMsTotal;
                if (buildMs < 0) {
                    buildMs = totalMs;
                }
            }
            long long liveNodesSum = 0;
            for (const auto& bundle : bundles) {
                liveNodesSum += static_cast<long long>(bundle.manager->getLiveNodeCount());
            }

            debugger.addInfo("manager_init_ms", std::to_string(initMsTotal));
            debugger.addInfo("manager_init_ms_max", std::to_string(initMsMax));
            debugger.addInfo("manager_init_components", std::to_string(bundles.size()));
            debugger.addInfo("live_nodes", std::to_string(liveNodesSum));
            debugger.addInfo("fastpath_components", std::to_string(fastStats.used));
            debugger.addInfo("fastpath_candidates", std::to_string(fastStats.candidates));
            debugger.addInfo("fastpath_skipped", std::to_string(fastStats.skipped));
            debugger.addInfo("fastpath_eval_ms", std::to_string(fastStats.evalMs));
            debugger.addInfo("fastpath_conj_components", std::to_string(conjStats.used));
            debugger.addInfo("fastpath_conj_candidates", std::to_string(conjStats.candidates));
            debugger.addInfo("fastpath_conj_skipped", std::to_string(conjStats.skipped));
            debugger.addInfo("fastpath_conj_eval_ms", std::to_string(conjStats.evalMs));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMsTotal));
                hybridStage->logMessage(Level::INFO, "manager_init_ms_max=" + std::to_string(initMsMax));
                hybridStage->logMessage(Level::INFO, "manager_init_components=" +
                        std::to_string(bundles.size()));
                hybridStage->logMessage(Level::INFO, "live_nodes=" + std::to_string(liveNodesSum));
                hybridStage->logMessage(Level::INFO, "fastpath_components=" + std::to_string(fastStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_candidates=" + std::to_string(fastStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_skipped=" + std::to_string(fastStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_eval_ms=" + std::to_string(fastStats.evalMs));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_components=" +
                        std::to_string(conjStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_candidates=" +
                        std::to_string(conjStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_skipped=" +
                        std::to_string(conjStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_eval_ms=" +
                        std::to_string(conjStats.evalMs));
            }
            if (logFastReasons) {
                std::size_t fastSingle = 0;
                std::size_t fastConj = 0;
                std::size_t slowCount = 0;
                for (const auto& decision : decisions) {
                    if (decision.mode == "fast_single") {
                        fastSingle++;
                    } else if (decision.mode == "fast_conj") {
                        fastConj++;
                    } else {
                        slowCount++;
                    }
                }
                std::cout << "[fc-component-info] total=" << decisions.size()
                          << " fast_single=" << fastSingle
                          << " fast_conj=" << fastConj
                          << " slow=" << slowCount
                          << std::endl;
                for (const auto& decision : decisions) {
                    std::cout << "[fc-component-info] id=" << decision.id
                              << " nodes=" << decision.nodes
                              << " edges=" << decision.edges
                              << " rand_vars=" << decision.randVars
                              << " has_negation=" << decision.hasNegation
                              << " has_or=" << decision.hasOr
                              << " has_cycle=" << decision.hasCycle
                              << " has_evidence=" << decision.hasEvidence
                              << " mode=" << decision.mode
                              << " reason=" << decision.reason
                              << std::endl;
                }
            }

            std::cout << "[pipeline] SDD formula build took " << buildMs << " ms\n";

            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            long long fastPathMs = fastStats.evalMs + conjStats.evalMs;

            probResult.clear();
            if (!fastComponents.empty()) {
                for (const auto& fast : fastComponents) {
                    double p = fast.var.probability;
                    for (const auto& node : fast.comp.nodes) {
                        if (!node->needOutput) {
                            continue;
                        }
                        auto itTrue = fast.valuesTrue.find(node);
                        auto itFalse = fast.valuesFalse.find(node);
                        if (itTrue == fast.valuesTrue.end() && itFalse == fast.valuesFalse.end()) {
                            continue;
                        }
                        bool vTrue = (itTrue != fast.valuesTrue.end()) ? itTrue->second : false;
                        bool vFalse = (itFalse != fast.valuesFalse.end()) ? itFalse->second : false;
                        double numerator = (vTrue ? p : 0.0) + (vFalse ? (1.0 - p) : 0.0);
                        double prob = numerator;
                        probResult[node] = prob;
                    }
                }
            }
            if (!conjComponents.empty()) {
                for (const auto& conj : conjComponents) {
                    for (const auto& [node, prob] : conj.probabilities) {
                        if (!node->needOutput) {
                            continue;
                        }
                        probResult[node] = prob;
                    }
                }
            }
            for (auto& bundle : bundles) {
                auto& manager = *bundle.manager;
                const auto& componentEvs = evidencesByComponent[bundle.id];

                auto buildStart = std::chrono::steady_clock::now();
                auto evidenceSdd = manager.getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = bundle.nodeFormulas.find(eNode);
                    if (it == bundle.nodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = manager.makeNot(lit);
                    }
                    evidenceSdd = manager.makeAnd(evidenceSdd, lit);
                }
                evidenceBuildMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - buildStart)
                                           .count();

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = manager.computeWeightedModelCount(evidenceSdd);
                }
                evidenceWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - wmcStart)
                                         .count();

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& [node, sdd] : bundle.nodeFormulas) {
                    if (!node->needOutput) {
                        continue;
                    }
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = manager.computeWeightedModelCount(sdd);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = manager.makeAnd(sdd, evidenceSdd);
                        double jointW = manager.computeWeightedModelCount(joint);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                perNodeWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - perNodeStart)
                                        .count();
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            view.dumpStatistics(std::cout);
            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] component evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took " << perNodeWmcMs << " ms\n";
            if (fastStats.used > 0 || conjStats.used > 0) {
                std::cout << "[pipeline] fastpath WMC took " << fastPathMs << " ms\n";
            }

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        } else {
            auto* fcStage = debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
            auto initStart = std::chrono::steady_clock::now();
            sddManager = std::make_unique<SddFormulaManager>();
            auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - initStart)
                                  .count();
            debugger.addInfo("manager_init_ms", std::to_string(initMs));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMs));
            }
            auto t0 = std::chrono::steady_clock::now();
            buildFormulasCyclewise(view, *sddManager, nodeFormulas, edgeFormulas, seedTrueNodes);
            auto t1 = std::chrono::steady_clock::now();
            std::cout << "[pipeline] SDD formula build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                      << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(*graph, evidences);
            auto t3 = std::chrono::steady_clock::now();

            auto components = buildComponentSubgraphs(view);
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;

            probResult.clear();
            for (const auto& comp : components) {
                const auto& componentEvs = evidencesByComponent[comp.id];
                auto evidenceBuildStart = std::chrono::steady_clock::now();
                auto evidenceSdd = sddManager->getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = nodeFormulas.find(eNode);
                    if (it == nodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = sddManager->makeNot(lit);
                    }
                    evidenceSdd = sddManager->makeAnd(evidenceSdd, lit);
                }
                evidenceBuildMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - evidenceBuildStart)
                                           .count();

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = sddManager->computeWeightedModelCount(evidenceSdd);
                }
                evidenceWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - wmcStart)
                                         .count();

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& node : comp.nodes) {
                    if (!node->needOutput) {
                        continue;
                    }
                    auto it = nodeFormulas.find(node);
                    if (it == nodeFormulas.end()) {
                        continue;
                    }
                    const auto& sdd = it->second;
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = sddManager->computeWeightedModelCount(sdd);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = sddManager->makeAnd(sdd, evidenceSdd);
                        double jointW = sddManager->computeWeightedModelCount(joint);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                perNodeWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - perNodeStart)
                                        .count();
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            view.dumpStatistics(std::cout);
            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] component evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took "
                      << perNodeWmcMs << " ms\n";

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        }
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");

    if (enableOnlineCli) {
        IncrementalCLI<SddNodeRef> cli(
                &program, graph.get(), &graph, &ruleManager, &queryManager, sddManager.get(), &nodeFormulas,
                &edgeFormulas);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

struct RewriteLaneResult {
    StageInfo* hybridStage = nullptr;
    bool rewritePerformed = false;
};

static bool fullRuntimeWillRunRewrite(const CmdOptions& opt) {
    return opt.isRewriteEnabled() && !opt.isDerivationOnly();
}

static const char* fullRuntimeLaneLabel(const CmdOptions& opt) {
    const bool rewriteEnabled = fullRuntimeWillRunRewrite(opt);
    if (opt.isScbfEnabled()) {
        if (opt.isImplicitRewriteEnabled() && rewriteEnabled) {
            return "experimental-scbf+rewrite-implicit";
        }
        if (rewriteEnabled) {
            return "experimental-scbf+rewrite-legacy";
        }
        return "experimental-scbf";
    }
    if (opt.isImplicitRewriteEnabled() && rewriteEnabled) {
        return "exact-rewrite-implicit";
    }
    if (rewriteEnabled) {
        return "exact-rewrite-legacy";
    }
    return "exact";
}

static const char* knowledgeBackendLabel(Knowledge knowledge) {
    switch (knowledge) {
        case Knowledge::BDD:
            return "bdd";
        case Knowledge::SDD:
            return "sdd";
        default:
            return "unknown";
    }
}

static void configureRuntimeFromOptions(const CmdOptions& opt) {
    fcProfileEnabled = opt.isFcProfileEnabled();
    incDeleteProfileEnabled = opt.isIncDeleteProfileEnabled();
    wmcProfileEnabled = opt.isWmcProfileEnabled();
    incRegionalProfileEnabled = opt.isIncRegionalProfileEnabled();
    incRegionalProfileHeavyEnabled = opt.isIncRegionalProfileHeavyEnabled();
    incRegionalTraceTuples = opt.getIncRegionalTraceTuples();
    depGraphProfileEnabled = opt.isDepGraphProfileEnabled();
    postDelEnabled = opt.isPostDelEnabled();
    incReorderEnabled = opt.isIncReorderEnabled();
    DerivationGraphViewInterface::setDumpDotEnabled(opt.isDumpDotEnabled());
    DerivationGraphViewInterface::setDumpJsonEnabled(opt.isDumpJsonEnabled());
    DerivationGraphViewInterface::setDumpStatsEnabled(opt.isDumpStatEnabled());
    DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
    DerivationGraph::setMergeBiImpEnabled(fullOnlyMode && opt.isMergeBiImpEnabled());
    DerivationGraph::setPruneExtraEnabled(opt.isPruneExtraEnabled());
    DerivationGraph::setConstFoldEnabled(opt.isConstFoldEnabled());
    DerivationGraph::setConstDumpEnabled(opt.isDumpConstEnabled());
}

static StageInfo* beginRewriteHybridStage(const CmdOptions& opt, Debugger& debugger) {
    if (!opt.isRewriteEnabled() || opt.isDerivationOnly()) {
        return nullptr;
    }
    return debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
}

static RewriteLaneResult runFullRewriteLane(const CmdOptions& opt, Debugger& debugger,
        std::unique_ptr<IncrementalDerivationGraph>& graph, IncSubgraphView& view) {
    RewriteLaneResult result;
    result.hybridStage = beginRewriteHybridStage(opt, debugger);
    if (!opt.isRewriteEnabled()) {
        return result;
    }
    if (opt.isDerivationOnly()) {
        std::cout << "[pipeline] derivation-only mode; skip rewrite" << std::endl;
        return result;
    }

    auto rewriteStart = std::chrono::steady_clock::now();
    GraphRewriteStats rewriteStats;
    if (opt.isImplicitRewriteEnabled()) {
        std::size_t originalOutputCount = 0;
        std::unordered_set<std::string> originalOutputRelations;
        for (const auto& node : view.getNodes()) {
            if (node && node->needOutput) {
                ++originalOutputCount;
                originalOutputRelations.insert(node->getTuple().relation_name);
            }
        }
        ImplicitSplitPipelineOptions rewriteOptions;
        rewriteOptions.splitMode = resolveImplicitSplitMode(opt.getSplitMode());
        rewriteOptions.runOverlayFastPaths = true;
        rewriteOptions.runOverlaySingleHyperedge =
                !envFlagDisabled("SOUFFLE_IMPLICIT_DISABLE_SINGLE");
        rewriteOptions.runOverlayAllFacts =
                !envFlagDisabled("SOUFFLE_IMPLICIT_DISABLE_ALL_FACTS");
        rewriteOptions.runOverlayLinearTwoEdge =
                !envFlagDisabled("SOUFFLE_IMPLICIT_DISABLE_LINEAR");
        rewriteOptions.runOverlayParallelEdge =
                !envFlagDisabled("SOUFFLE_IMPLICIT_DISABLE_PARALLEL");
        rewriteOptions.runOverlayFanOutConverge =
                !envFlagDisabled("SOUFFLE_IMPLICIT_DISABLE_FAN_OUT");
        rewriteOptions.runMaterializedGraphRewrite = true;
        rewriteOptions.computeOutputMarginals = false;
        rewriteOptions.collectPatternStats = false;
        rewriteOptions.iterateSplitRewrite = opt.isImplicitIterateSplitRewriteEnabled();
        auto implicitResult = runImplicitSplitRewritePipeline(view, rewriteOptions);
        rewriteStats = implicitResult.stats.graphRewriteStats;
        const bool allOutputsAlreadyPrecomputed =
                implicitResult.materialized.liveNodes.empty() && precomputedProbResult.empty() &&
                precomputedTupleProbResult.size() == originalOutputCount;
        std::unordered_set<UntypedTuple> originalOutputTuples;
        if (!allOutputsAlreadyPrecomputed) {
            originalOutputTuples.reserve(originalOutputCount);
            for (const auto& node : view.getNodes()) {
                if (node && node->needOutput) {
                    originalOutputTuples.insert(node->getTuple());
                }
            }
        }
        graph = std::move(implicitResult.materialized.graph);
        if (!graph) {
            graph = std::make_unique<IncrementalDerivationGraph>();
        }
        view = buildFullIncViewLocal(
                implicitResult.materialized.liveNodes, implicitResult.materialized.liveEdges);
        std::size_t recoveredIsolatedFacts = 0;
        std::size_t recoveredOutputFacts = 0;
        if (!allOutputsAlreadyPrecomputed) {
            std::unordered_set<UntypedTuple> liveViewTuples;
            liveViewTuples.reserve(view.getNodes().size());
            for (const auto& node : view.getNodes()) {
                if (node) {
                    liveViewTuples.insert(node->getTuple());
                }
            }
            std::unordered_set<UntypedTuple> precomputedTuples;
            std::unordered_set<std::string> precomputedTupleStrings;
            precomputedTuples.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
            precomputedTupleStrings.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
            for (const auto& [node, _] : precomputedProbResult) {
                if (node) {
                    precomputedTuples.insert(node->getTuple());
                    precomputedTupleStrings.insert(node->getTuple().toString());
                }
            }
            for (const auto& [tupleStr, _] : precomputedTupleProbResult) {
                precomputedTupleStrings.insert(tupleStr);
            }
            for (const auto& node : view.getNodes()) {
                if (node && originalOutputRelations.count(node->getTuple().relation_name) &&
                        !precomputedTuples.count(node->getTuple()) &&
                        !precomputedTupleStrings.count(node->getTuple().toString())) {
                    node->setQuery();
                }
            }
            recoveredIsolatedFacts = precomputeIsolatedOutputFactsLocal(view);
            if (recoveredIsolatedFacts > 0) {
                precomputedTuples.clear();
                precomputedTupleStrings.clear();
                precomputedTuples.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
                precomputedTupleStrings.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
                for (const auto& [node, _] : precomputedProbResult) {
                    if (node) {
                        precomputedTuples.insert(node->getTuple());
                        precomputedTupleStrings.insert(node->getTuple().toString());
                    }
                }
                for (const auto& [tupleStr, _] : precomputedTupleProbResult) {
                    precomputedTupleStrings.insert(tupleStr);
                }
            }
            for (const auto& tuple : originalOutputTuples) {
                if (liveViewTuples.count(tuple) || precomputedTuples.count(tuple) ||
                        precomputedTupleStrings.count(tuple.toString())) {
                    continue;
                }
                NodePtr recovered = graph ? graph->findNode(tuple) : nullptr;
                if (!recovered || !recovered->isFact) {
                    continue;
                }
                precomputedTupleProbResult.emplace(tuple.toString(), recovered->getProbability());
                ++recoveredOutputFacts;
            }
        }
        if (recoveredIsolatedFacts > 0 || recoveredOutputFacts > 0) {
            std::cout << "[pipeline] implicit recovered isolated_output_facts="
                      << recoveredIsolatedFacts
                      << " tuple_output_facts=" << recoveredOutputFacts << std::endl;
        }
        const GraphSummary implicitHandoffSummary = summarizeGraphLight(view);
        std::cout << "[pipeline] implicit handoff"
                  << " nodes=" << implicitHandoffSummary.nodes
                  << " edges=" << implicitHandoffSummary.edges
                  << " random_vars=" << implicitHandoffSummary.randomVariables
                  << " output_nodes=" << implicitHandoffSummary.outputNodes
                  << " precomputed_nodes=" << precomputedProbResult.size()
                  << " precomputed_tuples=" << precomputedTupleProbResult.size()
                  << std::endl;
        std::cout << "[pipeline] implicit rewrite total_ms=" << implicitResult.stats.totalMs
                  << " overlay_prep_ms=" << implicitResult.stats.overlayPrepMs
                  << " overlay_split_ms=" << implicitResult.stats.overlaySplitMs
                  << " overlay_fastpath_ms=" << implicitResult.stats.overlayFastPathMs
                  << " materialize_ms=" << implicitResult.stats.materializeMs
                  << " detect_ms=" << implicitResult.stats.graphDetectMs
                  << " graph_rewrite_ms=" << implicitResult.stats.graphRewriteMs
                  << " overlay_aliases=" << implicitResult.stats.overlayStats.aliasesCreated
                  << " overlay_edges_aliased=" << implicitResult.stats.overlayStats.edgesAliased
                  << " overlay_all_facts=" << implicitResult.stats.overlayStats.allFactsRewrites
                  << " overlay_single=" << implicitResult.stats.overlayStats.singleHyperedgeRewrites
                  << " overlay_linear=" << implicitResult.stats.overlayStats.linearTwoEdgeRewrites
                  << " overlay_parallel=" << implicitResult.stats.overlayStats.parallelEdgeRewrites
                  << " overlay_fan_out=" << implicitResult.stats.overlayStats.fanOutConvergeRewrites
                  << std::endl;
        if (result.hybridStage) {
            debugger.addInfo("rewrite_engine", "implicit");
            debugger.addInfo("implicit_overlay_prep_ms",
                    std::to_string(static_cast<long long>(implicitResult.stats.overlayPrepMs)));
            debugger.addInfo("implicit_graph_rewrite_ms",
                    std::to_string(static_cast<long long>(implicitResult.stats.graphRewriteMs)));
            result.hybridStage->logMessage(Level::INFO, "rewrite_engine=implicit");
            result.hybridStage->logMessage(Level::INFO,
                    "implicit_overlay_prep_ms=" +
                            std::to_string(static_cast<long long>(implicitResult.stats.overlayPrepMs)));
            result.hybridStage->logMessage(Level::INFO,
                    "implicit_graph_rewrite_ms=" +
                            std::to_string(static_cast<long long>(implicitResult.stats.graphRewriteMs)));
        }
    } else {
        GraphRewriter rewriter;
        RewriteFeatureFlags rewriteFlags;
        rewriteFlags.forceCompleteSisoDetect = opt.isForceCompleteSisoDetectEnabled();
        rewriteFlags.enableSingleHyperedge =
                !envFlagDisabled("SOUFFLE_REWRITE_DISABLE_SINGLE");
        rewriteFlags.enableAllFactsToSO = !envFlagDisabled("SOUFFLE_REWRITE_DISABLE_ALL_FACTS");
        rewriteFlags.enableLinearTwoEdge =
                !envFlagDisabled("SOUFFLE_REWRITE_DISABLE_LINEAR");
        rewriteFlags.enableParallelEdge =
                !envFlagDisabled("SOUFFLE_REWRITE_DISABLE_PARALLEL");
        rewriteFlags.enableFanOutConverge =
                !envFlagDisabled("SOUFFLE_REWRITE_DISABLE_FAN_OUT");
        rewriteFlags.enableCompaction =
                !envFlagDisabled("SOUFFLE_REWRITE_DISABLE_COMPACTION");
        const auto& splitMode = opt.getSplitMode();
        if (splitMode == "no-split") {
            rewriteFlags.splitMode = SplitMode::None;
        } else if (splitMode == "complete-split") {
            rewriteFlags.splitMode = SplitMode::Complete;
        } else {
            rewriteFlags.splitMode = SplitMode::Naive;
        }
        rewriteStats = rewriter.rewriteUntilFixpoint(*graph, view, opt.isProfiling(), rewriteFlags);
    }

    const auto rewriteMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - rewriteStart)
                                   .count();
    const auto randomVarsDelta = static_cast<long long>(rewriteStats.randomVarsBefore) -
            static_cast<long long>(rewriteStats.randomVarsAfter);
    const double randomVarsRatio = rewriteStats.randomVarsBefore == 0
            ? 0.0
            : static_cast<double>(rewriteStats.randomVarsAfter) /
                    static_cast<double>(rewriteStats.randomVarsBefore);
    std::cout << "[pipeline] rewrite took " << rewriteMs << " ms; iterations="
              << rewriteStats.numIterations << ", regions=" << rewriteStats.numRegionsRewritten
              << ", nodesRemoved=" << rewriteStats.numNodesRemoved
              << ", edgesRemoved=" << rewriteStats.numEdgesRemoved
              << ", edgesAdded=" << rewriteStats.numEdgesAdded
              << ", randomVarsBefore=" << rewriteStats.randomVarsBefore
              << ", randomVarsAfter=" << rewriteStats.randomVarsAfter
              << ", randomVarsDelta=" << randomVarsDelta
              << ", randomVarsRatio=" << randomVarsRatio
              << ", randomVarsRemoved=" << rewriteStats.totalRandomVars
              << ", simpleFactRegions=" << rewriteStats.simpleFactRegions << std::endl;
    if (result.hybridStage) {
        debugger.addInfo("rewrite_engine", opt.isImplicitRewriteEnabled() ? "implicit" : "legacy");
        debugger.addInfo("rewrite_ms", std::to_string(rewriteMs));
        result.hybridStage->logMessage(Level::INFO,
                std::string("rewrite_engine=") + (opt.isImplicitRewriteEnabled() ? "implicit" : "legacy"));
        result.hybridStage->logMessage(Level::INFO, "rewrite_ms=" + std::to_string(rewriteMs));
    }
    if (opt.isDumpDotEnabled()) {
        view.dumpDot(makeOutputPath(opt, "rewrite_final.dot"));
    }
    result.rewritePerformed = true;
    return result;
}

static bool resolveOnlineCliAvailability(
        const CmdOptions& opt, bool requested, bool rewritePerformed) {
    bool allowOnlineCli = requested;
    if (allowOnlineCli && rewritePerformed) {
        std::cout << "[pipeline] rewrite performed in full run; skip incremental CLI" << std::endl;
        allowOnlineCli = false;
    }
    if (allowOnlineCli && opt.isScbfEnabled()) {
        std::cout << "[pipeline] --scbf enabled; skip incremental CLI" << std::endl;
        allowOnlineCli = false;
    }
    return allowOnlineCli;
}

static void runKnowledgeRuntimeLane(const CmdOptions& opt, SouffleProgram& program,
        RuleManager& ruleManager, QueryManager& queryManager,
        std::unique_ptr<IncrementalDerivationGraph>& graph, IncSubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences, bool allowOnlineCli,
        StageInfo* rewriteHybridStage) {
    switch (program.getKnowledge()) {
        case souffle::Knowledge::BDD:
            runBddPipeline(
                    opt, program, ruleManager, queryManager, graph, view, evidences, allowOnlineCli,
                    rewriteHybridStage);
            return;
        case souffle::Knowledge::SDD:
            runSddPipeline(
                    opt, program, ruleManager, queryManager, graph, view, evidences, allowOnlineCli,
                    rewriteHybridStage);
            return;
        default:
            std::cerr << "Unknown knowledge representation" << std::endl;
            return;
    }
}

bool runNegationPostPass(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager) {
    const NegationPostPassMode mode = resolveNegationPostPassMode();
    if (mode != NegationPostPassMode::RuleApp) {
        if (mode == NegationPostPassMode::Off) {
            std::cout << "[negation-post-pass] mode=off" << std::endl;
        }
        return false;
    }

    auto probabilisticRelations = computeProbabilisticRelations(program, ruleManager, fact_prob);
    return runRuleAppNegationPostPass(program, ruleManager, probabilisticRelations);
}

void runPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        const std::unordered_map<UntypedTuple, double>& factProb,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli) {
    std::cout << std::fixed << std::setprecision(8);
    Debugger& debugger = Debugger::getInstance();
    configureRuntimeFromOptions(opt);
    precomputedProbResult.clear();
    precomputedTupleProbResult.clear();
    debugger.addInfo("full_runtime_lane", fullRuntimeLaneLabel(opt));
    debugger.addInfo("knowledge_backend", knowledgeBackendLabel(program.getKnowledge()));
    const NegationPostPassMode negationPostPassMode = resolveNegationPostPassMode();
    debugger.addInfo("negation_postpass_mode", negationPostPassModeLabel(negationPostPassMode));
    const auto probabilisticRelations = computeProbabilisticRelations(program, ruleManager, factProb);
    const auto seedTuples = buildSeedFactTuples(factProb, ruleManager);

    if (::detForceEnabled) {
        std::cout << "[det-force] enabled; skip derivation graph and emit prob=1.0" << std::endl;
        dumpDeterministicProbabilities(opt, program);
        if (enableOnlineCli) {
            std::cout << "[det-force] incremental CLI disabled" << std::endl;
        }
        debugger.endTurn();
        dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");
        return;
    }

    if (!isFullOnlyMode() && !evidences.empty()) {
        throw std::runtime_error("Evidence is only supported with --full-only (incremental mode disables evidence).");
    }

    debugger.startStage(StageKind::CREATE_GRAPH_FULL);
    debugger.addInfo("input_fact_size", std::to_string(countInitialInputFacts()));
    const RuleAppInventory ruleAppInventory = collectRuleAppInventory();
    debugger.addInfo("ruleapp_head_tuples", std::to_string(ruleAppInventory.headTuples));
    debugger.addInfo("ruleapp_null_sets", std::to_string(ruleAppInventory.nullRuleSets));
    debugger.addInfo("ruleapp_total", std::to_string(ruleAppInventory.totalRuleApps));
    debugger.addInfo("ruleapp_total_bindings", std::to_string(ruleAppInventory.totalBindings));
    debugger.addInfo("ruleapp_max_per_head", std::to_string(ruleAppInventory.maxRuleAppsPerHead));
    debugger.addInfo("ruleapp_unique_rules", std::to_string(ruleAppInventory.uniqueRuleIds));
    const double avgRuleAppsPerHead = ruleAppInventory.headTuples
            ? static_cast<double>(ruleAppInventory.totalRuleApps) /
                    static_cast<double>(ruleAppInventory.headTuples)
            : 0.0;
    const double avgBindingsPerRuleApp = ruleAppInventory.totalRuleApps
            ? static_cast<double>(ruleAppInventory.totalBindings) /
                    static_cast<double>(ruleAppInventory.totalRuleApps)
            : 0.0;
    debugger.addInfo("ruleapp_avg_per_head", std::to_string(avgRuleAppsPerHead));
    debugger.addInfo("ruleapp_avg_bindings", std::to_string(avgBindingsPerRuleApp));
    std::cout << "[pipeline] recorded ruleapps: heads=" << ruleAppInventory.headTuples
              << " null_sets=" << ruleAppInventory.nullRuleSets
              << " total=" << ruleAppInventory.totalRuleApps
              << " unique_rules=" << ruleAppInventory.uniqueRuleIds
              << " avg_per_head=" << avgRuleAppsPerHead
              << " avg_bindings=" << avgBindingsPerRuleApp
              << " max_per_head=" << ruleAppInventory.maxRuleAppsPerHead << std::endl;
    if (opt.isDumpJsonBeforeGraphEnabled()) {
        const auto tDump0 = std::chrono::steady_clock::now();
        dumpRuleAppsBeforeGraphJson(opt, ruleManager, factProb);
        const auto tDump1 = std::chrono::steady_clock::now();
        debugger.addInfo("dumpjson_before_graph_ms", std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(tDump1 - tDump0).count()));
        if (!needsMaterializedGraph(opt)) {
            std::cout << "[pipeline] skipping create graph; pre-graph dump satisfied requested outputs"
                      << std::endl;
            debugger.endStage();
            debugger.endTurn();
            dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");
            return;
        }
    }
    auto t0 = std::chrono::steady_clock::now();
    auto graph = std::unique_ptr<IncrementalDerivationGraph>(IncrementalDerivationGraph::createFrom(
            DerivationManager::untypedTuple2RuleApplications, ruleManager, queryManager, factProb, evidences));
    auto t1 = std::chrono::steady_clock::now();
    const GraphSummary createdSummary = summarizeGraphLight(*graph);
    addGraphSummaryInfo(debugger, "graph_", createdSummary);
    std::cout << "[pipeline] create graph took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
              << " ms\n";
    debugger.endStage();

    if (opt.isDumpDotEnabled()) {
        graph->dumpDot(makeOutputPath(opt, "before_prune.dot"));
    }
    if (opt.isDumpJsonBeforePruneEnabled()) {
        graph->dumpJson(makeOutputPath(opt, "derivation-before-prune.json"));
    }

    debugger.startStage(StageKind::PRUNING_FULL);
    auto t2 = std::chrono::steady_clock::now();
    IncSubgraphView view = buildFullIncViewLocal(*graph);
    const GraphSummary beforePrune = summarizeGraphLight(view);
    addGraphSummaryInfo(debugger, "before_", beforePrune);

    if (negationPostPassMode == NegationPostPassMode::Graph) {
        bool graphPostChanged = false;
        auto tNeg0 = std::chrono::steady_clock::now();
        view = runGraphNegationPostPass(
                opt, program, ruleManager, probabilisticRelations, seedTuples, *graph, graphPostChanged);
        auto tNeg1 = std::chrono::steady_clock::now();
        const GraphSummary afterGraphPost = summarizeGraphLight(view);
        addGraphSummaryInfo(debugger, "after_negation_", afterGraphPost);
        debugger.addInfo("negation_postpass_changed", graphPostChanged ? "1" : "0");
        debugger.addInfo("negation_postpass_ms", std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(tNeg1 - tNeg0).count()));
        view = pruneFilteredIncView(std::move(view), program.getOutputRelations());
        markGraphPrunedFlags(*graph, view);
    } else {
        view = graph->prune(program.getOutputRelations());
    }

    const GraphSummary afterPrune = summarizeGraphLight(view);
    addGraphSummaryInfo(debugger, "after_", afterPrune);
    debugger.addInfo("removed_nodes", std::to_string(
            beforePrune.nodes > afterPrune.nodes ? beforePrune.nodes - afterPrune.nodes : 0));
    debugger.addInfo("removed_edges", std::to_string(
            beforePrune.edges > afterPrune.edges ? beforePrune.edges - afterPrune.edges : 0));
    auto t3 = std::chrono::steady_clock::now();
    std::cout << "[pipeline] pruning took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
              << " ms\n";
    debugger.endStage();

    if (opt.isDumpDotEnabled()) {
        view.dumpDot(makeOutputPath(opt, "after_prune.dot"));
    }
    if (opt.isDumpJsonEnabled()) {
        view.dumpJson(makeOutputPath(opt, "derivation.json"));
    }
    std::cout << "[pipeline] selected runtime lane=" << fullRuntimeLaneLabel(opt) << std::endl;

    const RewriteLaneResult rewriteLane = runFullRewriteLane(opt, debugger, graph, view);
    const bool allowOnlineCli =
            resolveOnlineCliAvailability(opt, enableOnlineCli, rewriteLane.rewritePerformed);
    runKnowledgeRuntimeLane(
            opt, program, ruleManager, queryManager, graph, view, evidences, allowOnlineCli,
            rewriteLane.hybridStage);
}

}  // namespace souffle::problog
