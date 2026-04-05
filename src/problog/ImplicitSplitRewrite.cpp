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
    return node->getSemanticFactId();
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

ImplicitSplitOverlay::SplitDirtyTracker::SplitDirtyTracker(ImplicitSplitOverlay& overlay) : overlay_(overlay) {}

void ImplicitSplitOverlay::SplitDirtyTracker::markFact(const NodePtr& fact) {
    if (!overlay_.canSplitFact(fact)) {
        return;
    }
    if (factSet_.insert(fact).second) {
        facts_.push_back(fact);
    }
}

void ImplicitSplitOverlay::SplitDirtyTracker::markInputs(const std::vector<SplitNodeRef>& inputs) {
    for (const auto& input : inputs) {
        markFact(input.base);
    }
}

void ImplicitSplitOverlay::SplitDirtyTracker::seedAllFacts() {
    initialized_ = true;
    facts_.clear();
    factSet_.clear();
    facts_.reserve(overlay_.nodeState_.size());
    factSet_.reserve(overlay_.nodeState_.size());
    for (const auto& [node, _] : overlay_.nodeState_) {
        markFact(node);
    }
}

std::vector<NodePtr> ImplicitSplitOverlay::SplitDirtyTracker::takeFacts() {
    std::vector<NodePtr> facts = std::move(facts_);
    facts_.clear();
    factSet_.clear();
    return facts;
}

void ImplicitSplitOverlay::SplitDirtyTracker::noteEdgeRemoval(const std::vector<SplitNodeRef>& oldInputs) {
    markInputs(oldInputs);
}

void ImplicitSplitOverlay::SplitDirtyTracker::noteEdgeAddition(const std::vector<SplitNodeRef>& newInputs) {
    markInputs(newInputs);
}

void ImplicitSplitOverlay::SplitDirtyTracker::noteEdgeRewrite(
        const std::vector<SplitNodeRef>& oldInputs, const std::vector<SplitNodeRef>& newInputs) {
    (void)newInputs;
    markInputs(oldInputs);
}

void ImplicitSplitOverlay::SplitDirtyTracker::noteFactifiedNode(const NodePtr& node) {
    markFact(node);
}

bool ImplicitSplitOverlay::SplitDirtyTracker::initialized() const {
    return initialized_;
}

ImplicitSplitOverlay::ImplicitSplitOverlay(const IncrementalDerivationGraphViewInterface& view)
        : view_(view), splitDirty_(*this) {
    for (const auto& node : view_.getNodes()) {
        if (!node) {
            continue;
        }
        BaseNodeState state;
        state.originalIsFact = node->isOriginalFactNode();
        state.currentIsFact = node->isFact;
        state.factProbability = node->getProbability();
        state.needOutput = node->needOutput;
        state.hasEvidence = node->hasEvidence();
        state.factSupportTokensView = &node->getProbabilisticSupportTokens();
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
        overlayEdge.supportTokensView = &edge->getProbabilisticSupportTokens();
        for (const auto& input : view_.getInputs(edge)) {
            overlayEdge.inputs.push_back(SplitNodeRef{input, 0});
        }
        edges_.push_back(std::move(overlayEdge));
    }
    activeEdgeCount_ = edges_.size();

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

const std::vector<SupportToken>& ImplicitSplitOverlay::factSupportTokensOf(const BaseNodeState& state) const {
    if (state.hasOwnedFactSupportTokens) {
        return state.factSupportTokens;
    }
    static const std::vector<SupportToken> empty;
    return state.factSupportTokensView ? *state.factSupportTokensView : empty;
}

const std::vector<SupportToken>& ImplicitSplitOverlay::factSupportTokensOf(const NodePtr& node) const {
    static const std::vector<SupportToken> empty;
    auto it = nodeState_.find(node);
    if (it == nodeState_.end()) {
        return empty;
    }
    return factSupportTokensOf(it->second);
}

const std::vector<SupportToken>& ImplicitSplitOverlay::edgeSupportTokensOf(
        const ImplicitSplitOverlayEdge& edge) const {
    if (edge.hasOwnedSupportTokens) {
        return edge.supportTokens;
    }
    static const std::vector<SupportToken> empty;
    return edge.supportTokensView ? *edge.supportTokensView : empty;
}

void ImplicitSplitOverlay::setFactSupportTokens(BaseNodeState& state, std::vector<SupportToken> tokens) {
    sortUniqueSupportTokens(tokens);
    state.factSupportTokens = std::move(tokens);
    state.factSupportTokensView = nullptr;
    state.hasOwnedFactSupportTokens = true;
}

void ImplicitSplitOverlay::setEdgeSupportTokens(ImplicitSplitOverlayEdge& edge, std::vector<SupportToken> tokens) {
    sortUniqueSupportTokens(tokens);
    edge.supportTokens = std::move(tokens);
    edge.supportTokensView = nullptr;
    edge.hasOwnedSupportTokens = true;
}

std::size_t ImplicitSplitOverlay::activeIncomingCount(const NodePtr& node) const {
    return activeIncomingEdges(node).size();
}

std::size_t ImplicitSplitOverlay::activeOutgoingCount(const SplitNodeRef& ref) const {
    return activeOutgoingEdges(ref).size();
}

std::size_t ImplicitSplitOverlay::activeSemanticInputCount(const NodePtr& fact) const {
    ensureActiveEdgeIndices();
    auto it = activeSemanticInputOccurrencesByFact_.find(fact);
    if (it == activeSemanticInputOccurrencesByFact_.end()) {
        return 0;
    }
    return it->second;
}

const std::vector<std::size_t>& ImplicitSplitOverlay::activeIncomingEdges(const NodePtr& node) const {
    ensureActiveEdgeIndices();
    static const std::vector<std::size_t> empty;
    auto it = activeIncomingEdgeIdsByNode_.find(node);
    if (it == activeIncomingEdgeIdsByNode_.end()) {
        return empty;
    }
    return it->second;
}

const std::vector<std::size_t>& ImplicitSplitOverlay::activeOutgoingEdges(const SplitNodeRef& ref) const {
    ensureActiveEdgeIndices();
    return activeOutgoingEdgesSnapshot(ref);
}

const std::vector<std::size_t>& ImplicitSplitOverlay::activeOutgoingEdgesSnapshot(const SplitNodeRef& ref) const {
    static const std::vector<std::size_t> empty;
    auto it = activeOutgoingEdgeIdsByRef_.find(ref);
    if (it == activeOutgoingEdgeIdsByRef_.end()) {
        return empty;
    }
    return it->second;
}

bool ImplicitSplitOverlay::canSplitFact(const NodePtr& fact) const {
    if (!fact || fact->isShadow) {
        return false;
    }
    auto itState = nodeState_.find(fact);
    if (itState == nodeState_.end()) {
        return false;
    }
    const auto& state = itState->second;
    return state.originalIsFact && state.currentIsFact && !state.needOutput && !state.hasEvidence;
}

void ImplicitSplitOverlay::factifyNode(const NodePtr& node, double probability, std::vector<SupportToken> supportTokens) {
    if (!node) {
        return;
    }
    auto& outState = nodeState_.at(node);
    outState.currentIsFact = true;
    outState.factProbability = std::clamp(probability, 0.0, 1.0);
    setFactSupportTokens(outState, std::move(supportTokens));
    splitDirty_.noteFactifiedNode(node);
}

void ImplicitSplitOverlay::deactivateEdge(ImplicitSplitOverlayEdge& edge) {
    if (!edge.active) {
        return;
    }
    edge.active = false;
    if (activeEdgeCount_ > 0) {
        --activeEdgeCount_;
    }
    markActiveEdgeIndicesDirty();
}

void ImplicitSplitOverlay::removeEdge(ImplicitSplitOverlayEdge& edge) {
    splitDirty_.noteEdgeRemoval(edge.inputs);
    deactivateEdge(edge);
}

void ImplicitSplitOverlay::collapseEdgeToFact(
        ImplicitSplitOverlayEdge& edge, double probability, std::vector<SupportToken> supportTokens) {
    if (!edge.output) {
        return;
    }
    splitDirty_.noteEdgeRemoval(edge.inputs);
    factifyNode(edge.output, probability, std::move(supportTokens));
    deactivateEdge(edge);
}

void ImplicitSplitOverlay::rewriteEdgeInPlace(ImplicitSplitOverlayEdge& edge, const std::vector<SplitNodeRef>& oldInputs,
        std::vector<SplitNodeRef> newInputs, std::vector<bool> newNegations, double probability,
        std::vector<SupportToken> supportTokens) {
    splitDirty_.noteEdgeRewrite(oldInputs, newInputs);
    edge.inputs = std::move(newInputs);
    edge.negations = std::move(newNegations);
    edge.probability = std::clamp(probability, 0.0, 1.0);
    edge.deterministic = nearlyOne(edge.probability);
    setEdgeSupportTokens(edge, std::move(supportTokens));
    markActiveEdgeIndicesDirty();
}

void ImplicitSplitOverlay::addSyntheticEdge(std::vector<SplitNodeRef> inputs, std::vector<bool> negations,
        const NodePtr& output, double probability, std::vector<SupportToken> supportTokens) {
    if (!output || nearlyZero(probability)) {
        return;
    }
    splitDirty_.noteEdgeAddition(inputs);
    ImplicitSplitOverlayEdge newEdge;
    newEdge.baseEdge = nullptr;
    newEdge.inputs = std::move(inputs);
    newEdge.negations = std::move(negations);
    newEdge.output = output;
    newEdge.probability = std::clamp(probability, 0.0, 1.0);
    newEdge.deterministic = nearlyOne(newEdge.probability);
    newEdge.active = true;
    setEdgeSupportTokens(newEdge, std::move(supportTokens));
    edges_.push_back(std::move(newEdge));
    ++activeEdgeCount_;
    markActiveEdgeIndicesDirty();
}

void ImplicitSplitOverlay::replaceEdgesWithSyntheticEdge(const std::vector<std::size_t>& removedEdgeIndices,
        std::vector<SplitNodeRef> inputs, std::vector<bool> negations, const NodePtr& output, double probability,
        std::vector<SupportToken> supportTokens) {
    for (const auto edgeIndex : removedEdgeIndices) {
        if (edgeIndex >= edges_.size()) {
            continue;
        }
        removeEdge(edges_[edgeIndex]);
    }
    addSyntheticEdge(std::move(inputs), std::move(negations), output, probability, std::move(supportTokens));
}

void ImplicitSplitOverlay::markActiveEdgeIndicesDirty() {
    activeEdgeIndicesDirty_ = true;
}

void ImplicitSplitOverlay::ensureActiveEdgeIndices() const {
    if (!activeEdgeIndicesDirty_) {
        return;
    }
    rebuildActiveEdgeIndices();
}

void ImplicitSplitOverlay::rebuildActiveEdgeIndices() const {
    activeIncomingEdgeIdsByNode_.clear();
    activeOutgoingEdgeIdsByRef_.clear();
    activeSemanticInputOccurrencesByFact_.clear();
    activeEdgeIds_.clear();
    if (activeEdgeCount_ == 0) {
        activeEdgeIndicesDirty_ = false;
        return;
    }
    activeEdgeIds_.reserve(activeEdgeCount_);
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        const auto& edge = edges_[i];
        if (!edge.active || !edge.output) {
            continue;
        }
        activeEdgeIds_.push_back(i);
        activeIncomingEdgeIdsByNode_[edge.output].push_back(i);
        std::vector<SplitNodeRef> seenInputs;
        seenInputs.reserve(edge.inputs.size());
        for (const auto& input : edge.inputs) {
            if (input.base && isFactRef(input)) {
                ++activeSemanticInputOccurrencesByFact_[input.base];
            }
            bool duplicate = false;
            for (const auto& seen : seenInputs) {
                if (seen == input) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                seenInputs.push_back(input);
                activeOutgoingEdgeIdsByRef_[input].push_back(i);
            }
        }
    }
    activeEdgeIndicesDirty_ = false;
}

