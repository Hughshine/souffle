#include "souffle/problog/ImplicitSplitRewrite.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "souffle/problog/GraphRewriter.h"

namespace souffle::problog {
namespace {

constexpr double kImplicitSplitEps = 1e-12;
constexpr std::size_t kNaiveReachabilityCap = 50;
constexpr const char* kSplitShadowPrefix = "_split_shadow_";

bool nearlyZero(double value) {
    return std::abs(value) <= kImplicitSplitEps;
}

bool nearlyOne(double value) {
    return std::abs(1.0 - value) <= kImplicitSplitEps;
}

std::string nodeLabel(const NodePtr& node) {
    if (!node) {
        return "<null>";
    }
    return node->getTuple().toString() + "#" + std::to_string(node->getId());
}

bool assignmentBit(std::uint64_t mask, std::size_t index) {
    return ((mask >> index) & 1ULL) != 0ULL;
}

std::size_t canonicalFactIdForGraphNode(const NodePtr& node) {
    if (!node) {
        return 0;
    }
    if (!node->isShadow) {
        return node->getId();
    }
    const auto& tuple = node->getTuple();
    if (tuple.relation_name.rfind(kSplitShadowPrefix, 0) != 0) {
        return node->getId();
    }
    const std::string suffix = tuple.relation_name.substr(std::char_traits<char>::length(kSplitShadowPrefix));
    const auto pos = suffix.find('_');
    if (pos == std::string::npos) {
        return node->getId();
    }
    try {
        return static_cast<std::size_t>(std::stoull(suffix.substr(0, pos)));
    } catch (...) {
        return node->getId();
    }
}

using Clock = std::chrono::steady_clock;

double elapsedMs(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

IncSubgraphView buildFullIncView(IncrementalDerivationGraph& graph) {
    return IncSubgraphView(graph.getNodes(), graph.getEdges(), {}, {}, {}, {});
}

class ScopedCoutSilencer {
public:
    ScopedCoutSilencer() : old_(std::cout.rdbuf(sink_.rdbuf())) {}
    ~ScopedCoutSilencer() {
        std::cout.rdbuf(old_);
    }

private:
    std::ostringstream sink_;
    std::streambuf* old_ = nullptr;
};

}  // namespace

ImplicitSplitOverlay::ImplicitSplitOverlay(const IncrementalDerivationGraphViewInterface& view) : view_(view) {
    for (const auto& node : view_.getNodes()) {
        if (!node) {
            continue;
        }
        BaseNodeState state;
        state.originalIsFact = node->isFact;
        state.currentIsFact = node->isFact;
        state.factProbability = node->getProbability();
        state.needOutput = node->needOutput;
        state.hasEvidence = node->hasEvidence();
        nodeState_[node] = state;
        if (node->needOutput) {
            outputs_.push_back(node);
        }
    }

    for (const auto& edge : view_.getEdges()) {
        if (!edge) {
            continue;
        }
        ImplicitSplitOverlayEdge overlayEdge;
        overlayEdge.baseEdge = edge;
        overlayEdge.output = view_.getOutput(edge);
        overlayEdge.probability = edge->getProbability();
        overlayEdge.deterministic = edge->isDeterministic();
        overlayEdge.negations = view_.getBodyNegations(edge);
        for (const auto& input : view_.getInputs(edge)) {
            overlayEdge.inputs.push_back(SplitNodeRef{input, 0});
        }
        edges_.push_back(std::move(overlayEdge));
    }

    rebuildActiveEdgeIndices();
}

bool ImplicitSplitOverlay::isFactRef(const SplitNodeRef& ref) const {
    if (!ref.base) {
        return false;
    }
    if (ref.alias != 0) {
        auto it = nodeState_.find(ref.base);
        return it != nodeState_.end() && it->second.originalIsFact;
    }
    auto it = nodeState_.find(ref.base);
    return it != nodeState_.end() && it->second.currentIsFact;
}

// Alias refs inherit the original fact probability. Base refs use the current node state,
// which may change after overlay rewrites fold a derived node into a fact.
double ImplicitSplitOverlay::factProbabilityOf(const SplitNodeRef& ref) const {
    auto it = nodeState_.find(ref.base);
    if (it == nodeState_.end()) {
        return 0.0;
    }
    return it->second.factProbability;
}

std::size_t ImplicitSplitOverlay::activeIncomingCount(const NodePtr& node) const {
    return activeIncomingEdges(node).size();
}

std::size_t ImplicitSplitOverlay::activeOutgoingCount(const SplitNodeRef& ref) const {
    return activeOutgoingEdges(ref).size();
}

const std::vector<std::size_t>& ImplicitSplitOverlay::activeIncomingEdges(const NodePtr& node) const {
    static const std::vector<std::size_t> empty;
    auto it = activeIncomingEdgeIdsByNode_.find(node);
    if (it == activeIncomingEdgeIdsByNode_.end()) {
        return empty;
    }
    return it->second;
}

const std::vector<std::size_t>& ImplicitSplitOverlay::activeOutgoingEdges(const SplitNodeRef& ref) const {
    static const std::vector<std::size_t> empty;
    auto it = activeOutgoingEdgeIdsByRef_.find(ref);
    if (it == activeOutgoingEdgeIdsByRef_.end()) {
        return empty;
    }
    return it->second;
}

void ImplicitSplitOverlay::rebuildActiveEdgeIndices() {
    activeIncomingEdgeIdsByNode_.clear();
    activeOutgoingEdgeIdsByRef_.clear();
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        const auto& edge = edges_[i];
        if (!edge.active || !edge.output) {
            continue;
        }
        activeIncomingEdgeIdsByNode_[edge.output].push_back(i);
        std::unordered_set<SplitNodeRef, SplitNodeRefHash> seenInputs;
        for (const auto& input : edge.inputs) {
            if (seenInputs.insert(input).second) {
                activeOutgoingEdgeIdsByRef_[input].push_back(i);
            }
        }
    }
}

std::vector<std::vector<std::size_t>> ImplicitSplitOverlay::partitionFactOutgoingEdgesNaive(
        const NodePtr& fact, ImplicitSplitOverlayStats* stats) const {
    std::vector<std::vector<std::size_t>> groups;
    auto itState = nodeState_.find(fact);
    if (itState == nodeState_.end() || !itState->second.currentIsFact || !itState->second.originalIsFact ||
            itState->second.needOutput || itState->second.hasEvidence) {
        return groups;
    }

    const auto outs = activeOutgoingEdges(SplitNodeRef{fact, 0});
    if (outs.size() < 2) {
        return groups;
    }

    std::vector<std::unordered_set<NodePtr>> reachSets;
    reachSets.reserve(outs.size());
    bool skipFact = false;
    for (const auto edgeIndex : outs) {
        const auto& edge = edges_[edgeIndex];
        if (!edge.output) {
            skipFact = true;
            break;
        }
        if (stats) {
            ++stats->splitNaiveReachabilityRuns;
        }
        std::unordered_set<NodePtr> visited;
        std::queue<NodePtr> queue;
        visited.insert(edge.output);
        queue.push(edge.output);
        while (!queue.empty()) {
            NodePtr current = queue.front();
            queue.pop();
            for (const auto nextEdgeIndex : activeOutgoingEdges(SplitNodeRef{current, 0})) {
                const auto& nextEdge = edges_[nextEdgeIndex];
                if (!nextEdge.output) {
                    continue;
                }
                if (visited.insert(nextEdge.output).second) {
                    if (stats) {
                        ++stats->splitNaiveReachabilityVisited;
                    }
                    if (visited.size() > kNaiveReachabilityCap) {
                        skipFact = true;
                        if (stats) {
                            ++stats->splitNaiveCapSkips;
                        }
                        break;
                    }
                    queue.push(nextEdge.output);
                }
            }
            if (skipFact) {
                break;
            }
        }
        if (skipFact) {
            break;
        }
        reachSets.push_back(std::move(visited));
    }
    if (skipFact || reachSets.size() != outs.size()) {
        return {};
    }

    std::unordered_map<NodePtr, int> owner;
    std::vector<char> hasOverlap(reachSets.size(), 0);
    for (std::size_t i = 0; i < reachSets.size(); ++i) {
        for (const auto& node : reachSets[i]) {
            auto [it, inserted] = owner.emplace(node, static_cast<int>(i));
            if (inserted || it->second == static_cast<int>(i)) {
                continue;
            }
            hasOverlap[i] = 1;
            if (it->second >= 0) {
                hasOverlap[static_cast<std::size_t>(it->second)] = 1;
                it->second = -1;
            }
        }
    }

    std::vector<std::size_t> independent;
    for (std::size_t i = 0; i < hasOverlap.size(); ++i) {
        if (!hasOverlap[i]) {
            independent.push_back(i);
        }
    }
    if (independent.empty()) {
        return {};
    }

    std::unordered_set<std::size_t> independentSet(independent.begin(), independent.end());
    std::vector<std::size_t> baseGroup;
    for (std::size_t i = 0; i < outs.size(); ++i) {
        if (!independentSet.count(i)) {
            baseGroup.push_back(outs[i]);
        }
    }
    if (baseGroup.empty()) {
        baseGroup.push_back(outs[independent.front()]);
        independent.erase(independent.begin());
    }
    groups.push_back(std::move(baseGroup));
    for (const auto idx : independent) {
        groups.push_back({outs[idx]});
    }
    return groups.size() > 1 ? groups : std::vector<std::vector<std::size_t>>{};
}

std::vector<std::vector<std::size_t>> ImplicitSplitOverlay::partitionFactOutgoingEdgesComplete(
        const NodePtr& fact, ImplicitSplitOverlayStats* stats) const {
    (void)stats;
    std::vector<std::vector<std::size_t>> groups;
    auto itState = nodeState_.find(fact);
    if (itState == nodeState_.end() || !itState->second.currentIsFact || !itState->second.originalIsFact ||
            itState->second.needOutput || itState->second.hasEvidence) {
        return groups;
    }

    const auto outs = activeOutgoingEdges(SplitNodeRef{fact, 0});
    if (outs.size() < 2) {
        return groups;
    }

    std::vector<std::unordered_set<NodePtr>> reachSets;
    reachSets.reserve(outs.size());
    for (const auto edgeIndex : outs) {
        const auto& edge = edges_[edgeIndex];
        if (!edge.output) {
            return {};
        }
        std::unordered_set<NodePtr> visited;
        std::queue<NodePtr> queue;
        visited.insert(edge.output);
        queue.push(edge.output);
        while (!queue.empty()) {
            NodePtr current = queue.front();
            queue.pop();
            for (const auto nextEdgeIndex : activeOutgoingEdges(SplitNodeRef{current, 0})) {
                const auto& nextEdge = edges_[nextEdgeIndex];
                if (!nextEdge.output) {
                    continue;
                }
                if (visited.insert(nextEdge.output).second) {
                    queue.push(nextEdge.output);
                }
            }
        }
        reachSets.push_back(std::move(visited));
    }

    std::vector<int> parent(outs.size());
    std::iota(parent.begin(), parent.end(), 0);
    auto find = [&](auto&& self, int x) -> int {
        if (parent[x] == x) {
            return x;
        }
        parent[x] = self(self, parent[x]);
        return parent[x];
    };
    auto unite = [&](int a, int b) {
        a = find(find, a);
        b = find(find, b);
        if (a != b) {
            parent[b] = a;
        }
    };

    for (std::size_t i = 0; i < reachSets.size(); ++i) {
        for (std::size_t j = i + 1; j < reachSets.size(); ++j) {
            bool overlap = false;
            if (reachSets[i].size() < reachSets[j].size()) {
                for (const auto& node : reachSets[i]) {
                    if (reachSets[j].count(node)) {
                        overlap = true;
                        break;
                    }
                }
            } else {
                for (const auto& node : reachSets[j]) {
                    if (reachSets[i].count(node)) {
                        overlap = true;
                        break;
                    }
                }
            }
            if (overlap) {
                unite(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }

    std::unordered_map<int, std::vector<std::size_t>> byRoot;
    for (std::size_t i = 0; i < outs.size(); ++i) {
        byRoot[find(find, static_cast<int>(i))].push_back(outs[i]);
    }
    if (byRoot.size() <= 1) {
        return {};
    }

    for (auto& [_, edgeGroup] : byRoot) {
        groups.push_back(std::move(edgeGroup));
    }
    std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) {
        return a.size() > b.size();
    });
    return groups;
}

void ImplicitSplitOverlay::applyEdgeGroupsAsAliases(const NodePtr& fact,
        const std::vector<std::vector<std::size_t>>& groups, ImplicitSplitOverlayStats* stats) {
    if (groups.size() <= 1) {
        return;
    }
    for (std::size_t gi = 1; gi < groups.size(); ++gi) {
        const auto aliasId = ++nextAliasIdByFact_[fact];
        aliasesByFact_[fact].push_back(aliasId);
        if (stats) {
            ++stats->aliasesCreated;
        }
        for (const auto edgeIndex : groups[gi]) {
            auto& edge = edges_[edgeIndex];
            bool touched = false;
            for (auto& input : edge.inputs) {
                if (input.base == fact && input.alias == 0) {
                    auto itBaseOut = activeOutgoingEdgeIdsByRef_.find(SplitNodeRef{fact, 0});
                    if (itBaseOut != activeOutgoingEdgeIdsByRef_.end()) {
                        auto& baseList = itBaseOut->second;
                        baseList.erase(std::remove(baseList.begin(), baseList.end(), edgeIndex), baseList.end());
                    }
                    input.alias = aliasId;
                    auto& aliasList = activeOutgoingEdgeIdsByRef_[SplitNodeRef{fact, aliasId}];
                    if (std::find(aliasList.begin(), aliasList.end(), edgeIndex) == aliasList.end()) {
                        aliasList.push_back(edgeIndex);
                    }
                    touched = true;
                }
            }
            if (touched && stats) {
                ++stats->edgesAliased;
            }
        }
    }
}

void ImplicitSplitOverlay::applySplit(ImplicitSplitMode mode, ImplicitSplitOverlayStats* stats) {
    if (mode == ImplicitSplitMode::None) {
        return;
    }
    std::vector<NodePtr> facts;
    for (const auto& [node, state] : nodeState_) {
        if (state.originalIsFact && state.currentIsFact) {
            facts.push_back(node);
        }
    }
    std::sort(facts.begin(), facts.end(), [](const NodePtr& a, const NodePtr& b) {
        return a->getId() < b->getId();
    });
    if (stats) {
        stats->splitFactsConsidered += facts.size();
    }
    for (const auto& fact : facts) {
        std::vector<std::vector<std::size_t>> groups;
        const auto partitionStart = Clock::now();
        if (mode == ImplicitSplitMode::Naive) {
            groups = partitionFactOutgoingEdgesNaive(fact, stats);
            if (stats) {
                stats->splitNaiveMs += elapsedMs(partitionStart);
            }
        } else {
            groups = partitionFactOutgoingEdgesComplete(fact, stats);
            if (stats) {
                stats->splitCompleteMs += elapsedMs(partitionStart);
            }
        }
        const auto aliasStart = Clock::now();
        applyEdgeGroupsAsAliases(fact, groups, stats);
        if (stats) {
            stats->splitAliasApplyMs += elapsedMs(aliasStart);
            if (groups.size() > 1) {
                ++stats->splitFactsAliased;
            }
        }
    }
}

bool ImplicitSplitOverlay::rewriteAllFactsPass(ImplicitSplitOverlayStats* stats) {
    bool changed = false;
    for (auto& edge : edges_) {
        if (!edge.active || edge.inputs.empty() || !edge.output) {
            continue;
        }
        bool allFacts = true;
        for (const auto& input : edge.inputs) {
            if (!isFactRef(input)) {
                allFacts = false;
                break;
            }
            const auto& inputState = nodeState_.at(input.base);
            if (inputState.hasEvidence) {
                allFacts = false;
                break;
            }
            if (activeOutgoingCount(input) != 1) {
                allFacts = false;
                break;
            }
        }
        if (!allFacts || activeIncomingCount(edge.output) != 1) {
            continue;
        }

        double probability = edge.deterministic ? 1.0 : edge.probability;
        for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
            double inputProb = factProbabilityOf(edge.inputs[i]);
            if (i < edge.negations.size() && edge.negations[i]) {
                inputProb = 1.0 - inputProb;
            }
            probability *= inputProb;
        }
        auto& outState = nodeState_.at(edge.output);
        outState.currentIsFact = true;
        outState.factProbability = std::clamp(probability, 0.0, 1.0);
        edge.active = false;
        changed = true;
        if (stats) {
            ++stats->allFactsRewrites;
            ++stats->removedEdges;
            ++stats->factOutputsFolded;
        }
    }
    return changed;
}

bool ImplicitSplitOverlay::rewriteSingleHyperedgePass(ImplicitSplitOverlayStats* stats) {
    bool changed = false;
    for (auto& edge : edges_) {
        if (!edge.active || edge.inputs.size() <= 1 || !edge.output) {
            continue;
        }
        std::size_t factInputs = 0;
        SplitNodeRef si;
        bool sawSi = false;
        bool invalid = false;
        bool siNegated = false;
        for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
            const auto& input = edge.inputs[i];
            if (isFactRef(input)) {
                const auto& inputState = nodeState_.at(input.base);
                if (activeIncomingCount(input.base) != 0 || inputState.hasEvidence || inputState.needOutput) {
                    invalid = true;
                    break;
                }
                if (activeOutgoingCount(input) != 1) {
                    invalid = true;
                    break;
                }
                ++factInputs;
                continue;
            }
            if (sawSi) {
                invalid = true;
                break;
            }
            sawSi = true;
            si = input;
            siNegated = i < edge.negations.size() ? edge.negations[i] : false;
        }
        if (invalid || !sawSi || factInputs == 0 || isFactRef(si)) {
            continue;
        }

        double probability = edge.deterministic ? 1.0 : edge.probability;
        for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
            if (edge.inputs[i] == si) {
                continue;
            }
            double inputProb = factProbabilityOf(edge.inputs[i]);
            if (i < edge.negations.size() && edge.negations[i]) {
                inputProb = 1.0 - inputProb;
            }
            probability *= inputProb;
        }
        if (nearlyZero(probability)) {
            edge.active = false;
            changed = true;
            if (stats) {
                ++stats->singleHyperedgeRewrites;
                ++stats->removedEdges;
            }
            continue;
        }

