#include "souffle/problog/scbf/ScbfFormula.h"

#ifdef SOUFFLE_SCBF_FORMULA_BUNDLE_IR
#include "problog/scbf/ScbfIr.cpp"
#endif

#include <algorithm>
#include <fstream>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace souffle::problog::scbf {
namespace {

std::string formulaNodeSortKey(const NodePtr& node) {
    return node ? node->getTuple().toString() + "#" + std::to_string(node->getId()) : "<null>";
}

std::vector<NodePtr> sortFormulaNodes(const std::unordered_set<NodePtr>& nodes) {
    std::vector<NodePtr> result(nodes.begin(), nodes.end());
    std::sort(result.begin(), result.end(), [](const NodePtr& a, const NodePtr& b) {
        return formulaNodeSortKey(a) < formulaNodeSortKey(b);
    });
    return result;
}

std::vector<EdgePtr> sortFormulaEdges(const std::unordered_set<EdgePtr>& edges) {
    std::vector<EdgePtr> result(edges.begin(), edges.end());
    std::sort(result.begin(), result.end(), [](const EdgePtr& a, const EdgePtr& b) {
        const std::size_t aid = a ? a->getId() : 0;
        const std::size_t bid = b ? b->getId() : 0;
        if (aid != bid) {
            return aid < bid;
        }
        return (a && b) ? (a->toString() < b->toString()) : (a.get() < b.get());
    });
    return result;
}

void collectTargetSlice(const DerivationGraphViewInterface& view, const ScbfProgram& program, std::size_t cycleId,
        const NodePtr& target, std::unordered_set<NodePtr>& localNodes, std::unordered_set<EdgePtr>& localEdges,
        std::unordered_set<NodePtr>& importNodes) {
    if (!target) {
        return;
    }

    std::queue<NodePtr> pending;
    localNodes.insert(target);
    pending.push(target);

    while (!pending.empty()) {
        const NodePtr node = pending.front();
        pending.pop();
        const auto& incoming = view.getIncomingEdges(node);
        for (const auto& edge : incoming) {
            auto itEdgeCycle = program.edgeToCycleId.find(edge);
            if (itEdgeCycle == program.edgeToCycleId.end() || itEdgeCycle->second != cycleId) {
                continue;
            }
            localEdges.insert(edge);
            const auto& inputs = view.getInputs(edge);
            for (const auto& input : inputs) {
                auto itNodeCycle = program.nodeToCycleId.find(input);
                if (itNodeCycle != program.nodeToCycleId.end() && itNodeCycle->second == cycleId) {
                    if (localNodes.insert(input).second) {
                        pending.push(input);
                    }
                    continue;
                }
                if (itNodeCycle != program.nodeToCycleId.end()) {
                    importNodes.insert(input);
                    continue;
                }
                if (input && input->isFact) {
                    importNodes.insert(input);
                }
            }
        }
    }
}

bool inUnitInterval(double p) {
    return p >= 0.0 && p <= 1.0;
}

std::string jsonEscape(const std::string& in) {
    std::ostringstream oss;
    for (char c : in) {
        switch (c) {
            case '\\':
                oss << "\\\\";
                break;
            case '\"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
        }
    }
    return oss.str();
}

std::string sourceName(ScbfLiteralSource source) {
    switch (source) {
        case ScbfLiteralSource::LocalNode:
            return "local";
        case ScbfLiteralSource::ImportNode:
            return "import";
        case ScbfLiteralSource::Constant:
            return "constant";
    }
    return "unknown";
}

std::string nodeDisplay(const NodePtr& node) {
    if (!node) {
        return "<null>";
    }
    return node->getTuple().toString() + "#" + std::to_string(node->getId());
}

std::string dotEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        if (c == '\"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

}  // namespace

ScbfStratumFormulaBundle buildScbfStratumFormulaBundle(
        const DerivationGraphViewInterface& view,
        const ScbfProgram& program,
        std::size_t cycleId) {
    ScbfStratumFormulaBundle bundle;
    bundle.cycleId = cycleId;
    bundle.allowCrossTargetSharing = false;

    auto itTopo = program.cycleIdToTopoIndex.find(cycleId);
    if (itTopo == program.cycleIdToTopoIndex.end()) {
        return bundle;
    }
    bundle.topoIndex = itTopo->second;

    const auto plan = buildScbfFormulaArenaPlan(view, program, cycleId);
    for (const auto& targetPlan : plan.targets) {
        if (!targetPlan.target) {
            continue;
        }
        std::unordered_set<NodePtr> localNodeSet;
        std::unordered_set<EdgePtr> localEdgeSet;
        std::unordered_set<NodePtr> importNodeSet;
        collectTargetSlice(view, program, cycleId, targetPlan.target, localNodeSet, localEdgeSet, importNodeSet);
        if (localNodeSet.empty()) {
            localNodeSet.insert(targetPlan.target);
        }

        ScbfTargetFormula targetFormula;
        targetFormula.target = targetPlan.target;

        const auto localNodes = sortFormulaNodes(localNodeSet);
        const auto localEdges = sortFormulaEdges(localEdgeSet);
        const auto importNodes = sortFormulaNodes(importNodeSet);

        targetFormula.localNodes.reserve(localNodes.size());
        targetFormula.localNodeRules.resize(localNodes.size());
        std::unordered_map<NodePtr, std::size_t> localIndex;
        localIndex.reserve(localNodes.size());
        for (std::size_t i = 0; i < localNodes.size(); ++i) {
            const auto& node = localNodes[i];
            targetFormula.localNodes.push_back(ScbfLocalNodeDef{
                    node, node && node->isFact, node ? node->getProbability() : 0.0, node && node->needOutput});
            localIndex[node] = i;
            if (node == targetPlan.target) {
                targetFormula.targetLocalIndex = i;
            }
        }

        targetFormula.imports.reserve(importNodes.size());
        std::unordered_map<NodePtr, std::size_t> importIndex;
        importIndex.reserve(importNodes.size());
        for (std::size_t i = 0; i < importNodes.size(); ++i) {
            const auto& node = importNodes[i];
            std::size_t producerCycle = 0;
            auto itNodeCycle = program.nodeToCycleId.find(node);
            if (itNodeCycle != program.nodeToCycleId.end()) {
                producerCycle = itNodeCycle->second;
            }
            targetFormula.imports.push_back(ScbfImportDef{node, producerCycle});
            importIndex[node] = i;
        }

        for (const auto& edge : localEdges) {
            NodePtr output = view.getOutput(edge);
            auto itHead = localIndex.find(output);
            if (itHead == localIndex.end()) {
                continue;
            }

            ScbfRuleEquation rule;
            rule.sourceEdge = edge;
            rule.deterministic = edge ? edge->isDeterministic() : true;
            rule.edgeProbability = edge ? (edge->isDeterministic() ? 1.0 : edge->getProbability()) : 1.0;

            const auto& inputs = view.getInputs(edge);
            const auto& negs = view.getBodyNegations(edge);
            for (std::size_t i = 0; i < inputs.size(); ++i) {
                const auto& input = inputs[i];
                ScbfLiteralRef lit;
                lit.negated = i < negs.size() ? negs[i] : false;

                auto itLocal = localIndex.find(input);
                if (itLocal != localIndex.end()) {
                    lit.source = ScbfLiteralSource::LocalNode;
                    lit.index = itLocal->second;
                } else {
                    auto itImport = importIndex.find(input);
                    if (itImport != importIndex.end()) {
                        lit.source = ScbfLiteralSource::ImportNode;
                        lit.index = itImport->second;
                    } else if (input && input->isFact) {
                        lit.source = ScbfLiteralSource::Constant;
                        lit.constantProbability = input->getProbability();
                    } else {
                        lit.source = ScbfLiteralSource::Constant;
                        lit.constantProbability = 0.0;
                    }
                }
                rule.bodyLiterals.push_back(lit);
            }
            targetFormula.localNodeRules[itHead->second].push_back(std::move(rule));
        }

        bundle.targets.push_back(std::move(targetFormula));
    }

    return bundle;
}

bool validateScbfStratumFormulaBundle(const DerivationGraphViewInterface& view, const ScbfProgram& program,
        const ScbfStratumFormulaBundle& bundle, std::string* errorMessage) {
    auto fail = [&](const std::string& msg) {
        if (errorMessage) {
            *errorMessage = msg;
        }
        return false;
    };

    auto itTopo = program.cycleIdToTopoIndex.find(bundle.cycleId);
    if (itTopo == program.cycleIdToTopoIndex.end()) {
        return fail("bundle cycleId not present in program");
    }
    if (itTopo->second != bundle.topoIndex) {
        return fail("bundle topoIndex mismatch");
    }

    for (const auto& target : bundle.targets) {
        if (!target.target) {
            return fail("target node is null");
        }
        if (target.localNodes.empty()) {
            return fail("target has no local nodes");
        }
        if (target.targetLocalIndex >= target.localNodes.size()) {
            return fail("targetLocalIndex out of range");
        }
        if (target.localNodeRules.size() != target.localNodes.size()) {
            return fail("localNodeRules size mismatch with localNodes");
        }
        if (target.localNodes[target.targetLocalIndex].node != target.target) {
            return fail("targetLocalIndex does not point to target node");
        }

        std::unordered_map<NodePtr, std::size_t> localIndex;
        for (std::size_t i = 0; i < target.localNodes.size(); ++i) {
            localIndex[target.localNodes[i].node] = i;
            auto itNodeCycle = program.nodeToCycleId.find(target.localNodes[i].node);
            if (itNodeCycle == program.nodeToCycleId.end() || itNodeCycle->second != bundle.cycleId) {
                return fail("local node assigned to non-local cycle");
            }
            if (target.localNodes[i].isFact && !inUnitInterval(target.localNodes[i].factProbability)) {
                return fail("fact probability out of [0,1]");
            }
        }

        for (const auto& import : target.imports) {
            if (!import.node) {
                return fail("import node is null");
            }
            auto itImportTopo = program.cycleIdToTopoIndex.find(import.producerCycleId);
            if (itImportTopo == program.cycleIdToTopoIndex.end()) {
                return fail("import producer cycle missing");
            }
            if (itImportTopo->second >= bundle.topoIndex) {
                return fail("import producer is not from an earlier stratum");
            }
        }

        for (std::size_t head = 0; head < target.localNodeRules.size(); ++head) {
            const NodePtr headNode = target.localNodes[head].node;
            for (const auto& rule : target.localNodeRules[head]) {
                if (!rule.sourceEdge) {
                    return fail("rule sourceEdge is null");
                }
                auto itEdgeCycle = program.edgeToCycleId.find(rule.sourceEdge);
                if (itEdgeCycle == program.edgeToCycleId.end() || itEdgeCycle->second != bundle.cycleId) {
                    return fail("rule edge not in bundle cycle");
                }
                if (view.getOutput(rule.sourceEdge) != headNode) {
                    return fail("rule head/output mismatch");
                }
                if (!rule.deterministic && !inUnitInterval(rule.edgeProbability)) {
                    return fail("rule edge probability out of [0,1]");
                }
                for (const auto& lit : rule.bodyLiterals) {
                    if (lit.source == ScbfLiteralSource::LocalNode) {
                        if (lit.index >= target.localNodes.size()) {
                            return fail("local literal index out of range");
                        }
                    } else if (lit.source == ScbfLiteralSource::ImportNode) {
                        if (lit.index >= target.imports.size()) {
                            return fail("import literal index out of range");
                        }
                    } else if (!inUnitInterval(lit.constantProbability)) {
                        return fail("constant literal probability out of [0,1]");
                    }
                }
            }
        }
    }
    return true;
}

std::string summarizeScbfStratumFormulaBundle(const ScbfStratumFormulaBundle& bundle) {
    std::size_t totalLocalNodes = 0;
    std::size_t totalImports = 0;
    std::size_t totalRules = 0;
    std::size_t totalLiterals = 0;
    for (const auto& target : bundle.targets) {
        totalLocalNodes += target.localNodes.size();
        totalImports += target.imports.size();
        for (const auto& rules : target.localNodeRules) {
            totalRules += rules.size();
            for (const auto& rule : rules) {
                totalLiterals += rule.bodyLiterals.size();
            }
        }
    }

    std::ostringstream oss;
    oss << "[scbf-formula] cycle=" << bundle.cycleId
        << " topo=" << bundle.topoIndex
        << " targets=" << bundle.targets.size()
        << " local_nodes=" << totalLocalNodes
        << " imports=" << totalImports
        << " rules=" << totalRules
        << " literals=" << totalLiterals
        << " cross_target_sharing=" << (bundle.allowCrossTargetSharing ? 1 : 0);
    return oss.str();
}

std::string toScbfStratumFormulaBundleJson(const ScbfStratumFormulaBundle& bundle) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"cycleId\":" << bundle.cycleId << ",";
    oss << "\"topoIndex\":" << bundle.topoIndex << ",";
    oss << "\"allowCrossTargetSharing\":" << (bundle.allowCrossTargetSharing ? "true" : "false") << ",";
    oss << "\"targets\":[";
    for (std::size_t ti = 0; ti < bundle.targets.size(); ++ti) {
        const auto& target = bundle.targets[ti];
        if (ti > 0) {
            oss << ",";
        }
        oss << "{";
        oss << "\"target\":\"" << jsonEscape(nodeDisplay(target.target)) << "\",";
        oss << "\"targetLocalIndex\":" << target.targetLocalIndex << ",";
        oss << "\"localNodes\":[";
        for (std::size_t i = 0; i < target.localNodes.size(); ++i) {
            const auto& local = target.localNodes[i];
            if (i > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"index\":" << i << ",";
            oss << "\"node\":\"" << jsonEscape(nodeDisplay(local.node)) << "\",";
            oss << "\"isFact\":" << (local.isFact ? "true" : "false") << ",";
            oss << "\"factProbability\":" << local.factProbability << ",";
            oss << "\"needOutput\":" << (local.needOutput ? "true" : "false");
            oss << "}";
        }
        oss << "],";
        oss << "\"imports\":[";
        for (std::size_t i = 0; i < target.imports.size(); ++i) {
            const auto& imp = target.imports[i];
            if (i > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"index\":" << i << ",";
            oss << "\"node\":\"" << jsonEscape(nodeDisplay(imp.node)) << "\",";
            oss << "\"producerCycleId\":" << imp.producerCycleId;
            oss << "}";
        }
        oss << "],";
        oss << "\"rules\":[";
        bool firstRule = true;
        for (std::size_t head = 0; head < target.localNodeRules.size(); ++head) {
            const auto& rules = target.localNodeRules[head];
            for (std::size_t ri = 0; ri < rules.size(); ++ri) {
                if (!firstRule) {
                    oss << ",";
                }
                firstRule = false;
                const auto& rule = rules[ri];
                oss << "{";
                oss << "\"headLocalIndex\":" << head << ",";
                oss << "\"sourceEdgeId\":" << (rule.sourceEdge ? rule.sourceEdge->getId() : 0) << ",";
                oss << "\"deterministic\":" << (rule.deterministic ? "true" : "false") << ",";
                oss << "\"edgeProbability\":" << rule.edgeProbability << ",";
                oss << "\"literals\":[";
                for (std::size_t li = 0; li < rule.bodyLiterals.size(); ++li) {
                    const auto& lit = rule.bodyLiterals[li];
                    if (li > 0) {
                        oss << ",";
                    }
                    oss << "{";
                    oss << "\"source\":\"" << sourceName(lit.source) << "\",";
                    oss << "\"index\":" << lit.index << ",";
                    oss << "\"negated\":" << (lit.negated ? "true" : "false") << ",";
                    oss << "\"constantProbability\":" << lit.constantProbability;
                    oss << "}";
                }
                oss << "]";
                oss << "}";
            }
        }
        oss << "]";
        oss << "}";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

bool dumpScbfStratumFormulaBundleJson(
        const ScbfStratumFormulaBundle& bundle, const std::string& outputPath) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }
    out << toScbfStratumFormulaBundleJson(bundle) << "\n";
    out.close();
    return true;
}

bool dumpScbfStratumFormulaBundleDot(
        const ScbfStratumFormulaBundle& bundle, const std::string& outputPath) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }
    out << "digraph scbf_bundle_" << bundle.cycleId << " {\n";
    out << "  rankdir=LR;\n";
    out << "  graph [label=\"SCBF cycle " << bundle.cycleId << " topo " << bundle.topoIndex
        << "\", labelloc=t, fontsize=20];\n";