std::vector<std::vector<std::size_t>> ImplicitSplitOverlay::partitionFactOutgoingEdgesNaive(
        const NodePtr& fact, ImplicitSplitOverlayStats* stats) const {
    std::vector<std::vector<std::size_t>> groups;
    auto itState = nodeState_.find(fact);
    if (itState == nodeState_.end() || !itState->second.currentIsFact || !itState->second.originalIsFact ||
            itState->second.needOutput || itState->second.hasEvidence) {
        return groups;
    }

    const auto outs = activeOutgoingEdgesSnapshot(SplitNodeRef{fact, 0});
    if (outs.size() < 2) {
        return groups;
    }

    bool allImmediateSinks = true;
    std::unordered_map<NodePtr, std::vector<std::size_t>> sinkGroups;
    sinkGroups.reserve(outs.size());
    for (const auto edgeIndex : outs) {
        const auto& edge = edges_[edgeIndex];
        if (!edge.output || !activeOutgoingEdgesSnapshot(SplitNodeRef{edge.output, 0}).empty()) {
            allImmediateSinks = false;
            break;
        }
        sinkGroups[edge.output].push_back(edgeIndex);
    }
    if (allImmediateSinks) {
        if (sinkGroups.size() <= 1) {
            return {};
        }
        std::vector<std::vector<std::size_t>> sinkResult;
        sinkResult.reserve(sinkGroups.size());
        for (auto& [_, edgeGroup] : sinkGroups) {
            sinkResult.push_back(std::move(edgeGroup));
        }
        std::sort(sinkResult.begin(), sinkResult.end(), [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        return sinkResult;
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
            for (const auto nextEdgeIndex : activeOutgoingEdgesSnapshot(SplitNodeRef{current, 0})) {
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

    const auto outs = activeOutgoingEdgesSnapshot(SplitNodeRef{fact, 0});
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
            for (const auto nextEdgeIndex : activeOutgoingEdgesSnapshot(SplitNodeRef{current, 0})) {
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
                    input.alias = aliasId;
                    touched = true;
                }
            }
            if (touched && stats) {
                ++stats->edgesAliased;
            }
        }
    }
}

bool ImplicitSplitOverlay::applySplit(ImplicitSplitMode mode, ImplicitSplitOverlayStats* stats) {
    if (mode == ImplicitSplitMode::None) {
        return false;
    }
    bool changed = false;
    auto sameEdgeSet = [](const std::vector<std::size_t>& a, const std::vector<std::size_t>& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    };
    ensureActiveEdgeIndices();
    if (!splitDirty_.initialized()) {
        splitDirty_.seedAllFacts();
    }
    std::vector<NodePtr> facts = splitDirty_.takeFacts();
    if (facts.empty()) {
        return false;
    }
    std::sort(facts.begin(), facts.end(), [](const NodePtr& a, const NodePtr& b) {
        return a->getId() < b->getId();
    });
    for (const auto& fact : facts) {
        if (!canSplitFact(fact)) {
            cachedBaseOutgoingEdgesByFact_.erase(fact);
            continue;
        }
        const auto& outs = activeOutgoingEdgesSnapshot(SplitNodeRef{fact, 0});
        std::vector<std::size_t> currentOuts(outs.begin(), outs.end());
        auto itCached = cachedBaseOutgoingEdgesByFact_.find(fact);
        if (itCached != cachedBaseOutgoingEdgesByFact_.end() && sameEdgeSet(itCached->second, currentOuts)) {
            if (stats) {
                ++stats->splitFactsCacheHits;
            }
            continue;
        }
        if (stats) {
            ++stats->splitFactsConsidered;
        }
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
        changed = changed || groups.size() > 1;
        if (groups.size() > 1) {
            cachedBaseOutgoingEdgesByFact_[fact] = groups.front();
        } else {
            cachedBaseOutgoingEdgesByFact_[fact] = std::move(currentOuts);
        }
        if (stats) {
            stats->splitAliasApplyMs += elapsedMs(aliasStart);
            if (groups.size() > 1) {
                ++stats->splitFactsAliased;
            }
        }
    }
    if (changed) {
        markActiveEdgeIndicesDirty();
    }
    return changed;
}

std::vector<std::size_t> ImplicitSplitOverlay::collectAffectedEdgesForSemanticFact(const NodePtr& fact) const {
    std::vector<std::size_t> affected;
    if (!fact) {
        return affected;
    }
    ensureActiveEdgeIndices();
    std::unordered_set<std::size_t> seen;
    auto addRefEdges = [&](const SplitNodeRef& ref) {
        for (const auto edgeIndex : activeOutgoingEdges(ref)) {
            if (seen.insert(edgeIndex).second) {
                affected.push_back(edgeIndex);
            }
        }
    };
    addRefEdges(SplitNodeRef{fact, 0});
    auto itAliases = aliasesByFact_.find(fact);
    if (itAliases != aliasesByFact_.end()) {
        for (const auto aliasId : itAliases->second) {
            addRefEdges(SplitNodeRef{fact, aliasId});
        }
    }
    return affected;
}

std::vector<std::size_t> ImplicitSplitOverlay::collectAffectedEdgesForSingleHyperedgeRewrite(
        std::size_t edgeIndex) const {
    std::vector<std::size_t> affected;
    if (edgeIndex >= edges_.size()) {
        return affected;
    }
    ensureActiveEdgeIndices();
    std::unordered_set<std::size_t> seen;
    auto addEdge = [&](std::size_t idx) {
        if (seen.insert(idx).second) {
            affected.push_back(idx);
        }
    };
    auto addEdges = [&](const std::vector<std::size_t>& edgeIds) {
        for (const auto idx : edgeIds) {
            addEdge(idx);
        }
    };

    const auto& edge = edges_[edgeIndex];
    addEdge(edgeIndex);
    if (edge.output) {
        addEdges(activeIncomingEdges(edge.output));
        addEdges(activeOutgoingEdges(SplitNodeRef{edge.output, 0}));
    }
    for (const auto& input : edge.inputs) {
        addEdges(activeOutgoingEdges(input));
        if (input.base && isFactRef(input)) {
            addEdges(collectAffectedEdgesForSemanticFact(input.base));
        }
    }
    return affected;
}

bool ImplicitSplitOverlay::buildAllFactsCandidate(std::size_t edgeIndex, AllFactsCandidate* candidate) const {
    auto canAbsorbFactRef = [&](const SplitNodeRef& ref, std::size_t localOccurrences) {
        if (!isFactRef(ref) || !ref.base) {
            return false;
        }
        const auto& inputState = nodeState_.at(ref.base);
        if (inputState.hasEvidence || inputState.needOutput) {
            return false;
        }
        if (inputState.originalIsFact) {
            if (activeIncomingCount(ref.base) != 0) {
                return false;
            }
            const double p = factProbabilityOf(ref);
            return !(p > 0.0 && p < 1.0) || activeSemanticInputCount(ref.base) == localOccurrences;
        }
        const double p = factProbabilityOf(ref);
        return !(p > 0.0 && p < 1.0) && activeOutgoingCount(ref) == 1;
    };

    if (edgeIndex >= edges_.size()) {
        return false;
    }
    const auto& edge = edges_[edgeIndex];
    if (!edge.active || edge.inputs.empty() || !edge.output) {
        return false;
    }
    for (const auto& input : edge.inputs) {
        if (!canAbsorbFactRef(input, 1)) {
            return false;
        }
    }
    if (activeIncomingCount(edge.output) != 1) {
        return false;
    }
    if (candidate) {
        candidate->edgeIndex = edgeIndex;
    }
    return true;
}

bool ImplicitSplitOverlay::applyAllFactsCandidate(
        const AllFactsCandidate& candidate, ImplicitSplitOverlayStats* stats) {
    if (candidate.edgeIndex >= edges_.size()) {
        return false;
    }
    auto& edge = edges_[candidate.edgeIndex];
    if (!edge.active || edge.inputs.empty() || !edge.output) {
        return false;
    }
    double probability = edge.deterministic ? 1.0 : edge.probability;
    std::vector<SupportToken> factSupport = edgeSupportTokensOf(edge);
    for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
        double inputProb = factProbabilityOf(edge.inputs[i]);
        if (i < edge.negations.size() && edge.negations[i]) {
            inputProb = 1.0 - inputProb;
        }
        probability *= inputProb;
        factSupport = mergeSupportTokenLists({&factSupport, &factSupportTokensOf(edge.inputs[i].base)});
    }
    collapseEdgeToFact(edge, probability, std::move(factSupport));
    if (stats) {
        ++stats->allFactsRewrites;
        ++stats->removedEdges;
        ++stats->factOutputsFolded;
    }
    return true;
}