        edge.inputs = {si};
        edge.negations = {siNegated};
        edge.probability = std::clamp(probability, 0.0, 1.0);
        edge.deterministic = nearlyOne(edge.probability);
        changed = true;
        if (stats) {
            ++stats->singleHyperedgeRewrites;
        }
    }
    return changed;
}

void ImplicitSplitOverlay::rewriteFastPathsToFixpoint(
        bool enableSingleHyperedge, bool enableAllFacts, ImplicitSplitOverlayStats* stats) {
    struct FastPathCandidate {
        enum class Kind {
            SingleHyperedge,
            AllFacts,
        };

        Kind kind;
        std::size_t edgeIndex = 0;
        std::vector<SplitNodeRef> internalNodes;
    };

    const auto initialRebuildStart = Clock::now();
    rebuildActiveEdgeIndices();
    if (stats) {
        ++stats->rebuildIndexCount;
        stats->rebuildIndexMs += elapsedMs(initialRebuildStart);
    }

    auto collectInternalNodes = [&](std::size_t edgeIndex) {
        std::vector<SplitNodeRef> nodes;
        std::unordered_set<SplitNodeRef, SplitNodeRefHash> seen;
        if (edgeIndex >= edges_.size()) {
            return nodes;
        }
        const auto& edge = edges_[edgeIndex];
        auto addNode = [&](const SplitNodeRef& ref) {
            if (!ref.base) {
                return;
            }
            if (seen.insert(ref).second) {
                nodes.push_back(ref);
            }
        };
        addNode(SplitNodeRef{edge.output, 0});
        for (const auto& input : edge.inputs) {
            addNode(input);
        }
        return nodes;
    };

    auto classifySingleHyperedge = [&](std::size_t edgeIndex, SplitNodeRef* siOut,
                                       bool* siNegatedOut) {
        if (edgeIndex >= edges_.size()) {
            return false;
        }
        const auto& edge = edges_[edgeIndex];
        if (!edge.active || edge.inputs.size() <= 1 || !edge.output) {
            return false;
        }
        std::size_t factInputs = 0;
        SplitNodeRef si{};
        bool sawSi = false;
        bool invalid = false;
        bool siNegated = false;
        for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
            const auto& input = edge.inputs[i];
            if (isFactRef(input)) {
                const auto& inputState = nodeState_.at(input.base);
                if (activeIncomingCount(input.base) != 0 || inputState.hasEvidence || inputState.needOutput) {
                    invalid = true;
                    break;
                }
                if (activeOutgoingCount(input) != 1) {
                    invalid = true;
                    break;
                }
                ++factInputs;
                continue;
            }
            if (sawSi) {
                invalid = true;
                break;
            }
            sawSi = true;
            si = input;
            siNegated = i < edge.negations.size() ? edge.negations[i] : false;
        }
        if (invalid || !sawSi || factInputs == 0 || isFactRef(si) || !si.base || si.base == edge.output) {
            return false;
        }
        if (siOut) {
            *siOut = si;
        }
        if (siNegatedOut) {
            *siNegatedOut = siNegated;
        }
        return true;
    };

    auto isAllFactsCandidate = [&](std::size_t edgeIndex) {
        if (edgeIndex >= edges_.size()) {
            return false;
        }
        const auto& edge = edges_[edgeIndex];
        if (!edge.active || edge.inputs.empty() || !edge.output) {
            return false;
        }
        for (const auto& input : edge.inputs) {
            if (!isFactRef(input)) {
                return false;
            }
            const auto& inputState = nodeState_.at(input.base);
            if (inputState.hasEvidence) {
                return false;
            }
            if (activeOutgoingCount(input) != 1) {
                return false;
            }
        }
        return activeIncomingCount(edge.output) == 1;
    };

    auto applySingleHyperedge = [&](std::size_t edgeIndex) {
        SplitNodeRef si{};
        bool siNegated = false;
        if (!classifySingleHyperedge(edgeIndex, &si, &siNegated)) {
            return false;
        }
        auto& edge = edges_[edgeIndex];
        double probability = edge.deterministic ? 1.0 : edge.probability;
        for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
            if (edge.inputs[i] == si) {
                continue;
            }
            double inputProb = factProbabilityOf(edge.inputs[i]);
            if (i < edge.negations.size() && edge.negations[i]) {
                inputProb = 1.0 - inputProb;
            }
            probability *= inputProb;
        }
        if (nearlyZero(probability)) {
            edge.active = false;
            if (stats) {
                ++stats->singleHyperedgeRewrites;
                ++stats->removedEdges;
            }
            return true;
        }

        edge.inputs = {si};
        edge.negations = {siNegated};
        edge.probability = std::clamp(probability, 0.0, 1.0);
        edge.deterministic = nearlyOne(edge.probability);
        if (stats) {
            ++stats->singleHyperedgeRewrites;
        }
        return true;
    };

    auto applyAllFacts = [&](std::size_t edgeIndex) {
        if (!isAllFactsCandidate(edgeIndex)) {
            return false;
        }
        auto& edge = edges_[edgeIndex];
        double probability = edge.deterministic ? 1.0 : edge.probability;
        for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
            double inputProb = factProbabilityOf(edge.inputs[i]);
            if (i < edge.negations.size() && edge.negations[i]) {
                inputProb = 1.0 - inputProb;
            }
            probability *= inputProb;
        }
        auto& outState = nodeState_.at(edge.output);
        outState.currentIsFact = true;
        outState.factProbability = std::clamp(probability, 0.0, 1.0);
        edge.active = false;
        if (stats) {
            ++stats->allFactsRewrites;
            ++stats->removedEdges;
            ++stats->factOutputsFolded;
        }
        return true;
    };

    while (true) {
        if (stats) {
            ++stats->fastPathIterations;
        }
        std::vector<FastPathCandidate> candidates;
        candidates.reserve(edges_.size());
        for (std::size_t edgeIndex = 0; edgeIndex < edges_.size(); ++edgeIndex) {
            if (enableSingleHyperedge) {
                if (classifySingleHyperedge(edgeIndex, nullptr, nullptr)) {
                    candidates.push_back(FastPathCandidate{
                            FastPathCandidate::Kind::SingleHyperedge, edgeIndex, collectInternalNodes(edgeIndex)});
                }
            }
            if (enableAllFacts) {
                if (isAllFactsCandidate(edgeIndex)) {
                    candidates.push_back(FastPathCandidate{
                            FastPathCandidate::Kind::AllFacts, edgeIndex, collectInternalNodes(edgeIndex)});
                }
            }
        }

        std::stable_sort(candidates.begin(), candidates.end(),
                [](const FastPathCandidate& a, const FastPathCandidate& b) {
                    return a.internalNodes.size() < b.internalNodes.size();
                });

        std::vector<FastPathCandidate> selected;
        selected.reserve(candidates.size());
        std::unordered_set<SplitNodeRef, SplitNodeRefHash> usedNodes;
        for (const auto& candidate : candidates) {
            bool overlap = false;
            for (const auto& node : candidate.internalNodes) {
                if (usedNodes.count(node)) {
                    overlap = true;
                    break;
                }
            }
            if (overlap) {
                continue;
            }
            for (const auto& node : candidate.internalNodes) {
                usedNodes.insert(node);
            }
            selected.push_back(candidate);
        }

        bool changed = false;
        for (const auto& candidate : selected) {
            switch (candidate.kind) {
                case FastPathCandidate::Kind::SingleHyperedge: {
                    const auto singleStart = Clock::now();
                    const bool applied = applySingleHyperedge(candidate.edgeIndex);
                    if (stats) {
                        stats->fastPathSingleMs += elapsedMs(singleStart);
                    }
                    changed = applied || changed;
                    break;
                }
                case FastPathCandidate::Kind::AllFacts: {
                    const auto allFactsStart = Clock::now();
                    const bool applied = applyAllFacts(candidate.edgeIndex);
                    if (stats) {
                        stats->fastPathAllFactsMs += elapsedMs(allFactsStart);
                    }
                    changed = applied || changed;
                    break;
                }
            }
        }

        if (!changed) {
            break;
        }
        const auto rebuildStart = Clock::now();
        rebuildActiveEdgeIndices();
        if (stats) {
            ++stats->rebuildIndexCount;
            stats->rebuildIndexMs += elapsedMs(rebuildStart);
        }
    }
}