    for (std::size_t ti = 0; ti < bundle.targets.size(); ++ti) {
        const auto& target = bundle.targets[ti];
        out << "  subgraph cluster_t" << ti << " {\n";
        out << "    label=\"target " << dotEscape(nodeDisplay(target.target)) << "\";\n";
        out << "    color=lightgrey;\n";
        for (std::size_t i = 0; i < target.localNodes.size(); ++i) {
            const auto& local = target.localNodes[i];
            out << "    t" << ti << "_n" << i << " [shape=ellipse,label=\"L" << i << ":"
                << dotEscape(nodeDisplay(local.node)) << "\\n"
                << (local.isFact ? "fact" : "derived")
                << (i == target.targetLocalIndex ? "\\nTARGET" : "") << "\"];\n";
        }
        for (std::size_t i = 0; i < target.imports.size(); ++i) {
            const auto& imp = target.imports[i];
            out << "    t" << ti << "_i" << i << " [shape=box,style=dashed,label=\"I" << i << ":"
                << dotEscape(nodeDisplay(imp.node)) << "\\nfrom cycle " << imp.producerCycleId << "\"];\n";
        }
        for (std::size_t head = 0; head < target.localNodeRules.size(); ++head) {
            const auto& rules = target.localNodeRules[head];
            for (std::size_t ri = 0; ri < rules.size(); ++ri) {
                const auto& rule = rules[ri];
                out << "    t" << ti << "_r" << head << "_" << ri
                    << " [shape=diamond,label=\"R h=" << head
                    << "\\np=" << rule.edgeProbability << "\"];\n";
                out << "    t" << ti << "_r" << head << "_" << ri
                    << " -> t" << ti << "_n" << head << ";\n";
                for (std::size_t li = 0; li < rule.bodyLiterals.size(); ++li) {
                    const auto& lit = rule.bodyLiterals[li];
                    std::string litLabel = lit.negated ? "!" : "";
                    if (lit.source == ScbfLiteralSource::LocalNode) {
                        out << "    t" << ti << "_n" << lit.index
                            << " -> t" << ti << "_r" << head << "_" << ri
                            << " [label=\"" << litLabel << "\"];\n";
                    } else if (lit.source == ScbfLiteralSource::ImportNode) {
                        out << "    t" << ti << "_i" << lit.index
                            << " -> t" << ti << "_r" << head << "_" << ri
                            << " [label=\"" << litLabel << "\"];\n";
                    } else {
                        out << "    t" << ti << "_c" << head << "_" << ri << "_" << li
                            << " [shape=plaintext,label=\"const "
                            << (lit.negated ? (1.0 - lit.constantProbability) : lit.constantProbability)
                            << "\"];\n";
                        out << "    t" << ti << "_c" << head << "_" << ri << "_" << li
                            << " -> t" << ti << "_r" << head << "_" << ri << ";\n";
                    }
                }
            }
        }
        out << "  }\n";
    }