bool ImplicitSplitOverlay::rewriteAllFactsPass(ImplicitSplitOverlayStats* stats) {
    const auto detectStart = Clock::now();
    std::vector<AllFactsCandidate> candidates;
    candidates.reserve(activeEdgeIds_.size());
    for (const auto edgeIndex : activeEdgeIds_) {
        AllFactsCandidate candidate;
        if (!buildAllFactsCandidate(edgeIndex, &candidate)) {
            continue;
        }
        candidates.push_back(candidate);
    }
    if (stats) {
        stats->fastPathDetectMs += elapsedMs(detectStart);
    }

    const auto summarizeStart = Clock::now();
    bool changed = false;
    for (const auto& candidate : candidates) {
        if (!applyAllFactsCandidate(candidate, stats)) {
            continue;
        }
        changed = true;
    }
    if (stats) {
        stats->fastPathSummarizeMs += elapsedMs(summarizeStart);
    }
    return changed;
}

bool ImplicitSplitOverlay::buildDirectSingleHyperedgeCandidate(
        std::size_t edgeIndex, SingleHyperedgeCandidate* candidate) const {
    auto canAbsorbFactRef = [&](const SplitNodeRef& ref, std::size_t localOccurrences) {
        if (!isFactRef(ref) || !ref.base) {
            return false;
        }
        const auto& inputState = nodeState_.at(ref.base);
        if (inputState.hasEvidence || inputState.needOutput) {
            return false;
        }
        if (inputState.originalIsFact) {
            if (activeIncomingCount(ref.base) != 0) {
                return false;
            }
            const double p = factProbabilityOf(ref);
            return !(p > 0.0 && p < 1.0) || activeSemanticInputCount(ref.base) == localOccurrences;
        }
        const double p = factProbabilityOf(ref);
        return !(p > 0.0 && p < 1.0) && activeOutgoingCount(ref) == 1;
    };

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
            if (!canAbsorbFactRef(input, 1)) {
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
        return false;
    }
    if (candidate) {
        candidate->edgeIndex = edgeIndex;
        candidate->si = si;
        candidate->siNegated = siNegated;
        candidate->affectedEdges = collectAffectedEdgesForSingleHyperedgeRewrite(edgeIndex);
    }
    return true;
}

bool ImplicitSplitOverlay::applyDirectSingleHyperedgeCandidate(
        const SingleHyperedgeCandidate& candidate, ImplicitSplitOverlayStats* stats) {
    if (candidate.edgeIndex >= edges_.size()) {
        return false;
    }
    auto& edge = edges_[candidate.edgeIndex];
    if (!edge.active || edge.inputs.size() <= 1 || !edge.output) {
        return false;
    }
    const auto oldInputs = edge.inputs;

    double probability = edge.deterministic ? 1.0 : edge.probability;
    std::vector<SupportToken> newEdgeSupport = edgeSupportTokensOf(edge);
    for (std::size_t i = 0; i < edge.inputs.size(); ++i) {
        if (edge.inputs[i] == candidate.si) {
            continue;
        }
        double inputProb = factProbabilityOf(edge.inputs[i]);
        if (i < edge.negations.size() && edge.negations[i]) {
            inputProb = 1.0 - inputProb;
        }
        probability *= inputProb;
        newEdgeSupport = mergeSupportTokenLists({&newEdgeSupport, &factSupportTokensOf(edge.inputs[i].base)});
    }
    if (nearlyZero(probability)) {
        removeEdge(edge);
        if (stats) {
            ++stats->singleHyperedgeRewrites;
            ++stats->removedEdges;
        }
        return true;
    }

    rewriteEdgeInPlace(edge, oldInputs, {candidate.si}, {candidate.siNegated}, probability, std::move(newEdgeSupport));
    if (stats) {
        ++stats->singleHyperedgeRewrites;
    }
    return true;
}