std::vector<SplitNodeRef> ImplicitSplitOverlay::enumerateActiveFactRefs() const {
    std::vector<SplitNodeRef> refs;
    std::unordered_set<SplitNodeRef, SplitNodeRefHash> seen;
    auto addRef = [&](const SplitNodeRef& ref) {
        if (!isFactRef(ref)) {
            return;
        }
        if (seen.insert(ref).second) {
            refs.push_back(ref);
        }
    };

    for (const auto& output : outputs_) {
        if (nodeState_.at(output).currentIsFact) {
            addRef(SplitNodeRef{output, 0});
        }
    }
    for (const auto& edge : edges_) {
        if (!edge.active) {
            continue;
        }
        for (const auto& input : edge.inputs) {
            addRef(input);
        }
        if (nodeState_.at(edge.output).currentIsFact) {
            addRef(SplitNodeRef{edge.output, 0});
        }
    }
    std::sort(refs.begin(), refs.end(), [](const SplitNodeRef& a, const SplitNodeRef& b) {
        if (a.base->getId() != b.base->getId()) {
            return a.base->getId() < b.base->getId();
        }
        return a.alias < b.alias;
    });
    return refs;
}

std::vector<const ImplicitSplitOverlayEdge*> ImplicitSplitOverlay::activeEdgesSorted() const {
    std::vector<const ImplicitSplitOverlayEdge*> result;
    for (const auto& edge : edges_) {
        if (edge.active) {
            result.push_back(&edge);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) {
        const std::size_t aid = a->baseEdge ? a->baseEdge->getId() : 0;
        const std::size_t bid = b->baseEdge ? b->baseEdge->getId() : 0;
        return aid < bid;
    });
    return result;
}

std::vector<OverlayOutputProbability> ImplicitSplitOverlay::computeOutputMarginalsExact() const {
    const auto factRefs = enumerateActiveFactRefs();
    const auto activeEdges = activeEdgesSorted();

    std::vector<NodePtr> probabilisticFactBases;
    std::unordered_set<NodePtr> seenProbabilisticFactBases;
    for (const auto& ref : factRefs) {
        const double probability = factProbabilityOf(ref);
        if (probability > 0.0 && probability < 1.0 && seenProbabilisticFactBases.insert(ref.base).second) {
            probabilisticFactBases.push_back(ref.base);
        }
    }
    std::vector<const ImplicitSplitOverlayEdge*> probabilisticEdges;
    for (const auto* edge : activeEdges) {
        if (!edge->deterministic && edge->probability > 0.0 && edge->probability < 1.0) {
            probabilisticEdges.push_back(edge);
        }
    }

    const std::size_t totalVars = probabilisticFactBases.size() + probabilisticEdges.size();
    if (totalVars > 24) {
        throw std::runtime_error("implicit split exact evaluator only supports up to 24 random variables in smoke tests");
    }

    std::unordered_map<NodePtr, std::size_t> factVarIndex;
    std::unordered_map<const ImplicitSplitOverlayEdge*, std::size_t> edgeVarIndex;
    for (std::size_t i = 0; i < probabilisticFactBases.size(); ++i) {
        factVarIndex[probabilisticFactBases[i]] = i;
    }
    for (std::size_t i = 0; i < probabilisticEdges.size(); ++i) {
        edgeVarIndex[probabilisticEdges[i]] = probabilisticFactBases.size() + i;
    }

    std::unordered_map<NodePtr, double> marginals;
    for (const auto& output : outputs_) {
        marginals[output] = 0.0;
    }

    const std::uint64_t assignmentCount = 1ULL << totalVars;
    for (std::uint64_t mask = 0; mask < assignmentCount; ++mask) {
        double weight = 1.0;
        std::unordered_map<NodePtr, bool> baseFactTruth;
        for (const auto& base : probabilisticFactBases) {
            const double probability = factProbabilityOf(SplitNodeRef{base, 0});
            bool value = true;
            auto itIdx = factVarIndex.find(base);
            if (itIdx != factVarIndex.end()) {
                value = assignmentBit(mask, itIdx->second);
                weight *= value ? probability : (1.0 - probability);
            }
            baseFactTruth[base] = value;
        }
        if (nearlyZero(weight)) {
            continue;
        }

        std::unordered_map<NodePtr, bool> nodeTruth;
        for (const auto& [node, state] : nodeState_) {
            bool value = false;
            if (state.currentIsFact) {
                const double probability = factProbabilityOf(SplitNodeRef{node, 0});
                auto itTruth = baseFactTruth.find(node);
                value = (itTruth != baseFactTruth.end()) ? itTruth->second : (probability >= 1.0 - kImplicitSplitEps);
            }
            nodeTruth[node] = value;
        }
        std::unordered_map<const ImplicitSplitOverlayEdge*, bool> edgeTruth;
        edgeTruth.reserve(activeEdges.size());
        for (const auto* edge : activeEdges) {
            bool edgeEnabled = true;
            auto itEdgeVar = edgeVarIndex.find(edge);
            if (itEdgeVar != edgeVarIndex.end()) {
                const bool edgeValue = assignmentBit(mask, itEdgeVar->second);
                const double edgeProbability = edge->probability;
                weight *= edgeValue ? edgeProbability : (1.0 - edgeProbability);
                edgeEnabled = edgeValue;
            } else if (!edge->deterministic && nearlyZero(edge->probability)) {
                edgeEnabled = false;
            }
            edgeTruth[edge] = edgeEnabled;
        }
        if (nearlyZero(weight)) {
            continue;
        }

        bool changed = true;
        std::size_t rounds = 0;
        while (changed && rounds <= nodeState_.size() + activeEdges.size()) {
            changed = false;
            ++rounds;
            for (const auto* edge : activeEdges) {
                const bool edgeEnabled = edgeTruth[edge];
                if (!edgeEnabled) {
                    continue;
                }
                bool bodyTrue = true;
                for (std::size_t i = 0; i < edge->inputs.size(); ++i) {
                    const auto& input = edge->inputs[i];
                    bool value = false;
                    if (isFactRef(input)) {
                        const double probability = factProbabilityOf(input);
                        auto itTruth = baseFactTruth.find(input.base);
                        value = (itTruth != baseFactTruth.end()) ? itTruth->second : (probability >= 1.0 - kImplicitSplitEps);
                    } else {
                        value = nodeTruth[input.base];
                    }
                    if (i < edge->negations.size() && edge->negations[i]) {
                        value = !value;
                    }
                    if (!value) {
                        bodyTrue = false;
                        break;
                    }
                }
                if (bodyTrue && !nodeTruth[edge->output]) {
                    nodeTruth[edge->output] = true;
                    changed = true;
                }
            }
        }

        for (const auto& output : outputs_) {
            if (nodeTruth[output]) {
                marginals[output] += weight;
            }
        }
    }

    std::vector<OverlayOutputProbability> result;
    result.reserve(outputs_.size());
    for (const auto& output : outputs_) {
        result.push_back(OverlayOutputProbability{output, marginals[output]});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.output->getId() < b.output->getId();
    });
    return result;
}