    out << "}\n";
    out.close();
    return true;
}

}  // namespace souffle::problog::scbf

#ifdef SOUFFLE_SCBF_FORMULA_SMOKE_MAIN
using namespace souffle::problog;
using namespace souffle::problog::scbf;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

NodePtr findNodeByRelation(const DerivationGraphViewInterface& view, const std::string& relation) {
    for (const auto& node : view.getNodes()) {
        if (node && node->getTuple().relation_name == relation) {
            return node;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    try {
        DerivationGraph graph;
        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 0.8);
        f->isFact = true;
        NodePtr a = graph.createNode(UntypedTuple{"a", {1}}, 1.0);
        NodePtr b = graph.createNode(UntypedTuple{"b", {1}}, 1.0);
        NodePtr c = graph.createNode(UntypedTuple{"c", {1}}, 1.0);
        c->setQuery();

        EdgePtr e1 = graph.createHyperedge({f}, a);
        EdgePtr e2 = graph.createHyperedge({a}, b);
        EdgePtr e3 = graph.createHyperedge({b}, a);
        EdgePtr e4 = graph.createHyperedge({b}, c);
        require(e1 && e2 && e3 && e4, "failed to create smoke graph");
        e1->setProbability(0.9);
        e2->setProbability(0.7);
        e3->setProbability(0.4);
        e4->setProbability(0.5);

        ScbfProgram program = buildScbfProgram(graph);
        std::string err;
        require(validateScbfProgram(graph, program, &err), "invalid SCBF program: " + err);

        NodePtr nodeC = findNodeByRelation(graph, "c");
        require(nodeC != nullptr, "node c not found");
        std::size_t cycleC = program.nodeToCycleId.at(nodeC);

        auto bundleC = buildScbfStratumFormulaBundle(graph, program, cycleC);
        require(!bundleC.allowCrossTargetSharing, "cross-target sharing must stay disabled");
        require(!bundleC.targets.empty(), "bundleC has no targets");
        require(validateScbfStratumFormulaBundle(graph, program, bundleC, &err),
                "invalid SCBF formula bundle: " + err);

        bool hasImport = false;
        bool hasLocalRule = false;
        for (const auto& target : bundleC.targets) {
            if (target.target != nodeC) {
                continue;
            }
            hasImport = !target.imports.empty();
            const auto& rules = target.localNodeRules[target.targetLocalIndex];
            hasLocalRule = !rules.empty();
            break;
        }
        require(hasImport, "c target should have at least one import");
        require(hasLocalRule, "c target should have at least one local rule");

        std::cout << summarizeScbfProgram(program) << "\n";
        std::cout << summarizeScbfStratumFormulaBundle(bundleC) << "\n";
        std::cout << "[scbf-formula-smoke] ok cycle=" << cycleC
                  << " targets=" << bundleC.targets.size() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-formula-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}
#endif