bool ImplicitSplitOverlay::rewriteSingleHyperedgePass(ImplicitSplitOverlayStats* stats) {
    ensureActiveEdgeIndices();
    std::unordered_set<std::size_t> dirtyEdges(activeEdgeIds_.begin(), activeEdgeIds_.end());
    bool changedAny = false;

    while (!dirtyEdges.empty()) {
        std::vector<std::size_t> currentEdges(dirtyEdges.begin(), dirtyEdges.end());
        dirtyEdges.clear();
        std::sort(currentEdges.begin(), currentEdges.end());

        const auto detectStart = Clock::now();
        std::vector<SingleHyperedgeCandidate> candidateEdges;
        candidateEdges.reserve(currentEdges.size());
        for (const auto edgeIndex : currentEdges) {
            SingleHyperedgeCandidate candidate;
            if (!buildDirectSingleHyperedgeCandidate(edgeIndex, &candidate)) {
                continue;
            }
            candidateEdges.push_back(std::move(candidate));
        }
        if (stats) {
            stats->fastPathDetectMs += elapsedMs(detectStart);
        }
        if (candidateEdges.empty()) {
            continue;
        }

        const auto summarizeStart = Clock::now();
        bool changedThisRound = false;
        for (const auto& candidate : candidateEdges) {
            if (!applyDirectSingleHyperedgeCandidate(candidate, stats)) {
                continue;
            }
            changedAny = true;
            changedThisRound = true;
            for (const auto affectedEdgeIndex : candidate.affectedEdges) {
                dirtyEdges.insert(affectedEdgeIndex);
            }
        }
        if (stats) {
            stats->fastPathSummarizeMs += elapsedMs(summarizeStart);
        }
        if (!changedThisRound || activeEdgeCount_ == 0) {
            break;
        }
        ensureActiveEdgeIndices();
    }

    return changedAny;
}

bool ImplicitSplitOverlay::classifyLinearTwoEdge(std::size_t edgeIndex, SplitNodeRef* entryOut,
        bool* entryNegatedOut, NodePtr* midOut, std::size_t* nextEdgeOut) const {
    if (edgeIndex >= edges_.size()) {
        return false;
    }
    const auto& edge1 = edges_[edgeIndex];
    if (!edge1.active || edge1.inputs.size() != 1 || !edge1.output) {
        return false;
    }
    if (edge1.negations.size() > 1) {
        return false;
    }
    const SplitNodeRef entry = edge1.inputs[0];
    NodePtr mid = edge1.output;
    if (!entry.base || !mid || mid == entry.base) {
        return false;
    }
    const auto& midState = nodeState_.at(mid);
    if (midState.needOutput || midState.hasEvidence) {
        return false;
    }
    const auto& incomingMid = activeIncomingEdges(mid);
    if (incomingMid.size() != 1 || incomingMid[0] != edgeIndex) {
        return false;
    }
    const SplitNodeRef midRef{mid, 0};
    const auto& outgoingMid = activeOutgoingEdges(midRef);
    if (outgoingMid.size() != 1) {
        return false;
    }
    const std::size_t edge2Index = outgoingMid[0];
    if (edge2Index >= edges_.size()) {
        return false;
    }
    const auto& edge2 = edges_[edge2Index];
    if (!edge2.active || edge2.inputs.size() != 1 || !(edge2.inputs[0] == midRef) || !edge2.output) {
        return false;
    }
    if (edge2.negations.size() > 1 || (!edge2.negations.empty() && edge2.negations[0])) {
        return false;
    }
    if (edge2.output == mid || edge2.output == entry.base) {
        return false;
    }
    if (entryOut) {
        *entryOut = entry;
    }
    if (entryNegatedOut) {
        *entryNegatedOut = !edge1.negations.empty() && edge1.negations[0];
    }
    if (midOut) {
        *midOut = mid;
    }
    if (nextEdgeOut) {
        *nextEdgeOut = edge2Index;
    }
    return true;
}

bool ImplicitSplitOverlay::classifyFanOutConverge(
        const SplitNodeRef& entryRef, FanOutConvergeInfo* outInfo) const {
    if (!isFactRef(entryRef) || !entryRef.base) {
        return false;
    }
    const auto& entryState = nodeState_.at(entryRef.base);
    if (!entryState.originalIsFact || !entryState.currentIsFact || entryState.needOutput || entryState.hasEvidence) {
        return false;
    }
    const auto& outs = activeOutgoingEdges(entryRef);
    if (outs.size() < 2) {
        return false;
    }

    FanOutConvergeInfo info;
    std::unordered_set<NodePtr> xiSet;
    bool fanNegInit = false;
    std::size_t convEdgeIndex = std::numeric_limits<std::size_t>::max();
    for (const auto edgeIndex : outs) {
        if (edgeIndex >= edges_.size()) {
            return false;
        }
        const auto& fanEdge = edges_[edgeIndex];
        if (!fanEdge.active || fanEdge.inputs.size() != 1 || !(fanEdge.inputs[0] == entryRef) || !fanEdge.output) {
            return false;
        }
        const auto& xiState = nodeState_.at(fanEdge.output);
        if (xiState.needOutput || xiState.hasEvidence) {
            return false;
        }
        if (!xiSet.insert(fanEdge.output).second) {
            return false;
        }
        const auto& incomingXi = activeIncomingEdges(fanEdge.output);
        if (incomingXi.size() != 1 || incomingXi[0] != edgeIndex) {
            return false;
        }
        const SplitNodeRef xiRef{fanEdge.output, 0};
        const auto& outgoingXi = activeOutgoingEdges(xiRef);
        if (outgoingXi.size() != 1) {
            return false;
        }
        if (convEdgeIndex == std::numeric_limits<std::size_t>::max()) {
            convEdgeIndex = outgoingXi[0];
        } else if (convEdgeIndex != outgoingXi[0]) {
            return false;
        }
        const bool neg = !fanEdge.negations.empty() && fanEdge.negations[0];
        if (!fanNegInit) {
            info.fanNegated = neg;
            fanNegInit = true;
        } else if (info.fanNegated != neg) {
            info.mixedPolarity = true;
        }
        info.fanEdges.push_back(edgeIndex);
        info.xiRefs.push_back(xiRef);
    }

    if (convEdgeIndex == std::numeric_limits<std::size_t>::max() || convEdgeIndex >= edges_.size()) {
        return false;
    }
    const auto& convEdge = edges_[convEdgeIndex];
    if (!convEdge.active || !convEdge.output || convEdge.inputs.size() != info.xiRefs.size()) {
        return false;
    }
    if (convEdge.negations.size() > convEdge.inputs.size()) {
        return false;
    }
    for (bool neg : convEdge.negations) {
        if (neg) {
            return false;
        }
    }
    std::unordered_set<SplitNodeRef, SplitNodeRefHash> convInputs(convEdge.inputs.begin(), convEdge.inputs.end());
    std::unordered_set<SplitNodeRef, SplitNodeRefHash> xiInputs(info.xiRefs.begin(), info.xiRefs.end());
    if (convInputs != xiInputs || convEdge.output == entryRef.base) {
        return false;
    }
    info.convEdge = convEdgeIndex;
    info.exit = convEdge.output;
    const double p = factProbabilityOf(entryRef);
    if (p > 0.0 && p < 1.0 && activeSemanticInputCount(entryRef.base) != info.fanEdges.size()) {
        return false;
    }
    if (outInfo) {
        *outInfo = std::move(info);
    }
    return true;
}