std::vector<OverlayOutputProbability> ImplicitSplitOverlay::collectDirectOutputProbabilities() const {
    std::vector<OverlayOutputProbability> result;
    result.reserve(outputs_.size());
    for (const auto& output : outputs_) {
        const auto it = nodeState_.find(output);
        if (it == nodeState_.end()) {
            continue;
        }
        const auto& state = it->second;
        if (state.hasEvidence) {
            continue;
        }
        if (state.currentIsFact) {
            result.push_back(OverlayOutputProbability{output, state.factProbability});
            continue;
        }
        if (activeIncomingCount(output) == 0) {
            result.push_back(OverlayOutputProbability{output, 0.0});
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.output->getId() < b.output->getId();
    });
    return result;
}

OverlayGraphStats ImplicitSplitOverlay::computeStats() const {
    OverlayGraphStats stats;
    for (const auto& edge : edges_) {
        if (edge.active) {
            ++stats.activeEdges;
        }
    }
    for (const auto& [_, aliases] : aliasesByFact_) {
        stats.activeAliases += aliases.size();
    }
    for (const auto& [_, state] : nodeState_) {
        if (!state.originalIsFact && state.currentIsFact) {
            ++stats.derivedFactOverrides;
        }
    }
    return stats;
}

std::string ImplicitSplitOverlay::summarize() const {
    const auto stats = computeStats();
    std::ostringstream oss;
    oss << "[implicit-split-overlay] outputs=" << outputs_.size()
        << " active_edges=" << stats.activeEdges
        << " aliases=" << stats.activeAliases
        << " derived_fact_overrides=" << stats.derivedFactOverrides;
    return oss.str();
}

MaterializedImplicitSplitGraph ImplicitSplitOverlay::materializeToGraph(
        const std::unordered_set<NodePtr>* skippedOutputNodes) const {
    MaterializedImplicitSplitGraph out;
    out.graph = std::make_unique<IncrementalDerivationGraph>();

    std::unordered_map<NodePtr, NodePtr> baseNodeMap;
    std::unordered_map<SplitNodeRef, NodePtr, SplitNodeRefHash> refNodeMap;

    auto ensureBaseNode = [&](const NodePtr& base) -> NodePtr {
        auto it = baseNodeMap.find(base);
        if (it != baseNodeMap.end()) {
            return it->second;
        }
        const auto& state = nodeState_.at(base);
        NodePtr materialized = out.graph->createNode(base->getTuple(), state.factProbability);
        materialized->isFact = state.currentIsFact;
        materialized->setProbability(state.factProbability);
        const bool skipOutput = skippedOutputNodes && skippedOutputNodes->count(base) > 0;
        if (state.needOutput && !skipOutput) {
            materialized->setQuery();
            out.outputs.push_back(materialized);
        }
        if (state.hasEvidence) {
            materialized->setEvidence(base->getEvidenceValue());
        }
        baseNodeMap.emplace(base, materialized);
        refNodeMap.emplace(SplitNodeRef{base, 0}, materialized);
        return materialized;
    };

    auto ensureRefNode = [&](const SplitNodeRef& ref) -> NodePtr {
        auto it = refNodeMap.find(ref);
        if (it != refNodeMap.end()) {
            return it->second;
        }
        if (ref.alias == 0) {
            return ensureBaseNode(ref.base);
        }
        UntypedTuple shadowTuple = ref.base->getTuple();
        shadowTuple.relation_name = std::string(kSplitShadowPrefix) +
                std::to_string(ref.base->getId()) + "_alias" + std::to_string(ref.alias);
        NodePtr shadow = out.graph->createNode(shadowTuple, factProbabilityOf(ref));
        shadow->isFact = true;
        shadow->isShadow = true;
        shadow->setProbability(factProbabilityOf(ref));
        refNodeMap.emplace(ref, shadow);
        ++out.aliasNodes;
        return shadow;
    };

    for (const auto& output : outputs_) {
        if (skippedOutputNodes && skippedOutputNodes->count(output) > 0) {
            continue;
        }
        ensureBaseNode(output);
    }

    for (const auto& edge : edges_) {
        if (!edge.active || !edge.output) {
            continue;
        }
        std::vector<NodePtr> inputs;
        inputs.reserve(edge.inputs.size());
        for (const auto& input : edge.inputs) {
            inputs.push_back(ensureRefNode(input));
        }
        NodePtr output = ensureBaseNode(edge.output);
        bool preserveBaseEdge = false;
        if (edge.baseEdge) {
            const auto& baseInputs = edge.baseEdge->getInputs();
            const auto& baseNegs = edge.baseEdge->getBodyNegations();
            preserveBaseEdge = (edge.baseEdge->getOutput() == edge.output) &&
                    (baseInputs.size() == edge.inputs.size()) && (baseNegs == edge.negations) &&
                    (std::abs(edge.baseEdge->getProbability() - edge.probability) <= kImplicitSplitEps);
            if (preserveBaseEdge) {
                for (std::size_t i = 0; i < baseInputs.size(); ++i) {
                    if (edge.inputs[i].alias != 0 || edge.inputs[i].base != baseInputs[i]) {
                        preserveBaseEdge = false;
                        break;
                    }
                }
            }
        }
        const Rule* rule = preserveBaseEdge && edge.baseEdge ? edge.baseEdge->getRule() : nullptr;
        const RuleApplication ruleApp =
                preserveBaseEdge && edge.baseEdge ? edge.baseEdge->getRuleApp() : naiveRuleApplication;
        EdgePtr materializedEdge = out.graph->createHyperedge(inputs, output, rule, edge.negations, ruleApp);
        if (!materializedEdge) {
            throw std::runtime_error("implicit split materialization failed to create hyperedge");
        }
        materializedEdge->setProbability(edge.probability);
    }

    out.totalNodes = out.graph->getNodes().size();
    out.totalEdges = out.graph->getEdges().size();
    out.liveNodes = out.graph->getNodes();
    out.liveEdges = out.graph->getEdges();
    return out;
}

std::vector<OverlayOutputProbability> computeGraphOutputMarginalsExact(
        const IncrementalDerivationGraphViewInterface& view,
        const std::vector<NodePtr>& outputs,
        const std::unordered_map<NodePtr, double>* precomputedOutputs) {
    std::vector<NodePtr> activeOutputs;
    std::unordered_map<NodePtr, double> marginals;
    for (const auto& output : outputs) {
        if (precomputedOutputs && precomputedOutputs->count(output)) {
            marginals[output] = precomputedOutputs->at(output);
        } else {
            activeOutputs.push_back(output);
            marginals[output] = 0.0;
        }
    }
    if (activeOutputs.empty()) {
        std::vector<OverlayOutputProbability> result;
        result.reserve(outputs.size());
        for (const auto& output : outputs) {
            result.push_back(OverlayOutputProbability{output, marginals[output]});
        }
        return result;
    }

    std::vector<NodePtr> probabilisticFacts;
    std::vector<EdgePtr> probabilisticEdges;
    for (const auto& node : view.getNodes()) {
        if (node && node->isFact && node->getProbability() > 0.0 && node->getProbability() < 1.0) {
            probabilisticFacts.push_back(node);
        }
    }
    for (const auto& edge : view.getEdges()) {
        if (edge && !edge->isDeterministic() && edge->getProbability() > 0.0 && edge->getProbability() < 1.0) {
            probabilisticEdges.push_back(edge);
        }
    }
    std::unordered_map<std::size_t, std::size_t> factVarIndex;
    std::unordered_map<std::size_t, double> factProbabilities;
    std::unordered_map<EdgePtr, std::size_t> edgeVarIndex;
    std::vector<std::size_t> probabilisticFactIds;
    probabilisticFactIds.reserve(probabilisticFacts.size());
    for (const auto& fact : probabilisticFacts) {
        const auto canonicalId = canonicalFactIdForGraphNode(fact);
        if (factVarIndex.count(canonicalId)) {
            continue;
        }
        factVarIndex[canonicalId] = probabilisticFactIds.size();
        factProbabilities[canonicalId] = fact->getProbability();
        probabilisticFactIds.push_back(canonicalId);
    }
    const std::size_t totalVars = probabilisticFactIds.size() + probabilisticEdges.size();
    if (totalVars > 24) {
        throw std::runtime_error("graph exact evaluator only supports up to 24 random variables in smoke tests");
    }
    for (std::size_t i = 0; i < probabilisticEdges.size(); ++i) {
        edgeVarIndex[probabilisticEdges[i]] = probabilisticFactIds.size() + i;
    }

    const std::uint64_t assignmentCount = 1ULL << totalVars;
    for (std::uint64_t mask = 0; mask < assignmentCount; ++mask) {
        double weight = 1.0;
        std::unordered_map<std::size_t, bool> factTruth;
        for (const auto canonicalId : probabilisticFactIds) {
            const double probability = factProbabilities.at(canonicalId);
            const bool value = assignmentBit(mask, factVarIndex.at(canonicalId));
            weight *= value ? probability : (1.0 - probability);
            factTruth[canonicalId] = value;
        }
        std::unordered_map<NodePtr, bool> nodeTruth;
        for (const auto& node : view.getNodes()) {
            if (!node) {
                continue;
            }
            bool value = false;
            if (node->isFact) {
                const auto canonicalId = canonicalFactIdForGraphNode(node);
                const double probability = node->getProbability();
                auto itTruth = factTruth.find(canonicalId);
                if (itTruth != factTruth.end()) {
                    value = itTruth->second;
                } else {
                    value = probability >= 1.0 - kImplicitSplitEps;
                }
            }
            nodeTruth[node] = value;
        }
        if (nearlyZero(weight)) {
            continue;
        }
        std::unordered_map<EdgePtr, bool> edgeTruth;
        edgeTruth.reserve(view.getEdges().size());
        for (const auto& edge : view.getEdges()) {
            if (!edge) {
                continue;
            }
            bool edgeEnabled = true;
            auto itEdgeVar = edgeVarIndex.find(edge);
            if (itEdgeVar != edgeVarIndex.end()) {
                const bool edgeValue = assignmentBit(mask, itEdgeVar->second);
                const double edgeProbability = edge->getProbability();
                weight *= edgeValue ? edgeProbability : (1.0 - edgeProbability);
                edgeEnabled = edgeValue;
            } else if (!edge->isDeterministic() && nearlyZero(edge->getProbability())) {
                edgeEnabled = false;
            }
            edgeTruth[edge] = edgeEnabled;
        }
        if (nearlyZero(weight)) {
            continue;
        }

        bool changed = true;
        std::size_t rounds = 0;
        while (changed && rounds <= view.getNodes().size() + view.getEdges().size()) {
            changed = false;
            ++rounds;
            for (const auto& edge : view.getEdges()) {
                if (!edge) {
                    continue;
                }
                const bool edgeEnabled = edgeTruth[edge];
                if (!edgeEnabled) {
                    continue;
                }
                bool bodyTrue = true;
                const auto inputs = view.getInputs(edge);
                const auto negs = view.getBodyNegations(edge);
                for (std::size_t i = 0; i < inputs.size(); ++i) {
                    bool value = nodeTruth[inputs[i]];
                    if (i < negs.size() && negs[i]) {
                        value = !value;
                    }
                    if (!value) {
                        bodyTrue = false;
                        break;
                    }
                }
                if (bodyTrue) {
                    NodePtr output = view.getOutput(edge);
                    if (output && !nodeTruth[output]) {
                        nodeTruth[output] = true;
                        changed = true;
                    }
                }
            }
        }

        for (const auto& output : activeOutputs) {
            if (nodeTruth[output]) {
                marginals[output] += weight;
            }
        }
    }

    std::vector<OverlayOutputProbability> result;
    result.reserve(outputs.size());
    for (const auto& output : outputs) {
        result.push_back(OverlayOutputProbability{output, marginals[output]});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.output->getId() < b.output->getId();
    });
    return result;
}

std::string summarizeOverlayProbabilities(const std::vector<OverlayOutputProbability>& outputs) {
    std::ostringstream oss;
    oss << "[output-probabilities]";
    for (const auto& output : outputs) {
        oss << " " << nodeLabel(output.output) << "=" << output.probability;
    }
    return oss.str();
}

RewritePatternCounts countRewritePatterns(const std::vector<SISORegionInfo>& regions) {
    RewritePatternCounts counts;
    for (const auto& region : regions) {
        switch (region.kind) {
        case SISORegionKind::SingleHyperedge:
            ++counts.singleHyperedge;
            break;
        case SISORegionKind::LinearTwoEdge:
            ++counts.linearTwoEdge;
            break;
        case SISORegionKind::ParallelEdge:
            ++counts.parallelEdge;
            break;
        case SISORegionKind::AllFactsToSO:
            ++counts.allFactsToSO;
            break;
        case SISORegionKind::FanOutConverge:
            ++counts.fanOutConverge;
            break;
        case SISORegionKind::General:
            ++counts.general;
            break;
        default:
            ++counts.unknown;
            break;
        }
    }
    return counts;
}

std::string summarizeRewritePatternCounts(const RewritePatternCounts& counts) {
    std::ostringstream oss;
    oss << "[rewrite-patterns]"
        << " single=" << counts.singleHyperedge
        << " linear=" << counts.linearTwoEdge
        << " parallel=" << counts.parallelEdge
        << " all_facts=" << counts.allFactsToSO
        << " fan_out=" << counts.fanOutConverge
        << " general=" << counts.general
        << " unknown=" << counts.unknown;
    return oss.str();
}

static ImplicitSplitPipelineResult runImplicitSplitRewritePipelineSinglePass(
        const IncrementalDerivationGraphViewInterface& view,
        const ImplicitSplitPipelineOptions& options) {
    ImplicitSplitPipelineResult result;
    const auto totalStart = Clock::now();

    const auto overlayCreateStart = Clock::now();
    ImplicitSplitOverlay overlay(view);
    const auto overlayCreateMs = elapsedMs(overlayCreateStart);

    const auto splitStart = Clock::now();
    overlay.applySplit(options.splitMode, &result.stats.overlayStats);
    result.stats.overlaySplitMs = elapsedMs(splitStart);

    const auto fastPathStart = Clock::now();
    if (options.runOverlayFastPaths) {
        overlay.rewriteFastPathsToFixpoint(
                options.runOverlaySingleHyperedge, options.runOverlayAllFacts, &result.stats.overlayStats);
    }
    result.stats.overlayFastPathMs = elapsedMs(fastPathStart);
    result.stats.overlayPrepMs = overlayCreateMs + result.stats.overlaySplitMs + result.stats.overlayFastPathMs;

    const auto directOutputs = overlay.collectDirectOutputProbabilities();
    const auto overlayGraphStats = overlay.computeStats();
    const bool allOutputsDirect = directOutputs.size() == overlay.getOutputs().size();
    const bool needsResidualGraph = overlayGraphStats.activeEdges > 0 || !allOutputsDirect;
    if (!needsResidualGraph) {
        precomputedProbResult.clear();
        precomputedTupleProbResult.clear();
        result.carriedPrecomputedTupleProbs.reserve(directOutputs.size());
        for (const auto& output : directOutputs) {
            result.carriedPrecomputedTupleProbs.emplace_back(output.output->getTuple().toString(), output.probability);
        }
        for (const auto& [tupleStr, prob] : result.carriedPrecomputedTupleProbs) {
            precomputedTupleProbResult.emplace(tupleStr, prob);
        }
        result.outputProbabilities = directOutputs;
        result.stats.outerIterations = 1;
        result.stats.totalMs = elapsedMs(totalStart);
        return result;
    }

    const auto materializeStart = Clock::now();
    result.materialized = overlay.materializeToGraph();
    result.stats.materializeMs = elapsedMs(materializeStart);
    result.stats.materializedAliasNodes = result.materialized.aliasNodes;
    result.stats.outerIterations = 1;

    auto viewMaterialized = buildFullIncView(*result.materialized.graph);
    result.stats.materializedNodesBefore = viewMaterialized.getNodes().size();
    result.stats.materializedEdgesBefore = viewMaterialized.getEdges().size();

    if (options.collectPatternStats) {
        const auto detectBeforeStart = Clock::now();
        std::vector<SISORegionInfo> regionsBefore;
        {
            ScopedCoutSilencer silence;
            regionsBefore = GraphAnalyzer::detectAllSISOStrictFromExit(viewMaterialized);
        }
        result.stats.graphDetectMs = elapsedMs(detectBeforeStart);
        result.stats.materializedDetectedBefore = countRewritePatterns(regionsBefore);
    }

    if (options.runMaterializedGraphRewrite) {
        GraphRewriter rewriter;
        RewriteFeatureFlags flags;
        flags.splitMode = SplitMode::None;
        const auto rewriteStart = Clock::now();
        precomputedProbResult.clear();
        {
            ScopedCoutSilencer silence;
            result.stats.graphRewriteStats = rewriter.rewriteUntilFixpoint(
                    *result.materialized.graph, viewMaterialized, false, flags);
        }
        result.stats.graphRewriteMs = elapsedMs(rewriteStart);
    }

    result.stats.materializedNodesAfter = viewMaterialized.getNodes().size();
    result.stats.materializedEdgesAfter = viewMaterialized.getEdges().size();
    result.materialized.liveNodes = viewMaterialized.getNodes();
    result.materialized.liveEdges = viewMaterialized.getEdges();

    if (options.collectPatternStats) {
        std::vector<SISORegionInfo> regionsAfter;
        {
            ScopedCoutSilencer silence;
            regionsAfter = GraphAnalyzer::detectAllSISOStrictFromExit(viewMaterialized);
        }
        result.stats.materializedDetectedAfter = countRewritePatterns(regionsAfter);
    }
    if (options.computeOutputMarginals) {
        result.outputProbabilities = directOutputs;
        auto unresolvedProbabilities = computeGraphOutputMarginalsExact(
                viewMaterialized, result.materialized.outputs, &precomputedProbResult);
        result.outputProbabilities.insert(result.outputProbabilities.end(), unresolvedProbabilities.begin(),
                unresolvedProbabilities.end());
        std::sort(result.outputProbabilities.begin(), result.outputProbabilities.end(),
                [](const auto& a, const auto& b) { return a.output->getId() < b.output->getId(); });
    }
    precomputedTupleProbResult.clear();
    for (const auto& [tupleStr, prob] : result.carriedPrecomputedTupleProbs) {
        precomputedTupleProbResult.emplace(tupleStr, prob);
    }
    result.stats.totalMs = elapsedMs(totalStart);
    return result;
}