std::vector<ImplicitSplitOverlay::FastPathCandidate> ImplicitSplitOverlay::collectFastPathCandidates(
        bool enableLinearTwoEdge, bool enableParallelEdge, bool enableFanOutConverge) const {
    auto addUniqueNode = [](std::vector<SplitNodeRef>& nodes,
                             std::unordered_set<SplitNodeRef, SplitNodeRefHash>& seen,
                             const SplitNodeRef& ref) {
        if (!ref.base) {
            return;
        }
        if (seen.insert(ref).second) {
            nodes.push_back(ref);
        }
    };

    std::vector<FastPathCandidate> candidates;
    candidates.reserve(activeEdgeIds_.size());
    if (enableLinearTwoEdge) {
        for (const auto edgeIndex : activeEdgeIds_) {
            SplitNodeRef entry{};
            bool entryNegated = false;
            NodePtr mid = nullptr;
            std::size_t edge2Index = 0;
            if (!classifyLinearTwoEdge(edgeIndex, &entry, &entryNegated, &mid, &edge2Index)) {
                continue;
            }
            std::vector<SplitNodeRef> nodes;
            std::unordered_set<SplitNodeRef, SplitNodeRefHash> seen;
            addUniqueNode(nodes, seen, entry);
            addUniqueNode(nodes, seen, SplitNodeRef{mid, 0});
            addUniqueNode(nodes, seen, SplitNodeRef{edges_[edge2Index].output, 0});
            candidates.push_back(FastPathCandidate{
                    FastPathCandidate::Kind::LinearTwoEdge,
                    edgeIndex,
                    edge2Index,
                    {},
                    entry,
                    entryNegated,
                    false,
                    edges_[edge2Index].output,
                    std::move(nodes)});
        }
    }
    if (enableParallelEdge) {
        for (const auto& [output, incoming] : activeIncomingEdgeIdsByNode_) {
            struct ParallelKey {
                SplitNodeRef ref;
                bool negated = false;

                bool operator==(const ParallelKey& other) const {
                    return ref == other.ref && negated == other.negated;
                }
            };
            struct ParallelKeyHash {
                std::size_t operator()(const ParallelKey& key) const {
                    std::size_t h = SplitNodeRefHash{}(key.ref);
                    return h ^ (std::hash<bool>{}(key.negated) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U));
                }
            };
            std::unordered_map<ParallelKey, std::vector<std::size_t>, ParallelKeyHash> groups;
            for (const auto edgeIndex : incoming) {
                if (edgeIndex >= edges_.size()) {
                    continue;
                }
                const auto& edge = edges_[edgeIndex];
                if (!edge.active || edge.inputs.size() != 1 || !edge.output || edge.output != output) {
                    continue;
                }
                groups[ParallelKey{edge.inputs[0], !edge.negations.empty() && edge.negations[0]}].push_back(edgeIndex);
            }
            for (auto& [key, groupEdges] : groups) {
                if (groupEdges.size() < 2 || !key.ref.base || output == key.ref.base) {
                    continue;
                }
                candidates.push_back(FastPathCandidate{
                        FastPathCandidate::Kind::ParallelEdge,
                        0,
                        0,
                        groupEdges,
                        key.ref,
                        key.negated,
                        false,
                        output,
                        {key.ref, SplitNodeRef{output, 0}}});
            }
        }
    }
    if (enableFanOutConverge) {
        for (const auto& ref : enumerateActiveFactRefs()) {
            FanOutConvergeInfo info;
            if (!classifyFanOutConverge(ref, &info)) {
                continue;
            }
            std::vector<SplitNodeRef> nodes;
            std::unordered_set<SplitNodeRef, SplitNodeRefHash> seen;
            addUniqueNode(nodes, seen, ref);
            for (const auto& xiRef : info.xiRefs) {
                addUniqueNode(nodes, seen, xiRef);
            }
            addUniqueNode(nodes, seen, SplitNodeRef{info.exit, 0});
            candidates.push_back(FastPathCandidate{
                    FastPathCandidate::Kind::FanOutConverge,
                    info.convEdge,
                    0,
                    info.fanEdges,
                    ref,
                    info.fanNegated,
                    info.mixedPolarity,
                    info.exit,
                    std::move(nodes)});
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
    return selected;
}

bool ImplicitSplitOverlay::applyFastPathCandidate(
        const FastPathCandidate& candidate, ImplicitSplitOverlayStats* stats) {
    switch (candidate.kind) {
        case FastPathCandidate::Kind::LinearTwoEdge: {
            if (candidate.edgeIndex >= edges_.size() || candidate.auxEdgeIndex >= edges_.size() ||
                    !candidate.entryRef.base || !candidate.output) {
                return false;
            }
            auto& edge1 = edges_[candidate.edgeIndex];
            auto& edge2 = edges_[candidate.auxEdgeIndex];
            if (!edge1.active || !edge2.active || edge1.inputs.size() != 1 || edge2.inputs.size() != 1 ||
                    !edge1.output || !edge2.output || !(edge1.inputs[0] == candidate.entryRef) ||
                    !(edge2.inputs[0] == SplitNodeRef{edge1.output, 0}) || edge2.output != candidate.output) {
                return false;
            }
            const double probability =
                    std::clamp(edge1.probability, 0.0, 1.0) * std::clamp(edge2.probability, 0.0, 1.0);
            const auto& edge1Support = edgeSupportTokensOf(edge1);
            const auto& edge2Support = edgeSupportTokensOf(edge2);
            std::vector<SupportToken> newEdgeSupport = mergeSupportTokenLists({&edge1Support, &edge2Support});
            replaceEdgesWithSyntheticEdge({candidate.edgeIndex, candidate.auxEdgeIndex}, {candidate.entryRef},
                    {candidate.primaryNegated}, candidate.output, probability, std::move(newEdgeSupport));
            if (stats) {
                ++stats->linearTwoEdgeRewrites;
                stats->removedEdges += 2;
            }
            return true;
        }
        case FastPathCandidate::Kind::ParallelEdge: {
            if (candidate.edgeIndices.size() < 2 || !candidate.entryRef.base || !candidate.output) {
                return false;
            }
            double prod = 1.0;
            std::size_t validEdges = 0;
            for (const auto edgeIndex : candidate.edgeIndices) {
                if (edgeIndex >= edges_.size()) {
                    return false;
                }
                const auto& edge = edges_[edgeIndex];
                if (!edge.active || edge.inputs.size() != 1 || !edge.output) {
                    return false;
                }
                if (!(edge.inputs[0] == candidate.entryRef) || edge.output != candidate.output ||
                        (!edge.negations.empty() && edge.negations[0]) != candidate.primaryNegated) {
                    return false;
                }
                prod *= 1.0 - std::clamp(edge.probability, 0.0, 1.0);
                ++validEdges;
            }
            if (validEdges < 2 || candidate.output == candidate.entryRef.base) {
                return false;
            }
            std::vector<SupportToken> newEdgeSupport;
            for (const auto edgeIndex : candidate.edgeIndices) {
                const auto& edgeSupport = edgeSupportTokensOf(edges_[edgeIndex]);
                newEdgeSupport = mergeSupportTokenLists({&newEdgeSupport, &edgeSupport});
            }
            replaceEdgesWithSyntheticEdge(candidate.edgeIndices, {candidate.entryRef}, {candidate.primaryNegated},
                    candidate.output, std::clamp(1.0 - prod, 0.0, 1.0), std::move(newEdgeSupport));
            if (stats) {
                ++stats->parallelEdgeRewrites;
                stats->removedEdges += validEdges;
            }
            return true;
        }
        case FastPathCandidate::Kind::FanOutConverge: {
            if (!candidate.entryRef.base || candidate.edgeIndex >= edges_.size() || !candidate.output) {
                return false;
            }
            for (const auto edgeIndex : candidate.edgeIndices) {
                if (edgeIndex >= edges_.size() || !edges_[edgeIndex].active) {
                    return false;
                }
            }
            if (!edges_[candidate.edgeIndex].active) {
                return false;
            }
            std::vector<std::size_t> removedEdgeIndices = candidate.edgeIndices;
            removedEdgeIndices.push_back(candidate.edgeIndex);
            const std::size_t removed = candidate.edgeIndices.size() + 1;

            if (!candidate.mixedPolarity) {
                double probability = std::clamp(edges_[candidate.edgeIndex].probability, 0.0, 1.0);
                std::vector<SupportToken> newEdgeSupport = edgeSupportTokensOf(edges_[candidate.edgeIndex]);
                for (const auto edgeIndex : candidate.edgeIndices) {
                    probability *= std::clamp(edges_[edgeIndex].probability, 0.0, 1.0);
                    const auto& edgeSupport = edgeSupportTokensOf(edges_[edgeIndex]);
                    newEdgeSupport = mergeSupportTokenLists({&newEdgeSupport, &edgeSupport});
                }
                replaceEdgesWithSyntheticEdge(removedEdgeIndices, {candidate.entryRef}, {candidate.primaryNegated},
                        candidate.output, probability, std::move(newEdgeSupport));
            } else {
                for (const auto edgeIndex : removedEdgeIndices) {
                    if (edgeIndex < edges_.size()) {
                        removeEdge(edges_[edgeIndex]);
                    }
                }
            }
            if (stats) {
                ++stats->fanOutConvergeRewrites;
                stats->removedEdges += removed;
            }
            return true;
        }
    }
    return false;
}

void ImplicitSplitOverlay::ensureActiveEdgeIndicesWithStats(ImplicitSplitOverlayStats* stats) {
    if (!activeEdgeIndicesDirty_) {
        return;
    }
    const auto rebuildStart = Clock::now();
    rebuildActiveEdgeIndices();
    if (stats) {
        ++stats->rebuildIndexCount;
        stats->rebuildIndexMs += elapsedMs(rebuildStart);
    }
}

ImplicitSplitOverlay::DirectLocalFastPathResult ImplicitSplitOverlay::runDirectLocalFastPaths(
        bool enableAllFacts, bool enableSingleHyperedge, ImplicitSplitOverlayStats* stats) {
    DirectLocalFastPathResult result;
    if (enableAllFacts && activeEdgeCount_ > 0) {
        const auto allFactsStart = Clock::now();
        const bool allFactsChanged = rewriteAllFactsPass(stats);
        if (stats) {
            stats->fastPathAllFactsMs += elapsedMs(allFactsStart);
        }
        result.changed = allFactsChanged || result.changed;
    }
    if (enableSingleHyperedge && activeEdgeCount_ > 0) {
        const auto singleStart = Clock::now();
        const bool singleChanged = rewriteSingleHyperedgePass(stats);
        if (stats) {
            const double elapsed = elapsedMs(singleStart);
            stats->fastPathSingleMs += elapsed;
            stats->fastPathSummarizeMs += elapsed;
        }
        result.changed = singleChanged || result.changed;
    }
    result.activeEdgesExhausted = activeEdgeCount_ == 0;
    return result;
}

ImplicitSplitOverlay::GenericFastPathRoundResult ImplicitSplitOverlay::runGenericFastPathRound(
        bool enableLinearTwoEdge, bool enableParallelEdge, bool enableFanOutConverge, bool enableAllFacts,
        bool enableSingleHyperedge, ImplicitSplitOverlayStats* stats) {
    GenericFastPathRoundResult result;
    if (activeEdgeCount_ == 0 || (!enableLinearTwoEdge && !enableParallelEdge && !enableFanOutConverge)) {
        result.local.activeEdgesExhausted = activeEdgeCount_ == 0;
        return result;
    }

    if (stats) {
        ++stats->fastPathIterations;
    }
    const auto detectStart = Clock::now();
    std::vector<FastPathCandidate> selected =
            collectFastPathCandidates(enableLinearTwoEdge, enableParallelEdge, enableFanOutConverge);
    if (stats) {
        stats->fastPathDetectMs += elapsedMs(detectStart);
    }

    for (const auto& candidate : selected) {
        const auto summarizeStart = Clock::now();
        const bool applied = applyFastPathCandidate(candidate, stats);
        const double elapsed = elapsedMs(summarizeStart);
        switch (candidate.kind) {
            case FastPathCandidate::Kind::LinearTwoEdge: {
                if (stats) {
                    stats->fastPathLinearMs += elapsed;
                    stats->fastPathSummarizeMs += elapsed;
                }
                result.changed = applied || result.changed;
                break;
            }
            case FastPathCandidate::Kind::ParallelEdge: {
                if (stats) {
                    stats->fastPathParallelMs += elapsed;
                    stats->fastPathSummarizeMs += elapsed;
                }
                result.changed = applied || result.changed;
                break;
            }
            case FastPathCandidate::Kind::FanOutConverge: {
                if (stats) {
                    stats->fastPathFanOutMs += elapsed;
                    stats->fastPathSummarizeMs += elapsed;
                }
                result.changed = applied || result.changed;
                break;
            }
        }
    }

    if (result.changed) {
        ensureActiveEdgeIndicesWithStats(stats);
        result.rebuiltIndicesBeforeLocal = true;
    }

    result.local = runDirectLocalFastPaths(enableAllFacts, enableSingleHyperedge, stats);
    result.changed = result.local.changed || result.changed;
    return result;
}

bool ImplicitSplitOverlay::runGenericFastPathRounds(bool enableLinearTwoEdge, bool enableParallelEdge,
        bool enableFanOutConverge, bool enableAllFacts, bool enableSingleHyperedge,
        ImplicitSplitOverlayStats* stats) {
    if (activeEdgeCount_ == 0 || (!enableLinearTwoEdge && !enableParallelEdge && !enableFanOutConverge)) {
        return false;
    }

    bool changedAny = false;
    while (true) {
        const auto round = runGenericFastPathRound(
                enableLinearTwoEdge, enableParallelEdge, enableFanOutConverge, enableAllFacts,
                enableSingleHyperedge, stats);
        if (!round.changed) {
            break;
        }
        changedAny = true;
        if (round.local.activeEdgesExhausted) {
            break;
        }
        if (round.rebuiltIndicesBeforeLocal && !round.local.changed) {
            continue;
        }
    }
    return changedAny;
}

bool ImplicitSplitOverlay::rewriteFastPathsToFixpoint(bool enableSingleHyperedge, bool enableLinearTwoEdge,
        bool enableParallelEdge, bool enableFanOutConverge, bool enableAllFacts,
        ImplicitSplitOverlayStats* stats) {
    ensureActiveEdgeIndicesWithStats(stats);

    bool changedAny = false;
    const auto localResult = runDirectLocalFastPaths(enableAllFacts, enableSingleHyperedge, stats);
    changedAny = localResult.changed || changedAny;
    if (localResult.activeEdgesExhausted) {
        return changedAny;
    }
    changedAny = runGenericFastPathRounds(
                         enableLinearTwoEdge, enableParallelEdge, enableFanOutConverge, enableAllFacts,
                         enableSingleHyperedge, stats) ||
            changedAny;
    return changedAny;
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
    ensureActiveEdgeIndices();
    std::vector<const ImplicitSplitOverlayEdge*> result;
    result.reserve(activeEdgeIds_.size());
    for (const auto edgeIndex : activeEdgeIds_) {
        result.push_back(&edges_[edgeIndex]);
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

std::vector<OverlayFactCommit> ImplicitSplitOverlay::collectFactCommits() const {
    std::vector<OverlayFactCommit> result;
    result.reserve(nodeState_.size());
    for (const auto& [node, state] : nodeState_) {
        if (!node) {
            continue;
        }
        const bool factChanged = node->isFact != state.currentIsFact;
        const bool probChanged = std::abs(node->getProbability() - state.factProbability) > kImplicitSplitEps;
        if (!factChanged && !probChanged) {
            continue;
        }
        result.push_back(OverlayFactCommit{
                node,
                state.currentIsFact,
                state.factProbability,
                std::vector<SupportToken>(factSupportTokensOf(state))});
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.node->getId() < b.node->getId();
    });
    return result;
}

std::vector<OverlayEdgeCommit> ImplicitSplitOverlay::collectEdgeCommits() const {
    std::vector<OverlayEdgeCommit> result;
    result.reserve(edges_.size());
    for (const auto& edge : edges_) {
        if (!edge.active || !edge.output) {
            continue;
        }
        bool preserveBaseEdge = false;
        if (edge.baseEdge) {
            const auto& baseInputs = edge.baseEdge->getInputs();
            const auto& baseNegs = edge.baseEdge->getBodyNegations();
            const bool supportUnchanged = !edge.hasOwnedSupportTokens ||
                    edge.baseEdge->getProbabilisticSupportTokens() == edge.supportTokens;
            preserveBaseEdge = (edge.baseEdge->getOutput() == edge.output) &&
                    (baseInputs.size() == edge.inputs.size()) && (baseNegs == edge.negations) &&
                    (edge.baseEdge->isDeterministic() == edge.deterministic) &&
                    supportUnchanged &&
                    (std::abs(edge.baseEdge->getProbability() - edge.probability) <= kImplicitSplitEps);
            if (preserveBaseEdge) {
                for (std::size_t i = 0; i < baseInputs.size(); ++i) {
                    if (edge.inputs[i].base != baseInputs[i]) {
                        preserveBaseEdge = false;
                        break;
                    }
                }
            }
        }
        if (preserveBaseEdge) {
            continue;
        }
        OverlayEdgeCommit commit;
        commit.baseEdge = edge.baseEdge;
        commit.negations = edge.negations;
        commit.output = edge.output;
        commit.probability = edge.probability;
        commit.deterministic = edge.deterministic;
        commit.supportTokens = std::vector<SupportToken>(edgeSupportTokensOf(edge));
        commit.inputs.reserve(edge.inputs.size());
        for (const auto& input : edge.inputs) {
            commit.inputs.push_back(input.base);
        }
        result.push_back(std::move(commit));
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        const std::size_t aid = a.baseEdge ? a.baseEdge->getId() : 0;
        const std::size_t bid = b.baseEdge ? b.baseEdge->getId() : 0;
        if (aid != bid) {
            return aid < bid;
        }
        const std::size_t aout = a.output ? a.output->getId() : 0;
        const std::size_t bout = b.output ? b.output->getId() : 0;
        return aout < bout;
    });
    return result;
}

std::vector<EdgePtr> ImplicitSplitOverlay::collectInactiveBaseEdges() const {
    std::vector<EdgePtr> result;
    result.reserve(edges_.size());
    for (const auto& edge : edges_) {
        if (!edge.active && edge.baseEdge) {
            result.push_back(edge.baseEdge);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a->getId() < b->getId();
    });
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

OverlayGraphStats ImplicitSplitOverlay::computeStats() const {
    OverlayGraphStats stats;
    stats.activeEdges = activeEdgeCount_;
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
        materialized->setOriginalFact(state.originalIsFact);
        materialized->setProbability(state.factProbability);
        materialized->setSemanticFactId(base->getSemanticFactId());
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
        shadow->setOriginalFact(nodeState_.at(ref.base).originalIsFact);
        shadow->setProbability(factProbabilityOf(ref));
        shadow->setSemanticFactId(ref.base->getSemanticFactId());
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

void accumulateOverlayStats(ImplicitSplitOverlayStats& total, const ImplicitSplitOverlayStats& iter) {
    total.aliasesCreated += iter.aliasesCreated;
    total.edgesAliased += iter.edgesAliased;
    total.allFactsRewrites += iter.allFactsRewrites;
    total.singleHyperedgeRewrites += iter.singleHyperedgeRewrites;
    total.linearTwoEdgeRewrites += iter.linearTwoEdgeRewrites;
    total.parallelEdgeRewrites += iter.parallelEdgeRewrites;
    total.fanOutConvergeRewrites += iter.fanOutConvergeRewrites;
    total.removedEdges += iter.removedEdges;
    total.factOutputsFolded += iter.factOutputsFolded;
    total.splitFactsConsidered += iter.splitFactsConsidered;
    total.splitFactsAliased += iter.splitFactsAliased;
    total.splitFactsCacheHits += iter.splitFactsCacheHits;
    total.splitNaiveReachabilityRuns += iter.splitNaiveReachabilityRuns;
    total.splitNaiveReachabilityVisited += iter.splitNaiveReachabilityVisited;
    total.splitNaiveCapSkips += iter.splitNaiveCapSkips;
    total.rebuildIndexCount += iter.rebuildIndexCount;
    total.fastPathIterations += iter.fastPathIterations;
    total.splitNaiveMs += iter.splitNaiveMs;
    total.splitCompleteMs += iter.splitCompleteMs;
    total.splitAliasApplyMs += iter.splitAliasApplyMs;
    total.rebuildIndexMs += iter.rebuildIndexMs;
    total.fastPathDetectMs += iter.fastPathDetectMs;
    total.fastPathSummarizeMs += iter.fastPathSummarizeMs;
    total.fastPathSingleMs += iter.fastPathSingleMs;
    total.fastPathLinearMs += iter.fastPathLinearMs;
    total.fastPathParallelMs += iter.fastPathParallelMs;
    total.fastPathFanOutMs += iter.fastPathFanOutMs;
    total.fastPathAllFactsMs += iter.fastPathAllFactsMs;
}

void runOverlayRounds(ImplicitSplitOverlay& overlay, const ImplicitSplitPipelineOptions& options,
        ImplicitSplitPipelineStats& stats) {
    const std::size_t maxRounds =
            options.iterateSplitRewrite ? std::max<std::size_t>(1, options.maxOuterIterations) : 1;
    for (std::size_t round = 0; round < maxRounds; ++round) {
        ++stats.outerIterations;
        ImplicitSplitOverlayStats roundStats;

        const auto splitStart = Clock::now();
        const bool splitChanged = overlay.applySplit(options.splitMode, &roundStats);
        const auto splitMs = elapsedMs(splitStart);

        const auto fastPathStart = Clock::now();
        bool fastPathChanged = false;
        if (options.runOverlayFastPaths) {
            fastPathChanged = overlay.rewriteFastPathsToFixpoint(
                    options.runOverlaySingleHyperedge, options.runOverlayLinearTwoEdge,
                    options.runOverlayParallelEdge, options.runOverlayFanOutConverge,
                    options.runOverlayAllFacts, &roundStats);
        }
        const auto fastPathMs = elapsedMs(fastPathStart);

        stats.overlaySplitMs += splitMs;
        stats.overlayFastPathMs += fastPathMs;
        stats.overlayPrepMs += splitMs + fastPathMs;
        accumulateOverlayStats(stats.overlayStats, roundStats);

        if (!options.iterateSplitRewrite || !splitChanged) {
            break;
        }
    }
}

ImplicitSplitPipelineResult runImplicitSplitRewritePipeline(
        const IncrementalDerivationGraphViewInterface& view,
        const ImplicitSplitPipelineOptions& options) {
    ImplicitSplitPipelineResult result;
    const auto totalStart = Clock::now();

    const auto overlayCreateStart = Clock::now();
    ImplicitSplitOverlay overlay(view);
    result.stats.overlayPrepMs = elapsedMs(overlayCreateStart);

    runOverlayRounds(overlay, options, result.stats);

    const auto directOutputs = overlay.collectDirectOutputProbabilities();
    result.directOutputs = directOutputs;
    result.factCommits = overlay.collectFactCommits();
    const bool needsEdgeCommits = result.stats.overlayStats.singleHyperedgeRewrites > 0 ||
            result.stats.overlayStats.linearTwoEdgeRewrites > 0 ||
            result.stats.overlayStats.parallelEdgeRewrites > 0 ||
            result.stats.overlayStats.fanOutConvergeRewrites > 0;
    if (needsEdgeCommits) {
        result.edgeCommits = overlay.collectEdgeCommits();
    }
    result.inactiveBaseEdges = overlay.collectInactiveBaseEdges();
    const auto overlayGraphStats = overlay.computeStats();
    const bool allOutputsDirect = directOutputs.size() == overlay.getOutputs().size();
    const bool needsResidualGraph = overlayGraphStats.activeEdges > 0 || !allOutputsDirect;
    result.needsResidualGraph = needsResidualGraph;
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
        result.stats.totalMs = elapsedMs(totalStart);
        return result;
    }

    const auto& overlayStats = result.stats.overlayStats;
    const bool canCommitOverlayInPlace =
            !result.factCommits.empty() || !result.edgeCommits.empty() || !result.inactiveBaseEdges.empty();
    if (canCommitOverlayInPlace && !options.computeOutputMarginals && !options.collectPatternStats) {
        precomputedProbResult.clear();
        precomputedTupleProbResult.clear();
        result.outputProbabilities = directOutputs;
        result.stats.totalMs = elapsedMs(totalStart);
        return result;
    }

    std::unordered_set<NodePtr> skippedDirectOutputs;
    skippedDirectOutputs.reserve(directOutputs.size());
    result.carriedPrecomputedTupleProbs.reserve(directOutputs.size());
    precomputedTupleProbResult.clear();
    for (const auto& output : directOutputs) {
        skippedDirectOutputs.insert(output.output);
        result.carriedPrecomputedTupleProbs.emplace_back(output.output->getTuple().toString(), output.probability);
    }
    for (const auto& [tupleStr, prob] : result.carriedPrecomputedTupleProbs) {
        precomputedTupleProbResult.emplace(tupleStr, prob);
    }

    const auto materializeStart = Clock::now();
    result.materialized = overlay.materializeToGraph(
            skippedDirectOutputs.empty() ? nullptr : &skippedDirectOutputs);
    result.stats.materializeMs = elapsedMs(materializeStart);
    result.stats.materializedAliasNodes = result.materialized.aliasNodes;

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

}  // namespace souffle::problog

namespace souffle::problog {
namespace {

struct JsonBenchmarkOptions {
    std::string jsonPath;
    bool skipLegacyNoSplit = false;
    bool skipLegacySplit = false;
    bool skipImplicit = false;
    bool runtimeLikeImplicit = false;
    bool iterateSplitRewrite = false;
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
              << " [--iterate-split-rewrite]"
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
        } else if (arg == "--iterate-split-rewrite") {
            opt.iterateSplitRewrite = true;
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
        const std::string& jsonPath, ImplicitSplitMode splitMode, bool runtimeLike,
        bool iterateSplitRewrite, double* loadMsOut) {
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
    options.iterateSplitRewrite = iterateSplitRewrite;
    if (runtimeLike) {
        options.runOverlayFastPaths = false;
        options.collectPatternStats = false;
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
              << " overlay_siso_detect_ms=" << stats.overlayStats.fastPathDetectMs
              << " overlay_siso_summarize_ms=" << stats.overlayStats.fastPathSummarizeMs
              << " materialize_ms=" << stats.materializeMs
              << " detect_ms=" << stats.graphDetectMs
              << " graph_rewrite_ms=" << stats.graphRewriteMs
              << " overlay_aliases=" << stats.overlayStats.aliasesCreated
              << " overlay_edges_aliased=" << stats.overlayStats.edgesAliased
              << " overlay_all_facts=" << stats.overlayStats.allFactsRewrites
              << " overlay_single=" << stats.overlayStats.singleHyperedgeRewrites
              << " overlay_linear=" << stats.overlayStats.linearTwoEdgeRewrites
              << " overlay_parallel=" << stats.overlayStats.parallelEdgeRewrites
              << " overlay_fan_out=" << stats.overlayStats.fanOutConvergeRewrites
              << " split_facts=" << stats.overlayStats.splitFactsConsidered
              << " split_facts_aliased=" << stats.overlayStats.splitFactsAliased
              << " split_cache_hits=" << stats.overlayStats.splitFactsCacheHits
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
              << " fastpath_linear_ms=" << stats.overlayStats.fastPathLinearMs
              << " fastpath_parallel_ms=" << stats.overlayStats.fastPathParallelMs
              << " fastpath_fan_out_ms=" << stats.overlayStats.fastPathFanOutMs
              << " fastpath_allfacts_ms=" << stats.overlayStats.fastPathAllFactsMs
              << " materialized_alias_nodes=" << stats.materializedAliasNodes
              << " nodes_before=" << stats.materializedNodesBefore
              << " edges_before=" << stats.materializedEdgesBefore
              << " nodes_after=" << stats.materializedNodesAfter
              << " edges_after=" << stats.materializedEdgesAfter
              << " graph_iterations=" << stats.graphRewriteStats.numIterations
              << " graph_regions=" << stats.graphRewriteStats.numRegionsRewritten
              << " graph_general_regions=" << stats.graphRewriteStats.numGeneralRegionsRewritten
              << " graph_rv_before=" << stats.graphRewriteStats.randomVarsBefore
              << " graph_rv_after=" << stats.graphRewriteStats.randomVarsAfter
              << " graph_detect_total_ms=" << stats.graphRewriteStats.totalDetectMs
              << " graph_bdd_manager_init_ms=" << stats.graphRewriteStats.totalBddManagerInitMs
              << " graph_bdd_compile_ms=" << stats.graphRewriteStats.totalBddBuildMs
              << " graph_bdd_wmc_ms=" << stats.graphRewriteStats.totalBddWmcMs
              << " graph_apply_ms=" << stats.graphRewriteStats.totalApplyMs
              << " before=" << summarizeRewritePatternCounts(stats.materializedDetectedBefore)
              << " after=" << summarizeRewritePatternCounts(stats.materializedDetectedAfter)
              << "\n";

    auto countSemanticRandomVarsInView = [](const IncrementalDerivationGraphViewInterface& view) {
        std::size_t out = 0;
        std::unordered_set<std::size_t> seenFacts;
        for (const auto& node : view.getNodes()) {
            if (node && node->isFact) {
                const double p = node->getProbability();
                if (p > 0.0 && p < 1.0 &&
                        seenFacts.insert(canonicalFactIdForGraphNode(node)).second) {
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
        std::unordered_set<std::size_t> seenFacts;
        for (const auto& node : comp.nodes) {
            if (node && node->isFact) {
                const double p = node->getProbability();
                if (p > 0.0 && p < 1.0 &&
                        seenFacts.insert(canonicalFactIdForGraphNode(node)).second) {
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
                    opt.jsonPath, opt.splitMode, opt.runtimeLikeImplicit, opt.iterateSplitRewrite, &loadMs);
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
    OverlayLinearTwoEdge,
    OverlayParallelEdge,
    OverlayFanOutConverge,
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

NodePtr makeSmokeFact(IncrementalDerivationGraph& graph, const UntypedTuple& tuple, double probability) {
    NodePtr fact = graph.createNode(tuple, probability);
    fact->isFact = true;
    fact->setOriginalFact(true);
    return fact;
}

std::vector<ExampleCase> buildExamples() {
    return {
            ExampleCase{
                    "disjoint_all_facts",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = makeSmokeFact(graph, UntypedTuple{"f", {1}}, 0.4);
                        NodePtr g = makeSmokeFact(graph, UntypedTuple{"g", {1}}, 0.7);
                        NodePtr h = makeSmokeFact(graph, UntypedTuple{"h", {1}}, 0.9);
                        NodePtr q1 = graph.createNode(UntypedTuple{"q1", {1}}, 1.0);
                        NodePtr q2 = graph.createNode(UntypedTuple{"q2", {1}}, 1.0);
                        q1->setQuery();
                        q2->setQuery();
                        graph.createHyperedge({f, g}, q1);
                        graph.createHyperedge({f, h}, q2);
                        outputs = {q1, q2};
                    },
                    true,
                    ExpectedPattern::None,
            },
            ExampleCase{
                    "all_facts_single_region",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr a = makeSmokeFact(graph, UntypedTuple{"af", {1}}, 0.4);
                        NodePtr b = makeSmokeFact(graph, UntypedTuple{"bf", {1}}, 0.7);
                        NodePtr q = graph.createNode(UntypedTuple{"aq", {1}}, 1.0);
                        q->setQuery();
                        graph.createHyperedge({a, b}, q);
                        outputs = {q};
                    },
                    false,
                    ExpectedPattern::OverlayAllFacts,
            },
            ExampleCase{
                    "fanout_reconverge_no_split",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = makeSmokeFact(graph, UntypedTuple{"f", {1}}, 0.35);
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
                        NodePtr f = makeSmokeFact(graph, UntypedTuple{"f", {1}}, 1.0);
                        NodePtr u = makeSmokeFact(graph, UntypedTuple{"u", {1}}, 0.3);
                        NodePtr v = makeSmokeFact(graph, UntypedTuple{"v", {1}}, 0.5);
                        NodePtr w = makeSmokeFact(graph, UntypedTuple{"w", {1}}, 0.6);
                        NodePtr x = makeSmokeFact(graph, UntypedTuple{"x", {1}}, 0.2);

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
                        NodePtr f = makeSmokeFact(graph, UntypedTuple{"lf", {1}}, 0.4);
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
                    ExpectedPattern::OverlayLinearTwoEdge,
            },
            ExampleCase{
                    "parallel_edge",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = makeSmokeFact(graph, UntypedTuple{"pf", {1}}, 0.5);
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
                    ExpectedPattern::OverlayParallelEdge,
            },
            ExampleCase{
                    "fan_out_converge",
                    [](IncrementalDerivationGraph& graph, std::vector<NodePtr>& outputs) {
                        NodePtr f = makeSmokeFact(graph, UntypedTuple{"ff", {1}}, 0.5);
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
                    ExpectedPattern::OverlayFanOutConverge,
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
            case ExpectedPattern::OverlayLinearTwoEdge:
                require(implicitStats.overlayStats.linearTwoEdgeRewrites > 0,
                        ex.name + ": expected overlay linear-two-edge rewrite");
                break;
            case ExpectedPattern::OverlayParallelEdge:
                require(implicitStats.overlayStats.parallelEdgeRewrites > 0,
                        ex.name + ": expected overlay parallel-edge rewrite");
                break;
            case ExpectedPattern::OverlayFanOutConverge:
                require(implicitStats.overlayStats.fanOutConvergeRewrites > 0,
                        ex.name + ": expected overlay fan-out-converge rewrite");
                break;
            case ExpectedPattern::None:
                break;
            }

            std::cout << "[implicit-split-smoke] case=" << ex.name
                      << " explicit_regions=" << explicitStats.numRegionsRewritten
                      << " implicit_aliases=" << implicitStats.overlayStats.aliasesCreated
                      << " implicit_all_facts=" << implicitStats.overlayStats.allFactsRewrites
                      << " implicit_single=" << implicitStats.overlayStats.singleHyperedgeRewrites
                      << " implicit_linear=" << implicitStats.overlayStats.linearTwoEdgeRewrites
                      << " implicit_parallel=" << implicitStats.overlayStats.parallelEdgeRewrites
                      << " implicit_fan_out=" << implicitStats.overlayStats.fanOutConvergeRewrites
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