ImplicitSplitPipelineResult runImplicitSplitRewritePipeline(
        const IncrementalDerivationGraphViewInterface& view,
        const ImplicitSplitPipelineOptions& options) {
    if (!options.iterateSplitRewrite) {
        return runImplicitSplitRewritePipelineSinglePass(view, options);
    }

    ImplicitSplitPipelineResult result;
    const auto totalStart = Clock::now();
    const IncrementalDerivationGraphViewInterface* currentView = &view;
    std::unique_ptr<IncrementalDerivationGraph> currentGraph;
    std::unique_ptr<IncSubgraphView> currentOwnedView;
    std::unordered_map<std::string, double> carriedPrecomputedTupleProbs;
    bool sawRewriteStats = false;

    auto accumulateOverlayStats = [&](const ImplicitSplitOverlayStats& iterStats) {
        auto& total = result.stats.overlayStats;
        total.aliasesCreated += iterStats.aliasesCreated;
        total.edgesAliased += iterStats.edgesAliased;
        total.allFactsRewrites += iterStats.allFactsRewrites;
        total.singleHyperedgeRewrites += iterStats.singleHyperedgeRewrites;
        total.removedEdges += iterStats.removedEdges;
        total.factOutputsFolded += iterStats.factOutputsFolded;
        total.splitFactsConsidered += iterStats.splitFactsConsidered;
        total.splitFactsAliased += iterStats.splitFactsAliased;
        total.splitNaiveReachabilityRuns += iterStats.splitNaiveReachabilityRuns;
        total.splitNaiveReachabilityVisited += iterStats.splitNaiveReachabilityVisited;
        total.splitNaiveCapSkips += iterStats.splitNaiveCapSkips;
        total.rebuildIndexCount += iterStats.rebuildIndexCount;
        total.fastPathIterations += iterStats.fastPathIterations;
        total.splitNaiveMs += iterStats.splitNaiveMs;
        total.splitCompleteMs += iterStats.splitCompleteMs;
        total.splitAliasApplyMs += iterStats.splitAliasApplyMs;
        total.rebuildIndexMs += iterStats.rebuildIndexMs;
        total.fastPathSingleMs += iterStats.fastPathSingleMs;
        total.fastPathAllFactsMs += iterStats.fastPathAllFactsMs;
    };

    auto accumulateRewriteStats = [&](const GraphRewriteStats& iterStats) {
        auto& total = result.stats.graphRewriteStats;
        if (!sawRewriteStats) {
            total.randomVarsBefore = iterStats.randomVarsBefore;
            total.maxRandomVars = iterStats.maxRandomVars;
            sawRewriteStats = true;
        } else {
            total.maxRandomVars = std::max(total.maxRandomVars, iterStats.maxRandomVars);
        }
        total.numIterations += iterStats.numIterations;
        total.numRegionsRewritten += iterStats.numRegionsRewritten;
        total.numNodesRemoved += iterStats.numNodesRemoved;
        total.numEdgesRemoved += iterStats.numEdgesRemoved;
        total.numEdgesAdded += iterStats.numEdgesAdded;
        total.simpleFactRegions += iterStats.simpleFactRegions;
        total.totalRandomVars += iterStats.totalRandomVars;
        total.randomVarsAfter = iterStats.randomVarsAfter;
    };

    auto carryPrecomputedTuples = [&]() {
        for (const auto& [node, prob] : precomputedProbResult) {
            if (!node) {
                continue;
            }
            carriedPrecomputedTupleProbs[node->getTuple().toString()] = prob;
        }
    };

    const std::size_t maxOuterIterations = std::max<std::size_t>(1, options.maxOuterIterations);
    while (result.stats.outerIterations < maxOuterIterations) {
        ++result.stats.outerIterations;

        const auto overlayCreateStart = Clock::now();
        ImplicitSplitOverlay overlay(*currentView);
        const auto overlayCreateMs = elapsedMs(overlayCreateStart);

        ImplicitSplitOverlayStats iterOverlayStats;
        const auto splitStart = Clock::now();
        overlay.applySplit(options.splitMode, &iterOverlayStats);
        const auto splitMs = elapsedMs(splitStart);

        const auto fastPathStart = Clock::now();
        if (options.runOverlayFastPaths) {
            overlay.rewriteFastPathsToFixpoint(
                    options.runOverlaySingleHyperedge, options.runOverlayAllFacts, &iterOverlayStats);
        }
        const auto fastPathMs = elapsedMs(fastPathStart);

        result.stats.overlaySplitMs += splitMs;
        result.stats.overlayFastPathMs += fastPathMs;
        result.stats.overlayPrepMs += overlayCreateMs + splitMs + fastPathMs;
        accumulateOverlayStats(iterOverlayStats);

        const auto overlayGraphStats = overlay.computeStats();
        const auto directOutputs = overlay.collectDirectOutputProbabilities();
        const bool allOutputsDirect = directOutputs.size() == overlay.getOutputs().size();
        const bool needsResidualGraph = overlayGraphStats.activeEdges > 0 || !allOutputsDirect;
        if (!needsResidualGraph) {
            result.materialized = {};
            precomputedProbResult.clear();
            if (options.computeOutputMarginals) {
                result.outputProbabilities = directOutputs;
            }
            for (const auto& output : directOutputs) {
                carriedPrecomputedTupleProbs[output.output->getTuple().toString()] = output.probability;
            }
            break;
        }

        const auto materializeStart = Clock::now();
        auto materialized = overlay.materializeToGraph();
        result.stats.materializeMs += elapsedMs(materializeStart);
        result.stats.materializedAliasNodes = materialized.aliasNodes;

        auto viewMaterialized = buildFullIncView(*materialized.graph);
        result.stats.materializedNodesBefore = viewMaterialized.getNodes().size();
        result.stats.materializedEdgesBefore = viewMaterialized.getEdges().size();

        if (options.collectPatternStats) {
            const auto detectBeforeStart = Clock::now();
            std::vector<SISORegionInfo> regionsBefore;
            {
                ScopedCoutSilencer silence;
                regionsBefore = GraphAnalyzer::detectAllSISOStrictFromExit(viewMaterialized);
            }
            result.stats.graphDetectMs += elapsedMs(detectBeforeStart);
            result.stats.materializedDetectedBefore = countRewritePatterns(regionsBefore);
        }

        GraphRewriteStats iterRewriteStats;
        if (options.runMaterializedGraphRewrite) {
            GraphRewriter rewriter;
            RewriteFeatureFlags flags;
            flags.splitMode = SplitMode::None;
            const auto rewriteStart = Clock::now();
            precomputedProbResult.clear();
            {
                ScopedCoutSilencer silence;
                iterRewriteStats = rewriter.rewriteUntilFixpoint(
                        *materialized.graph, viewMaterialized, false, flags);
            }
            result.stats.graphRewriteMs += elapsedMs(rewriteStart);
            carryPrecomputedTuples();
            accumulateRewriteStats(iterRewriteStats);
        } else {
            precomputedProbResult.clear();
        }

        result.stats.materializedNodesAfter = viewMaterialized.getNodes().size();
        result.stats.materializedEdgesAfter = viewMaterialized.getEdges().size();
        materialized.liveNodes = viewMaterialized.getNodes();
        materialized.liveEdges = viewMaterialized.getEdges();

        if (options.collectPatternStats) {
            std::vector<SISORegionInfo> regionsAfter;
            {
                ScopedCoutSilencer silence;
                regionsAfter = GraphAnalyzer::detectAllSISOStrictFromExit(viewMaterialized);
            }
            result.stats.materializedDetectedAfter = countRewritePatterns(regionsAfter);
        }

        materialized.outputs.clear();
        for (const auto& node : viewMaterialized.getNodes()) {
            if (node && node->needOutput) {
                materialized.outputs.push_back(node);
            }
        }
        for (const auto& [node, _] : precomputedProbResult) {
            if (node) {
                materialized.outputs.push_back(node);
            }
        }

        result.materialized = std::move(materialized);
        const bool splitChanged = iterOverlayStats.aliasesCreated > 0 || iterOverlayStats.edgesAliased > 0;
        const bool rewriteChanged = iterRewriteStats.numRegionsRewritten > 0;
        if (!options.iterateSplitRewrite || (!splitChanged && !rewriteChanged)) {
            break;
        }

        currentGraph = std::move(result.materialized.graph);
        currentOwnedView = std::make_unique<IncSubgraphView>(buildFullIncView(*currentGraph));
        currentView = currentOwnedView.get();
        result.materialized.graph = nullptr;
        result.materialized.outputs.clear();
    }

    precomputedTupleProbResult = carriedPrecomputedTupleProbs;
    result.carriedPrecomputedTupleProbs.reserve(carriedPrecomputedTupleProbs.size());
    for (const auto& [tupleStr, prob] : carriedPrecomputedTupleProbs) {
        result.carriedPrecomputedTupleProbs.emplace_back(tupleStr, prob);
    }

    if (options.computeOutputMarginals && result.materialized.graph) {
        auto finalView = buildFullIncView(*result.materialized.graph);
        auto unresolvedProbabilities = computeGraphOutputMarginalsExact(
                finalView, result.materialized.outputs, &precomputedProbResult);
        result.outputProbabilities.insert(result.outputProbabilities.end(), unresolvedProbabilities.begin(),
                unresolvedProbabilities.end());
        std::sort(result.outputProbabilities.begin(), result.outputProbabilities.end(),
                [](const auto& a, const auto& b) { return a.output->getId() < b.output->getId(); });
    }
    result.stats.totalMs = elapsedMs(totalStart);
    return result;
}

}  // namespace souffle::problog

namespace souffle::problog {
namespace {

struct JsonBenchmarkOptions {
    std::string jsonPath;
    bool skipLegacyNoSplit = false;
    bool skipLegacySplit = false;
    bool skipImplicit = false;
    bool runtimeLikeImplicit = false;
    ImplicitSplitMode splitMode = ImplicitSplitMode::Naive;
};

class JsonBenchmarkScopedCoutSilencer {
public:
    JsonBenchmarkScopedCoutSilencer() : old_(std::cout.rdbuf(sink_.rdbuf())) {}
    ~JsonBenchmarkScopedCoutSilencer() {
        std::cout.rdbuf(old_);
    }

private:
    std::ostringstream sink_;
    std::streambuf* old_ = nullptr;
};

double benchmarkElapsedMs(const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

[[noreturn]] void benchmarkUsage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " --json <derivation.json>"
              << " [--skip-legacy-no-split] [--skip-legacy-split] [--skip-implicit]"
              << " [--runtime-like-implicit]"
              << " [--split-complete]\n";
    throw std::runtime_error("invalid arguments");
}

JsonBenchmarkOptions parseBenchmarkArgs(int argc, char** argv) {
    JsonBenchmarkOptions opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc) {
            opt.jsonPath = argv[++i];
        } else if (arg == "--skip-legacy-no-split") {
            opt.skipLegacyNoSplit = true;
        } else if (arg == "--skip-legacy-split") {
            opt.skipLegacySplit = true;
        } else if (arg == "--skip-implicit") {
            opt.skipImplicit = true;
        } else if (arg == "--runtime-like-implicit") {
            opt.runtimeLikeImplicit = true;
        } else if (arg == "--split-complete") {
            opt.splitMode = ImplicitSplitMode::Complete;
        } else {
            benchmarkUsage(argv[0]);
        }
    }
    if (opt.jsonPath.empty()) {
        benchmarkUsage(argv[0]);
    }
    return opt;
}

IncSubgraphView buildBenchmarkFullView(IncrementalDerivationGraph& graph) {
    return IncSubgraphView(graph.getNodes(), graph.getEdges(), {}, {}, {}, {});
}

std::size_t benchmarkCountOutputNodes(const IncrementalDerivationGraph& graph) {
    std::size_t out = 0;
    for (const auto& node : graph.getNodes()) {
        if (node && node->needOutput) {
            ++out;
        }
    }
    return out;
}

std::size_t benchmarkCountFacts(const IncrementalDerivationGraph& graph) {
    std::size_t out = 0;
    for (const auto& node : graph.getNodes()) {
        if (node && node->isFact) {
            ++out;
        }
    }
    return out;
}

std::size_t benchmarkCountSinkNodes(const IncrementalDerivationGraph& graph) {
    std::size_t out = 0;
    for (const auto& node : graph.getNodes()) {
        if (node && graph.getOutgoingEdges(node).empty()) {
            ++out;
        }
    }
    return out;
}

std::size_t benchmarkMarkSinkNodesAsOutputs(IncrementalDerivationGraph& graph) {
    std::size_t marked = 0;
    for (const auto& node : graph.getNodes()) {
        if (!node) {
            continue;
        }
        if (!graph.getOutgoingEdges(node).empty()) {
            continue;
        }
        if (!node->needOutput) {
            node->setQuery();
            ++marked;
        }
    }
    return marked;
}

void ensureBenchmarkOutputs(IncrementalDerivationGraph& graph, std::ostream& out) {
    const auto outputs = benchmarkCountOutputNodes(graph);
    if (outputs > 0) {
        out << "[output-policy] mode=preserve outputs=" << outputs << "\n";
        return;
    }
    const auto sinks = benchmarkCountSinkNodes(graph);
    const auto marked = benchmarkMarkSinkNodesAsOutputs(graph);
    out << "[output-policy] mode=mark-sinks previous_outputs=0 sinks=" << sinks
        << " marked=" << marked << "\n";
}

struct BenchmarkLegacyRunResult {
    double loadMs = 0.0;
    double rewriteMs = 0.0;
    std::size_t nodesBefore = 0;
    std::size_t edgesBefore = 0;
    std::size_t nodesAfter = 0;
    std::size_t edgesAfter = 0;
    GraphRewriteStats stats;
};

BenchmarkLegacyRunResult runBenchmarkLegacyRewrite(const std::string& jsonPath, SplitMode splitMode) {
    const auto loadStart = std::chrono::steady_clock::now();
    auto graph = std::unique_ptr<IncrementalDerivationGraph>(
            IncrementalDerivationGraph::loadFromJsonInc(jsonPath));
    ensureBenchmarkOutputs(*graph, std::cout);
    BenchmarkLegacyRunResult out;
    out.loadMs = benchmarkElapsedMs(loadStart);

    auto view = buildBenchmarkFullView(*graph);
    out.nodesBefore = view.getNodes().size();
    out.edgesBefore = view.getEdges().size();

    GraphRewriter rewriter;
    RewriteFeatureFlags flags;
    flags.splitMode = splitMode;
    const auto rewriteStart = std::chrono::steady_clock::now();
    {
        JsonBenchmarkScopedCoutSilencer silence;
        out.stats = rewriter.rewriteUntilFixpoint(*graph, view, false, flags);
    }
    out.rewriteMs = benchmarkElapsedMs(rewriteStart);
    out.nodesAfter = view.getNodes().size();
    out.edgesAfter = view.getEdges().size();
    return out;
}

ImplicitSplitPipelineResult runBenchmarkImplicitRewrite(
        const std::string& jsonPath, ImplicitSplitMode splitMode, bool runtimeLike, double* loadMsOut) {
    const auto loadStart = std::chrono::steady_clock::now();
    auto graph = std::unique_ptr<IncrementalDerivationGraph>(
            IncrementalDerivationGraph::loadFromJsonInc(jsonPath));
    ensureBenchmarkOutputs(*graph, std::cout);
    if (loadMsOut) {
        *loadMsOut = benchmarkElapsedMs(loadStart);
    }
    auto view = buildBenchmarkFullView(*graph);
    ImplicitSplitPipelineOptions options;
    options.splitMode = splitMode;
    options.computeOutputMarginals = false;
    if (runtimeLike) {
        options.runOverlayFastPaths = false;
        options.collectPatternStats = false;
        options.iterateSplitRewrite = false;
    }
    return runImplicitSplitRewritePipeline(view, options);
}

void printBenchmarkLegacySummary(const std::string& label, const BenchmarkLegacyRunResult& run) {
    std::cout << "[" << label << "]"
              << " load_ms=" << run.loadMs
              << " rewrite_ms=" << run.rewriteMs
              << " iterations=" << run.stats.numIterations
              << " regions=" << run.stats.numRegionsRewritten
              << " rv_before=" << run.stats.randomVarsBefore
              << " rv_after=" << run.stats.randomVarsAfter
              << " simple_fact_regions=" << run.stats.simpleFactRegions
              << " nodes_before=" << run.nodesBefore
              << " edges_before=" << run.edgesBefore
              << " nodes_after=" << run.nodesAfter
              << " edges_after=" << run.edgesAfter
              << " edges_added=" << run.stats.numEdgesAdded
              << " nodes_removed=" << run.stats.numNodesRemoved
              << " edges_removed=" << run.stats.numEdgesRemoved
              << "\n";
}

void printBenchmarkImplicitSummary(const ImplicitSplitPipelineResult& run, double loadMs) {
    const auto& stats = run.stats;
    std::cout << "[implicit-split]"
              << " load_ms=" << loadMs
              << " total_ms=" << stats.totalMs
              << " overlay_prep_ms=" << stats.overlayPrepMs
              << " overlay_split_ms=" << stats.overlaySplitMs
              << " overlay_fastpath_ms=" << stats.overlayFastPathMs
              << " materialize_ms=" << stats.materializeMs
              << " detect_ms=" << stats.graphDetectMs
              << " graph_rewrite_ms=" << stats.graphRewriteMs
              << " overlay_aliases=" << stats.overlayStats.aliasesCreated
              << " overlay_edges_aliased=" << stats.overlayStats.edgesAliased
              << " overlay_all_facts=" << stats.overlayStats.allFactsRewrites
              << " overlay_single=" << stats.overlayStats.singleHyperedgeRewrites
              << " split_facts=" << stats.overlayStats.splitFactsConsidered
              << " split_facts_aliased=" << stats.overlayStats.splitFactsAliased
              << " split_naive_ms=" << stats.overlayStats.splitNaiveMs
              << " split_complete_ms=" << stats.overlayStats.splitCompleteMs
              << " split_alias_apply_ms=" << stats.overlayStats.splitAliasApplyMs
              << " split_naive_bfs=" << stats.overlayStats.splitNaiveReachabilityRuns
              << " split_naive_visited=" << stats.overlayStats.splitNaiveReachabilityVisited
              << " split_naive_cap_skips=" << stats.overlayStats.splitNaiveCapSkips
              << " rebuild_index_count=" << stats.overlayStats.rebuildIndexCount
              << " rebuild_index_ms=" << stats.overlayStats.rebuildIndexMs
              << " fastpath_iterations=" << stats.overlayStats.fastPathIterations
              << " fastpath_single_ms=" << stats.overlayStats.fastPathSingleMs
              << " fastpath_allfacts_ms=" << stats.overlayStats.fastPathAllFactsMs
              << " materialized_alias_nodes=" << stats.materializedAliasNodes
              << " nodes_before=" << stats.materializedNodesBefore
              << " edges_before=" << stats.materializedEdgesBefore
              << " nodes_after=" << stats.materializedNodesAfter
              << " edges_after=" << stats.materializedEdgesAfter
              << " graph_iterations=" << stats.graphRewriteStats.numIterations
              << " graph_regions=" << stats.graphRewriteStats.numRegionsRewritten
              << " graph_rv_before=" << stats.graphRewriteStats.randomVarsBefore
              << " graph_rv_after=" << stats.graphRewriteStats.randomVarsAfter
              << " before=" << summarizeRewritePatternCounts(stats.materializedDetectedBefore)
              << " after=" << summarizeRewritePatternCounts(stats.materializedDetectedAfter)
              << "\n";

    auto countSemanticRandomVarsInView = [](const IncrementalDerivationGraphViewInterface& view) {
        std::size_t out = 0;
        for (const auto& node : view.getNodes()) {
            if (node && node->isFact) {
                const double p = node->getProbability();
                if (p > 0.0 && p < 1.0) {
                    ++out;
                }
            }
        }
        for (const auto& edge : view.getEdges()) {
            if (edge) {
                const double p = edge->getProbability();
                if (p > 0.0 && p < 1.0) {
                    ++out;
                }
            }
        }
        return out;
    };
    auto countSemanticRandomVarsInComp = [](const ComponentSubgraph& comp) {
        std::size_t out = 0;
        for (const auto& node : comp.nodes) {
            if (node && node->isFact) {
                const double p = node->getProbability();
                if (p > 0.0 && p < 1.0) {
                    ++out;
                }
            }
        }
        for (const auto& edge : comp.edges) {
            if (edge) {
                const double p = edge->getProbability();
                if (p > 0.0 && p < 1.0) {
                    ++out;
                }
            }
        }
        return out;
    };
    if (run.materialized.graph) {
        auto handoffView =
                IncSubgraphView(run.materialized.liveNodes, run.materialized.liveEdges, {}, {}, {}, {});
        const auto components = buildComponentSubgraphs(handoffView);
        std::size_t legacyRvTotal = 0;
        std::size_t semanticRvTotal = 0;
        std::size_t legacyRvComponents = 0;
        std::size_t semanticRvComponents = 0;
        std::size_t tiny4e1LegacyRv2 = 0;
        std::size_t tiny4e1SemanticRv2 = 0;
        std::size_t tiny4e1SemanticRv0LegacyRv2 = 0;
        for (const auto& comp : components) {
            const auto legacyRv = countComponentRandomVars(comp);
            const auto semanticRv = countSemanticRandomVarsInComp(comp);
            legacyRvTotal += legacyRv;
            semanticRvTotal += semanticRv;
            if (legacyRv > 0) {
                ++legacyRvComponents;
            }
            if (semanticRv > 0) {
                ++semanticRvComponents;
            }
            if (comp.nodes.size() == 4 && comp.edges.size() == 1) {
                if (legacyRv == 2) {
                    ++tiny4e1LegacyRv2;
                }
                if (semanticRv == 2) {
                    ++tiny4e1SemanticRv2;
                }
                if (semanticRv == 0 && legacyRv == 2) {
                    ++tiny4e1SemanticRv0LegacyRv2;
                }
            }
        }
        std::cout << "[implicit-handoff]"
                  << " nodes=" << handoffView.getNodes().size()
                  << " edges=" << handoffView.getEdges().size()
                  << " precomputed_nodes=" << precomputedProbResult.size()
                  << " precomputed_tuples=" << precomputedTupleProbResult.size()
                  << " legacy_rv_total=" << legacyRvTotal
                  << " semantic_rv_total=" << semanticRvTotal
                  << " handoff_semantic_rv_total=" << countSemanticRandomVarsInView(handoffView)
                  << " legacy_rv_components=" << legacyRvComponents
                  << " semantic_rv_components=" << semanticRvComponents
                  << " tiny4e1_legacy_rv2=" << tiny4e1LegacyRv2
                  << " tiny4e1_semantic_rv2=" << tiny4e1SemanticRv2
                  << " tiny4e1_semantic_rv0_legacy_rv2=" << tiny4e1SemanticRv0LegacyRv2
                  << "\n";
    }
}

}  // namespace

int runImplicitSplitJsonBenchmarkMain(int argc, char** argv) {
    try {
        const JsonBenchmarkOptions opt = parseBenchmarkArgs(argc, argv);
        std::cout << std::fixed << std::setprecision(3);

        const auto loadStart = std::chrono::steady_clock::now();
        auto graphInfo = std::unique_ptr<IncrementalDerivationGraph>(
                IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
        ensureBenchmarkOutputs(*graphInfo, std::cout);
        const double loadInfoMs = benchmarkElapsedMs(loadStart);
        std::cout << "[input] json=" << opt.jsonPath
                  << " nodes=" << graphInfo->getNodes().size()
                  << " edges=" << graphInfo->getEdges().size()
                  << " facts=" << benchmarkCountFacts(*graphInfo)
                  << " outputs=" << benchmarkCountOutputNodes(*graphInfo)
                  << " load_ms=" << loadInfoMs
                  << "\n";

        if (!opt.skipLegacyNoSplit) {
            printBenchmarkLegacySummary("legacy-no-split", runBenchmarkLegacyRewrite(opt.jsonPath, SplitMode::None));
        }
        if (!opt.skipLegacySplit) {
            printBenchmarkLegacySummary("legacy-explicit-split", runBenchmarkLegacyRewrite(opt.jsonPath, SplitMode::Naive));
        }
        if (!opt.skipImplicit) {
            double loadMs = 0.0;
            const auto result = runBenchmarkImplicitRewrite(
                    opt.jsonPath, opt.splitMode, opt.runtimeLikeImplicit, &loadMs);
            printBenchmarkImplicitSummary(result, loadMs);
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[implicit-split-json-benchmark] failed: " << ex.what() << "\n";
        return 1;
    }
}

}  // namespace souffle::problog

using namespace souffle::problog;

namespace {

enum class ExpectedPattern {
    None,
    OverlayAllFacts,
    OverlaySingleHyperedge,
    MaterializedLinearTwoEdge,
    MaterializedParallelEdge,
    MaterializedFanOutConverge,
};

struct ExampleCase {
    std::string name;
    std::function<void(IncrementalDerivationGraph&, std::vector<NodePtr>&)> build;
    bool expectAlias = false;
    ExpectedPattern expectedPattern = ExpectedPattern::None;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

IncSubgraphView buildFullView(IncrementalDerivationGraph& graph) {
    return buildFullIncView(graph);
}

bool probsClose(const std::vector<OverlayOutputProbability>& a,
        const std::vector<OverlayOutputProbability>& b, double eps = 1e-9) {
    if (a.size() != b.size()) {
        return false;
    }
    std::unordered_map<std::string, double> amap;
    std::unordered_map<std::string, double> bmap;
    for (const auto& item : a) {
        amap[item.output->getTuple().toString()] = item.probability;
    }
    for (const auto& item : b) {
        bmap[item.output->getTuple().toString()] = item.probability;
    }
    if (amap.size() != bmap.size()) {
        return false;
    }
    for (const auto& [label, prob] : amap) {
        auto it = bmap.find(label);
        if (it == bmap.end()) {
            return false;
        }
        if (std::abs(prob - it->second) > eps) {
            return false;
        }
    }
    return true;
}

std::vector<OverlayOutputProbability> runExplicitRewrite(
        const ExampleCase& ex, GraphRewriteStats* outStats = nullptr) {
    IncrementalDerivationGraph graph;
    std::vector<NodePtr> outputs;
    ex.build(graph, outputs);
    auto view = buildFullView(graph);
    auto baselineOutputs = outputs;
    GraphRewriter rewriter;
    RewriteFeatureFlags flags;
    flags.splitMode = SplitMode::Naive;
    precomputedProbResult.clear();
    auto stats = rewriter.rewriteUntilFixpoint(graph, view, false, flags);
    if (outStats) {
        *outStats = stats;
    }
    return computeGraphOutputMarginalsExact(view, baselineOutputs, &precomputedProbResult);
}

std::vector<OverlayOutputProbability> runBaseline(const ExampleCase& ex) {
    IncrementalDerivationGraph graph;
    std::vector<NodePtr> outputs;
    ex.build(graph, outputs);
    auto view = buildFullView(graph);
    return computeGraphOutputMarginalsExact(view, outputs, nullptr);
}

std::pair<std::vector<OverlayOutputProbability>, ImplicitSplitPipelineStats> runImplicitPipeline(
        const ExampleCase& ex) {
    IncrementalDerivationGraph graph;
    std::vector<NodePtr> outputs;
    ex.build(graph, outputs);
    auto view = buildFullView(graph);
    auto result = runImplicitSplitRewritePipeline(view);
    return {result.outputProbabilities, result.stats};
}

std::vector<ExampleCase> buildExamples() {
    return {
            ExampleCase{
                    "disjoint_all_facts",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 0.4);
                        f->isFact = true;
                        NodePtr g = graph.createNode(UntypedTuple{"g", {1}}, 0.7);
                        g->isFact = true;
                        NodePtr h = graph.createNode(UntypedTuple{"h", {1}}, 0.9);
                        h->isFact = true;
                        NodePtr q1 = graph.createNode(UntypedTuple{"q1", {1}}, 1.0);
                        NodePtr q2 = graph.createNode(UntypedTuple{"q2", {1}}, 1.0);
                        q1->setQuery();
                        q2->setQuery();
                        graph.createHyperedge({f, g}, q1);
                        graph.createHyperedge({f, h}, q2);
                        outputs = {q1, q2};
                    },
                    true,
                    ExpectedPattern::OverlayAllFacts,
            },
            ExampleCase{
                    "fanout_reconverge_no_split",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 0.35);
                        f->isFact = true;
                        NodePtr a = graph.createNode(UntypedTuple{"a", {1}}, 1.0);
                        NodePtr b = graph.createNode(UntypedTuple{"b", {1}}, 1.0);
                        NodePtr q = graph.createNode(UntypedTuple{"q", {1}}, 1.0);
                        q->setQuery();
                        graph.createHyperedge({f}, a);
                        graph.createHyperedge({f}, b);
                        graph.createHyperedge({a, b}, q);
                        outputs = {q};
                    },
                    false,
                    ExpectedPattern::None,
            },
            ExampleCase{
                    "split_single_hyperedge",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 0.4);
                        f->isFact = true;
                        NodePtr u = graph.createNode(UntypedTuple{"u", {1}}, 0.3);
                        u->isFact = true;
                        NodePtr v = graph.createNode(UntypedTuple{"v", {1}}, 0.5);
                        v->isFact = true;
                        NodePtr w = graph.createNode(UntypedTuple{"w", {1}}, 0.6);
                        w->isFact = true;
                        NodePtr x = graph.createNode(UntypedTuple{"x", {1}}, 0.2);
                        x->isFact = true;

                        NodePtr a = graph.createNode(UntypedTuple{"a", {1}}, 1.0);
                        NodePtr b = graph.createNode(UntypedTuple{"b", {1}}, 1.0);
                        NodePtr q1 = graph.createNode(UntypedTuple{"q1s", {1}}, 1.0);
                        NodePtr q2 = graph.createNode(UntypedTuple{"q2s", {1}}, 1.0);
                        q1->setQuery();
                        q2->setQuery();

                        graph.createHyperedge({u}, a);
                        graph.createHyperedge({v}, a);
                        graph.createHyperedge({w}, b);
                        graph.createHyperedge({x}, b);
                        graph.createHyperedge({f, a}, q1);
                        graph.createHyperedge({f, b}, q2);
                        outputs = {q1, q2};
                    },
                    true,
                    ExpectedPattern::OverlaySingleHyperedge,
            },
            ExampleCase{
                    "linear_two_edge",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = graph.createNode(UntypedTuple{"lf", {1}}, 0.4);
                        f->isFact = true;
                        f->setEvidence(true);
                        NodePtr mid = graph.createNode(UntypedTuple{"lmid", {1}}, 1.0);
                        NodePtr q = graph.createNode(UntypedTuple{"lq", {1}}, 1.0);
                        q->setQuery();
                        EdgePtr e1 = graph.createHyperedge({f}, mid);
                        EdgePtr e2 = graph.createHyperedge({mid}, q);
                        require(e1 && e2, "failed to create linear_two_edge hyperedges");
                        e1->setProbability(0.6);
                        e2->setProbability(0.7);
                        outputs = {q};
                    },
                    false,
                    ExpectedPattern::MaterializedLinearTwoEdge,
            },
            ExampleCase{
                    "parallel_edge",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = graph.createNode(UntypedTuple{"pf", {1}}, 0.5);
                        f->isFact = true;
                        NodePtr q = graph.createNode(UntypedTuple{"pq", {1}}, 1.0);
                        q->setQuery();
                        EdgePtr e1 = graph.createHyperedge({f}, q);
                        EdgePtr e2 = graph.createHyperedge({f}, q);
                        require(e1 && e2, "failed to create parallel_edge hyperedges");
                        e1->setProbability(0.2);
                        e2->setProbability(0.4);
                        outputs = {q};
                    },
                    false,
                    ExpectedPattern::MaterializedParallelEdge,
            },
            ExampleCase{
                    "fan_out_converge",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = graph.createNode(UntypedTuple{"ff", {1}}, 0.5);
                        f->isFact = true;
                        NodePtr a = graph.createNode(UntypedTuple{"fa", {1}}, 1.0);
                        NodePtr b = graph.createNode(UntypedTuple{"fb", {1}}, 1.0);
                        NodePtr q = graph.createNode(UntypedTuple{"fq", {1}}, 1.0);
                        q->setQuery();
                        EdgePtr e1 = graph.createHyperedge({f}, a);
                        EdgePtr e2 = graph.createHyperedge({f}, b);
                        EdgePtr e3 = graph.createHyperedge({a, b}, q);
                        require(e1 && e2 && e3, "failed to create fan_out_converge hyperedges");
                        e1->setProbability(0.8);
                        e2->setProbability(0.6);
                        e3->setProbability(0.9);
                        outputs = {q};
                    },
                    false,
                    ExpectedPattern::MaterializedFanOutConverge,
            },
    };
}

}  // namespace

int souffle::problog::runImplicitSplitSmokeMain() {
    try {
        const auto examples = buildExamples();
        for (const auto& ex : examples) {
            const auto baseline = runBaseline(ex);
            GraphRewriteStats explicitStats;
            const auto explicitRewrite = runExplicitRewrite(ex, &explicitStats);
            const auto [implicitRewrite, implicitStats] = runImplicitPipeline(ex);

            if (!probsClose(baseline, explicitRewrite) || !probsClose(baseline, implicitRewrite)) {
                std::cout << "[implicit-split-smoke] mismatch case=" << ex.name << "\n";
                std::cout << "  baseline  " << summarizeOverlayProbabilities(baseline) << "\n";
                std::cout << "  explicit  " << summarizeOverlayProbabilities(explicitRewrite) << "\n";
                std::cout << "  implicit  " << summarizeOverlayProbabilities(implicitRewrite) << "\n";
            }

            require(probsClose(baseline, explicitRewrite),
                    ex.name + ": explicit rewrite diverged from baseline");
            require(probsClose(baseline, implicitRewrite),
                    ex.name + ": implicit rewrite diverged from baseline");

            if (ex.expectAlias) {
                require(implicitStats.overlayStats.aliasesCreated > 0, ex.name + ": expected implicit aliases");
            } else {
                require(implicitStats.overlayStats.aliasesCreated == 0, ex.name + ": expected no aliases");
            }

            switch (ex.expectedPattern) {
            case ExpectedPattern::OverlayAllFacts:
                require(implicitStats.overlayStats.allFactsRewrites > 0,
                        ex.name + ": expected overlay all-facts rewrite");
                break;
            case ExpectedPattern::OverlaySingleHyperedge:
                require(implicitStats.overlayStats.singleHyperedgeRewrites > 0,
                        ex.name + ": expected overlay single-hyperedge rewrite");
                break;
            case ExpectedPattern::MaterializedLinearTwoEdge:
                require(implicitStats.materializedDetectedBefore.linearTwoEdge > 0,
                        ex.name + ": expected materialized linear-two-edge support");
                break;
            case ExpectedPattern::MaterializedParallelEdge:
                require(implicitStats.materializedDetectedBefore.parallelEdge > 0,
                        ex.name + ": expected materialized parallel-edge support");
                break;
            case ExpectedPattern::MaterializedFanOutConverge:
                require(implicitStats.materializedDetectedBefore.fanOutConverge > 0,
                        ex.name + ": expected materialized fan-out-converge support");
                break;
            case ExpectedPattern::None:
                break;
            }

            std::cout << "[implicit-split-smoke] case=" << ex.name
                      << " explicit_regions=" << explicitStats.numRegionsRewritten
                      << " implicit_aliases=" << implicitStats.overlayStats.aliasesCreated
                      << " implicit_all_facts=" << implicitStats.overlayStats.allFactsRewrites
                      << " implicit_single=" << implicitStats.overlayStats.singleHyperedgeRewrites
                      << " detected_before=" << summarizeRewritePatternCounts(implicitStats.materializedDetectedBefore)
                      << "\n";
            std::cout << "  baseline  " << summarizeOverlayProbabilities(baseline) << "\n";
            std::cout << "  explicit  " << summarizeOverlayProbabilities(explicitRewrite) << "\n";
            std::cout << "  implicit  " << summarizeOverlayProbabilities(implicitRewrite) << "\n";
        }
        std::cout << "[implicit-split-smoke] ok\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[implicit-split-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}

#ifdef SOUFFLE_IMPLICIT_SPLIT_SMOKE_MAIN
int main() {
    return souffle::problog::runImplicitSplitSmokeMain();
}
#endif
