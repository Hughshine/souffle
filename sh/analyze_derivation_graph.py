#!/usr/bin/env python3
"""
Standalone analyzer for derivation graph JSON dumps.

Supported inputs:
1) Full derivation dumps from --dumpjson:
   - derivation.json
   - derivation-*-after-prune*.json (may include "delta")
2) Lightweight graph stats files from --dumpstat:
   - graph-*.json

For full derivation dumps, the tool can run heavier "general SISO" analysis
using a dominator/support based algorithm inspired by GraphAnalyzer.h.

Interactive mode can generate a local HTML report for step-by-step browsing
of detected SISO regions.
"""

from __future__ import annotations

import argparse
import base64
import json
import random
import sys
import time
from collections import Counter, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple


def _now_ms() -> float:
    return time.perf_counter() * 1000.0


def _iter_bits(mask: int) -> Iterable[int]:
    while mask:
        lsb = mask & -mask
        yield lsb.bit_length() - 1
        mask ^= lsb


def _quantile(sorted_vals: Sequence[int], p: float) -> int:
    if not sorted_vals:
        return 0
    idx = int((len(sorted_vals) - 1) * p)
    return sorted_vals[idx]


def compute_predicate_strata_stats(
    dependencies: Iterable[Tuple[str, str, bool]],
    predicates: Optional[Iterable[str]] = None,
) -> Dict[str, object]:
    dep_set: Set[Tuple[str, str, bool]] = set()
    pred_set: Set[str] = set(predicates or [])
    for body_pred, head_pred, is_neg in dependencies:
        b = str(body_pred).strip()
        h = str(head_pred).strip()
        if not b or not h:
            continue
        dep_set.add((b, h, bool(is_neg)))
        pred_set.add(b)
        pred_set.add(h)

    if not pred_set:
        return {
            "predicate_count": 0,
            "dependency_edges": 0,
            "negative_dependency_edges": 0,
            "predicate_scc_count": 0,
            "predicate_recursive_scc_count": 0,
            "predicate_max_scc_size": 0,
            "predicate_condensation_layers": 0,
            "stratifiable": True,
            "max_stratum": -1,
            "strata_count": 0,
            "negation_max_stratum": -1,
            "negation_strata_count": 0,
        }

    pred_list = sorted(pred_set)
    idx_of = {p: i for i, p in enumerate(pred_list)}
    dep_rows = [(idx_of[b], idx_of[h], 1 if neg else 0) for (b, h, neg) in dep_set]

    succ: List[Set[int]] = [set() for _ in pred_list]
    self_loop = [False] * len(pred_list)
    for u, v, _ in dep_rows:
        succ[u].add(v)
        if u == v:
            self_loop[u] = True
    succ_list: List[List[int]] = [list(s) for s in succ]
    comps = kosaraju_components(succ_list)
    comp_id: Dict[int, int] = {}
    for cid, comp_nodes in enumerate(comps):
        for nid in comp_nodes:
            comp_id[nid] = cid
    comp_sizes = [len(c) for c in comps]
    recursive_comp_count = 0
    for cid, comp_nodes in enumerate(comps):
        if len(comp_nodes) > 1:
            recursive_comp_count += 1
            continue
        if comp_nodes and self_loop[comp_nodes[0]]:
            recursive_comp_count += 1

    comp_succ: List[Set[int]] = [set() for _ in comps]
    indeg = [0] * len(comps)
    for u, v, _ in dep_rows:
        cu = comp_id.get(u, -1)
        cv = comp_id.get(v, -1)
        if cu < 0 or cv < 0 or cu == cv:
            continue
        if cv not in comp_succ[cu]:
            comp_succ[cu].add(cv)
            indeg[cv] += 1

    # Longest-layer depth on condensation DAG (1-based if non-empty).
    comp_layer = [1] * len(comps)
    q: deque[int] = deque([i for i in range(len(comps)) if indeg[i] == 0])
    indeg_work = list(indeg)
    while q:
        u = q.popleft()
        for v in comp_succ[u]:
            cand = comp_layer[u] + 1
            if cand > comp_layer[v]:
                comp_layer[v] = cand
            indeg_work[v] -= 1
            if indeg_work[v] == 0:
                q.append(v)
    condensation_layers = max(comp_layer) if comp_layer else 0

    # Constraint system:
    #   stratum(head) >= stratum(body) + w, where w=1 for neg edges and w=0 otherwise.
    # Minimal solution exists iff there is no cycle with positive total weight.
    dist = [0] * len(pred_list)
    changed = False
    for _ in range(len(pred_list)):
        changed = False
        for u, v, w in dep_rows:
            cand = dist[u] + w
            if cand > dist[v]:
                dist[v] = cand
                changed = True
        if not changed:
            break

    stratifiable = not changed
    if stratifiable:
        negation_max_stratum = max(dist) if dist else -1
        negation_strata_count = negation_max_stratum + 1 if negation_max_stratum >= 0 else 0
    else:
        negation_max_stratum = None
        negation_strata_count = None

    # In this analyzer, `strata_count` now means predicate SCC strata count (Souffle-style component view),
    # while negation-based stratification is reported separately.
    strata_count = len(comps)
    max_stratum = strata_count - 1 if strata_count > 0 else -1

    return {
        "predicate_count": len(pred_list),
        "dependency_edges": len(dep_rows),
        "negative_dependency_edges": sum(1 for _, _, w in dep_rows if w > 0),
        "predicate_scc_count": len(comps),
        "predicate_recursive_scc_count": recursive_comp_count,
        "predicate_max_scc_size": max(comp_sizes) if comp_sizes else 0,
        "predicate_condensation_layers": condensation_layers,
        "stratifiable": stratifiable,
        "max_stratum": max_stratum,
        "strata_count": strata_count,
        "negation_max_stratum": negation_max_stratum,
        "negation_strata_count": negation_strata_count,
    }


@dataclass
class RegionResult:
    entry: str
    exit: str
    node_count: int
    edge_count: int
    random_vars: int
    entry_pred_count: int
    mode: str
    candidate_exit: str
    kind: str = "unknown"
    shape: str = "unknown"
    has_probabilistic: bool = False
    region_graph: Optional[Dict[str, object]] = None
    cone_nodes: int = 0
    cone_edges: int = 0
    cone_truncated: bool = False


SUPPORTED_SISO_KIND_ORDER = [
    "SingleHyperedge",
    "LinearTwoEdge",
    "ParallelEdge",
    "AllFactsToSO",
    "FanOutConverge",
    "General",
    "Unknown",
]


@dataclass
class MutableNode:
    id: int
    name: str
    is_fact: bool
    probability: float
    has_evidence: bool = False
    need_output: bool = False
    is_shadow: bool = False
    active: bool = True


@dataclass
class MutableEdge:
    id: int
    inputs: List[int]
    head: int
    negated_inputs: List[bool]
    probability: float
    active: bool = True


@dataclass
class SupportedSisoRegion:
    kind: str
    entry: int
    exit: int
    internal_nodes: Set[int] = field(default_factory=set)
    internal_edges: Set[int] = field(default_factory=set)


@dataclass
class RewriteIterationSummary:
    iteration: int
    before_stats: Dict[str, object]
    detected_regions: int
    detected_by_kind: Dict[str, int]
    rewritten_regions: int
    rewritten_by_kind: Dict[str, int]
    split_applied: bool
    split_mode: str
    split_nodes_added: int
    split_edges_rewritten: int
    split_time_ms: float
    compaction_applied: bool
    compact_edges: int
    compact_removed_edges: int
    compact_added_edges: int
    compact_time_ms: float
    cleanup_removed_nodes: int
    cleanup_time_ms: float
    after_stats: Dict[str, object]
    regions: List[Dict[str, object]]


class DerivationHyperGraph:
    def __init__(self) -> None:
        self.node_index: Dict[str, int] = {}
        self.node_names: List[str] = []
        self.is_fact: List[bool] = []
        self.fact_prob: List[Optional[float]] = []

        self.edge_heads: List[int] = []
        self.edge_inputs: List[List[int]] = []
        self.edge_neg: List[List[bool]] = []
        self.edge_prob: List[float] = []

        self.incoming_edges: List[List[int]] = []
        self.outgoing_edges: List[List[int]] = []

    @staticmethod
    def _is_probabilistic(p: Optional[float]) -> bool:
        if p is None:
            return False
        return 0.0 < p < 1.0

    def add_node(self, name: str) -> int:
        idx = self.node_index.get(name)
        if idx is not None:
            return idx
        idx = len(self.node_names)
        self.node_index[name] = idx
        self.node_names.append(name)
        self.is_fact.append(False)
        self.fact_prob.append(None)
        self.incoming_edges.append([])
        self.outgoing_edges.append([])
        return idx

    def add_fact(self, name: str, prob: float) -> int:
        idx = self.add_node(name)
        self.is_fact[idx] = True
        self.fact_prob[idx] = prob
        return idx

    def add_rule(self, head_name: str, prob: float, body_atoms: Sequence[Tuple[str, bool]]) -> int:
        head = self.add_node(head_name)
        inputs: List[int] = []
        negs: List[bool] = []
        for atom_name, is_neg in body_atoms:
            inputs.append(self.add_node(atom_name))
            negs.append(bool(is_neg))
        edge_id = len(self.edge_heads)
        self.edge_heads.append(head)
        self.edge_inputs.append(inputs)
        self.edge_neg.append(negs)
        self.edge_prob.append(prob)
        self.incoming_edges[head].append(edge_id)
        for inp in inputs:
            self.outgoing_edges[inp].append(edge_id)
        return edge_id

    @property
    def num_nodes(self) -> int:
        return len(self.node_names)

    @property
    def num_edges(self) -> int:
        return len(self.edge_heads)

    @classmethod
    def from_derivation_json(cls, obj: Dict[str, object]) -> "DerivationHyperGraph":
        g = cls()
        facts = obj.get("facts", [])
        if not isinstance(facts, list):
            raise ValueError("Invalid JSON: 'facts' must be a list")
        for entry in facts:
            if not isinstance(entry, dict):
                continue
            name = str(entry.get("name", "")).strip()
            if not name:
                continue
            prob_raw = entry.get("probability", 1.0)
            prob = float(prob_raw)
            g.add_fact(name, prob)

        rules = obj.get("rules", [])
        if not isinstance(rules, list):
            raise ValueError("Invalid JSON: 'rules' must be a list")
        for r in rules:
            if not isinstance(r, dict):
                continue
            head_name = str(r.get("head", "")).strip()
            if not head_name:
                continue
            prob = float(r.get("probability", 1.0))
            bodies_raw = r.get("bodies", [])
            body_atoms: List[Tuple[str, bool]] = []
            if isinstance(bodies_raw, list):
                for b in bodies_raw:
                    if not isinstance(b, dict):
                        continue
                    nm = str(b.get("name", "")).strip()
                    if not nm:
                        continue
                    body_atoms.append((nm, bool(b.get("negation", False))))
            g.add_rule(head_name, prob, body_atoms)
        return g

    def basic_stats(self) -> Dict[str, object]:
        in_deg = [len(self.incoming_edges[i]) for i in range(self.num_nodes)]
        out_deg = [len(self.outgoing_edges[i]) for i in range(self.num_nodes)]
        in_deg_sorted = sorted(in_deg)
        out_deg_sorted = sorted(out_deg)
        arities = [len(inp) for inp in self.edge_inputs]
        arities_sorted = sorted(arities)

        fact_nodes = sum(1 for x in self.is_fact if x)
        derived_nodes = self.num_nodes - fact_nodes
        prob_fact_nodes = sum(1 for p, is_fact in zip(self.fact_prob, self.is_fact) if is_fact and self._is_probabilistic(p))
        prob_edges = sum(1 for p in self.edge_prob if self._is_probabilistic(p))

        disjunction_nodes = 0
        for i in range(self.num_nodes):
            if (not self.is_fact[i] and in_deg[i] > 1) or (self.is_fact[i] and in_deg[i] > 0):
                disjunction_nodes += 1

        all_preds = {_predicate_of(name) for name in self.node_names}
        dep_rows: List[Tuple[str, str, bool]] = []
        for eid, head in enumerate(self.edge_heads):
            head_pred = _predicate_of(self.node_names[head])
            inputs = self.edge_inputs[eid]
            negs = self.edge_neg[eid]
            for i, src in enumerate(inputs):
                src_pred = _predicate_of(self.node_names[src])
                is_neg = bool(negs[i] if i < len(negs) else False)
                dep_rows.append((src_pred, head_pred, is_neg))
        strata_stats = compute_predicate_strata_stats(dep_rows, predicates=all_preds)

        return {
            "nodes": self.num_nodes,
            "edges": self.num_edges,
            "fact_nodes": fact_nodes,
            "derived_nodes": derived_nodes,
            "prob_fact_nodes": prob_fact_nodes,
            "prob_rule_edges": prob_edges,
            "random_variables": prob_fact_nodes + prob_edges,
            "disjunction_nodes": disjunction_nodes,
            "max_in_degree": max(in_deg_sorted) if in_deg_sorted else 0,
            "max_out_degree": max(out_deg_sorted) if out_deg_sorted else 0,
            "avg_in_degree": (sum(in_deg_sorted) / self.num_nodes) if self.num_nodes else 0.0,
            "avg_out_degree": (sum(out_deg_sorted) / self.num_nodes) if self.num_nodes else 0.0,
            "in_degree_p50": _quantile(in_deg_sorted, 0.50),
            "in_degree_p90": _quantile(in_deg_sorted, 0.90),
            "in_degree_p99": _quantile(in_deg_sorted, 0.99),
            "out_degree_p50": _quantile(out_deg_sorted, 0.50),
            "out_degree_p90": _quantile(out_deg_sorted, 0.90),
            "out_degree_p99": _quantile(out_deg_sorted, 0.99),
            "max_hyperedge_inputs": max(arities_sorted) if arities_sorted else 0,
            "avg_hyperedge_inputs": (sum(arities_sorted) / self.num_edges) if self.num_edges else 0.0,
            "arity_histogram_top10": Counter(arities).most_common(10),
            "predicate_count": strata_stats["predicate_count"],
            "dependency_edges": strata_stats["dependency_edges"],
            "negative_dependency_edges": strata_stats["negative_dependency_edges"],
            "predicate_scc_count": strata_stats["predicate_scc_count"],
            "predicate_recursive_scc_count": strata_stats["predicate_recursive_scc_count"],
            "predicate_max_scc_size": strata_stats["predicate_max_scc_size"],
            "predicate_condensation_layers": strata_stats["predicate_condensation_layers"],
            "stratifiable": strata_stats["stratifiable"],
            "max_stratum": strata_stats["max_stratum"],
            "strata_count": strata_stats["strata_count"],
            "negation_max_stratum": strata_stats["negation_max_stratum"],
            "negation_strata_count": strata_stats["negation_strata_count"],
        }

    def projected_adjacency(self) -> List[List[int]]:
        succ: List[Set[int]] = [set() for _ in range(self.num_nodes)]
        for eid, head in enumerate(self.edge_heads):
            for src in self.edge_inputs[eid]:
                succ[src].add(head)
        return [list(v) for v in succ]


class MutableDerivationGraph:
    def __init__(self) -> None:
        self.nodes: List[MutableNode] = []
        self.edges: List[MutableEdge] = []
        self._incoming: Dict[int, List[int]] = {}
        self._outgoing: Dict[int, List[int]] = {}
        self._cache_valid = False

    @staticmethod
    def _clamp_prob(p: float) -> float:
        if p < 0.0:
            return 0.0
        if p > 1.0:
            return 1.0
        return p

    @classmethod
    def from_immutable(cls, g: DerivationHyperGraph) -> "MutableDerivationGraph":
        out = cls()
        for nid in range(g.num_nodes):
            p = 1.0
            if g.is_fact[nid]:
                fp = g.fact_prob[nid]
                p = float(fp) if fp is not None else 1.0
            out.nodes.append(
                MutableNode(
                    id=nid,
                    name=g.node_names[nid],
                    is_fact=bool(g.is_fact[nid]),
                    probability=cls._clamp_prob(float(p)),
                )
            )
        for eid in range(g.num_edges):
            out.edges.append(
                MutableEdge(
                    id=eid,
                    inputs=list(g.edge_inputs[eid]),
                    head=g.edge_heads[eid],
                    negated_inputs=list(g.edge_neg[eid]),
                    probability=cls._clamp_prob(float(g.edge_prob[eid])),
                )
            )
        out._invalidate_cache()
        return out

    def clone(self) -> "MutableDerivationGraph":
        out = MutableDerivationGraph()
        out.nodes = [
            MutableNode(
                id=n.id,
                name=n.name,
                is_fact=n.is_fact,
                probability=n.probability,
                has_evidence=n.has_evidence,
                need_output=n.need_output,
                is_shadow=n.is_shadow,
                active=n.active,
            )
            for n in self.nodes
        ]
        out.edges = [
            MutableEdge(
                id=e.id,
                inputs=list(e.inputs),
                head=e.head,
                negated_inputs=list(e.negated_inputs),
                probability=e.probability,
                active=e.active,
            )
            for e in self.edges
        ]
        out._invalidate_cache()
        return out

    def _invalidate_cache(self) -> None:
        self._cache_valid = False

    def _ensure_cache(self) -> None:
        if self._cache_valid:
            return
        incoming: Dict[int, List[int]] = {}
        outgoing: Dict[int, List[int]] = {}
        for node in self.nodes:
            if not node.active:
                continue
            incoming[node.id] = []
            outgoing[node.id] = []
        for e in self.edges:
            if not e.active:
                continue
            if e.head in incoming:
                incoming[e.head].append(e.id)
            for src in e.inputs:
                if src in outgoing:
                    outgoing[src].append(e.id)
        self._incoming = incoming
        self._outgoing = outgoing
        self._cache_valid = True

    def active_node_ids(self) -> List[int]:
        return [n.id for n in self.nodes if n.active]

    def active_edge_ids(self) -> List[int]:
        return [e.id for e in self.edges if e.active]

    def has_node(self, nid: int) -> bool:
        return 0 <= nid < len(self.nodes) and self.nodes[nid].active

    def has_edge(self, eid: int) -> bool:
        return 0 <= eid < len(self.edges) and self.edges[eid].active

    def incoming_edges(self, nid: int) -> List[int]:
        self._ensure_cache()
        return list(self._incoming.get(nid, []))

    def outgoing_edges(self, nid: int) -> List[int]:
        self._ensure_cache()
        return list(self._outgoing.get(nid, []))

    def add_edge(self, inputs: Sequence[int], head: int, negated_inputs: Sequence[bool], probability: float) -> int:
        eid = len(self.edges)
        negs = [bool(x) for x in negated_inputs]
        if len(negs) < len(inputs):
            negs.extend([False] * (len(inputs) - len(negs)))
        elif len(negs) > len(inputs):
            negs = negs[: len(inputs)]
        self.edges.append(
            MutableEdge(
                id=eid,
                inputs=list(inputs),
                head=head,
                negated_inputs=negs,
                probability=self._clamp_prob(float(probability)),
                active=True,
            )
        )
        self._invalidate_cache()
        return eid

    def remove_edge(self, eid: int) -> bool:
        if not self.has_edge(eid):
            return False
        self.edges[eid].active = False
        self._invalidate_cache()
        return True

    def remove_node(self, nid: int) -> bool:
        if not self.has_node(nid):
            return False
        self.nodes[nid].active = False
        self._invalidate_cache()
        return True

    def add_shadow_node(self, fact_id: int, edge_id_hint: int) -> Optional[int]:
        if not self.has_node(fact_id):
            return None
        fact = self.nodes[fact_id]
        rel = _predicate_of(fact.name)
        new_id = len(self.nodes)
        name = f"Shadow_{rel}_{fact_id}_{edge_id_hint}"
        self.nodes.append(
            MutableNode(
                id=new_id,
                name=name,
                is_fact=True,
                probability=self._clamp_prob(float(fact.probability)),
                has_evidence=False,
                need_output=False,
                is_shadow=True,
                active=True,
            )
        )
        self._invalidate_cache()
        return new_id

    def count_random_vars(self) -> int:
        total = 0
        for n in self.nodes:
            if not n.active:
                continue
            if n.is_fact and DerivationHyperGraph._is_probabilistic(n.probability):
                total += 1
        for e in self.edges:
            if not e.active:
                continue
            if DerivationHyperGraph._is_probabilistic(e.probability):
                total += 1
        return total

    def is_isolated(self, nid: int) -> bool:
        if not self.has_node(nid):
            return True
        return len(self.incoming_edges(nid)) == 0 and len(self.outgoing_edges(nid)) == 0

    def to_immutable_snapshot(self) -> DerivationHyperGraph:
        out = DerivationHyperGraph()
        old_to_new: Dict[int, int] = {}
        for n in self.nodes:
            if not n.active:
                continue
            idx = out.add_node(n.name)
            old_to_new[n.id] = idx
            out.is_fact[idx] = n.is_fact
            out.fact_prob[idx] = float(n.probability) if n.is_fact else None
        for e in self.edges:
            if not e.active:
                continue
            if e.head not in old_to_new:
                continue
            mapped_inputs: List[int] = []
            mapped_neg: List[bool] = []
            for i, src in enumerate(e.inputs):
                if src not in old_to_new:
                    continue
                mapped_inputs.append(old_to_new[src])
                mapped_neg.append(bool(e.negated_inputs[i] if i < len(e.negated_inputs) else False))
            head = old_to_new[e.head]
            eid = len(out.edge_heads)
            out.edge_heads.append(head)
            out.edge_inputs.append(mapped_inputs)
            out.edge_neg.append(mapped_neg)
            out.edge_prob.append(float(e.probability))
            out.incoming_edges[head].append(eid)
            for src in mapped_inputs:
                out.outgoing_edges[src].append(eid)
        return out


def kosaraju_scc(succ: List[List[int]]) -> Dict[str, object]:
    n = len(succ)
    rev: List[List[int]] = [[] for _ in range(n)]
    for u in range(n):
        for v in succ[u]:
            rev[v].append(u)

    visited = [False] * n
    order: List[int] = []

    for start in range(n):
        if visited[start]:
            continue
        stack: List[Tuple[int, int]] = [(start, 0)]
        while stack:
            node, phase = stack.pop()
            if phase == 0:
                if visited[node]:
                    continue
                visited[node] = True
                stack.append((node, 1))
                for nei in succ[node]:
                    if not visited[nei]:
                        stack.append((nei, 0))
            else:
                order.append(node)

    comp_id = [-1] * n
    comp_sizes: List[int] = []
    cid = 0
    for start in reversed(order):
        if comp_id[start] != -1:
            continue
        size = 0
        stack = [start]
        comp_id[start] = cid
        while stack:
            u = stack.pop()
            size += 1
            for nei in rev[u]:
                if comp_id[nei] == -1:
                    comp_id[nei] = cid
                    stack.append(nei)
        comp_sizes.append(size)
        cid += 1

    comp_edge_pairs: Set[Tuple[int, int]] = set()
    for u in range(n):
        cu = comp_id[u]
        for v in succ[u]:
            cv = comp_id[v]
            if cu != cv:
                comp_edge_pairs.add((cu, cv))

    cyclic_components = sum(1 for x in comp_sizes if x > 1)
    largest_component = max(comp_sizes) if comp_sizes else 0
    comp_sizes_sorted = sorted(comp_sizes)

    return {
        "components": len(comp_sizes),
        "cyclic_components": cyclic_components,
        "largest_component_size": largest_component,
        "component_size_p50": _quantile(comp_sizes_sorted, 0.50),
        "component_size_p90": _quantile(comp_sizes_sorted, 0.90),
        "component_size_p99": _quantile(comp_sizes_sorted, 0.99),
        "condensation_edges": len(comp_edge_pairs),
    }


def kosaraju_components(succ: List[List[int]]) -> List[List[int]]:
    n = len(succ)
    rev: List[List[int]] = [[] for _ in range(n)]
    for u in range(n):
        for v in succ[u]:
            rev[v].append(u)

    visited = [False] * n
    order: List[int] = []
    for start in range(n):
        if visited[start]:
            continue
        stack: List[Tuple[int, int]] = [(start, 0)]
        while stack:
            node, phase = stack.pop()
            if phase == 0:
                if visited[node]:
                    continue
                visited[node] = True
                stack.append((node, 1))
                for nei in succ[node]:
                    if not visited[nei]:
                        stack.append((nei, 0))
            else:
                order.append(node)

    comp_id = [-1] * n
    components: List[List[int]] = []
    for start in reversed(order):
        if comp_id[start] != -1:
            continue
        cid = len(components)
        cur: List[int] = []
        stack = [start]
        comp_id[start] = cid
        while stack:
            u = stack.pop()
            cur.append(u)
            for nei in rev[u]:
                if comp_id[nei] == -1:
                    comp_id[nei] = cid
                    stack.append(nei)
        components.append(cur)
    return components


class GeneralSisoDetector:
    def __init__(
        self,
        graph: DerivationHyperGraph,
        node_subset: Optional[Set[int]] = None,
        edge_subset: Optional[Set[int]] = None,
    ) -> None:
        self.g = graph
        self.nodes: Set[int] = set(range(graph.num_nodes)) if node_subset is None else set(node_subset)
        if edge_subset is None:
            candidate_edges = set(range(graph.num_edges))
        else:
            candidate_edges = set(edge_subset)

        # Keep only edges fully inside the current analysis scope.
        self.edges: Set[int] = set()
        for eid in candidate_edges:
            head = graph.edge_heads[eid]
            if head not in self.nodes:
                continue
            if any(inp not in self.nodes for inp in graph.edge_inputs[eid]):
                continue
            self.edges.add(eid)

        self.node_list: List[int] = sorted(self.nodes)
        self.incoming: Dict[int, List[int]] = {n: [] for n in self.node_list}
        self.outgoing: Dict[int, List[int]] = {n: [] for n in self.node_list}
        for eid in self.edges:
            head = graph.edge_heads[eid]
            self.incoming[head].append(eid)
            for inp in graph.edge_inputs[eid]:
                self.outgoing[inp].append(eid)

        self.index_of: Dict[int, int] = {}
        self.nodes_by_idx: List[int] = []
        self.dom_preds: List[List[int]] = []
        self.dom_entry_idx: int = -1
        self.dom_bits: List[int] = []
        self.idom: List[int] = []
        self.support: Dict[int, Set[int]] = {}
        self.dom_inputs: Dict[int, List[int]] = {}
        self.timings_ms: Dict[str, float] = {}
        self.prepared = False

    def _build_support_map(self) -> None:
        t0 = _now_ms()
        support: Dict[int, Set[int]] = {}
        for start in self.node_list:
            facts: Set[int] = set()
            q: deque[int] = deque([start])
            vis: Set[int] = {start}
            while q:
                u = q.popleft()
                in_edges = self.incoming.get(u, [])
                if not in_edges or self.g.is_fact[u]:
                    facts.add(u)
                    continue
                for eid in in_edges:
                    for v in self.g.edge_inputs[eid]:
                        if v in self.nodes and v not in vis:
                            vis.add(v)
                            q.append(v)
            support[start] = facts
        self.support = support
        self.timings_ms["support_map"] = _now_ms() - t0

    def _compute_dom_inputs(self) -> None:
        t0 = _now_ms()
        out: Dict[int, List[int]] = {}
        for eid in self.edges:
            inputs = self.g.edge_inputs[eid]
            if not inputs:
                continue
            groups: List[Tuple[List[int], Set[int]]] = []
            for v in inputs:
                sv = self.support.get(v, set())
                found = -1
                for i, (_, sup_union) in enumerate(groups):
                    if sv & sup_union:
                        found = i
                        break
                if found < 0:
                    groups.append(([v], set(sv)))
                else:
                    groups[found][0].append(v)
                    groups[found][1].update(sv)
            if not groups:
                continue
            best_members, _ = max(groups, key=lambda item: len(item[1]))
            out[eid] = best_members
        self.dom_inputs = out
        self.timings_ms["edge_dom_inputs"] = _now_ms() - t0

    def _build_dom_graph(self) -> None:
        t0 = _now_ms()
        self.nodes_by_idx = list(self.node_list)
        self.index_of = {node: i for i, node in enumerate(self.nodes_by_idx)}
        n = len(self.nodes_by_idx)
        preds: List[Set[int]] = [set() for _ in range(n)]
        for eid in self.edges:
            head = self.g.edge_heads[eid]
            out_idx = self.index_of.get(head)
            if out_idx is None:
                continue
            for inp in self.g.edge_inputs[eid]:
                in_idx = self.index_of.get(inp)
                if in_idx is None:
                    continue
                preds[out_idx].add(in_idx)
        self.dom_preds = [list(x) for x in preds]

        self.dom_entry_idx = -1
        for i, p in enumerate(self.dom_preds):
            if not p:
                self.dom_entry_idx = i
                break
        if self.dom_entry_idx < 0 and n > 0:
            self.dom_entry_idx = 0
        self.timings_ms["dom_graph"] = _now_ms() - t0

    def _compute_dominators(self) -> None:
        t0 = _now_ms()
        n = len(self.nodes_by_idx)
        self.dom_bits = [0] * n
        self.idom = [-1] * n
        if n == 0 or self.dom_entry_idx < 0:
            self.timings_ms["dominators"] = _now_ms() - t0
            return

        all_mask = (1 << n) - 1
        for i in range(n):
            self.dom_bits[i] = all_mask
        self.dom_bits[self.dom_entry_idx] = 1 << self.dom_entry_idx

        changed = True
        while changed:
            changed = False
            for node_idx in range(n):
                if node_idx == self.dom_entry_idx:
                    continue
                preds = self.dom_preds[node_idx]
                if not preds:
                    new_mask = 1 << node_idx
                else:
                    inter = all_mask
                    for p in preds:
                        inter &= self.dom_bits[p]
                    new_mask = inter | (1 << node_idx)
                if new_mask != self.dom_bits[node_idx]:
                    self.dom_bits[node_idx] = new_mask
                    changed = True

        for node_idx in range(n):
            if node_idx == self.dom_entry_idx:
                continue
            cand_mask = self.dom_bits[node_idx] & ~(1 << node_idx)
            cands = list(_iter_bits(cand_mask))
            if not cands:
                continue
            imm = -1
            for d in cands:
                # d is immediate if d does not dominate any other strict dominator.
                dominates_other = False
                for other in cands:
                    if other == d:
                        continue
                    if ((self.dom_bits[other] >> d) & 1) != 0:
                        dominates_other = True
                        break
                if not dominates_other:
                    imm = d
                    break
            if imm < 0:
                # Fallback (should be rare): pick deepest candidate.
                imm = max(cands, key=lambda x: self.dom_bits[x].bit_count())
            self.idom[node_idx] = imm
        self.timings_ms["dominators"] = _now_ms() - t0

    def prepare(self) -> None:
        if self.prepared:
            return
        self._build_support_map()
        self._compute_dom_inputs()
        self._build_dom_graph()
        self._compute_dominators()
        self.prepared = True

    def _lca(self, a: int, b: int) -> int:
        if a < 0 or b < 0:
            return -1
        if a == b:
            return a

        def depth(x: int) -> int:
            d = 0
            while x >= 0:
                x = self.idom[x]
                d += 1
            return d

        da = depth(a)
        db = depth(b)
        while da > db:
            a = self.idom[a]
            da -= 1
        while db > da:
            b = self.idom[b]
            db -= 1
        while a != b and a >= 0 and b >= 0:
            a = self.idom[a]
            b = self.idom[b]
        return a if a == b else -1

    def _confluence(self, idxs: List[int]) -> int:
        if not idxs:
            return -1
        cur = idxs[0]
        for i in idxs[1:]:
            cur = self._lca(cur, i)
            if cur < 0:
                break
        return cur

    def _find_candidate(self, exit_node: int) -> Optional[int]:
        exit_idx = self.index_of.get(exit_node)
        if exit_idx is None:
            return None
        src_idxs: List[int] = []
        for eid in self.incoming.get(exit_node, []):
            dom_group = self.dom_inputs.get(eid, self.g.edge_inputs[eid])
            for src_node in dom_group:
                idx = self.index_of.get(src_node)
                if idx is not None:
                    src_idxs.append(idx)
        if not src_idxs:
            return None
        entry_idx = self._confluence(src_idxs)
        if entry_idx < 0 or entry_idx == exit_idx:
            return None
        return self.nodes_by_idx[entry_idx]

    def _build_region(self, entry: int, exit_node: int, use_dom_inputs: bool) -> Tuple[Set[int], Set[int], bool]:
        nodes: Set[int] = {exit_node}
        edges: Set[int] = set()
        q: deque[int] = deque([exit_node])
        reached_entry = (entry == exit_node)
        while q:
            u = q.popleft()
            if u == entry:
                reached_entry = True
                continue
            for eid in self.incoming.get(u, []):
                edges.add(eid)
                if use_dom_inputs:
                    srcs = self.dom_inputs.get(eid, self.g.edge_inputs[eid])
                else:
                    srcs = self.g.edge_inputs[eid]
                for src in srcs:
                    if src in self.nodes and src not in nodes:
                        nodes.add(src)
                        q.append(src)
        return nodes, edges, reached_entry

    def _single_entry_boundary(self, region_nodes: Set[int], entry: int) -> bool:
        for node in region_nodes:
            if node == entry:
                continue
            for eid in self.incoming.get(node, []):
                for src in self.g.edge_inputs[eid]:
                    if src not in region_nodes:
                        return False
        return True

    def _no_escape(self, region_nodes: Set[int], entry: int, exit_node: int) -> bool:
        for node in region_nodes:
            if node == entry or node == exit_node:
                continue
            for eid in self.outgoing.get(node, []):
                out_node = self.g.edge_heads[eid]
                if out_node not in region_nodes:
                    return False
        return True

    def _entry_preds(self, region_nodes: Set[int], entry: int) -> Set[int]:
        preds: Set[int] = set()
        for eid in self.incoming.get(entry, []):
            for src in self.g.edge_inputs[eid]:
                if src not in region_nodes:
                    preds.add(src)
        return preds

    def _random_vars(self, region_nodes: Set[int], region_edges: Set[int], entry: int, exit_node: int) -> int:
        total = 0
        for n in region_nodes:
            if n == entry or n == exit_node:
                continue
            if self.g.is_fact[n] and DerivationHyperGraph._is_probabilistic(self.g.fact_prob[n]):
                total += 1
        for eid in region_edges:
            if DerivationHyperGraph._is_probabilistic(self.g.edge_prob[eid]):
                total += 1
        return total

    def detect_for_exit(self, exit_node: int, min_random_vars: int) -> Optional[Tuple[int, int, Set[int], Set[int], int, int]]:
        entry = self._find_candidate(exit_node)
        if entry is None:
            return None

        strict_nodes, strict_edges, strict_ok = self._build_region(entry, exit_node, use_dom_inputs=True)
        if not strict_ok:
            return None
        full_nodes, full_edges, full_ok = self._build_region(entry, exit_node, use_dom_inputs=False)
        if not full_ok:
            return None
        if len(full_nodes) < 2:
            return None
        if not self._single_entry_boundary(full_nodes, entry):
            return None
        if not self._no_escape(full_nodes, entry, exit_node):
            return None
        rv = self._random_vars(full_nodes, full_edges, entry, exit_node)
        if rv < min_random_vars:
            return None
        entry_preds = len(self._entry_preds(full_nodes, entry))
        return entry, exit_node, full_nodes, full_edges, rv, entry_preds


def build_backward_cone(
    g: DerivationHyperGraph,
    exit_node: int,
    depth_limit: int,
    node_limit: int,
) -> Tuple[Set[int], Set[int], bool]:
    nodes: Set[int] = {exit_node}
    seen_edges: Set[int] = set()
    q: deque[Tuple[int, int]] = deque([(exit_node, 0)])
    truncated = False
    while q:
        node, depth = q.popleft()
        if depth >= depth_limit:
            continue
        for eid in g.incoming_edges[node]:
            seen_edges.add(eid)
            for src in g.edge_inputs[eid]:
                if src in nodes:
                    continue
                if len(nodes) >= node_limit:
                    truncated = True
                    continue
                nodes.add(src)
                q.append((src, depth + 1))
    # Keep only edges closed inside cone.
    edges = {
        eid
        for eid in seen_edges
        if g.edge_heads[eid] in nodes and all(src in nodes for src in g.edge_inputs[eid])
    }
    return nodes, edges, truncated


def top_exit_candidates(g: DerivationHyperGraph, max_candidates: int) -> List[int]:
    scored: List[Tuple[int, int]] = []
    for n in range(g.num_nodes):
        indeg = len(g.incoming_edges[n])
        if indeg <= 0:
            continue
        # Prioritize non-facts first, then larger indegree.
        fact_bias = 1 if g.is_fact[n] else 0
        scored.append((fact_bias, -indeg, n))
    scored.sort()
    return [n for _, _, n in scored[:max_candidates]]


def _kind_counts_from_regions(regions: Sequence[SupportedSisoRegion]) -> Dict[str, int]:
    counts: Dict[str, int] = {k: 0 for k in SUPPORTED_SISO_KIND_ORDER}
    for r in regions:
        counts[r.kind] = counts.get(r.kind, 0) + 1
    return {k: counts.get(k, 0) for k in SUPPORTED_SISO_KIND_ORDER if counts.get(k, 0) > 0}


def _region_random_vars_mutable(g: MutableDerivationGraph, region: SupportedSisoRegion) -> int:
    total = 0
    for n in region.internal_nodes:
        if n == region.entry or n == region.exit:
            continue
        if not g.has_node(n):
            continue
        node = g.nodes[n]
        if node.is_fact and DerivationHyperGraph._is_probabilistic(node.probability):
            total += 1
    for eid in region.internal_edges:
        if not g.has_edge(eid):
            continue
        if DerivationHyperGraph._is_probabilistic(g.edges[eid].probability):
            total += 1
    return total


def _build_region_graph_payload_mutable(g: MutableDerivationGraph, region: SupportedSisoRegion) -> Dict[str, object]:
    in_deg: Dict[int, int] = {n: 0 for n in region.internal_nodes}
    out_deg: Dict[int, int] = {n: 0 for n in region.internal_nodes}
    for eid in sorted(region.internal_edges):
        if not g.has_edge(eid):
            continue
        e = g.edges[eid]
        if e.head in in_deg:
            in_deg[e.head] += 1
        for src in e.inputs:
            if src in out_deg:
                out_deg[src] += 1

    nodes_payload: List[Dict[str, object]] = []
    for nid in sorted(region.internal_nodes):
        if not g.has_node(nid):
            continue
        n = g.nodes[nid]
        nodes_payload.append(
            {
                "name": n.name,
                "predicate": _predicate_of(n.name),
                "is_fact": n.is_fact,
                "fact_probability": n.probability if n.is_fact else None,
                "is_entry": nid == region.entry,
                "is_exit": nid == region.exit,
                "in_degree_region": in_deg.get(nid, 0),
                "out_degree_region": out_deg.get(nid, 0),
            }
        )

    edges_payload: List[Dict[str, object]] = []
    for eid in sorted(region.internal_edges):
        if not g.has_edge(eid):
            continue
        e = g.edges[eid]
        edges_payload.append(
            {
                "edge_id": e.id,
                "head": g.nodes[e.head].name if g.has_node(e.head) else f"node#{e.head}",
                "head_predicate": _predicate_of(g.nodes[e.head].name) if g.has_node(e.head) else f"node#{e.head}",
                "inputs": [g.nodes[src].name if g.has_node(src) else f"node#{src}" for src in e.inputs],
                "input_predicates": [
                    _predicate_of(g.nodes[src].name) if g.has_node(src) else f"node#{src}" for src in e.inputs
                ],
                "negated_inputs": [bool(x) for x in e.negated_inputs],
                "probability": e.probability,
                "is_probabilistic": DerivationHyperGraph._is_probabilistic(e.probability),
                "arity": len(e.inputs),
            }
        )

    predicates = sorted({row["predicate"] for row in nodes_payload})
    return {"nodes": nodes_payload, "edges": edges_payload, "predicates": predicates}


def _supported_region_to_dict(
    g: MutableDerivationGraph,
    region: SupportedSisoRegion,
    mode: str,
    include_region_graph: bool,
    candidate_exit_name: Optional[str] = None,
) -> Dict[str, object]:
    entry_name = g.nodes[region.entry].name if g.has_node(region.entry) else f"node#{region.entry}"
    exit_name = g.nodes[region.exit].name if g.has_node(region.exit) else f"node#{region.exit}"
    rv = _region_random_vars_mutable(g, region)
    entry_preds: Set[int] = set()
    for eid in g.incoming_edges(region.entry):
        if eid in region.internal_edges or not g.has_edge(eid):
            continue
        e = g.edges[eid]
        for src in e.inputs:
            if src not in region.internal_nodes and g.has_node(src):
                entry_preds.add(src)
    return {
        "entry": entry_name,
        "exit": exit_name,
        "node_count": len(region.internal_nodes),
        "edge_count": len(region.internal_edges),
        "random_vars": rv,
        "entry_pred_count": len(entry_preds),
        "mode": mode,
        "candidate_exit": candidate_exit_name if candidate_exit_name is not None else exit_name,
        "kind": region.kind,
        "shape": "supported-fast-path",
        "has_probabilistic": rv > 0,
        "region_graph": _build_region_graph_payload_mutable(g, region) if include_region_graph else None,
        "cone_nodes": 0,
        "cone_edges": 0,
        "cone_truncated": False,
    }


def detect_supported_siso_regions(
    g: MutableDerivationGraph,
    candidate_nodes: Optional[Set[int]] = None,
    candidate_edges: Optional[Set[int]] = None,
) -> Tuple[List[SupportedSisoRegion], Dict[str, object]]:
    t_start = _now_ms()
    all_edges = g.active_edge_ids() if candidate_edges is None else [eid for eid in sorted(candidate_edges) if g.has_edge(eid)]
    all_nodes = g.active_node_ids() if candidate_nodes is None else [nid for nid in sorted(candidate_nodes) if g.has_node(nid)]
    regions: List[SupportedSisoRegion] = []
    breakdown_ms: Dict[str, float] = {}
    candidate_by_kind: Dict[str, int] = {k: 0 for k in SUPPORTED_SISO_KIND_ORDER}

    def pure_fact_input(node_id: int, owning_edge: int) -> bool:
        if not g.has_node(node_id):
            return False
        n = g.nodes[node_id]
        if not n.is_fact or n.has_evidence or n.need_output:
            return False
        if len(g.incoming_edges(node_id)) != 0:
            return False
        outs = g.outgoing_edges(node_id)
        return len(outs) == 1 and outs[0] == owning_edge

    # 1) SingleHyperedge
    t0 = _now_ms()
    for eid in all_edges:
        if not g.has_edge(eid):
            continue
        e = g.edges[eid]
        inputs = list(e.inputs)
        if len(inputs) <= 1:
            continue
        if not g.has_node(e.head):
            continue
        exit_node = e.head
        si: Optional[int] = None
        fact_inputs = 0
        invalid = False
        for src in inputs:
            if pure_fact_input(src, eid):
                fact_inputs += 1
            elif si is None:
                si = src
            else:
                invalid = True
                break
        if invalid or si is None:
            continue
        if fact_inputs == 0:
            continue
        if not g.has_node(si) or g.nodes[si].is_fact:
            continue
        if si == exit_node:
            continue
        nodes = set(inputs)
        nodes.add(exit_node)
        regions.append(
            SupportedSisoRegion(
                kind="SingleHyperedge",
                entry=si,
                exit=exit_node,
                internal_nodes=nodes,
                internal_edges={eid},
            )
        )
        candidate_by_kind["SingleHyperedge"] += 1
    breakdown_ms["SingleHyperedge"] = _now_ms() - t0

    # 2) LinearTwoEdge
    t0 = _now_ms()
    for eid1 in all_edges:
        if not g.has_edge(eid1):
            continue
        e1 = g.edges[eid1]
        if len(e1.inputs) != 1 or not g.has_node(e1.head):
            continue
        entry = e1.inputs[0]
        mid = e1.head
        if entry == mid:
            continue
        if not g.has_node(mid):
            continue
        mid_node = g.nodes[mid]
        if mid_node.has_evidence or mid_node.need_output:
            continue
        mid_out = g.outgoing_edges(mid)
        if len(mid_out) != 1:
            continue
        eid2 = mid_out[0]
        if not g.has_edge(eid2):
            continue
        e2 = g.edges[eid2]
        if len(e2.inputs) != 1 or e2.inputs[0] != mid:
            continue
        neg2 = e2.negated_inputs[0] if e2.negated_inputs else False
        if neg2:
            continue
        exit_node = e2.head
        if not g.has_node(exit_node):
            continue
        if exit_node == entry or exit_node == mid:
            continue
        nodes = {entry, mid, exit_node}
        regions.append(
            SupportedSisoRegion(
                kind="LinearTwoEdge",
                entry=entry,
                exit=exit_node,
                internal_nodes=nodes,
                internal_edges={eid1, eid2},
            )
        )
        candidate_by_kind["LinearTwoEdge"] += 1
    breakdown_ms["LinearTwoEdge"] = _now_ms() - t0

    # 3) ParallelEdge
    t0 = _now_ms()
    for so in all_nodes:
        if not g.has_node(so):
            continue
        incoming = g.incoming_edges(so)
        if len(incoming) < 2:
            continue
        buckets: Dict[Tuple[int, bool], List[int]] = {}
        for eid in incoming:
            if not g.has_edge(eid):
                continue
            e = g.edges[eid]
            if len(e.inputs) != 1:
                continue
            if len(e.negated_inputs) > 1:
                continue
            si = e.inputs[0]
            if not g.has_node(si):
                continue
            is_neg = bool(e.negated_inputs[0]) if e.negated_inputs else False
            buckets.setdefault((si, is_neg), []).append(eid)
        for (si, _), group_edges in buckets.items():
            if len(group_edges) < 2:
                continue
            if si == so:
                continue
            nodes = {si, so}
            regions.append(
                SupportedSisoRegion(
                    kind="ParallelEdge",
                    entry=si,
                    exit=so,
                    internal_nodes=nodes,
                    internal_edges=set(group_edges),
                )
            )
            candidate_by_kind["ParallelEdge"] += 1
    breakdown_ms["ParallelEdge"] = _now_ms() - t0

    # 4) FanOutConverge
    t0 = _now_ms()
    for si in all_nodes:
        if not g.has_node(si):
            continue
        si_node = g.nodes[si]
        if not si_node.is_fact or si_node.has_evidence or si_node.need_output:
            continue
        outs = g.outgoing_edges(si)
        if len(outs) < 2:
            continue
        fan_edges: List[int] = []
        xi_nodes: List[int] = []
        bad = False
        for eid in outs:
            if not g.has_edge(eid):
                bad = True
                break
            e = g.edges[eid]
            if len(e.inputs) != 1 or e.inputs[0] != si:
                bad = True
                break
            fan_edges.append(eid)
            xi_nodes.append(e.head)
        if bad or len(fan_edges) < 2:
            continue
        xi_set: Set[int] = set()
        for x in xi_nodes:
            if not g.has_node(x):
                bad = True
                break
            if x in xi_set:
                bad = True
                break
            xi_set.add(x)
            if len(g.incoming_edges(x)) != 1 or len(g.outgoing_edges(x)) != 1:
                bad = True
                break
        if bad:
            continue
        conv: Optional[int] = None
        for x in xi_set:
            out_x = g.outgoing_edges(x)
            if not out_x:
                bad = True
                break
            if conv is None:
                conv = out_x[0]
            elif conv != out_x[0]:
                bad = True
                break
        if bad or conv is None or not g.has_edge(conv):
            continue
        conv_edge = g.edges[conv]
        conv_inputs = list(conv_edge.inputs)
        if len(conv_inputs) != len(xi_set):
            continue
        if any(bool(x) for x in conv_edge.negated_inputs):
            continue
        if set(conv_inputs) != xi_set:
            continue
        so = conv_edge.head
        if not g.has_node(so) or so == si:
            continue
        region_edges = set(fan_edges)
        region_edges.add(conv)
        region_nodes = set(xi_set)
        region_nodes.add(si)
        region_nodes.add(so)
        regions.append(
            SupportedSisoRegion(
                kind="FanOutConverge",
                entry=si,
                exit=so,
                internal_nodes=region_nodes,
                internal_edges=region_edges,
            )
        )
        candidate_by_kind["FanOutConverge"] += 1
    breakdown_ms["FanOutConverge"] = _now_ms() - t0

    # 5) AllFactsToSO
    t0 = _now_ms()
    for eid in all_edges:
        if not g.has_edge(eid):
            continue
        e = g.edges[eid]
        inputs = list(e.inputs)
        if not inputs:
            continue
        all_facts = True
        for src in inputs:
            if not g.has_node(src):
                all_facts = False
                break
            n = g.nodes[src]
            if not n.is_fact or n.has_evidence:
                all_facts = False
                break
            outs = g.outgoing_edges(src)
            if len(outs) != 1 or outs[0] != eid:
                all_facts = False
                break
        if not all_facts:
            continue
        exit_node = e.head
        if not g.has_node(exit_node):
            continue
        exit_in = g.incoming_edges(exit_node)
        if len(exit_in) != 1 or exit_in[0] != eid:
            continue
        entry = inputs[0]
        nodes = set(inputs)
        nodes.add(exit_node)
        regions.append(
            SupportedSisoRegion(
                kind="AllFactsToSO",
                entry=entry,
                exit=exit_node,
                internal_nodes=nodes,
                internal_edges={eid},
            )
        )
        candidate_by_kind["AllFactsToSO"] += 1
    breakdown_ms["AllFactsToSO"] = _now_ms() - t0

    sort_start = _now_ms()
    regions.sort(key=lambda r: len(r.internal_nodes))
    sort_ms = _now_ms() - sort_start
    used_nodes: Set[int] = set()
    dedup: List[SupportedSisoRegion] = []
    filter_start = _now_ms()
    for r in regions:
        if any(n in used_nodes for n in r.internal_nodes):
            continue
        dedup.append(r)
        used_nodes.update(r.internal_nodes)
    filter_ms = _now_ms() - filter_start

    kept_by_kind = _kind_counts_from_regions(dedup)
    cand_by_kind_clean = {k: v for k, v in candidate_by_kind.items() if v > 0}
    meta = {
        "mode": "full",
        "candidate_regions": len(regions),
        "regions_found": len(dedup),
        "candidates_by_kind": cand_by_kind_clean,
        "kept_by_kind": kept_by_kind,
        "timing_ms": {
            **breakdown_ms,
            "sort": sort_ms,
            "filter": filter_ms,
            "total": _now_ms() - t_start,
        },
    }
    return dedup, meta


def analyze_supported_siso(
    g: MutableDerivationGraph,
    include_region_graph: bool,
    mode_label: str = "supported-fast",
) -> Dict[str, object]:
    regions, meta = detect_supported_siso_regions(g)
    rows = [_supported_region_to_dict(g, r, mode=mode_label, include_region_graph=include_region_graph) for r in regions]
    return {
        "mode": mode_label,
        "regions_found": len(rows),
        "candidates_tested": meta.get("candidate_regions", len(rows)),
        "timing_ms": meta.get("timing_ms", {}),
        "candidates_by_kind": meta.get("candidates_by_kind", {}),
        "kept_by_kind": meta.get("kept_by_kind", {}),
        "regions": rows,
    }


def _weighted_choice_index(rng: random.Random, weights: Sequence[float]) -> int:
    total = 0.0
    for w in weights:
        total += max(0.0, float(w))
    if total <= 0.0:
        return rng.randrange(len(weights))
    r = rng.random() * total
    acc = 0.0
    for i, w in enumerate(weights):
        acc += max(0.0, float(w))
        if acc >= r:
            return i
    return len(weights) - 1


def _build_seed_weights(
    g: DerivationHyperGraph,
    policy: str,
) -> Tuple[List[int], List[float]]:
    node_ids = list(range(g.num_nodes))
    in_deg = [len(g.incoming_edges[i]) for i in node_ids]
    out_deg = [len(g.outgoing_edges[i]) for i in node_ids]
    edge_prob_hot: List[int] = [0 for _ in node_ids]
    for i in node_ids:
        hot = 0
        for eid in g.incoming_edges[i]:
            if DerivationHyperGraph._is_probabilistic(g.edge_prob[eid]):
                hot += 1
        for eid in g.outgoing_edges[i]:
            if DerivationHyperGraph._is_probabilistic(g.edge_prob[eid]):
                hot += 1
        edge_prob_hot[i] = hot

    weights: List[float] = []
    for n in node_ids:
        if policy == "uniform":
            w = 1.0
        elif policy == "high-in":
            w = 1.0 + float(in_deg[n])
        elif policy == "high-out":
            w = 1.0 + float(out_deg[n])
        elif policy == "disjunction":
            w = 1.0 + 3.0 * max(0, in_deg[n] - 1)
        elif policy == "prob-hot":
            w = 1.0 + 2.0 * float(edge_prob_hot[n])
            if g.is_fact[n] and DerivationHyperGraph._is_probabilistic(g.fact_prob[n]):
                w += 3.0
        else:
            w = 1.0
        weights.append(max(0.0, w))
    return node_ids, weights


def _is_disjunction_node(g: DerivationHyperGraph, nid: int) -> bool:
    in_deg = len(g.incoming_edges[nid])
    if g.is_fact[nid]:
        return in_deg > 0
    return in_deg > 1


def _collect_disjunction_nodes(g: DerivationHyperGraph) -> List[int]:
    return [nid for nid in range(g.num_nodes) if _is_disjunction_node(g, nid)]


def _trim_nodes_by_bfs(
    g: DerivationHyperGraph,
    nodes: Set[int],
    anchors: Sequence[int],
    max_nodes: int,
) -> Tuple[Set[int], bool]:
    if len(nodes) <= max_nodes:
        return set(nodes), False
    kept: Set[int] = set()
    q: deque[int] = deque()
    for a in anchors:
        if a in nodes and a not in kept:
            kept.add(a)
            q.append(a)
            if len(kept) >= max_nodes:
                break
    if not q:
        for n in sorted(nodes):
            kept.add(n)
            q.append(n)
            break

    while q and len(kept) < max_nodes:
        u = q.popleft()
        for eid in g.incoming_edges[u]:
            for src in g.edge_inputs[eid]:
                if src in nodes and src not in kept:
                    kept.add(src)
                    q.append(src)
                    if len(kept) >= max_nodes:
                        break
            if len(kept) >= max_nodes:
                break
        if len(kept) >= max_nodes:
            break
        for eid in g.outgoing_edges[u]:
            head = g.edge_heads[eid]
            if head in nodes and head not in kept:
                kept.add(head)
                q.append(head)
                if len(kept) >= max_nodes:
                    break

    if len(kept) < max_nodes:
        for n in sorted(nodes):
            if n in kept:
                continue
            kept.add(n)
            if len(kept) >= max_nodes:
                break
    return kept, True


def _closed_edges_for_nodes(g: DerivationHyperGraph, nodes: Set[int]) -> Set[int]:
    edges: Set[int] = set()
    for head in nodes:
        for eid in g.incoming_edges[head]:
            ins = g.edge_inputs[eid]
            if all(src in nodes for src in ins):
                edges.add(eid)
    return edges


def _limit_edges(edges: Set[int], max_edges: int) -> Tuple[Set[int], bool]:
    if len(edges) <= max_edges:
        return set(edges), False
    out = set(sorted(edges)[:max_edges])
    return out, True


def collect_backward_cone_immutable(
    g: DerivationHyperGraph,
    seed: int,
    depth_limit: int,
    max_nodes: int,
    max_edges: int,
) -> Tuple[Set[int], Set[int], bool]:
    nodes: Set[int] = {seed}
    edges: Set[int] = set()
    q: deque[Tuple[int, int]] = deque([(seed, 0)])
    truncated = False
    while q:
        node, depth = q.popleft()
        if depth >= depth_limit:
            continue
        for eid in g.incoming_edges[node]:
            if eid not in edges and len(edges) >= max_edges:
                truncated = True
                continue
            edges.add(eid)
            for src in g.edge_inputs[eid]:
                if src in nodes:
                    continue
                if len(nodes) >= max_nodes:
                    truncated = True
                    continue
                nodes.add(src)
                q.append((src, depth + 1))
    edges = {eid for eid in edges if g.edge_heads[eid] in nodes and all(src in nodes for src in g.edge_inputs[eid])}
    edges, cut_e = _limit_edges(edges, max_edges)
    return nodes, edges, truncated or cut_e


def collect_backward_cone_with_depths_immutable(
    g: DerivationHyperGraph,
    seed: int,
    depth_limit: int,
    max_nodes: int,
    max_edges: int,
) -> Tuple[Set[int], Set[int], bool, Dict[int, int]]:
    nodes: Set[int] = {seed}
    edges: Set[int] = set()
    depth_of: Dict[int, int] = {seed: 0}
    q: deque[Tuple[int, int]] = deque([(seed, 0)])
    truncated = False
    while q:
        node, depth = q.popleft()
        if depth >= depth_limit:
            continue
        next_depth = depth + 1
        for eid in g.incoming_edges[node]:
            if eid not in edges and len(edges) >= max_edges:
                truncated = True
                continue
            edges.add(eid)
            for src in g.edge_inputs[eid]:
                old_depth = depth_of.get(src)
                if old_depth is None or next_depth < old_depth:
                    depth_of[src] = next_depth
                if src in nodes:
                    continue
                if len(nodes) >= max_nodes:
                    truncated = True
                    continue
                nodes.add(src)
                q.append((src, next_depth))
    edges = {eid for eid in edges if g.edge_heads[eid] in nodes and all(src in nodes for src in g.edge_inputs[eid])}
    edges, cut_e = _limit_edges(edges, max_edges)
    return nodes, edges, truncated or cut_e, depth_of


def collect_disjunction_backward_cone_immutable(
    g: DerivationHyperGraph,
    seed: int,
    depth_limit: int,
    max_nodes: int,
    max_edges: int,
) -> Tuple[Set[int], Set[int], bool, Dict[str, object]]:
    # Oversample first so we can keep a middle slice when the backward cone is too large.
    raw_max_nodes = max(max_nodes * 4, max_nodes + 200)
    raw_max_edges = max(max_edges * 4, max_edges + 500)
    raw_nodes, raw_edges, raw_truncated, depth_of = collect_backward_cone_with_depths_immutable(
        g,
        seed=seed,
        depth_limit=depth_limit,
        max_nodes=raw_max_nodes,
        max_edges=raw_max_edges,
    )
    meta: Dict[str, object] = {
        "raw_nodes": len(raw_nodes),
        "raw_edges": len(raw_edges),
        "raw_truncated": raw_truncated,
        "middle_slice_applied": False,
    }
    if not raw_nodes:
        return raw_nodes, raw_edges, raw_truncated, meta

    needs_middle = bool(raw_truncated or len(raw_nodes) > max_nodes or len(raw_edges) > max_edges)
    if not needs_middle:
        return raw_nodes, raw_edges, raw_truncated, meta

    max_depth = max(depth_of.get(n, 0) for n in raw_nodes)
    mid_depth = max_depth / 2.0
    pivot = min(
        raw_nodes,
        key=lambda n: (
            abs(depth_of.get(n, 0) - mid_depth),
            -(len(g.incoming_edges[n]) + len(g.outgoing_edges[n])),
            n,
        ),
    )
    mid_nodes, cut_mid = _trim_nodes_by_bfs(g, raw_nodes, anchors=[pivot], max_nodes=max_nodes)
    mid_edges = _closed_edges_for_nodes(g, mid_nodes)
    mid_edges, cut_e = _limit_edges(mid_edges, max_edges)
    truncated = raw_truncated or cut_mid or cut_e or len(raw_nodes) > len(mid_nodes) or len(raw_edges) > len(mid_edges)

    # Fallback: keep seed-centered view if pivot slice has no edges.
    if not mid_edges:
        mid_nodes, cut_seed = _trim_nodes_by_bfs(g, raw_nodes, anchors=[seed], max_nodes=max_nodes)
        mid_edges = _closed_edges_for_nodes(g, mid_nodes)
        mid_edges, cut_e_seed = _limit_edges(mid_edges, max_edges)
        cut_mid = cut_mid or cut_seed
        cut_e = cut_e or cut_e_seed
        truncated = truncated or cut_seed or cut_e_seed
        meta["middle_slice_fallback_seed"] = True

    meta.update(
        {
            "middle_slice_applied": True,
            "middle_pivot": g.node_names[pivot],
            "middle_pivot_depth": depth_of.get(pivot, 0),
            "middle_target_depth": mid_depth,
            "middle_nodes": len(mid_nodes),
            "middle_edges": len(mid_edges),
        }
    )
    return mid_nodes, mid_edges, truncated, meta


def collect_forward_cone_immutable(
    g: DerivationHyperGraph,
    seed: int,
    depth_limit: int,
    max_nodes: int,
    max_edges: int,
) -> Tuple[Set[int], Set[int], bool]:
    nodes: Set[int] = {seed}
    edges: Set[int] = set()
    q: deque[Tuple[int, int]] = deque([(seed, 0)])
    truncated = False
    while q:
        node, depth = q.popleft()
        if depth >= depth_limit:
            continue
        for eid in g.outgoing_edges[node]:
            if eid not in edges and len(edges) >= max_edges:
                truncated = True
                continue
            edges.add(eid)
            head = g.edge_heads[eid]
            if head not in nodes:
                if len(nodes) >= max_nodes:
                    truncated = True
                else:
                    nodes.add(head)
                    q.append((head, depth + 1))
    edges = {eid for eid in edges if g.edge_heads[eid] in nodes and all(src in nodes for src in g.edge_inputs[eid])}
    edges, cut_e = _limit_edges(edges, max_edges)
    return nodes, edges, truncated or cut_e


def collect_bi_cone_immutable(
    g: DerivationHyperGraph,
    source: int,
    target: int,
    depth_limit: int,
    max_nodes: int,
    max_edges: int,
) -> Tuple[Set[int], Set[int], bool, Dict[str, object]]:
    f_nodes, _, f_trunc = collect_forward_cone_immutable(
        g, seed=source, depth_limit=depth_limit, max_nodes=max_nodes * 2, max_edges=max_edges * 2
    )
    b_nodes, _, b_trunc = collect_backward_cone_immutable(
        g, seed=target, depth_limit=depth_limit, max_nodes=max_nodes * 2, max_edges=max_edges * 2
    )
    inter = f_nodes & b_nodes
    truncated = f_trunc or b_trunc
    meta: Dict[str, object] = {
        "forward_nodes": len(f_nodes),
        "backward_nodes": len(b_nodes),
        "intersection_nodes": len(inter),
        "source": g.node_names[source],
        "target": g.node_names[target],
    }
    if inter:
        nodes = set(inter)
        nodes.add(source)
        nodes.add(target)
    else:
        nodes = set()
        edges = set()
        truncated = True
        meta["intersection_empty"] = True
        return nodes, edges, truncated, meta
    nodes, cut_n = _trim_nodes_by_bfs(g, nodes, anchors=[source, target], max_nodes=max_nodes)
    truncated = truncated or cut_n
    edges = _closed_edges_for_nodes(g, nodes)
    edges, cut_e = _limit_edges(edges, max_edges)
    truncated = truncated or cut_e
    return nodes, edges, truncated, meta


def _prune_to_best_component(
    g: DerivationHyperGraph,
    nodes: Set[int],
    edges: Set[int],
    anchors: Sequence[int],
) -> Tuple[Set[int], Set[int], bool]:
    if not nodes:
        return set(), set(), False
    if not edges:
        return set(nodes), set(), False

    adj: Dict[int, Set[int]] = {n: set() for n in nodes}
    for eid in edges:
        head = g.edge_heads[eid]
        if head not in nodes:
            continue
        for src in g.edge_inputs[eid]:
            if src not in nodes:
                continue
            adj[head].add(src)
            adj[src].add(head)

    comps: List[Set[int]] = []
    seen: Set[int] = set()
    for n in nodes:
        if n in seen:
            continue
        q: deque[int] = deque([n])
        seen.add(n)
        cur: Set[int] = set([n])
        while q:
            u = q.popleft()
            for v in adj.get(u, set()):
                if v in seen:
                    continue
                seen.add(v)
                cur.add(v)
                q.append(v)
        comps.append(cur)

    if len(comps) <= 1:
        return set(nodes), set(edges), False

    anchor_set = {a for a in anchors if a in nodes}
    best = max(
        comps,
        key=lambda c: (
            sum(1 for a in anchor_set if a in c),
            len(c),
        ),
    )
    pruned_nodes = set(best)
    pruned_edges = {
        eid
        for eid in edges
        if g.edge_heads[eid] in pruned_nodes and all(src in pruned_nodes for src in g.edge_inputs[eid])
    }
    changed = len(pruned_nodes) != len(nodes)
    return pruned_nodes, pruned_edges, changed


def _count_isolated_nodes(
    g: DerivationHyperGraph,
    nodes: Set[int],
    edges: Set[int],
) -> int:
    if not nodes:
        return 0
    in_deg: Dict[int, int] = {n: 0 for n in nodes}
    out_deg: Dict[int, int] = {n: 0 for n in nodes}
    for eid in edges:
        head = g.edge_heads[eid]
        if head in nodes:
            in_deg[head] += 1
        for src in g.edge_inputs[eid]:
            if src in nodes:
                out_deg[src] += 1
    return sum(1 for n in nodes if in_deg.get(n, 0) == 0 and out_deg.get(n, 0) == 0)


def build_immutable_subgraph(
    g: DerivationHyperGraph,
    nodes: Set[int],
    edges: Set[int],
) -> Tuple[DerivationHyperGraph, Dict[int, int], Dict[int, int]]:
    out = DerivationHyperGraph()
    old_to_new: Dict[int, int] = {}
    for old in sorted(nodes):
        new_idx = out.add_node(g.node_names[old])
        old_to_new[old] = new_idx
        out.is_fact[new_idx] = bool(g.is_fact[old])
        out.fact_prob[new_idx] = g.fact_prob[old] if g.is_fact[old] else None
    for old_eid in sorted(edges):
        head_old = g.edge_heads[old_eid]
        if head_old not in old_to_new:
            continue
        ins_old = g.edge_inputs[old_eid]
        if any(src not in old_to_new for src in ins_old):
            continue
        head = old_to_new[head_old]
        ins = [old_to_new[src] for src in ins_old]
        neg = [bool(x) for x in g.edge_neg[old_eid]]
        if len(neg) < len(ins):
            neg.extend([False] * (len(ins) - len(neg)))
        elif len(neg) > len(ins):
            neg = neg[: len(ins)]
        eid = len(out.edge_heads)
        out.edge_heads.append(head)
        out.edge_inputs.append(ins)
        out.edge_neg.append(neg)
        out.edge_prob.append(float(g.edge_prob[old_eid]))
        out.incoming_edges[head].append(eid)
        for src in ins:
            out.outgoing_edges[src].append(eid)
    new_to_old = {v: k for k, v in old_to_new.items()}
    return out, old_to_new, new_to_old


def analyze_random_explore_samples(
    g: DerivationHyperGraph,
    mode: str,
    sample_count: int,
    random_seed: int,
    seed_policy: str,
    depth_limit: int,
    max_nodes: int,
    max_edges: int,
    min_nodes: int,
    min_edges: int,
    max_attempts: int,
    include_region_graph: bool,
) -> Dict[str, object]:
    t_start = _now_ms()
    rng = random.Random(random_seed)
    node_ids, node_weights = _build_seed_weights(g, seed_policy)
    if not node_ids:
        return {
            "mode": mode,
            "sample_count": 0,
            "seed": random_seed,
            "seed_policy": seed_policy,
            "depth_limit": depth_limit,
            "max_nodes": max_nodes,
            "max_edges": max_edges,
            "min_nodes": min_nodes,
            "min_edges": min_edges,
            "max_attempts": max_attempts,
            "samples": [],
            "timing_ms": {"total": _now_ms() - t_start},
        }
    _, high_in_weights = _build_seed_weights(g, "high-in")
    sample_modes = [
        "random-backward-cone",
        "random-disjunction-backward-cone",
        "random-forward-cone",
        "random-bi-cone",
        "random-scc",
        "random-supported-siso",
    ]
    projected = g.projected_adjacency()
    scc_components_cache: Optional[List[List[int]]] = None
    supported_regions_cache: Optional[List[SupportedSisoRegion]] = None

    if mode in ("random-scc", "mixed"):
        scc_components_cache = kosaraju_components(projected)
    if mode in ("random-supported-siso", "mixed"):
        tmp_mut = MutableDerivationGraph.from_immutable(g)
        supported_regions_cache, _ = detect_supported_siso_regions(tmp_mut)
    disjunction_nodes = _collect_disjunction_nodes(g)
    disjunction_weights = [1.0 + float(len(g.incoming_edges[n])) for n in disjunction_nodes]

    samples: List[Dict[str, object]] = []
    attempts = 0
    max_try = max(max_attempts, sample_count)
    while len(samples) < max(0, sample_count) and attempts < max_try:
        attempts += 1
        sid = len(samples) + 1
        sample_mode = mode if mode != "mixed" else sample_modes[rng.randrange(len(sample_modes))]
        src_idx = _weighted_choice_index(rng, node_weights)
        src = node_ids[src_idx]
        target = src
        nodes: Set[int] = set()
        edges: Set[int] = set()
        truncated = False
        meta: Dict[str, object] = {}
        anchor_entry: Optional[int] = src
        anchor_exit: Optional[int] = None

        if sample_mode == "random-backward-cone":
            nodes, edges, truncated = collect_backward_cone_immutable(
                g, seed=src, depth_limit=depth_limit, max_nodes=max_nodes, max_edges=max_edges
            )
            anchor_exit = src
        elif sample_mode == "random-disjunction-backward-cone":
            if not disjunction_nodes:
                continue
            d_idx = _weighted_choice_index(rng, disjunction_weights)
            src = disjunction_nodes[d_idx]
            target = src
            anchor_entry = src
            anchor_exit = src
            nodes, edges, truncated, d_meta = collect_disjunction_backward_cone_immutable(
                g,
                seed=src,
                depth_limit=depth_limit,
                max_nodes=max_nodes,
                max_edges=max_edges,
            )
            meta.update(d_meta)
            meta["seed_is_disjunction"] = True
        elif sample_mode == "random-forward-cone":
            nodes, edges, truncated = collect_forward_cone_immutable(
                g, seed=src, depth_limit=depth_limit, max_nodes=max_nodes, max_edges=max_edges
            )
            anchor_exit = src
        elif sample_mode == "random-bi-cone":
            tgt_idx = _weighted_choice_index(rng, high_in_weights)
            target = node_ids[tgt_idx]
            nodes, edges, truncated, meta = collect_bi_cone_immutable(
                g, source=src, target=target, depth_limit=depth_limit, max_nodes=max_nodes, max_edges=max_edges
            )
            if not nodes:
                # No meaningful source->target overlap; resample.
                continue
            anchor_exit = target
        elif sample_mode == "random-scc":
            comps = scc_components_cache if scc_components_cache is not None else kosaraju_components(projected)
            if not comps:
                nodes = {src}
            else:
                comp_sizes = [max(1, len(c)) for c in comps]
                comp_idx = _weighted_choice_index(rng, [float(x) for x in comp_sizes])
                comp = comps[comp_idx]
                comp_nodes = set(comp)
                if len(comp_nodes) > max_nodes:
                    chosen_anchor = comp[rng.randrange(len(comp))]
                    comp_nodes, cut_n = _trim_nodes_by_bfs(g, comp_nodes, [chosen_anchor], max_nodes=max_nodes)
                    truncated = truncated or cut_n
                nodes = comp_nodes
                meta["scc_size"] = len(comp)
            edges = _closed_edges_for_nodes(g, nodes)
            edges, cut_e = _limit_edges(edges, max_edges)
            truncated = truncated or cut_e
            anchor_entry = src
            anchor_exit = src
        elif sample_mode == "random-supported-siso":
            regions = supported_regions_cache if supported_regions_cache is not None else []
            if regions:
                r = regions[rng.randrange(len(regions))]
                nodes = set(r.internal_nodes)
                edges = set(r.internal_edges)
                anchor_entry = r.entry
                anchor_exit = r.exit
                meta["supported_kind"] = r.kind
            else:
                nodes = {src}
                edges = set()
                meta["supported_empty"] = True
                anchor_exit = src
        else:
            nodes = {src}
            edges = set()
            anchor_exit = src
            meta["unsupported_mode"] = sample_mode

        if not nodes:
            nodes = {src}
        nodes, cut_n = _trim_nodes_by_bfs(g, nodes, anchors=[src, target], max_nodes=max_nodes)
        # Always render the induced edge set of the sampled nodes to avoid "missing edges" in visualization.
        edges = _closed_edges_for_nodes(g, nodes)
        edges, cut_e = _limit_edges(edges, max_edges)
        truncated = truncated or cut_n or cut_e
        nodes, edges, cut_comp = _prune_to_best_component(g, nodes, edges, anchors=[src, target])
        truncated = truncated or cut_comp
        isolated_nodes = _count_isolated_nodes(g, nodes, edges)
        if isolated_nodes > 0:
            # Skip sparse/disconnected samples that are hard to interpret in the visual explorer.
            continue

        sub, old_to_new, _ = build_immutable_subgraph(g, nodes, edges)
        sub_stats = sub.basic_stats()
        if sub_stats.get("nodes", 0) < max(0, min_nodes) or sub_stats.get("edges", 0) < max(0, min_edges):
            continue
        sub_scc = kosaraju_scc(sub.projected_adjacency())
        sub_mut = MutableDerivationGraph.from_immutable(sub)
        _, sub_siso_meta = detect_supported_siso_regions(sub_mut)

        entry_new = old_to_new.get(anchor_entry) if anchor_entry is not None else None
        exit_new = old_to_new.get(anchor_exit) if anchor_exit is not None else None
        region_graph = (
            build_region_graph_payload(
                sub,
                set(range(sub.num_nodes)),
                set(range(sub.num_edges)),
                entry=entry_new,
                exit_node=exit_new,
            )
            if include_region_graph
            else None
        )
        entry_in_sample = (anchor_entry is not None and anchor_entry in nodes)
        exit_in_sample = (anchor_exit is not None and anchor_exit in nodes)
        entry_name = (
            g.node_names[anchor_entry]
            if entry_in_sample and anchor_entry is not None
            else "(none)"
        )
        exit_name = (
            g.node_names[anchor_exit]
            if exit_in_sample and anchor_exit is not None
            else "(none)"
        )
        row = {
            "sample_id": sid,
            "sample_mode": sample_mode,
            "entry": entry_name,
            "exit": exit_name,
            "node_count": sub_stats.get("nodes", 0),
            "edge_count": sub_stats.get("edges", 0),
            "random_vars": sub_stats.get("random_variables", 0),
            "entry_pred_count": 0,
            "mode": f"explore-{sample_mode}",
            "candidate_exit": exit_name,
            "kind": f"Explore/{sample_mode}",
            "shape": "explore",
            "has_probabilistic": bool(sub_stats.get("random_variables", 0) > 0),
            "region_graph": region_graph,
            "cone_nodes": sub_stats.get("nodes", 0),
            "cone_edges": sub_stats.get("edges", 0),
            "cone_truncated": truncated,
            "graph_stats": sub_stats,
            "scc_stats": sub_scc,
            "supported_siso_in_sample": sub_siso_meta.get("kept_by_kind", {}),
            "meta": {
                **meta,
                "seed_node": g.node_names[src],
                "target_node": g.node_names[target],
                "depth_limit": depth_limit,
                "max_nodes": max_nodes,
                "max_edges": max_edges,
                "seed_policy": seed_policy,
                "truncated": truncated,
                "edge_projection": "induced",
                "entry_in_sample": entry_in_sample,
                "exit_in_sample": exit_in_sample,
            },
        }
        samples.append(row)

    return {
        "mode": mode,
        "sample_count": sample_count,
        "sample_count_generated": len(samples),
        "seed": random_seed,
        "seed_policy": seed_policy,
        "depth_limit": depth_limit,
        "max_nodes": max_nodes,
        "max_edges": max_edges,
        "min_nodes": min_nodes,
        "min_edges": min_edges,
        "attempts": attempts,
        "max_attempts": max_try,
        "samples": samples,
        "timing_ms": {"total": _now_ms() - t_start},
    }


def maybe_confirm_full_graph_render(
    *,
    enabled: bool,
    graph_stats: Dict[str, object],
    warn_nodes: int,
    warn_edges: int,
    force: bool,
) -> None:
    if not enabled:
        return
    nodes = int(graph_stats.get("nodes", 0))
    edges = int(graph_stats.get("edges", 0))
    if nodes <= warn_nodes and edges <= warn_edges:
        return
    if force:
        print(
            "[full-graph] "
            f"force=1 skip confirmation (nodes={nodes}, edges={edges}, "
            f"warn_nodes={warn_nodes}, warn_edges={warn_edges})"
        )
        return
    prompt = (
        f"[full-graph] Large graph detected (nodes={nodes}, edges={edges}; "
        f"warn_nodes={warn_nodes}, warn_edges={warn_edges}). Continue rendering full graph? [y/N]: "
    )
    if not sys.stdin.isatty():
        raise SystemExit(
            "full-graph exceeds warning threshold in non-interactive shell; "
            "re-run with --full-graph-force to continue"
        )
    answer = input(prompt).strip().lower()
    if answer not in ("y", "yes"):
        raise SystemExit("aborted full-graph interactive rendering")


def build_full_graph_view(
    g: DerivationHyperGraph,
    include_region_graph: bool,
) -> Dict[str, object]:
    t0 = _now_ms()
    stats = g.basic_stats()
    scc = kosaraju_scc(g.projected_adjacency())
    row = {
        "sample_id": 1,
        "sample_mode": "full-graph",
        "entry": "(none)",
        "exit": "(none)",
        "node_count": stats.get("nodes", 0),
        "edge_count": stats.get("edges", 0),
        "random_vars": stats.get("random_variables", 0),
        "entry_pred_count": 0,
        "mode": "explore-full-graph",
        "candidate_exit": "(none)",
        "kind": "Explore/full-graph",
        "shape": "full-graph",
        "has_probabilistic": bool(stats.get("random_variables", 0) > 0),
        "region_graph": (
            build_region_graph_payload(
                g,
                set(range(g.num_nodes)),
                set(range(g.num_edges)),
                entry=None,
                exit_node=None,
            )
            if include_region_graph
            else None
        ),
        "cone_nodes": stats.get("nodes", 0),
        "cone_edges": stats.get("edges", 0),
        "cone_truncated": False,
        "graph_stats": stats,
        "scc_stats": scc,
        "supported_siso_in_sample": {},
        "meta": {
            "truncated": False,
            "edge_projection": "full",
        },
    }
    return {
        "mode": "full-graph",
        "sample_count": 1,
        "sample_count_generated": 1,
        "samples": [row],
        "timing_ms": {"total": _now_ms() - t0},
    }


def _snapshot_stats_mutable(g: MutableDerivationGraph) -> Dict[str, object]:
    snap = g.to_immutable_snapshot()
    scc = kosaraju_scc(snap.projected_adjacency())
    return {
        "graph": snap.basic_stats(),
        "scc": scc,
    }


def _region_signature(row: Dict[str, object]) -> Optional[Tuple[str, str, Tuple[Tuple[str, Tuple[str, ...], Tuple[int, ...]], ...]]]:
    entry = str(row.get("entry", ""))
    exit_node = str(row.get("exit", ""))
    rg = row.get("region_graph")
    if not isinstance(rg, dict):
        return None
    edges = rg.get("edges", [])
    if not isinstance(edges, list):
        return None
    edge_tokens: List[Tuple[str, Tuple[str, ...], Tuple[int, ...]]] = []
    for e in edges:
        if not isinstance(e, dict):
            continue
        head = str(e.get("head", ""))
        inputs_raw = e.get("inputs", [])
        neg_raw = e.get("negated_inputs", [])
        inputs = tuple(str(x) for x in inputs_raw) if isinstance(inputs_raw, list) else tuple()
        neg = tuple((1 if bool(x) else 0) for x in neg_raw) if isinstance(neg_raw, list) else tuple()
        edge_tokens.append((head, inputs, neg))
    edge_tokens.sort()
    return (entry, exit_node, tuple(edge_tokens))


def filter_general_regions_against_fast(
    general_rows: List[Dict[str, object]],
    fast_rows: List[Dict[str, object]],
) -> Tuple[List[Dict[str, object]], Dict[str, object]]:
    fast_pairs: Set[Tuple[str, str]] = {
        (str(r.get("entry", "")), str(r.get("exit", ""))) for r in fast_rows if isinstance(r, dict)
    }
    fast_sigs: Set[Tuple[str, str, Tuple[Tuple[str, Tuple[str, ...], Tuple[int, ...]], ...]]] = set()
    for r in fast_rows:
        if not isinstance(r, dict):
            continue
        sig = _region_signature(r)
        if sig is not None:
            fast_sigs.add(sig)

    kept: List[Dict[str, object]] = []
    filtered_by_pair = 0
    filtered_by_sig = 0
    for r in general_rows:
        if not isinstance(r, dict):
            continue
        pair = (str(r.get("entry", "")), str(r.get("exit", "")))
        if pair in fast_pairs:
            filtered_by_pair += 1
            continue
        sig = _region_signature(r)
        if sig is not None and sig in fast_sigs:
            filtered_by_sig += 1
            continue
        kept.append(r)

    return kept, {
        "input_regions": len(general_rows),
        "filtered_by_entry_exit": filtered_by_pair,
        "filtered_by_exact_signature": filtered_by_sig,
        "kept_regions": len(kept),
    }


def _build_interactive_rewrite_state(
    g: MutableDerivationGraph,
    state_index: int,
    label: str,
    include_region_graph: bool,
    rewrite_action: Optional[Dict[str, object]],
    general_siso_mode: str,
    max_dom_nodes: int,
    max_candidates: int,
    min_random_vars: int,
    cone_depth: int,
    cone_node_limit: int,
    region_limit: int,
    explore_mode: str,
    explore_sample_count: int,
    explore_seed: int,
    explore_seed_policy: str,
    explore_depth: int,
    explore_max_nodes: int,
    explore_max_edges: int,
    explore_min_nodes: int,
    explore_min_edges: int,
    explore_max_attempts: int,
) -> Dict[str, object]:
    snap = g.to_immutable_snapshot()
    full_view = build_full_graph_view(snap, include_region_graph=include_region_graph)
    full_rows = full_view.get("samples", [])
    full_row = full_rows[0] if isinstance(full_rows, list) and full_rows else {}

    fast = analyze_supported_siso(
        g,
        include_region_graph=include_region_graph,
        mode_label=f"state-{state_index}-fast-path",
    )
    fast_rows = fast.get("regions", []) if isinstance(fast, dict) else []
    if not isinstance(fast_rows, list):
        fast_rows = []
    fast_rows = [r for r in fast_rows if isinstance(r, dict)]

    general_result: Dict[str, object] = {
        "enabled": False,
        "mode": "off",
        "regions_found": 0,
        "regions_after_fast_filter": 0,
        "timing_ms": {},
        "filter_meta": {
            "input_regions": 0,
            "filtered_by_entry_exit": 0,
            "filtered_by_exact_signature": 0,
            "kept_regions": 0,
        },
    }
    general_rows_filtered: List[Dict[str, object]] = []
    if general_siso_mode != "off":
        g_general = analyze_general_siso(
            snap,
            mode=general_siso_mode,
            max_dom_nodes=max_dom_nodes,
            max_candidates=max_candidates,
            min_random_vars=min_random_vars,
            cone_depth=cone_depth,
            cone_node_limit=cone_node_limit,
            region_limit=region_limit,
            include_region_graph=include_region_graph,
        )
        general_rows = g_general.get("regions", []) if isinstance(g_general, dict) else []
        if not isinstance(general_rows, list):
            general_rows = []
        general_rows = [r for r in general_rows if isinstance(r, dict)]
        general_rows_filtered, filter_meta = filter_general_regions_against_fast(general_rows, fast_rows)
        general_result = {
            "enabled": True,
            "mode": str(g_general.get("mode", general_siso_mode)),
            "regions_found": len(general_rows),
            "regions_after_fast_filter": len(general_rows_filtered),
            "timing_ms": g_general.get("timing_ms", {}),
            "filter_meta": filter_meta,
        }

    explore_rows: List[Dict[str, object]] = []
    explore_meta: Dict[str, object] = {
        "enabled": False,
        "mode": "off",
        "sample_count": 0,
        "timing_ms": {},
    }
    if explore_mode != "off":
        explore_result = analyze_random_explore_samples(
            snap,
            mode=explore_mode,
            sample_count=explore_sample_count,
            random_seed=explore_seed,
            seed_policy=explore_seed_policy,
            depth_limit=explore_depth,
            max_nodes=explore_max_nodes,
            max_edges=explore_max_edges,
            min_nodes=explore_min_nodes,
            min_edges=explore_min_edges,
            max_attempts=explore_max_attempts,
            include_region_graph=include_region_graph,
        )
        rows = explore_result.get("samples", []) if isinstance(explore_result, dict) else []
        if not isinstance(rows, list):
            rows = []
        explore_rows = [r for r in rows if isinstance(r, dict)]
        explore_meta = {
            "enabled": True,
            "mode": str(explore_result.get("mode", explore_mode)),
            "sample_count": len(explore_rows),
            "timing_ms": explore_result.get("timing_ms", {}),
        }

    return {
        "state_index": state_index,
        "label": label,
        "full_graph": full_row,
        "fast_regions": fast_rows,
        "fast_meta": {
            "regions_found": fast.get("regions_found", len(fast_rows)) if isinstance(fast, dict) else len(fast_rows),
            "candidates_tested": fast.get("candidates_tested", len(fast_rows)) if isinstance(fast, dict) else len(fast_rows),
            "timing_ms": fast.get("timing_ms", {}) if isinstance(fast, dict) else {},
            "kept_by_kind": fast.get("kept_by_kind", {}) if isinstance(fast, dict) else {},
        },
        "general_regions": general_rows_filtered,
        "general_meta": general_result,
        "explore_regions": explore_rows,
        "explore_meta": explore_meta,
        "graph_stats": full_row.get("graph_stats", {}) if isinstance(full_row, dict) else {},
        "scc_stats": full_row.get("scc_stats", {}) if isinstance(full_row, dict) else {},
        "rewrite_action": rewrite_action if rewrite_action is not None else {},
    }


def _remove_isolated_facts(g: MutableDerivationGraph) -> int:
    removed = 0
    for nid in g.active_node_ids():
        if not g.has_node(nid):
            continue
        n = g.nodes[nid]
        if not n.is_fact:
            continue
        if n.need_output or n.has_evidence:
            continue
        if g.is_isolated(nid):
            if g.remove_node(nid):
                removed += 1
    return removed


def _apply_region_single_hyperedge(g: MutableDerivationGraph, region: SupportedSisoRegion) -> bool:
    if len(region.internal_edges) != 1:
        return False
    eid = next(iter(region.internal_edges))
    if not g.has_edge(eid):
        return False
    e = g.edges[eid]
    if not g.has_node(region.entry) or not g.has_node(region.exit):
        return False
    p = MutableDerivationGraph._clamp_prob(float(e.probability))
    entry_neg = False
    absorbed_inputs: List[int] = []
    for i, src in enumerate(e.inputs):
        if src == region.entry:
            entry_neg = bool(e.negated_inputs[i] if i < len(e.negated_inputs) else False)
            continue
        if not g.has_node(src):
            continue
        n = g.nodes[src]
        if not n.is_fact:
            continue
        neg = bool(e.negated_inputs[i] if i < len(e.negated_inputs) else False)
        np = MutableDerivationGraph._clamp_prob(float(n.probability))
        p *= (1.0 - np) if neg else np
        absorbed_inputs.append(src)
    g.add_edge([region.entry], region.exit, [entry_neg], p)
    g.remove_edge(eid)
    for src in absorbed_inputs:
        if g.has_node(src) and g.is_isolated(src):
            g.remove_node(src)
    return True


def _apply_region_all_facts_to_so(g: MutableDerivationGraph, region: SupportedSisoRegion) -> bool:
    if len(region.internal_edges) != 1:
        return False
    eid = next(iter(region.internal_edges))
    if not g.has_edge(eid) or not g.has_node(region.exit):
        return False
    e = g.edges[eid]
    p = MutableDerivationGraph._clamp_prob(float(e.probability))
    for i, src in enumerate(e.inputs):
        if not g.has_node(src):
            continue
        n = g.nodes[src]
        np = MutableDerivationGraph._clamp_prob(float(n.probability))
        neg = bool(e.negated_inputs[i] if i < len(e.negated_inputs) else False)
        p *= (1.0 - np) if neg else np
    exit_node = g.nodes[region.exit]
    exit_node.is_fact = True
    exit_node.probability = MutableDerivationGraph._clamp_prob(p)
    g.remove_edge(eid)
    for src in e.inputs:
        if g.has_node(src) and g.is_isolated(src):
            g.remove_node(src)
    return True


def _apply_region_linear_two_edge(g: MutableDerivationGraph, region: SupportedSisoRegion) -> bool:
    if len(region.internal_edges) != 2:
        return False
    if not g.has_node(region.entry) or not g.has_node(region.exit):
        return False
    eids = [eid for eid in region.internal_edges if g.has_edge(eid)]
    if len(eids) != 2:
        return False
    into_mid: Optional[int] = None
    out_mid: Optional[int] = None
    mid: Optional[int] = None
    for eid in eids:
        e = g.edges[eid]
        if e.head != region.entry and e.head != region.exit:
            mid = e.head
            into_mid = eid
            break
    if mid is None:
        return False
    for eid in eids:
        if eid == into_mid:
            continue
        e = g.edges[eid]
        if len(e.inputs) == 1 and e.inputs[0] == mid:
            out_mid = eid
            break
    if into_mid is None or out_mid is None:
        return False
    e1 = g.edges[into_mid]
    e2 = g.edges[out_mid]
    if len(e1.inputs) != 1 or e1.inputs[0] != region.entry:
        return False
    if len(e2.inputs) != 1 or e2.inputs[0] != mid:
        return False
    entry_neg = bool(e1.negated_inputs[0]) if e1.negated_inputs else False
    if e2.negated_inputs and bool(e2.negated_inputs[0]):
        return False
    p = MutableDerivationGraph._clamp_prob(e1.probability) * MutableDerivationGraph._clamp_prob(e2.probability)
    g.add_edge([region.entry], region.exit, [entry_neg], p)
    g.remove_edge(into_mid)
    g.remove_edge(out_mid)
    if g.has_node(mid) and g.is_isolated(mid):
        g.remove_node(mid)
    return True


def _apply_region_parallel_edge(g: MutableDerivationGraph, region: SupportedSisoRegion) -> bool:
    if len(region.internal_edges) < 2:
        return False
    if not g.has_node(region.entry) or not g.has_node(region.exit):
        return False
    neg_flag: Optional[bool] = None
    prod = 1.0
    active_edges: List[int] = []
    for eid in sorted(region.internal_edges):
        if not g.has_edge(eid):
            return False
        e = g.edges[eid]
        if len(e.inputs) != 1 or e.inputs[0] != region.entry or e.head != region.exit:
            return False
        cur_neg = bool(e.negated_inputs[0]) if e.negated_inputs else False
        if neg_flag is None:
            neg_flag = cur_neg
        elif neg_flag != cur_neg:
            return False
        p = MutableDerivationGraph._clamp_prob(e.probability)
        prod *= (1.0 - p)
        active_edges.append(eid)
    p_eff = MutableDerivationGraph._clamp_prob(1.0 - prod)
    g.add_edge([region.entry], region.exit, [bool(neg_flag)], p_eff)
    for eid in active_edges:
        g.remove_edge(eid)
    return True


def _apply_region_fan_out_converge(g: MutableDerivationGraph, region: SupportedSisoRegion) -> bool:
    if len(region.internal_edges) < 3:
        return False
    if not g.has_node(region.entry) or not g.has_node(region.exit):
        return False
    entry = region.entry
    exit_node = region.exit
    fan_edges: List[int] = []
    conv_edge: Optional[int] = None
    for eid in region.internal_edges:
        if not g.has_edge(eid):
            continue
        e = g.edges[eid]
        if len(e.inputs) == 1 and e.inputs[0] == entry:
            fan_edges.append(eid)
        else:
            conv_edge = eid
    if conv_edge is None or len(fan_edges) < 2 or not g.has_edge(conv_edge):
        return False
    conv = g.edges[conv_edge]
    fan_outputs: Set[int] = set()
    for eid in fan_edges:
        if not g.has_edge(eid):
            return False
        fan_outputs.add(g.edges[eid].head)
    if set(conv.inputs) != fan_outputs:
        return False
    if any(bool(x) for x in conv.negated_inputs):
        return False

    first_neg: Optional[bool] = None
    mixed = False
    for eid in fan_edges:
        fe = g.edges[eid]
        cur_neg = bool(fe.negated_inputs[0]) if fe.negated_inputs else False
        if first_neg is None:
            first_neg = cur_neg
        elif first_neg != cur_neg:
            mixed = True
            break

    removed_nodes = []
    for eid in fan_edges:
        if g.has_edge(eid):
            out = g.edges[eid].head
            g.remove_edge(eid)
            removed_nodes.append(out)
    if g.has_edge(conv_edge):
        g.remove_edge(conv_edge)
    for nid in removed_nodes:
        if g.has_node(nid) and g.is_isolated(nid):
            g.remove_node(nid)

    if mixed:
        if g.has_node(entry) and g.is_isolated(entry):
            g.remove_node(entry)
        return True

    if first_neg is None:
        first_neg = False
    entry_node = g.nodes[entry]
    p_entry = MutableDerivationGraph._clamp_prob(entry_node.probability)
    p = (1.0 - p_entry) if first_neg else p_entry
    for eid in fan_edges:
        if eid < len(g.edges):
            old_prob = MutableDerivationGraph._clamp_prob(g.edges[eid].probability)
            p *= old_prob
    p *= MutableDerivationGraph._clamp_prob(conv.probability)
    entry_node.is_fact = True
    entry_node.probability = MutableDerivationGraph._clamp_prob(p)
    g.add_edge([entry], exit_node, [first_neg], 1.0)
    return True


def apply_supported_region_rewrite(g: MutableDerivationGraph, region: SupportedSisoRegion) -> bool:
    if region.kind == "SingleHyperedge":
        return _apply_region_single_hyperedge(g, region)
    if region.kind == "LinearTwoEdge":
        return _apply_region_linear_two_edge(g, region)
    if region.kind == "ParallelEdge":
        return _apply_region_parallel_edge(g, region)
    if region.kind == "AllFactsToSO":
        return _apply_region_all_facts_to_so(g, region)
    if region.kind == "FanOutConverge":
        return _apply_region_fan_out_converge(g, region)
    return False


def run_compaction_pass(g: MutableDerivationGraph) -> Dict[str, object]:
    t0 = _now_ms()
    compacted = 0
    removed_edges = 0
    added_edges = 0
    for eid in list(g.active_edge_ids()):
        if not g.has_edge(eid):
            continue
        e = g.edges[eid]
        if not e.inputs:
            continue
        keep_inputs: List[int] = []
        keep_negs: List[bool] = []
        p = MutableDerivationGraph._clamp_prob(e.probability)
        changed = False
        for i, src in enumerate(e.inputs):
            neg = bool(e.negated_inputs[i] if i < len(e.negated_inputs) else False)
            if g.has_node(src):
                n = g.nodes[src]
                outs = g.outgoing_edges(src)
                if n.is_fact and (not n.has_evidence) and (not n.need_output) and len(outs) == 1 and outs[0] == eid:
                    np = MutableDerivationGraph._clamp_prob(n.probability)
                    p *= (1.0 - np) if neg else np
                    changed = True
                    continue
            keep_inputs.append(src)
            keep_negs.append(neg)
        if (not changed) or (not keep_inputs):
            continue
        g.add_edge(keep_inputs, e.head, keep_negs, p)
        if g.remove_edge(eid):
            removed_edges += 1
        added_edges += 1
        compacted += 1
    cleanup_removed_nodes = _remove_isolated_facts(g)
    return {
        "applied": compacted > 0,
        "compacted_edges": compacted,
        "removed_edges": removed_edges,
        "added_edges": added_edges,
        "removed_nodes": cleanup_removed_nodes,
        "time_ms": _now_ms() - t0,
    }


def split_fanout_naive(g: MutableDerivationGraph, max_reachable: int = 50) -> Dict[str, object]:
    t0 = _now_ms()
    nodes_added = 0
    edges_rewritten = 0
    for fact_id in g.active_node_ids():
        if not g.has_node(fact_id):
            continue
        fact = g.nodes[fact_id]
        if (not fact.is_fact) or fact.has_evidence or fact.need_output:
            continue
        outs = g.outgoing_edges(fact_id)
        if len(outs) < 2:
            continue

        reach_sets: List[Set[int]] = []
        skip = False
        for eid in outs:
            if not g.has_edge(eid):
                skip = True
                break
            start = g.edges[eid].head
            if not g.has_node(start):
                skip = True
                break
            visited: Set[int] = {start}
            q: deque[int] = deque([start])
            while q:
                cur = q.popleft()
                for oeid in g.outgoing_edges(cur):
                    if not g.has_edge(oeid):
                        continue
                    out_node = g.edges[oeid].head
                    if not g.has_node(out_node):
                        continue
                    if out_node not in visited:
                        visited.add(out_node)
                        if len(visited) > max_reachable:
                            skip = True
                            break
                        q.append(out_node)
                if skip:
                    break
            if skip:
                break
            reach_sets.append(visited)
        if skip or len(reach_sets) != len(outs):
            continue

        node_owner: Dict[int, int] = {}
        has_overlap = [False] * len(reach_sets)
        for i, rset in enumerate(reach_sets):
            for nid in rset:
                prev = node_owner.get(nid)
                if prev is None:
                    node_owner[nid] = i
                    continue
                if prev == i:
                    continue
                has_overlap[i] = True
                if prev >= 0:
                    has_overlap[prev] = True
                    node_owner[nid] = -1

        independent_idx = [i for i, over in enumerate(has_overlap) if not over]
        if not independent_idx:
            continue

        for idx in independent_idx:
            old_eid = outs[idx]
            if not g.has_edge(old_eid):
                continue
            old_edge = g.edges[old_eid]
            if fact_id not in old_edge.inputs:
                continue
            shadow = g.add_shadow_node(fact_id, old_eid)
            if shadow is None:
                continue
            new_inputs = [shadow if src == fact_id else src for src in old_edge.inputs]
            g.add_edge(new_inputs, old_edge.head, old_edge.negated_inputs, old_edge.probability)
            if g.remove_edge(old_eid):
                edges_rewritten += 1
            nodes_added += 1

    return {
        "applied": (nodes_added > 0 or edges_rewritten > 0),
        "mode": "naive",
        "nodes_added": nodes_added,
        "edges_rewritten": edges_rewritten,
        "time_ms": _now_ms() - t0,
    }


def split_fanout_complete(
    g: MutableDerivationGraph,
    max_new_nodes_per_pass: int = 5000,
    max_new_edges_per_pass: int = 50000,
    max_groups_per_node: int = 2,
    min_group_edges: int = 1,
) -> Dict[str, object]:
    t0 = _now_ms()
    if max_groups_per_node < 2 or max_new_nodes_per_pass <= 0 or max_new_edges_per_pass <= 0:
        return {
            "applied": False,
            "mode": "complete",
            "nodes_added": 0,
            "edges_rewritten": 0,
            "time_ms": _now_ms() - t0,
        }

    active_nodes = g.active_node_ids()
    if not active_nodes:
        return {
            "applied": False,
            "mode": "complete",
            "nodes_added": 0,
            "edges_rewritten": 0,
            "time_ms": _now_ms() - t0,
        }

    node_idx = {nid: i for i, nid in enumerate(active_nodes)}
    has_rv_reach = [False] * len(active_nodes)
    q: deque[int] = deque()

    def mark_node(nid: int) -> None:
        idx = node_idx.get(nid)
        if idx is None:
            return
        if not has_rv_reach[idx]:
            has_rv_reach[idx] = True
            q.append(idx)

    for nid in active_nodes:
        n = g.nodes[nid]
        if n.is_fact and DerivationHyperGraph._is_probabilistic(n.probability):
            mark_node(nid)
    for eid in g.active_edge_ids():
        e = g.edges[eid]
        if DerivationHyperGraph._is_probabilistic(e.probability):
            for src in e.inputs:
                mark_node(src)
    while q:
        idx = q.popleft()
        cur = active_nodes[idx]
        for ieid in g.incoming_edges(cur):
            if not g.has_edge(ieid):
                continue
            for src in g.edges[ieid].inputs:
                mark_node(src)

    class UF:
        def __init__(self, n: int) -> None:
            self.parent = list(range(n))
            self.size = [1] * n

        def find(self, x: int) -> int:
            while self.parent[x] != x:
                self.parent[x] = self.parent[self.parent[x]]
                x = self.parent[x]
            return x

        def union(self, a: int, b: int) -> None:
            ra = self.find(a)
            rb = self.find(b)
            if ra == rb:
                return
            if self.size[ra] < self.size[rb]:
                ra, rb = rb, ra
            self.parent[rb] = ra
            self.size[ra] += self.size[rb]

    k_none = -1
    k_multi = -2
    nodes_added = 0
    edges_rewritten = 0
    remaining_nodes = max_new_nodes_per_pass
    remaining_edges = max_new_edges_per_pass

    queue_facts: deque[int] = deque()
    in_queue: Set[int] = set()
    for nid in active_nodes:
        if not g.has_node(nid):
            continue
        n = g.nodes[nid]
        if (not n.is_fact) or n.has_evidence or n.need_output:
            continue
        if len(g.outgoing_edges(nid)) < 2:
            continue
        idx = node_idx.get(nid)
        if idx is None or not has_rv_reach[idx]:
            continue
        queue_facts.append(nid)
        in_queue.add(nid)

    while queue_facts and remaining_nodes > 0 and remaining_edges > 0:
        fact = queue_facts.popleft()
        in_queue.discard(fact)
        if not g.has_node(fact):
            continue
        nfact = g.nodes[fact]
        if (not nfact.is_fact) or nfact.has_evidence or nfact.need_output:
            continue
        outs = g.outgoing_edges(fact)
        if len(outs) < 2:
            continue
        idx_fact = node_idx.get(fact)
        if idx_fact is None or not has_rv_reach[idx_fact]:
            continue

        uf = UF(len(outs))
        owner: Dict[int, int] = {}
        rep: Dict[int, int] = {}
        work: deque[int] = deque()

        def assign(nid: int, incoming_owner: int, incoming_rep: int) -> None:
            if nid not in owner:
                owner[nid] = incoming_owner
                rep[nid] = incoming_rep if incoming_owner == k_multi else incoming_owner
                work.append(nid)
                return
            if owner[nid] == incoming_owner:
                return
            old_rep = rep[nid]
            if owner[nid] != k_multi:
                owner[nid] = k_multi
                rep[nid] = old_rep
                work.append(nid)
            inc_rep = incoming_rep if incoming_owner == k_multi else incoming_owner
            if old_rep >= 0 and inc_rep >= 0:
                uf.union(old_rep, inc_rep)

        for i, eid in enumerate(outs):
            if not g.has_edge(eid):
                continue
            out_node = g.edges[eid].head
            if not g.has_node(out_node):
                continue
            assign(out_node, i, i)

        while work:
            cur = work.popleft()
            ox = owner.get(cur, k_none)
            rx = rep.get(cur, k_none)
            if ox == k_none:
                continue
            for oeid in g.outgoing_edges(cur):
                if not g.has_edge(oeid):
                    continue
                out_node = g.edges[oeid].head
                if not g.has_node(out_node):
                    continue
                if ox == k_multi:
                    assign(out_node, k_multi, rx)
                else:
                    assign(out_node, ox, ox)

        groups_map: Dict[int, List[int]] = {}
        for i, eid in enumerate(outs):
            root = uf.find(i)
            groups_map.setdefault(root, []).append(eid)
        groups = list(groups_map.values())
        if len(groups) <= 1:
            continue
        groups.sort(key=lambda x: len(x), reverse=True)

        mg = max(1, min_group_edges)
        if mg > 1 and len(groups) > 1:
            filtered: List[List[int]] = [list(groups[0])]
            for grp in groups[1:]:
                if len(grp) < mg:
                    filtered[0].extend(grp)
                else:
                    filtered.append(grp)
            groups = filtered
        if len(groups) <= 1:
            continue

        if len(groups) > max_groups_per_node:
            for grp in groups[max_groups_per_node:]:
                groups[0].extend(grp)
            groups = groups[:max_groups_per_node]
        if len(groups) <= 1:
            continue

        selected: List[List[int]] = [list(groups[0])]
        for grp in groups[1:]:
            if remaining_nodes <= 0 or remaining_edges < len(grp):
                selected[0].extend(grp)
                continue
            selected.append(list(grp))
            remaining_nodes -= 1
            remaining_edges -= len(grp)
        groups = selected
        if len(groups) <= 1:
            continue

        for grp in groups[1:]:
            if not grp:
                continue
            shadow = g.add_shadow_node(fact, grp[0])
            if shadow is None:
                continue
            rewired = 0
            for old_eid in grp:
                if not g.has_edge(old_eid):
                    continue
                old_edge = g.edges[old_eid]
                if fact not in old_edge.inputs:
                    continue
                new_inputs = [shadow if src == fact else src for src in old_edge.inputs]
                g.add_edge(new_inputs, old_edge.head, old_edge.negated_inputs, old_edge.probability)
                if g.remove_edge(old_eid):
                    rewired += 1
            if rewired > 0:
                nodes_added += 1
                edges_rewritten += rewired

    return {
        "applied": (nodes_added > 0 or edges_rewritten > 0),
        "mode": "complete",
        "nodes_added": nodes_added,
        "edges_rewritten": edges_rewritten,
        "time_ms": _now_ms() - t0,
    }


def run_rewrite_fixpoint_simulation(
    base_graph: MutableDerivationGraph,
    split_mode: str,
    max_iterations: int,
    enable_compaction: bool,
    include_region_graph: bool,
    split_max_new_nodes_per_pass: int,
    split_max_new_edges_per_pass: int,
    split_max_groups_per_node: int,
    split_min_group_edges: int,
    capture_interactive_states: bool = False,
    interactive_state_include_region_graph: bool = False,
    interactive_general_siso_mode: str = "off",
    interactive_max_dom_nodes: int = 12000,
    interactive_max_candidates: int = 40,
    interactive_min_random_vars: int = 0,
    interactive_cone_depth: int = 16,
    interactive_cone_node_limit: int = 20000,
    interactive_region_limit: int = 200,
    interactive_explore_mode: str = "off",
    interactive_explore_samples: int = 12,
    interactive_explore_seed: int = 42,
    interactive_explore_seed_policy: str = "prob-hot",
    interactive_explore_depth: int = 10,
    interactive_explore_max_nodes: int = 400,
    interactive_explore_max_edges: int = 1200,
    interactive_explore_min_nodes: int = 2,
    interactive_explore_min_edges: int = 1,
    interactive_explore_max_attempts: int = 500,
) -> Dict[str, object]:
    g = base_graph.clone()
    history: List[RewriteIterationSummary] = []
    total_rewritten = 0
    total_rewritten_by_kind: Dict[str, int] = {k: 0 for k in SUPPORTED_SISO_KIND_ORDER}
    fixpoint = False
    interactive_states: List[Dict[str, object]] = []

    if capture_interactive_states:
        interactive_states.append(
            _build_interactive_rewrite_state(
                g=g,
                state_index=0,
                label="iter0-original",
                include_region_graph=interactive_state_include_region_graph,
                rewrite_action=None,
                general_siso_mode=interactive_general_siso_mode,
                max_dom_nodes=interactive_max_dom_nodes,
                max_candidates=interactive_max_candidates,
                min_random_vars=interactive_min_random_vars,
                cone_depth=interactive_cone_depth,
                cone_node_limit=interactive_cone_node_limit,
                region_limit=interactive_region_limit,
                explore_mode=interactive_explore_mode,
                explore_sample_count=interactive_explore_samples,
                explore_seed=interactive_explore_seed,
                explore_seed_policy=interactive_explore_seed_policy,
                explore_depth=interactive_explore_depth,
                explore_max_nodes=interactive_explore_max_nodes,
                explore_max_edges=interactive_explore_max_edges,
                explore_min_nodes=interactive_explore_min_nodes,
                explore_min_edges=interactive_explore_min_edges,
                explore_max_attempts=interactive_explore_max_attempts,
            )
        )

    for it in range(1, max(1, max_iterations) + 1):
        before = _snapshot_stats_mutable(g)
        regions, detect_meta = detect_supported_siso_regions(g)
        detected_by_kind = dict(detect_meta.get("kept_by_kind", {}))
        rewritten = 0
        rewritten_by_kind: Dict[str, int] = {k: 0 for k in SUPPORTED_SISO_KIND_ORDER}
        rewritten_rows: List[Dict[str, object]] = []

        for region in regions:
            row = _supported_region_to_dict(
                g,
                region,
                mode=f"rewrite-iter-{it}",
                include_region_graph=include_region_graph,
            )
            if apply_supported_region_rewrite(g, region):
                rewritten += 1
                rewritten_by_kind[region.kind] = rewritten_by_kind.get(region.kind, 0) + 1
                total_rewritten_by_kind[region.kind] = total_rewritten_by_kind.get(region.kind, 0) + 1
                rewritten_rows.append(row)

        compaction_applied = False
        compact_edges = 0
        compact_removed_edges = 0
        compact_added_edges = 0
        compact_time_ms = 0.0
        cleanup_removed_nodes = 0
        cleanup_time_ms = 0.0
        split_applied = False
        split_nodes_added = 0
        split_edges_rewritten = 0
        split_time_ms = 0.0
        split_mode_eff = split_mode

        if rewritten > 0 and enable_compaction:
            compact = run_compaction_pass(g)
            compaction_applied = bool(compact.get("applied", False))
            compact_edges = int(compact.get("compacted_edges", 0))
            compact_removed_edges = int(compact.get("removed_edges", 0))
            compact_added_edges = int(compact.get("added_edges", 0))
            compact_time_ms = float(compact.get("time_ms", 0.0))

        t_cleanup = _now_ms()
        cleanup_removed_nodes = _remove_isolated_facts(g)
        cleanup_time_ms = _now_ms() - t_cleanup

        if rewritten == 0:
            if split_mode == "naive":
                split = split_fanout_naive(g)
                split_applied = bool(split.get("applied", False))
                split_nodes_added = int(split.get("nodes_added", 0))
                split_edges_rewritten = int(split.get("edges_rewritten", 0))
                split_time_ms = float(split.get("time_ms", 0.0))
            elif split_mode == "complete":
                split = split_fanout_complete(
                    g,
                    max_new_nodes_per_pass=split_max_new_nodes_per_pass,
                    max_new_edges_per_pass=split_max_new_edges_per_pass,
                    max_groups_per_node=split_max_groups_per_node,
                    min_group_edges=split_min_group_edges,
                )
                split_applied = bool(split.get("applied", False))
                split_nodes_added = int(split.get("nodes_added", 0))
                split_edges_rewritten = int(split.get("edges_rewritten", 0))
                split_time_ms = float(split.get("time_ms", 0.0))
            else:
                split_mode_eff = "none"

        after = _snapshot_stats_mutable(g)
        history.append(
            RewriteIterationSummary(
                iteration=it,
                before_stats=before,
                detected_regions=len(regions),
                detected_by_kind={k: v for k, v in detected_by_kind.items() if v > 0},
                rewritten_regions=rewritten,
                rewritten_by_kind={k: v for k, v in rewritten_by_kind.items() if v > 0},
                split_applied=split_applied,
                split_mode=split_mode_eff,
                split_nodes_added=split_nodes_added,
                split_edges_rewritten=split_edges_rewritten,
                split_time_ms=split_time_ms,
                compaction_applied=compaction_applied,
                compact_edges=compact_edges,
                compact_removed_edges=compact_removed_edges,
                compact_added_edges=compact_added_edges,
                compact_time_ms=compact_time_ms,
                cleanup_removed_nodes=cleanup_removed_nodes,
                cleanup_time_ms=cleanup_time_ms,
                after_stats=after,
                regions=rewritten_rows,
            )
        )

        if capture_interactive_states:
            interactive_states.append(
                _build_interactive_rewrite_state(
                    g=g,
                    state_index=it,
                    label=f"iter{it}-after-rewrite",
                    include_region_graph=interactive_state_include_region_graph,
                    rewrite_action={
                        "iteration": it,
                        "detected_regions": len(regions),
                        "detected_by_kind": {k: v for k, v in detected_by_kind.items() if v > 0},
                        "rewritten_regions": rewritten,
                        "rewritten_by_kind": {k: v for k, v in rewritten_by_kind.items() if v > 0},
                        "split_applied": split_applied,
                        "split_mode": split_mode_eff,
                        "split_nodes_added": split_nodes_added,
                        "split_edges_rewritten": split_edges_rewritten,
                        "split_time_ms": split_time_ms,
                        "compaction_applied": compaction_applied,
                        "compact_edges": compact_edges,
                        "compact_removed_edges": compact_removed_edges,
                        "compact_added_edges": compact_added_edges,
                        "compact_time_ms": compact_time_ms,
                        "cleanup_removed_nodes": cleanup_removed_nodes,
                        "cleanup_time_ms": cleanup_time_ms,
                    },
                    general_siso_mode=interactive_general_siso_mode,
                    max_dom_nodes=interactive_max_dom_nodes,
                    max_candidates=interactive_max_candidates,
                    min_random_vars=interactive_min_random_vars,
                    cone_depth=interactive_cone_depth,
                    cone_node_limit=interactive_cone_node_limit,
                    region_limit=interactive_region_limit,
                    explore_mode=interactive_explore_mode,
                    explore_sample_count=interactive_explore_samples,
                    explore_seed=interactive_explore_seed,
                    explore_seed_policy=interactive_explore_seed_policy,
                    explore_depth=interactive_explore_depth,
                    explore_max_nodes=interactive_explore_max_nodes,
                    explore_max_edges=interactive_explore_max_edges,
                    explore_min_nodes=interactive_explore_min_nodes,
                    explore_min_edges=interactive_explore_min_edges,
                    explore_max_attempts=interactive_explore_max_attempts,
                )
            )

        total_rewritten += rewritten
        if rewritten == 0 and not split_applied:
            fixpoint = True
            break

    final_supported = analyze_supported_siso(g, include_region_graph=include_region_graph, mode_label="rewrite-final")
    total_rewritten_by_kind = {k: v for k, v in total_rewritten_by_kind.items() if v > 0}
    return {
        "enabled": True,
        "split_mode": split_mode,
        "max_iterations": max_iterations,
        "iterations": [it.__dict__ for it in history],
        "iterations_run": len(history),
        "fixpoint_reached": fixpoint,
        "regions_rewritten_total": total_rewritten,
        "rewritten_by_kind_total": total_rewritten_by_kind,
        "final_stats": _snapshot_stats_mutable(g),
        "final_supported_siso": final_supported,
        "interactive_states": interactive_states,
    }


def _predicate_of(atom_name: str) -> str:
    pos = atom_name.find("(")
    return atom_name if pos < 0 else atom_name[:pos]


def classify_region(
    g: DerivationHyperGraph,
    region_nodes: Set[int],
    region_edges: Set[int],
    entry: int,
    exit_node: int,
    random_vars: int,
) -> Tuple[str, str, bool]:
    in_deg: Dict[int, int] = {n: 0 for n in region_nodes}
    out_deg: Dict[int, int] = {n: 0 for n in region_nodes}
    for eid in region_edges:
        head = g.edge_heads[eid]
        if head in region_nodes:
            in_deg[head] += 1
        for src in g.edge_inputs[eid]:
            if src in region_nodes:
                out_deg[src] += 1

    branching = False
    for n in region_nodes:
        if n == entry or n == exit_node:
            continue
        if in_deg.get(n, 0) > 1 or out_deg.get(n, 0) > 1:
            branching = True
            break

    local_nodes = sorted(region_nodes)
    idx_of = {n: i for i, n in enumerate(local_nodes)}
    succ: List[List[int]] = [[] for _ in local_nodes]
    for eid in region_edges:
        head = g.edge_heads[eid]
        head_i = idx_of.get(head)
        if head_i is None:
            continue
        for src in g.edge_inputs[eid]:
            src_i = idx_of.get(src)
            if src_i is None:
                continue
            succ[src_i].append(head_i)
    scc_stats = kosaraju_scc(succ)
    cyclic = bool(scc_stats["cyclic_components"] > 0)

    if cyclic:
        shape = "cyclic"
    elif branching:
        shape = "branching"
    else:
        shape = "chain"

    probabilistic = random_vars > 0
    kind = ("probabilistic" if probabilistic else "deterministic") + f"-{shape}"
    return kind, shape, probabilistic


def build_region_graph_payload(
    g: DerivationHyperGraph,
    region_nodes: Set[int],
    region_edges: Set[int],
    entry: Optional[int] = None,
    exit_node: Optional[int] = None,
) -> Dict[str, object]:
    in_deg: Dict[int, int] = {n: 0 for n in region_nodes}
    out_deg: Dict[int, int] = {n: 0 for n in region_nodes}
    for eid in region_edges:
        head = g.edge_heads[eid]
        if head in region_nodes:
            in_deg[head] += 1
        for src in g.edge_inputs[eid]:
            if src in region_nodes:
                out_deg[src] += 1

    node_rows: List[Dict[str, object]] = []
    for n in sorted(region_nodes):
        nm = g.node_names[n]
        node_rows.append(
            {
                "name": nm,
                "predicate": _predicate_of(nm),
                "is_fact": g.is_fact[n],
                "fact_probability": g.fact_prob[n],
                "is_entry": (entry is not None and n == entry),
                "is_exit": (exit_node is not None and n == exit_node),
                "in_degree_region": in_deg.get(n, 0),
                "out_degree_region": out_deg.get(n, 0),
            }
        )

    edge_rows: List[Dict[str, object]] = []
    for eid in sorted(region_edges):
        head = g.edge_heads[eid]
        inputs = g.edge_inputs[eid]
        negs = g.edge_neg[eid]
        edge_rows.append(
            {
                "edge_id": eid,
                "head": g.node_names[head],
                "head_predicate": _predicate_of(g.node_names[head]),
                "inputs": [g.node_names[src] for src in inputs],
                "input_predicates": [_predicate_of(g.node_names[src]) for src in inputs],
                "negated_inputs": [bool(x) for x in negs],
                "probability": g.edge_prob[eid],
                "is_probabilistic": DerivationHyperGraph._is_probabilistic(g.edge_prob[eid]),
                "arity": len(inputs),
            }
        )

    predicates = sorted({row["predicate"] for row in node_rows})
    dep_rows: List[Tuple[str, str, bool]] = []
    for row in edge_rows:
        head_pred = str(row.get("head_predicate", ""))
        inp_preds = row.get("input_predicates", [])
        negs = row.get("negated_inputs", [])
        if not isinstance(inp_preds, list) or not isinstance(negs, list):
            continue
        for i, inp_pred in enumerate(inp_preds):
            dep_rows.append((str(inp_pred), head_pred, bool(negs[i] if i < len(negs) else False)))
    strata_stats = compute_predicate_strata_stats(dep_rows, predicates=predicates)
    return {
        "nodes": node_rows,
        "edges": edge_rows,
        "predicates": predicates,
        "strata_stats": strata_stats,
    }


def analyze_general_siso(
    g: DerivationHyperGraph,
    mode: str,
    max_dom_nodes: int,
    max_candidates: int,
    min_random_vars: int,
    cone_depth: int,
    cone_node_limit: int,
    region_limit: int,
    include_region_graph: bool,
) -> Dict[str, object]:
    def is_trivial_single_input_single_edge_region(region_edges: Set[int]) -> bool:
        if len(region_edges) != 1:
            return False
        eid = next(iter(region_edges))
        if eid < 0 or eid >= len(g.edge_inputs):
            return False
        inputs = g.edge_inputs[eid]
        if len(inputs) == 0:
            return True
        # Treat one-edge regions with <=1 non-fact driver as trivial:
        # they are effectively direct propagation with optional fact leaves.
        non_fact_inputs = 0
        for src in inputs:
            if 0 <= src < len(g.is_fact) and not g.is_fact[src]:
                non_fact_inputs += 1
                if non_fact_inputs > 1:
                    return False
        return True

    t_start = _now_ms()
    selected_mode = mode
    if selected_mode == "auto":
        selected_mode = "global" if g.num_nodes <= max_dom_nodes else "cone"

    candidates = top_exit_candidates(g, max_candidates)
    regions: List[RegionResult] = []
    tested = 0
    skipped_trivial_single_input_single_edge = 0
    timing_ms: Dict[str, float] = {}

    if selected_mode == "off":
        return {
            "mode": "off",
            "candidates_tested": 0,
            "regions_found": 0,
            "timing_ms": {"total": _now_ms() - t_start},
            "regions": [],
        }

    if selected_mode == "global":
        detector = GeneralSisoDetector(g)
        detector.prepare()
        timing_ms.update(detector.timings_ms)
        for exit_node in candidates:
            tested += 1
            found = detector.detect_for_exit(exit_node, min_random_vars=min_random_vars)
            if not found:
                continue
            entry, exit_real, nodes, edges, rv, entry_pred_count = found
            if is_trivial_single_input_single_edge_region(edges):
                skipped_trivial_single_input_single_edge += 1
                continue
            kind, shape, has_prob = classify_region(g, nodes, edges, entry, exit_real, rv)
            regions.append(
                RegionResult(
                    entry=g.node_names[entry],
                    exit=g.node_names[exit_real],
                    node_count=len(nodes),
                    edge_count=len(edges),
                    random_vars=rv,
                    entry_pred_count=entry_pred_count,
                    mode="global",
                    candidate_exit=g.node_names[exit_node],
                    kind=kind,
                    shape=shape,
                    has_probabilistic=has_prob,
                    region_graph=(
                        build_region_graph_payload(g, nodes, edges, entry, exit_real)
                        if include_region_graph
                        else None
                    ),
                )
            )
    elif selected_mode == "cone":
        prep_total = 0.0
        for exit_node in candidates:
            tested += 1
            cone_nodes, cone_edges, truncated = build_backward_cone(
                g, exit_node=exit_node, depth_limit=cone_depth, node_limit=cone_node_limit
            )
            if len(cone_nodes) < 2 or len(cone_edges) == 0:
                continue
            detector = GeneralSisoDetector(g, node_subset=cone_nodes, edge_subset=cone_edges)
            detector.prepare()
            prep_total += sum(detector.timings_ms.values())
            found = detector.detect_for_exit(exit_node, min_random_vars=min_random_vars)
            if not found:
                continue
            entry, exit_real, nodes, edges, rv, entry_pred_count = found
            if is_trivial_single_input_single_edge_region(edges):
                skipped_trivial_single_input_single_edge += 1
                continue
            kind, shape, has_prob = classify_region(g, nodes, edges, entry, exit_real, rv)
            regions.append(
                RegionResult(
                    entry=g.node_names[entry],
                    exit=g.node_names[exit_real],
                    node_count=len(nodes),
                    edge_count=len(edges),
                    random_vars=rv,
                    entry_pred_count=entry_pred_count,
                    mode="cone",
                    candidate_exit=g.node_names[exit_node],
                    kind=kind,
                    shape=shape,
                    has_probabilistic=has_prob,
                    region_graph=(
                        build_region_graph_payload(g, nodes, edges, entry, exit_real)
                        if include_region_graph
                        else None
                    ),
                    cone_nodes=len(cone_nodes),
                    cone_edges=len(cone_edges),
                    cone_truncated=truncated,
                )
            )
        timing_ms["cone_prepare_total"] = prep_total
    else:
        raise ValueError(f"Unsupported general-siso mode: {selected_mode}")

    uniq: Dict[Tuple[str, str], RegionResult] = {}
    for r in regions:
        key = (r.entry, r.exit)
        old = uniq.get(key)
        if old is None or r.random_vars > old.random_vars:
            uniq[key] = r
    dedup_regions = list(uniq.values())
    dedup_regions.sort(key=lambda r: (r.random_vars, r.node_count, r.edge_count), reverse=True)
    if len(dedup_regions) > region_limit:
        dedup_regions = dedup_regions[:region_limit]

    covered_nodes = sum(r.node_count for r in dedup_regions)
    covered_edges = sum(r.edge_count for r in dedup_regions)
    timing_ms["total"] = _now_ms() - t_start

    return {
        "mode": selected_mode,
        "candidates_tested": tested,
        "regions_found": len(dedup_regions),
        "skipped_trivial_single_input_single_edge": skipped_trivial_single_input_single_edge,
        "covered_nodes_sum": covered_nodes,
        "covered_edges_sum": covered_edges,
        "timing_ms": timing_ms,
        "regions": [r.__dict__ for r in dedup_regions],
    }


def summarize_delta(obj: Dict[str, object]) -> Dict[str, object]:
    delta = obj.get("delta")
    if not isinstance(delta, dict):
        return {}
    out: Dict[str, object] = {}
    for op in ("insert", "delete"):
        part = delta.get(op)
        if not isinstance(part, dict):
            continue
        out[op] = {
            "nodes": len(part.get("nodes", [])) if isinstance(part.get("nodes"), list) else 0,
            "edges": len(part.get("edges", [])) if isinstance(part.get("edges"), list) else 0,
            "facts": len(part.get("facts", [])) if isinstance(part.get("facts"), list) else 0,
        }
    return out


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze derivation graph JSON dumps")
    parser.add_argument("input_json", type=Path, help="Path to derivation JSON or graph-*.json")
    parser.add_argument("--out-json", type=Path, default=None, help="Optional path to write full report JSON")
    parser.add_argument(
        "--general-siso-mode",
        choices=["off", "auto", "global", "cone"],
        default="auto",
        help="General SISO analysis mode (default: auto)",
    )
    parser.add_argument("--max-dom-nodes", type=int, default=12000, help="Global dominator mode node cap (auto mode)")
    parser.add_argument("--max-candidates", type=int, default=40, help="Max exit candidates for general SISO")
    parser.add_argument("--min-random-vars", type=int, default=0, help="Skip regions with fewer random vars")
    parser.add_argument("--cone-depth", type=int, default=16, help="Backward cone BFS depth in cone mode")
    parser.add_argument("--cone-node-limit", type=int, default=20000, help="Backward cone node cap in cone mode")
    parser.add_argument("--region-limit", type=int, default=200, help="Max regions kept in output")
    parser.add_argument("--print-top-regions", type=int, default=10, help="How many top regions to print")
    parser.add_argument(
        "--supported-siso-mode",
        choices=["on", "off"],
        default="on",
        help="Run Souffle-supported fast-path SISO detection (default: on)",
    )
    parser.add_argument(
        "--rewrite-fixpoint",
        action=argparse.BooleanOptionalAction,
        default=True,
        help=(
            "Run standalone graph-only rewrite simulation to fixpoint "
            "(no BDD/SDD/probability evaluation; default: on)"
        ),
    )
    parser.add_argument(
        "--rewrite-split-mode",
        choices=["none", "naive", "complete"],
        default="naive",
        help="Split mode used by rewrite simulation",
    )
    parser.add_argument(
        "--rewrite-max-iterations",
        type=int,
        default=20,
        help="Max rewrite iterations for rewrite simulation",
    )
    parser.add_argument(
        "--rewrite-no-compaction",
        action="store_true",
        help="Disable edge compaction pass in rewrite simulation",
    )
    parser.add_argument(
        "--rewrite-split-max-new-nodes-per-pass",
        type=int,
        default=5000,
        help="complete-split budget: max new shadow nodes per pass",
    )
    parser.add_argument(
        "--rewrite-split-max-new-edges-per-pass",
        type=int,
        default=50000,
        help="complete-split budget: max rewired edges per pass",
    )
    parser.add_argument(
        "--rewrite-split-max-groups-per-node",
        type=int,
        default=2,
        help="complete-split cap: max groups kept per node (including original)",
    )
    parser.add_argument(
        "--rewrite-split-min-group-edges",
        type=int,
        default=1,
        help="complete-split threshold: minimum edges in a split group",
    )
    parser.add_argument(
        "--explore-mode",
        choices=[
            "off",
            "random-backward-cone",
            "random-disjunction-backward-cone",
            "random-forward-cone",
            "random-bi-cone",
            "random-scc",
            "random-supported-siso",
            "mixed",
        ],
        default="mixed",
        help="Random subgraph exploration mode for large graphs (default: mixed)",
    )
    parser.add_argument("--explore-samples", type=int, default=12, help="Number of exploration samples")
    parser.add_argument("--explore-seed", type=int, default=42, help="Random seed for exploration sampling")
    parser.add_argument(
        "--explore-seed-policy",
        choices=["uniform", "high-in", "high-out", "disjunction", "prob-hot"],
        default="prob-hot",
        help="Seed node sampling policy",
    )
    parser.add_argument("--explore-depth", type=int, default=10, help="Depth limit for cone-based exploration")
    parser.add_argument("--explore-max-nodes", type=int, default=400, help="Node budget per exploration sample")
    parser.add_argument("--explore-max-edges", type=int, default=1200, help="Edge budget per exploration sample")
    parser.add_argument("--explore-min-nodes", type=int, default=2, help="Minimum nodes per accepted exploration sample")
    parser.add_argument("--explore-min-edges", type=int, default=1, help="Minimum edges per accepted exploration sample")
    parser.add_argument("--explore-max-attempts", type=int, default=500, help="Maximum sampling attempts to satisfy sample count")
    parser.add_argument(
        "--full-graph-warn-nodes",
        type=int,
        default=2000,
        help="Prompt confirmation for full-graph interactive rendering when nodes exceed this threshold",
    )
    parser.add_argument(
        "--full-graph-warn-edges",
        type=int,
        default=10000,
        help="Prompt confirmation for full-graph interactive rendering when edges exceed this threshold",
    )
    parser.add_argument(
        "--full-graph-force",
        action="store_true",
        help="Skip full-graph large-size confirmation prompt",
    )
    parser.add_argument(
        "--include-all-sources",
        action=argparse.BooleanOptionalAction,
        default=True,
        help=(
            "Include all available interactive sources in one HTML payload "
            "(default: on; use --no-include-all-sources to keep only selected source)"
        ),
    )
    parser.add_argument(
        "--include-full-graph-source",
        action="store_true",
        help="Include full-graph as an additional source in interactive HTML selector",
    )
    parser.add_argument(
        "--interactive",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Emit an interactive HTML viewer for detected SISO regions (default: on)",
    )
    parser.add_argument(
        "--interactive-source",
        choices=["supported", "general", "rewrite-final", "rewrite-iter", "explore", "full-graph"],
        default="full-graph",
        help="Region source for interactive HTML",
    )
    parser.add_argument(
        "--interactive-html",
        type=Path,
        default=Path("/tmp/explore.html"),
        help="Path for interactive HTML output (default: /tmp/explore.html, overwrite if exists)",
    )
    return parser.parse_args()


def print_report(report: Dict[str, object], top_regions: int) -> None:
    print(f"[input] file={report['input_file']}")
    print(f"[input] kind={report['input_kind']}")

    if report["input_kind"] == "graph_summary":
        stats = report.get("graph_summary", {})
        for k in sorted(stats):
            print(f"[summary] {k}={stats[k]}")
        return

    stats = report["graph_stats"]
    print(
        "[graph] "
        f"nodes={stats['nodes']} edges={stats['edges']} facts={stats['fact_nodes']} "
        f"derived={stats['derived_nodes']} disjunction_nodes={stats['disjunction_nodes']} "
        f"random_vars={stats['random_variables']}"
    )
    strata_count = stats.get("strata_count")
    strata_txt = "n/a" if strata_count is None else str(strata_count)
    stratifiable = stats.get("stratifiable")
    strat_txt = "n/a" if stratifiable is None else ("1" if stratifiable else "0")
    print(
        "[graph] "
        f"predicates={stats.get('predicate_count', 0)} dep_edges={stats.get('dependency_edges', 0)} "
        f"neg_dep_edges={stats.get('negative_dependency_edges', 0)} "
        f"strata={strata_txt} recursive_scc={stats.get('predicate_recursive_scc_count', 0)} "
        f"cond_layers={stats.get('predicate_condensation_layers', 0)} "
        f"neg_strata={stats.get('negation_strata_count', 0)} stratifiable={strat_txt}"
    )
    print(
        "[graph] "
        f"max_in={stats['max_in_degree']} max_out={stats['max_out_degree']} "
        f"avg_arity={stats['avg_hyperedge_inputs']:.3f} max_arity={stats['max_hyperedge_inputs']}"
    )
    print(
        "[graph] "
        f"in_p50/p90/p99={stats['in_degree_p50']}/{stats['in_degree_p90']}/{stats['in_degree_p99']} "
        f"out_p50/p90/p99={stats['out_degree_p50']}/{stats['out_degree_p90']}/{stats['out_degree_p99']}"
    )

    scc = report["scc_stats"]
    print(
        "[scc] "
        f"components={scc['components']} cyclic={scc['cyclic_components']} "
        f"largest={scc['largest_component_size']} condensation_edges={scc['condensation_edges']}"
    )

    if report.get("delta_stats"):
        print(f"[delta] {json.dumps(report['delta_stats'], sort_keys=True)}")

    gs = report["general_siso"]
    skipped_trivial = gs.get("skipped_trivial_single_input_single_edge", 0)
    print(
        "[general-siso] "
        f"mode={gs['mode']} candidates={gs['candidates_tested']} "
        f"regions={gs['regions_found']} skipped_trivial={skipped_trivial} "
        f"time_ms={gs['timing_ms'].get('total', 0):.1f}"
    )
    for i, region in enumerate(gs["regions"][:max(0, top_regions)], start=1):
        print(
            "[general-siso] "
            f"#{i} entry={region['entry']} exit={region['exit']} "
            f"nodes={region['node_count']} edges={region['edge_count']} "
            f"random_vars={region['random_vars']} entry_preds={region['entry_pred_count']} "
            f"mode={region['mode']} kind={region.get('kind', 'unknown')}"
        )

    supported = report.get("supported_siso")
    if isinstance(supported, dict):
        print(
            "[supported-siso] "
            f"mode={supported.get('mode', 'supported-fast')} "
            f"regions={supported.get('regions_found', 0)} "
            f"time_ms={supported.get('timing_ms', {}).get('total', 0):.1f}"
        )
        kept = supported.get("kept_by_kind", {})
        if isinstance(kept, dict) and kept:
            row = " ".join(f"{k}={v}" for k, v in kept.items())
            print(f"[supported-siso] kept-by-kind {row}")

    rewrite = report.get("rewrite_sim")
    if isinstance(rewrite, dict) and rewrite.get("enabled"):
        print(
            "[rewrite-sim] "
            f"split={rewrite.get('split_mode', 'naive')} "
            f"iters={rewrite.get('iterations_run', 0)}(max={rewrite.get('max_iterations', 0)}) "
            f"fixpoint={1 if rewrite.get('fixpoint_reached') else 0} "
            f"rewritten={rewrite.get('regions_rewritten_total', 0)}"
        )
        final_supported = rewrite.get("final_supported_siso", {})
        if isinstance(final_supported, dict):
            print(
                "[rewrite-sim] "
                f"final-supported-regions={final_supported.get('regions_found', 0)} "
                f"final-detect-ms={final_supported.get('timing_ms', {}).get('total', 0):.1f}"
            )

    explore = report.get("explore_samples")
    if isinstance(explore, dict):
        samples = explore.get("samples", [])
        print(
            "[explore] "
            f"mode={explore.get('mode', 'off')} "
            f"samples={len(samples) if isinstance(samples, list) else 0}/{explore.get('sample_count', 0)} "
            f"attempts={explore.get('attempts', 0)}/{explore.get('max_attempts', 0)} "
            f"seed={explore.get('seed')} "
            f"time_ms={explore.get('timing_ms', {}).get('total', 0):.1f}"
        )

    full_graph = report.get("full_graph_view")
    if isinstance(full_graph, dict):
        samples = full_graph.get("samples", [])
        print(
            "[full-graph] "
            f"samples={len(samples) if isinstance(samples, list) else 0} "
            f"time_ms={full_graph.get('timing_ms', {}).get('total', 0):.1f}"
        )


def resolve_interactive_regions(report: Dict[str, object], source: str) -> Tuple[str, List[Dict[str, object]]]:
    if source == "general":
        gs = report.get("general_siso", {})
        regions = gs.get("regions", []) if isinstance(gs, dict) else []
        return "general-siso", [r for r in regions if isinstance(r, dict) and isinstance(r.get("region_graph"), dict)]

    if source == "supported":
        ss = report.get("supported_siso", {})
        regions = ss.get("regions", []) if isinstance(ss, dict) else []
        return "supported-fast-siso", [r for r in regions if isinstance(r, dict) and isinstance(r.get("region_graph"), dict)]

    if source == "rewrite-final":
        rs = report.get("rewrite_sim", {})
        fs = rs.get("final_supported_siso", {}) if isinstance(rs, dict) else {}
        regions = fs.get("regions", []) if isinstance(fs, dict) else []
        return "rewrite-final-supported-siso", [
            r for r in regions if isinstance(r, dict) and isinstance(r.get("region_graph"), dict)
        ]

    if source == "rewrite-iter":
        rs = report.get("rewrite_sim", {})
        rows: List[Dict[str, object]] = []
        if isinstance(rs, dict):
            iterations = rs.get("iterations", [])
            if isinstance(iterations, list):
                for item in iterations:
                    if not isinstance(item, dict):
                        continue
                    it = item.get("iteration")
                    regions = item.get("regions", [])
                    if not isinstance(regions, list):
                        continue
                    for r in regions:
                        if not isinstance(r, dict):
                            continue
                        if not isinstance(r.get("region_graph"), dict):
                            continue
                        x = dict(r)
                        x["mode"] = f"rewrite-iter-{it}"
                        x["candidate_exit"] = f"{x.get('candidate_exit', '')} @iter{it}"
                        rows.append(x)
        return "rewrite-iter-rewritten-regions", rows

    if source == "explore":
        ex = report.get("explore_samples", {})
        rows = ex.get("samples", []) if isinstance(ex, dict) else []
        return "explore-random-samples", [r for r in rows if isinstance(r, dict) and isinstance(r.get("region_graph"), dict)]

    if source == "full-graph":
        fg = report.get("full_graph_view", {})
        rows = fg.get("samples", []) if isinstance(fg, dict) else []
        return "full-graph", [r for r in rows if isinstance(r, dict) and isinstance(r.get("region_graph"), dict)]

    return "unknown", []


def collect_interactive_sources(report: Dict[str, object]) -> Dict[str, Dict[str, object]]:
    sources: Dict[str, Dict[str, object]] = {}
    for key in ["supported", "general", "rewrite-final", "rewrite-iter", "explore", "full-graph"]:
        label, rows = resolve_interactive_regions(report, key)
        if rows:
            sources[key] = {
                "label": label,
                "regions": rows,
            }
    return sources


def write_interactive_html_stateful(report: Dict[str, object], out_html: Path, source: str) -> bool:
    if report.get("input_kind") != "derivation":
        return False

    rs = report.get("rewrite_sim", {})
    states_raw = rs.get("interactive_states", []) if isinstance(rs, dict) else []
    if not isinstance(states_raw, list) or not states_raw:
        return False
    states = [s for s in states_raw if isinstance(s, dict)]
    if not states:
        return False

    ex = report.get("explore_samples", {})
    explore_rows_raw = ex.get("samples", []) if isinstance(ex, dict) else []
    explore_rows_global = [
        r
        for r in explore_rows_raw
        if isinstance(r, dict) and isinstance(r.get("region_graph"), dict)
    ]

    source_map = {
        "full-graph": "full-graph",
        "supported": "fast-path",
        "rewrite-final": "fast-path",
        "rewrite-iter": "fast-path",
        "general": "general-siso",
        "explore": "explore",
    }
    selected_source = source_map.get(source, "full-graph")

    payload = {
        "input_file": report.get("input_file"),
        "graph_stats": report.get("graph_stats", {}),
        "rewrite_summary": {
            "enabled": bool(rs.get("enabled", False)) if isinstance(rs, dict) else False,
            "split_mode": rs.get("split_mode", "naive") if isinstance(rs, dict) else "naive",
            "iterations_run": rs.get("iterations_run", 0) if isinstance(rs, dict) else 0,
            "max_iterations": rs.get("max_iterations", 0) if isinstance(rs, dict) else 0,
            "fixpoint_reached": bool(rs.get("fixpoint_reached", False)) if isinstance(rs, dict) else False,
            "regions_rewritten_total": rs.get("regions_rewritten_total", 0) if isinstance(rs, dict) else 0,
        },
        "selected_source": selected_source,
        "rewrite_states": states,
        "explore_regions_global": explore_rows_global,
    }
    payload_b64 = base64.b64encode(json.dumps(payload, ensure_ascii=False).encode("utf-8")).decode("ascii")

    html = """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Derivation Graph Interactive Analyzer</title>
  <style>
    :root {
      --bg: #f6f7f9;
      --panel: #ffffff;
      --ink: #111827;
      --muted: #6b7280;
      --line: #d1d5db;
      --accent: #0f766e;
      --warn: #b45309;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "IBM Plex Sans", "Segoe UI", -apple-system, BlinkMacSystemFont, sans-serif;
      background: radial-gradient(1400px 800px at 15% -15%, #e7f8f5 0%, #f6f7f9 42%) fixed;
      color: var(--ink);
    }
    .wrap {
      max-width: 1680px;
      margin: 0 auto;
      padding: 14px;
      display: grid;
      gap: 10px;
    }
    .card {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 10px;
    }
    .row {
      display: grid;
      grid-template-columns: 1.2fr 1fr 1fr 1fr 1fr auto auto auto;
      gap: 8px;
      align-items: end;
    }
    .action-row {
      display: grid;
      grid-template-columns: auto auto auto auto auto auto 1fr;
      gap: 8px;
      align-items: center;
      margin-top: 8px;
    }
    .meta {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
    .meta pre {
      margin: 0;
      padding: 8px;
      background: #f9fafb;
      border: 1px solid var(--line);
      border-radius: 8px;
      overflow: auto;
      font-size: 12px;
      line-height: 1.35;
    }
    label {
      font-size: 12px;
      color: var(--muted);
      display: block;
      margin-bottom: 4px;
    }
    select, button, input[type="text"] {
      width: 100%;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
      font-size: 13px;
    }
    input[type="text"]::placeholder {
      color: #9ca3af;
    }
    button {
      cursor: pointer;
      font-weight: 600;
    }
    button:hover:not(:disabled) {
      border-color: var(--accent);
    }
    button:disabled {
      color: #9ca3af;
      cursor: not-allowed;
      background: #f9fafb;
    }
    #position, #rewritePos {
      text-align: center;
      font-size: 13px;
      color: var(--muted);
      min-width: 74px;
    }
    #rewriteMeta {
      font-size: 12px;
      color: var(--muted);
      overflow-wrap: anywhere;
    }
    .search-row {
      display: grid;
      grid-template-columns: 2fr auto auto auto 2fr;
      gap: 8px;
      align-items: center;
      margin-top: 8px;
    }
    #searchStatus {
      font-size: 12px;
      color: var(--muted);
      overflow-wrap: anywhere;
    }
    #graphCy, #graphSvg {
      width: 100%;
      height: 700px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
    }
    #graphSvg circle.search-hit {
      stroke: #f59e0b !important;
      stroke-width: 2.2 !important;
    }
    #graphSvg circle.search-focus {
      stroke: #dc2626 !important;
      stroke-width: 3.2 !important;
    }
    #graphSvg text.search-hit {
      font-weight: 600;
    }
    #graphSvg text.search-focus {
      font-weight: 700;
      fill: #b91c1c !important;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 12px;
    }
    th, td {
      border: 1px solid var(--line);
      text-align: left;
      padding: 6px 8px;
      vertical-align: top;
      word-break: break-word;
    }
    th {
      background: #f3f4f6;
      position: sticky;
      top: 0;
    }
    .table-wrap {
      max-height: 320px;
      overflow: auto;
      border: 1px solid var(--line);
      border-radius: 8px;
    }
    .pill {
      display: inline-block;
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid var(--line);
      font-size: 11px;
      color: #374151;
      background: #f9fafb;
      margin-right: 4px;
    }
  </style>
  <script src="https://unpkg.com/cytoscape@3.29.2/dist/cytoscape.min.js"></script>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <div><strong>Derivation Graph Interactive Analyzer</strong></div>
      <div style="margin-top:4px; font-size:12px; color:#6b7280;">Input: <code id="inputPath"></code></div>
      <div style="margin-top:2px; font-size:12px; color:#6b7280;">Graph: <span id="graphSummary"></span></div>
      <div style="margin-top:2px; font-size:12px; color:#6b7280;">Source: <span id="sourceLabel"></span></div>
      <div style="margin-top:2px; font-size:12px; color:#6b7280;">Rewrite: <span id="rewriteSummary"></span></div>
    </div>

    <div class="card">
      <div class="row">
        <div>
          <label for="sourceFilter">Source</label>
          <select id="sourceFilter"></select>
        </div>
        <div>
          <label for="kindFilter">SISO kind</label>
          <select id="kindFilter"></select>
        </div>
        <div>
          <label for="modeFilter">Detect mode</label>
          <select id="modeFilter"></select>
        </div>
        <div>
          <label for="entryPredFilter">Entry predicate</label>
          <select id="entryPredFilter"></select>
        </div>
        <div>
          <label for="exitPredFilter">Exit predicate</label>
          <select id="exitPredFilter"></select>
        </div>
        <button id="prevBtn">Previous</button>
        <div id="position">0/0</div>
        <button id="nextBtn">Next</button>
      </div>
      <div class="action-row">
        <button id="rewritePrevBtn">Undo Rewrite</button>
        <button id="rewriteNextBtn">Rewrite +1</button>
        <button id="rewriteFinalBtn">Rewrite to End</button>
        <button id="rewriteResetBtn">Reset Rewrite</button>
        <button id="detectGeneralBtn">Detect General SISO</button>
        <div id="rewritePos">state 0/0</div>
        <div id="rewriteMeta"></div>
      </div>
      <div class="search-row">
        <input id="nodeSearchInput" type="text" placeholder="Search node in full graph, e.g. pt(0,10,197)" />
        <button id="nodeSearchBtn">Locate (Full Graph)</button>
        <button id="nodeSearchNextBtn">Next Match</button>
        <button id="nodeSearchClearBtn">Clear</button>
        <div id="searchStatus">Search target node by name.</div>
      </div>
    </div>

    <div class="card meta">
      <pre id="regionMeta"></pre>
      <pre id="regionPredMeta"></pre>
    </div>

    <div class="card">
      <div id="graphCy"></div>
      <svg id="graphSvg" viewBox="0 0 1400 700" preserveAspectRatio="xMidYMid meet" style="display:none"></svg>
      <div style="margin-top:8px; font-size:12px; color:#6b7280;">
        <span class="pill">entry: green</span>
        <span class="pill">exit: red</span>
        <span class="pill">fact: blue</span>
        <span class="pill">derived: gray</span>
        <span class="pill">rule node: square</span>
        <span class="pill">probabilistic edge: orange</span>
      </div>
    </div>

    <div class="card">
      <div style="margin-bottom:8px;"><strong>Region rules</strong></div>
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>edge_id</th>
              <th>head</th>
              <th>inputs</th>
              <th>negated</th>
              <th>probability</th>
              <th>prob-edge</th>
            </tr>
          </thead>
          <tbody id="edgeRows"></tbody>
        </table>
      </div>
    </div>
  </div>

  <script id="payloadB64" type="application/octet-stream">__PAYLOAD_B64__</script>
  <script>
    const payload = JSON.parse(atob(document.getElementById("payloadB64").textContent.trim()));
    const rewriteStates = Array.isArray(payload.rewrite_states) ? payload.rewrite_states : [];
    const exploreRegionsGlobal = Array.isArray(payload.explore_regions_global)
      ? payload.explore_regions_global
      : (Array.isArray(payload.explore_regions) ? payload.explore_regions : []);
    let rewriteIndex = 0;
    let currentSourceKey = String(payload.selected_source || "full-graph");
    const generalDetected = new Set();
    const regionNav = { index: 0 };
    const searchState = { query: "", matches: [], index: -1, renderToken: "" };
    let cy = null;

    const sourceDefs = [
      { key: "full-graph", label: "Full Graph" },
      { key: "fast-path", label: "Fast-path SISO" },
      { key: "general-siso", label: "General SISO (filtered by fast-path)" },
    ];
    if (exploreRegionsGlobal.length > 0 || rewriteStates.some(st => Array.isArray(st.explore_regions) && st.explore_regions.length > 0)) {
      sourceDefs.push({ key: "explore", label: "Explore Samples" });
    }
    if (!sourceDefs.some(d => d.key === currentSourceKey)) {
      currentSourceKey = "full-graph";
    }

    const els = {
      inputPath: document.getElementById("inputPath"),
      graphSummary: document.getElementById("graphSummary"),
      sourceLabel: document.getElementById("sourceLabel"),
      rewriteSummary: document.getElementById("rewriteSummary"),
      sourceSel: document.getElementById("sourceFilter"),
      kind: document.getElementById("kindFilter"),
      mode: document.getElementById("modeFilter"),
      entryPred: document.getElementById("entryPredFilter"),
      exitPred: document.getElementById("exitPredFilter"),
      prev: document.getElementById("prevBtn"),
      next: document.getElementById("nextBtn"),
      pos: document.getElementById("position"),
      rewritePrev: document.getElementById("rewritePrevBtn"),
      rewriteNext: document.getElementById("rewriteNextBtn"),
      rewriteFinal: document.getElementById("rewriteFinalBtn"),
      rewriteReset: document.getElementById("rewriteResetBtn"),
      detectGeneral: document.getElementById("detectGeneralBtn"),
      rewritePos: document.getElementById("rewritePos"),
      rewriteMeta: document.getElementById("rewriteMeta"),
      searchInput: document.getElementById("nodeSearchInput"),
      searchBtn: document.getElementById("nodeSearchBtn"),
      searchNextBtn: document.getElementById("nodeSearchNextBtn"),
      searchClearBtn: document.getElementById("nodeSearchClearBtn"),
      searchStatus: document.getElementById("searchStatus"),
      regionMeta: document.getElementById("regionMeta"),
      regionPredMeta: document.getElementById("regionPredMeta"),
      edgeRows: document.getElementById("edgeRows"),
      cyDiv: document.getElementById("graphCy"),
      svg: document.getElementById("graphSvg"),
    };

    function currentState() {
      if (rewriteStates.length === 0) return null;
      const idx = Math.max(0, Math.min(rewriteIndex, rewriteStates.length - 1));
      return rewriteStates[idx];
    }

    function uniqSorted(values) {
      return [...new Set(values)].sort((a, b) => String(a).localeCompare(String(b)));
    }

    function predOf(atomName) {
      const i = atomName.indexOf("(");
      return i < 0 ? atomName : atomName.slice(0, i);
    }

    function normalizeQuery(q) {
      return String(q || "").trim().toLowerCase();
    }

    function currentRenderToken() {
      return `${rewriteIndex}|${currentSourceKey}|${regionNav.index}`;
    }

    function fillSelect(sel, values, allLabel) {
      sel.innerHTML = "";
      const first = document.createElement("option");
      first.value = "__all__";
      first.textContent = allLabel;
      sel.appendChild(first);
      for (const v of values) {
        const op = document.createElement("option");
        op.value = String(v);
        op.textContent = String(v);
        sel.appendChild(op);
      }
    }

    function sourceRows(key) {
      const st = currentState();
      if (key === "explore") {
        if (st && Array.isArray(st.explore_regions) && st.explore_regions.length > 0) {
          return st.explore_regions;
        }
        return exploreRegionsGlobal;
      }
      if (!st) return [];
      if (key === "full-graph") {
        return (st.full_graph && st.full_graph.region_graph) ? [st.full_graph] : [];
      }
      if (key === "fast-path") {
        return Array.isArray(st.fast_regions) ? st.fast_regions : [];
      }
      if (key === "general-siso") {
        if (!generalDetected.has(rewriteIndex)) return [];
        return Array.isArray(st.general_regions) ? st.general_regions : [];
      }
      return [];
    }

    function allSourceRowsForCount(key) {
      const st = currentState();
      if (key === "explore") {
        if (st && Array.isArray(st.explore_regions) && st.explore_regions.length > 0) {
          return st.explore_regions;
        }
        return exploreRegionsGlobal;
      }
      if (!st) return [];
      if (key === "full-graph") return st.full_graph ? [st.full_graph] : [];
      if (key === "fast-path") return Array.isArray(st.fast_regions) ? st.fast_regions : [];
      if (key === "general-siso") return Array.isArray(st.general_regions) ? st.general_regions : [];
      return [];
    }

    function currentFilter() {
      return {
        kind: els.kind.value || "__all__",
        mode: els.mode.value || "__all__",
        entryPred: els.entryPred.value || "__all__",
        exitPred: els.exitPred.value || "__all__",
      };
    }

    function filteredRegions() {
      const regs = sourceRows(currentSourceKey);
      const f = currentFilter();
      return regs.filter(r => {
        if (f.kind !== "__all__" && String(r.kind) !== f.kind) return false;
        if (f.mode !== "__all__" && String(r.mode) !== f.mode) return false;
        if (f.entryPred !== "__all__" && predOf(String(r.entry)) !== f.entryPred) return false;
        if (f.exitPred !== "__all__" && predOf(String(r.exit)) !== f.exitPred) return false;
        return true;
      });
    }

    function refreshSourceSelect() {
      els.sourceSel.innerHTML = "";
      for (const def of sourceDefs) {
        const op = document.createElement("option");
        op.value = def.key;
        const allRows = allSourceRowsForCount(def.key);
        const activeRows = sourceRows(def.key);
        if (def.key === "general-siso") {
          const st = currentState();
          const gm = (st && st.general_meta) ? st.general_meta : {};
          const enabled = !!gm.enabled;
          const before = Number(gm.regions_found || 0);
          const after = Number(gm.regions_after_fast_filter || 0);
          const isDetected = generalDetected.has(rewriteIndex);
          if (!enabled) {
            op.textContent = `${def.label} (off)`;
            op.disabled = true;
          } else if (isDetected) {
            op.textContent = `${def.label} (ready ${after}/${before})`;
          } else {
            op.textContent = `${def.label} (pending ${after}/${before})`;
          }
        } else {
          op.textContent = `${def.label} (n=${activeRows.length || allRows.length})`;
        }
        els.sourceSel.appendChild(op);
      }
      const keys = sourceDefs.map(d => d.key);
      if (!keys.includes(currentSourceKey)) currentSourceKey = "full-graph";
      if (currentSourceKey === "general-siso" && !generalDetected.has(rewriteIndex)) {
        currentSourceKey = "full-graph";
      }
      els.sourceSel.value = currentSourceKey;
    }

    function refreshFiltersForSource() {
      const regs = sourceRows(currentSourceKey);
      fillSelect(els.kind, uniqSorted(regs.map(r => r.kind || "unknown")), "all kinds");
      fillSelect(els.mode, uniqSorted(regs.map(r => r.mode || "unknown")), "all modes");
      fillSelect(els.entryPred, uniqSorted(regs.map(r => predOf(String(r.entry)))), "all entry predicates");
      fillSelect(els.exitPred, uniqSorted(regs.map(r => predOf(String(r.exit)))), "all exit predicates");

      const st = currentState();
      const stateLabel = st ? String(st.label || `state-${rewriteIndex}`) : "n/a";
      const sourceDef = sourceDefs.find(d => d.key === currentSourceKey);
      els.sourceLabel.textContent = `${stateLabel} | ${sourceDef ? sourceDef.label : currentSourceKey}`;
    }

    function refreshHeaderSummary() {
      const st = currentState();
      const s = (st && st.graph_stats) ? st.graph_stats : (payload.graph_stats || {});
      const strata = (s.strata_count === null || s.strata_count === undefined) ? "n/a" : s.strata_count;
      const negStrata = (s.negation_strata_count === null || s.negation_strata_count === undefined) ? "n/a" : s.negation_strata_count;
      const stratifiable = (s.stratifiable === undefined) ? "n/a" : (s.stratifiable ? 1 : 0);
      els.graphSummary.textContent =
        `nodes=${s.nodes || 0}, edges=${s.edges || 0}, disjunction=${s.disjunction_nodes || 0}, random_vars=${s.random_variables || 0}, predicates=${s.predicate_count || 0}, strata=${strata}, recursive_scc=${s.predicate_recursive_scc_count || 0}, cond_layers=${s.predicate_condensation_layers || 0}, neg_strata=${negStrata}, stratifiable=${stratifiable}`;

      const rs = payload.rewrite_summary || {};
      els.rewriteSummary.textContent =
        `iters=${rs.iterations_run || 0} (max=${rs.max_iterations || 0}), fixpoint=${rs.fixpoint_reached ? 1 : 0}, rewritten=${rs.regions_rewritten_total || 0}, split=${rs.split_mode || "naive"}`;
    }

    function refreshRewriteControls() {
      const totalStates = rewriteStates.length;
      els.rewritePos.textContent = `state ${rewriteIndex}/${Math.max(0, totalStates - 1)}`;
      els.rewritePrev.disabled = (rewriteIndex <= 0);
      els.rewriteReset.disabled = (rewriteIndex <= 0);
      els.rewriteNext.disabled = (rewriteIndex + 1 >= totalStates);
      els.rewriteFinal.disabled = (rewriteIndex + 1 >= totalStates);

      const st = currentState();
      const action = (st && st.rewrite_action) ? st.rewrite_action : {};
      const bits = [];
      if (Number(action.iteration || 0) > 0) bits.push(`iter=${action.iteration}`);
      if (action.detected_regions !== undefined) bits.push(`detect=${action.detected_regions}`);
      if (action.rewritten_regions !== undefined) bits.push(`rewrite=${action.rewritten_regions}`);
      if (action.split_applied) bits.push(`split=${action.split_mode || "naive"}(+${action.split_nodes_added || 0}n/${action.split_edges_rewritten || 0}e)`);
      if (action.compaction_applied) bits.push(`compact=${action.compact_edges || 0}`);
      if (action.cleanup_removed_nodes) bits.push(`cleanup=${action.cleanup_removed_nodes}`);
      els.rewriteMeta.textContent = bits.length > 0 ? bits.join(" | ") : "initial graph state";

      const gm = (st && st.general_meta) ? st.general_meta : {};
      if (!gm.enabled) {
        els.detectGeneral.disabled = true;
        els.detectGeneral.textContent = "General SISO (off)";
      } else {
        const before = Number(gm.regions_found || 0);
        const after = Number(gm.regions_after_fast_filter || 0);
        const done = generalDetected.has(rewriteIndex);
        els.detectGeneral.disabled = false;
        els.detectGeneral.textContent = done
          ? `General ready (${after}/${before})`
          : `Detect General SISO (${after}/${before})`;
      }
    }

    function setSearchStatus(msg) {
      els.searchStatus.textContent = String(msg || "");
    }

    function clearSearchVisuals() {
      searchState.matches = [];
      searchState.index = -1;
      searchState.renderToken = "";
      if (cy) {
        cy.nodes(".search-hit").removeClass("search-hit");
        cy.nodes(".search-focus").removeClass("search-focus");
      }
      const circles = els.svg.querySelectorAll("circle.atom-node");
      for (const c of circles) {
        c.classList.remove("search-hit", "search-focus");
        c.setAttribute("r", "8");
      }
      const labels = els.svg.querySelectorAll("text.atom-label");
      for (const t of labels) {
        t.classList.remove("search-hit", "search-focus");
      }
    }

    function resetSearchStateButKeepQuery() {
      searchState.matches = [];
      searchState.index = -1;
      searchState.renderToken = "";
      if (cy) {
        cy.nodes(".search-hit").removeClass("search-hit");
        cy.nodes(".search-focus").removeClass("search-focus");
      }
      const circles = els.svg.querySelectorAll("circle.atom-node");
      for (const c of circles) {
        c.classList.remove("search-hit", "search-focus");
        c.setAttribute("r", "8");
      }
      const labels = els.svg.querySelectorAll("text.atom-label");
      for (const t of labels) {
        t.classList.remove("search-hit", "search-focus");
      }
    }

    function switchToFullGraphForSearch() {
      if (currentSourceKey !== "full-graph") {
        currentSourceKey = "full-graph";
        regionNav.index = 0;
        refreshSourceSelect();
        refreshFiltersForSource();
      }
      for (const sel of [els.kind, els.mode, els.entryPred, els.exitPred]) {
        if (sel && sel.querySelector('option[value="__all__"]')) {
          sel.value = "__all__";
        }
      }
      render();
    }

    function searchInRenderedGraph(queryRaw, advance) {
      const q = normalizeQuery(queryRaw);
      if (!q) {
        clearSearchVisuals();
        searchState.query = "";
        setSearchStatus("Search target node by name.");
        return;
      }
      const renderToken = currentRenderToken();
      const sameContext = (searchState.query === q && searchState.renderToken === renderToken && searchState.matches.length > 0);
      if (!sameContext) {
        resetSearchStateButKeepQuery();
        searchState.query = q;
        searchState.renderToken = renderToken;

        if (cy && els.cyDiv.style.display !== "none") {
          const atomNodes = cy.nodes('node[type = "atom"]');
          const matches = [];
          atomNodes.forEach((n) => {
            const nm = String(n.data("full_label") || n.data("label") || "");
            if (nm.toLowerCase().includes(q)) matches.push(n);
          });
          searchState.matches = matches;
          for (const n of matches) n.addClass("search-hit");
          if (matches.length > 0) {
            let exactIdx = matches.findIndex((n) => String(n.data("full_label") || "").toLowerCase() === q);
            if (exactIdx < 0) exactIdx = 0;
            searchState.index = exactIdx;
          }
        } else {
          const circles = [...els.svg.querySelectorAll("circle.atom-node")];
          const matches = circles.filter((c) => String(c.getAttribute("data-node-label") || "").toLowerCase().includes(q));
          searchState.matches = matches;
          for (const c of matches) {
            c.classList.add("search-hit");
            const idx = c.getAttribute("data-node-index");
            if (!idx) continue;
            for (const t of els.svg.querySelectorAll(`text.atom-label[data-node-index="${idx}"]`)) {
              t.classList.add("search-hit");
            }
          }
          if (matches.length > 0) {
            let exactIdx = matches.findIndex((c) => String(c.getAttribute("data-node-label") || "").toLowerCase() === q);
            if (exactIdx < 0) exactIdx = 0;
            searchState.index = exactIdx;
          }
        }
      } else if (advance && searchState.matches.length > 0) {
        searchState.index = (searchState.index + 1) % searchState.matches.length;
      }

      if (searchState.matches.length === 0) {
        setSearchStatus(`No match for "${queryRaw}" in current graph view.`);
        return;
      }

      if (searchState.index < 0 || searchState.index >= searchState.matches.length) {
        searchState.index = 0;
      }

      if (cy && els.cyDiv.style.display !== "none") {
        cy.nodes(".search-focus").removeClass("search-focus");
        const focus = searchState.matches[searchState.index];
        focus.addClass("search-focus");
        cy.animate({ fit: { eles: focus, padding: 140 }, duration: 220 });
        const label = String(focus.data("full_label") || focus.data("label") || "");
        setSearchStatus(`Match ${searchState.index + 1}/${searchState.matches.length}: ${label}`);
      } else {
        const circles = els.svg.querySelectorAll("circle.atom-node");
        for (const c of circles) {
          c.classList.remove("search-focus");
          c.setAttribute("r", "8");
        }
        const labels = els.svg.querySelectorAll("text.atom-label");
        for (const t of labels) t.classList.remove("search-focus");

        const focus = searchState.matches[searchState.index];
        focus.classList.add("search-focus");
        focus.setAttribute("r", "10");
        const idx = focus.getAttribute("data-node-index");
        if (idx) {
          for (const t of els.svg.querySelectorAll(`text.atom-label[data-node-index="${idx}"]`)) {
            t.classList.add("search-focus");
          }
        }
        const x = Number(focus.getAttribute("cx") || 0);
        const y = Number(focus.getAttribute("cy") || 0);
        const vb = `${Math.max(0, x - 350)} ${Math.max(0, y - 180)} 700 360`;
        els.svg.setAttribute("viewBox", vb);
        const label = String(focus.getAttribute("data-node-label") || "");
        setSearchStatus(`Match ${searchState.index + 1}/${searchState.matches.length}: ${label}`);
      }
    }

    function clearNode(el) {
      while (el.firstChild) el.removeChild(el.firstChild);
    }

    function ensureCy() {
      if (typeof window.cytoscape !== "function") return null;
      if (cy) return cy;
      cy = window.cytoscape({
        container: els.cyDiv,
        elements: [],
        style: [
          {
            selector: 'node[type = "atom"]',
            style: {
              shape: "ellipse",
              width: 14,
              height: 14,
              "background-color": "#6b7280",
              label: "data(label)",
              color: "#111827",
              "font-size": 9,
              "text-wrap": "wrap",
              "text-max-width": 180,
              "text-halign": "right",
              "text-valign": "center",
              "text-margin-x": 9,
            },
          },
          { selector: 'node[type = "atom"][entry = 1]', style: { "background-color": "#15803d" } },
          { selector: 'node[type = "atom"][exit = 1]', style: { "background-color": "#b91c1c" } },
          { selector: 'node[type = "atom"][fact = 1]', style: { "background-color": "#1d4ed8" } },
          {
            selector: 'node[type = "atom"].search-hit',
            style: {
              "border-width": 2.6,
              "border-color": "#f59e0b",
            },
          },
          {
            selector: 'node[type = "atom"].search-focus',
            style: {
              "border-width": 4.0,
              "border-color": "#dc2626",
              width: 18,
              height: 18,
              "z-index": 9999,
            },
          },
          {
            selector: 'node[type = "rule"]',
            style: {
              shape: "round-rectangle",
              width: 10,
              height: 10,
              "background-color": "#f3f4f6",
              "border-width": 1,
              "border-color": "#374151",
              label: "",
            },
          },
          { selector: 'node[type = "rule"][prob = 1]', style: { "background-color": "#f59e0b" } },
          {
            selector: "edge",
            style: {
              "curve-style": "bezier",
              "target-arrow-shape": "triangle",
              "arrow-scale": 0.7,
              "line-color": "#9ca3af",
              "target-arrow-color": "#9ca3af",
              width: 1.2,
              opacity: 0.9,
            },
          },
          { selector: "edge[prob = 1]", style: { "line-color": "#b45309", "target-arrow-color": "#b45309", width: 2.0 } },
          { selector: "edge[neg = 1]", style: { "line-style": "dashed" } },
        ],
        wheelSensitivity: 0.18,
        motionBlur: true,
      });
      return cy;
    }

    function buildCyElements(region) {
      const graph = region.region_graph || { nodes: [], edges: [] };
      const atoms = Array.isArray(graph.nodes) ? graph.nodes : [];
      const rules = Array.isArray(graph.edges) ? graph.edges : [];
      const showAllLabels = atoms.length <= 420;

      const elements = [];
      const atomId = new Map();
      for (let i = 0; i < atoms.length; i += 1) {
        const n = atoms[i];
        const nm = String(n.name || `node_${i}`);
        const id = `a_${i}`;
        atomId.set(nm, id);
        const entry = n.is_entry ? 1 : 0;
        const exit = n.is_exit ? 1 : 0;
        const fact = n.is_fact ? 1 : 0;
        const label = (showAllLabels || entry || exit) ? nm : "";
        elements.push({
          group: "nodes",
          data: { id, type: "atom", label, full_label: nm, entry, exit, fact },
        });
      }

      let edgeCounter = 0;
      for (let i = 0; i < rules.length; i += 1) {
        const e = rules[i] || {};
        const ruleId = `r_${i}_${Number(e.edge_id || i)}`;
        const prob = e.is_probabilistic ? 1 : 0;
        elements.push({ group: "nodes", data: { id: ruleId, type: "rule", prob } });

        const inputs = Array.isArray(e.inputs) ? e.inputs : [];
        const negs = Array.isArray(e.negated_inputs) ? e.negated_inputs : [];
        for (let j = 0; j < inputs.length; j += 1) {
          const inpName = String(inputs[j]);
          const src = atomId.get(inpName);
          if (!src) continue;
          const neg = negs[j] ? 1 : 0;
          elements.push({
            group: "edges",
            data: {
              id: `in_${edgeCounter++}`,
              source: src,
              target: ruleId,
              prob,
              neg,
            },
          });
        }

        const headName = String(e.head || "");
        const dst = atomId.get(headName);
        if (dst) {
          elements.push({
            group: "edges",
            data: {
              id: `out_${edgeCounter++}`,
              source: ruleId,
              target: dst,
              prob,
              neg: 0,
            },
          });
        }
      }
      return elements;
    }

    function drawRegionCy(region) {
      const c = ensureCy();
      if (!c) return false;
      const elements = buildCyElements(region);
      c.elements().remove();
      c.add(elements);

      const n = c.nodes().length;
      const e = c.edges().length;
      let layout = { name: "breadthfirst", directed: true, padding: 20, spacingFactor: 1.1, animate: false };
      if (n <= 320 && e <= 1300) {
        layout = { name: "cose", animate: false, fit: true, padding: 26, nodeRepulsion: 40000, idealEdgeLength: 56 };
      } else if (n > 2400 || e > 12000) {
        layout = { name: "concentric", animate: false, fit: true, minNodeSpacing: 20, padding: 16 };
      }
      c.layout(layout).run();
      c.fit(undefined, 22);
      els.cyDiv.style.display = "block";
      els.svg.style.display = "none";
      return true;
    }

    function drawRegionSvg(region) {
      const svg = els.svg;
      clearNode(svg);
      const graph = region.region_graph || { nodes: [], edges: [] };
      const atoms = graph.nodes || [];
      const rules = graph.edges || [];
      const byName = new Map(atoms.map(n => [n.name, n]));

      const succ = new Map(atoms.map(n => [n.name, new Set()]));
      const atomInDegree = new Map(atoms.map(n => [n.name, 0]));
      for (const e of rules) {
        if (!byName.has(e.head)) continue;
        const inputs = e.inputs || [];
        for (const inp of inputs) {
          if (!byName.has(inp)) continue;
          succ.get(inp).add(e.head);
          atomInDegree.set(e.head, (atomInDegree.get(e.head) || 0) + 1);
        }
      }

      const layer = new Map();
      const q = [];
      if (byName.has(region.entry)) {
        layer.set(region.entry, 0);
        q.push(region.entry);
      } else {
        for (const n of atoms) {
          if ((atomInDegree.get(n.name) || 0) === 0) {
            layer.set(n.name, 0);
            q.push(n.name);
          }
        }
        if (q.length === 0 && atoms.length > 0) {
          layer.set(atoms[0].name, 0);
          q.push(atoms[0].name);
        }
      }

      while (q.length > 0) {
        const u = q.shift();
        const d = layer.get(u) || 0;
        const nexts = succ.get(u) || [];
        for (const v of nexts) {
          if (!byName.has(v)) continue;
          if (!layer.has(v)) {
            layer.set(v, d + 1);
            q.push(v);
          } else if ((layer.get(v) || 0) < d + 1) {
            layer.set(v, d + 1);
          }
        }
      }

      let maxLayer = 0;
      for (const n of atoms) {
        const d = layer.has(n.name) ? (layer.get(n.name) || 0) : 0;
        maxLayer = Math.max(maxLayer, d);
      }
      const fallbackLayer = maxLayer + 1;
      for (const n of atoms) {
        if (!layer.has(n.name)) layer.set(n.name, fallbackLayer);
      }
      maxLayer = Math.max(maxLayer, fallbackLayer);

      const groups = new Map();
      for (const n of atoms) {
        const d = layer.get(n.name) || 0;
        if (!groups.has(d)) groups.set(d, []);
        groups.get(d).push(n.name);
      }
      for (const arr of groups.values()) arr.sort();

      const width = 1400;
      const height = 700;
      const padX = 110;
      const padY = 45;
      const spanX = Math.max(1, width - 2 * padX);
      const spanY = Math.max(1, height - 2 * padY);
      const denom = Math.max(1, maxLayer);

      const atomPos = new Map();
      const orderedLayers = [...groups.keys()].sort((a, b) => a - b);
      for (const d of orderedLayers) {
        const items = groups.get(d) || [];
        const x = padX + (d / denom) * spanX;
        for (let i = 0; i < items.length; i++) {
          const y = padY + ((i + 1) / (items.length + 1)) * spanY;
          atomPos.set(items[i], { x, y });
        }
      }

      function el(name, attrs = {}, text = null) {
        const x = document.createElementNS("http://www.w3.org/2000/svg", name);
        for (const [k, v] of Object.entries(attrs)) x.setAttribute(k, String(v));
        if (text !== null) x.textContent = text;
        return x;
      }

      const defs = el("defs");
      const marker = el("marker", {
        id: "arrow", markerWidth: 8, markerHeight: 8, refX: 7, refY: 3.5, orient: "auto",
      });
      marker.appendChild(el("path", { d: "M0,0 L8,3.5 L0,7 z", fill: "#6b7280" }));
      defs.appendChild(marker);
      svg.appendChild(defs);

      const rulePos = new Map();
      for (const e of rules) {
        const headPos = atomPos.get(e.head);
        if (!headPos) continue;
        const inputs = (e.inputs || []).map(name => atomPos.get(name)).filter(Boolean);
        const edgeId = Number(e.edge_id || 0);
        let x = headPos.x - 60;
        if (inputs.length > 0) {
          const maxInX = Math.max(...inputs.map(p => p.x));
          x = maxInX + Math.max(30, (headPos.x - maxInX) * 0.55);
        }
        const baseY = inputs.length > 0
          ? ((inputs.reduce((s, p) => s + p.y, 0) + headPos.y) / (inputs.length + 1))
          : headPos.y;
        const y = baseY + ((edgeId % 11) - 5) * 3;
        rulePos.set(edgeId, { x, y });
      }

      for (const e of rules) {
        const headPos = atomPos.get(e.head);
        const edgeId = Number(e.edge_id || 0);
        const ep = rulePos.get(edgeId);
        if (!headPos || !ep) continue;
        const inputs = e.inputs || [];
        const negated = e.negated_inputs || [];
        for (let i = 0; i < inputs.length; i++) {
          const inp = inputs[i];
          const srcPos = atomPos.get(inp);
          if (!srcPos) continue;
          const isProb = !!e.is_probabilistic;
          const isNeg = !!negated[i];
          const mx = (srcPos.x + ep.x) / 2;
          const my = (srcPos.y + ep.y) / 2 + ((i % 3) - 1) * 2;
          const d = `M${srcPos.x},${srcPos.y} Q${mx},${my} ${ep.x},${ep.y}`;
          svg.appendChild(el("path", {
            d,
            fill: "none",
            stroke: isProb ? "#b45309" : "#9ca3af",
            "stroke-width": isProb ? 2.0 : 1.2,
            "stroke-dasharray": isNeg ? "5,4" : "",
            "marker-end": "url(#arrow)",
            opacity: 0.9,
          }));
        }

        const d2 = `M${ep.x},${ep.y} L${headPos.x},${headPos.y}`;
        svg.appendChild(el("path", {
          d: d2,
          fill: "none",
          stroke: (!!e.is_probabilistic) ? "#b45309" : "#6b7280",
          "stroke-width": (!!e.is_probabilistic) ? 2.0 : 1.5,
          "marker-end": "url(#arrow)",
          opacity: 0.95,
        }));

        const ruleFill = (!!e.is_probabilistic) ? "#f59e0b" : "#f3f4f6";
        svg.appendChild(el("rect", {
          x: ep.x - 6, y: ep.y - 6, width: 12, height: 12,
          rx: 2, ry: 2, fill: ruleFill, stroke: "#374151", "stroke-width": 0.8,
        }));
      }

      for (let i = 0; i < atoms.length; i += 1) {
        const n = atoms[i];
        const p = atomPos.get(n.name);
        if (!p) continue;
        let fill = "#6b7280";
        if (n.is_entry) fill = "#15803d";
        else if (n.is_exit) fill = "#b91c1c";
        else if (n.is_fact) fill = "#1d4ed8";

        svg.appendChild(el("circle", {
          cx: p.x, cy: p.y, r: 8, fill,
          stroke: "#111827", "stroke-width": 0.5,
          "data-node-label": n.name,
          "data-node-index": i,
          class: "atom-node",
        }));
        svg.appendChild(el("text", {
          x: p.x + 10, y: p.y + 4,
          "font-size": 11, fill: "#111827",
          "data-node-label": n.name,
          "data-node-index": i,
          class: "atom-label",
        }, n.name));
      }

      els.cyDiv.style.display = "none";
      els.svg.style.display = "block";
      return true;
    }

    function drawRegion(region) {
      if (!drawRegionCy(region)) {
        drawRegionSvg(region);
      }
    }

    function renderRegionTable(region) {
      clearNode(els.edgeRows);
      const edges = (region.region_graph && region.region_graph.edges) ? region.region_graph.edges : [];
      for (const e of edges) {
        const tr = document.createElement("tr");
        const cells = [
          e.edge_id,
          e.head,
          (e.inputs || []).join(", "),
          (e.negated_inputs || []).map(x => x ? "1" : "0").join(", "),
          e.probability,
          e.is_probabilistic ? "yes" : "no",
        ];
        for (const c of cells) {
          const td = document.createElement("td");
          td.textContent = String(c);
          tr.appendChild(td);
        }
        els.edgeRows.appendChild(tr);
      }
    }

    function render() {
      const rows = filteredRegions();
      if (regionNav.index >= rows.length) regionNav.index = Math.max(0, rows.length - 1);

      if (rows.length === 0) {
        els.pos.textContent = "0/0";
        const st = currentState();
        if (currentSourceKey === "general-siso" && st && st.general_meta && st.general_meta.enabled && !generalDetected.has(rewriteIndex)) {
          els.regionMeta.textContent = "General SISO is available but not detected for this rewrite state. Click `Detect General SISO`.";
        } else {
          els.regionMeta.textContent = "No region matches current filter.";
        }
        els.regionPredMeta.textContent = "";
        if (cy) {
          cy.elements().remove();
        }
        clearNode(els.svg);
        clearNode(els.edgeRows);
        resetSearchStateButKeepQuery();
        setSearchStatus("No graph region is currently displayed for search.");
        return;
      }

      const r = rows[regionNav.index];
      const st = currentState();
      els.pos.textContent = `${regionNav.index + 1}/${rows.length}`;
      const regionNodes = (r.region_graph && r.region_graph.nodes) ? r.region_graph.nodes : [];
      const regionEdges = (r.region_graph && r.region_graph.edges) ? r.region_graph.edges : [];
      const preds = (r.region_graph && r.region_graph.predicates) ? r.region_graph.predicates : [];
      const strataStats = (r.region_graph && r.region_graph.strata_stats) ? r.region_graph.strata_stats : {};
      const subStrata = (strataStats.strata_count === null || strataStats.strata_count === undefined)
        ? "n/a"
        : strataStats.strata_count;
      const subNegStrata = (strataStats.negation_strata_count === null || strataStats.negation_strata_count === undefined)
        ? "n/a"
        : strataStats.negation_strata_count;
      const subStratifiable = (strataStats.stratifiable === undefined)
        ? "n/a"
        : (strataStats.stratifiable ? 1 : 0);

      const gm = (st && st.general_meta) ? st.general_meta : {};
      const fm = (st && st.fast_meta) ? st.fast_meta : {};
      const action = (st && st.rewrite_action) ? st.rewrite_action : {};
      els.regionMeta.textContent = [
        `rewrite_state=${rewriteIndex}/${Math.max(0, rewriteStates.length - 1)}`,
        `state_label=${st ? st.label : "n/a"}`,
        `entry=${r.entry}`,
        `exit=${r.exit}`,
        `kind=${r.kind}`,
        `mode=${r.mode}`,
        `nodes=${r.node_count}`,
        `edges=${r.edge_count}`,
        `random_vars=${r.random_vars}`,
        `entry_pred_count=${r.entry_pred_count}`,
        `candidate_exit=${r.candidate_exit}`,
        `cone_nodes=${r.cone_nodes || 0}`,
        `cone_edges=${r.cone_edges || 0}`,
        `cone_truncated=${!!r.cone_truncated}`,
        `subgraph_predicates=${strataStats.predicate_count || preds.length || 0}`,
        `subgraph_strata=${subStrata}`,
        `subgraph_recursive_scc=${strataStats.predicate_recursive_scc_count || 0}`,
        `subgraph_cond_layers=${strataStats.predicate_condensation_layers || 0}`,
        `subgraph_neg_strata=${subNegStrata}`,
        `subgraph_stratifiable=${subStratifiable}`,
        `subgraph_neg_dep_edges=${strataStats.negative_dependency_edges || 0}`,
        `fast_regions_current=${fm.regions_found || 0}`,
        `general_detected=${gm.regions_found || 0}`,
        `general_after_fast_filter=${gm.regions_after_fast_filter || 0}`,
        `last_rewrite_detect=${action.detected_regions !== undefined ? action.detected_regions : "-"}`,
        `last_rewrite_apply=${action.rewritten_regions !== undefined ? action.rewritten_regions : "-"}`,
      ].join("\\n");

      els.regionPredMeta.textContent = [
        `entry_predicate=${predOf(String(r.entry))}`,
        `exit_predicate=${predOf(String(r.exit))}`,
        `predicates_in_region=${preds.length}`,
        "",
        preds.join("\\n"),
        "",
        `node_rows=${regionNodes.length}`,
        `edge_rows=${regionEdges.length}`,
      ].join("\\n");

      drawRegion(r);
      renderRegionTable(r);

      const q = String(els.searchInput.value || "").trim();
      if (q) {
        searchInRenderedGraph(q, false);
      } else {
        clearSearchVisuals();
      }
    }

    function onRewriteStateChanged() {
      if (currentSourceKey === "general-siso" && !generalDetected.has(rewriteIndex)) {
        currentSourceKey = "full-graph";
      }
      regionNav.index = 0;
      refreshHeaderSummary();
      refreshRewriteControls();
      refreshSourceSelect();
      refreshFiltersForSource();
      setSearchStatus("Search target node by name.");
      render();
    }

    function locateNodeInFullGraph(advance) {
      const q = String(els.searchInput.value || "").trim();
      if (!q) {
        clearSearchVisuals();
        searchState.query = "";
        setSearchStatus("Type a node name first.");
        return;
      }
      switchToFullGraphForSearch();
      searchInRenderedGraph(q, !!advance);
    }

    function setup() {
      els.inputPath.textContent = String(payload.input_file || "");
      if (!rewriteStates.length) {
        els.regionMeta.textContent = "No rewrite states are available.";
        return;
      }

      refreshHeaderSummary();
      refreshRewriteControls();
      refreshSourceSelect();
      refreshFiltersForSource();

      els.sourceSel.addEventListener("change", () => {
        currentSourceKey = els.sourceSel.value || currentSourceKey;
        regionNav.index = 0;
        refreshFiltersForSource();
        render();
      });
      els.prev.addEventListener("click", () => {
        if (regionNav.index > 0) {
          regionNav.index -= 1;
          render();
        }
      });
      els.next.addEventListener("click", () => {
        const rows = filteredRegions();
        if (regionNav.index + 1 < rows.length) {
          regionNav.index += 1;
          render();
        }
      });
      els.rewritePrev.addEventListener("click", () => {
        if (rewriteIndex > 0) {
          rewriteIndex -= 1;
          onRewriteStateChanged();
        }
      });
      els.rewriteNext.addEventListener("click", () => {
        if (rewriteIndex + 1 < rewriteStates.length) {
          rewriteIndex += 1;
          onRewriteStateChanged();
        }
      });
      els.rewriteFinal.addEventListener("click", () => {
        if (rewriteStates.length > 0 && rewriteIndex + 1 < rewriteStates.length) {
          rewriteIndex = rewriteStates.length - 1;
          onRewriteStateChanged();
        }
      });
      els.rewriteReset.addEventListener("click", () => {
        rewriteIndex = 0;
        onRewriteStateChanged();
      });
      els.detectGeneral.addEventListener("click", () => {
        const st = currentState();
        const gm = (st && st.general_meta) ? st.general_meta : {};
        if (!gm.enabled) return;
        generalDetected.add(rewriteIndex);
        currentSourceKey = "general-siso";
        regionNav.index = 0;
        refreshRewriteControls();
        refreshSourceSelect();
        refreshFiltersForSource();
        render();
      });
      els.searchBtn.addEventListener("click", () => {
        locateNodeInFullGraph(false);
      });
      els.searchNextBtn.addEventListener("click", () => {
        locateNodeInFullGraph(true);
      });
      els.searchClearBtn.addEventListener("click", () => {
        els.searchInput.value = "";
        clearSearchVisuals();
        searchState.query = "";
        setSearchStatus("Search target node by name.");
      });
      els.searchInput.addEventListener("keydown", (ev) => {
        if (ev.key === "Enter") {
          ev.preventDefault();
          locateNodeInFullGraph(false);
        }
      });
      els.searchInput.addEventListener("input", () => {
        if (!String(els.searchInput.value || "").trim()) {
          clearSearchVisuals();
          searchState.query = "";
          setSearchStatus("Search target node by name.");
        }
      });

      for (const sel of [els.kind, els.mode, els.entryPred, els.exitPred]) {
        sel.addEventListener("change", () => {
          regionNav.index = 0;
          render();
        });
      }

      render();
    }

    setup();
  </script>
</body>
</html>
"""
    html = html.replace("__PAYLOAD_B64__", payload_b64)
    out_html.parent.mkdir(parents=True, exist_ok=True)
    out_html.write_text(html, encoding="utf-8")
    return True


def write_interactive_html(report: Dict[str, object], out_html: Path, source: str) -> None:
    if write_interactive_html_stateful(report, out_html, source):
        return

    if report.get("input_kind") != "derivation":
        raise SystemExit("interactive mode requires derivation input (facts+rules)")

    sources = collect_interactive_sources(report)
    if not sources:
        raise SystemExit("no interactive data source is available for this report")
    selected_source = source if source in sources else next(iter(sources.keys()))
    source_label = str(sources[selected_source].get("label", selected_source))
    regions = sources[selected_source].get("regions", [])

    payload = {
        "input_file": report.get("input_file"),
        "source_label": source_label,
        "selected_source": selected_source,
        "sources": sources,
        "graph_stats": report.get("graph_stats", {}),
        "general_siso": {
            "mode": source_label,
            "candidates_tested": len(regions),
            "regions_found": len(regions),
            "regions": regions,
        },
    }
    payload_b64 = base64.b64encode(json.dumps(payload, ensure_ascii=False).encode("utf-8")).decode("ascii")

    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Derivation Graph Interactive Analyzer</title>
  <style>
    :root {{
      --bg: #f6f7f9;
      --panel: #ffffff;
      --ink: #222;
      --muted: #6b7280;
      --line: #d1d5db;
      --accent: #0f766e;
      --warn: #b45309;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      font-family: ui-sans-serif, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: var(--bg);
      color: var(--ink);
    }}
    .wrap {{
      max-width: 1600px;
      margin: 0 auto;
      padding: 16px;
      display: grid;
      gap: 12px;
    }}
    .card {{
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 12px;
    }}
    .row {{
      display: grid;
      grid-template-columns: 1.3fr 1fr 1fr 1fr 1fr auto auto auto;
      gap: 8px;
      align-items: center;
    }}
    .meta {{
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }}
    .meta pre {{
      margin: 0;
      padding: 10px;
      background: #f9fafb;
      border: 1px solid var(--line);
      border-radius: 8px;
      overflow: auto;
      font-size: 12px;
      line-height: 1.4;
    }}
    label {{
      font-size: 12px;
      color: var(--muted);
      display: block;
      margin-bottom: 4px;
    }}
    select, button {{
      width: 100%;
      padding: 8px 10px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
      font-size: 13px;
    }}
    button {{
      cursor: pointer;
      font-weight: 600;
    }}
    button:hover {{
      border-color: var(--accent);
    }}
    #position {{
      text-align: center;
      font-size: 13px;
      color: var(--muted);
      min-width: 64px;
    }}
    #graph {{
      width: 100%;
      height: 640px;
      border: 1px solid var(--line);
      border-radius: 8px;
      background: #fff;
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      font-size: 12px;
    }}
    th, td {{
      border: 1px solid var(--line);
      text-align: left;
      padding: 6px 8px;
      vertical-align: top;
      word-break: break-word;
    }}
    th {{
      background: #f3f4f6;
      position: sticky;
      top: 0;
    }}
    .table-wrap {{
      max-height: 320px;
      overflow: auto;
      border: 1px solid var(--line);
      border-radius: 8px;
    }}
    .pill {{
      display: inline-block;
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid var(--line);
      font-size: 11px;
      color: #374151;
      background: #f9fafb;
    }}
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <div><strong>Derivation Graph Interactive Analyzer</strong></div>
      <div style="margin-top:4px; font-size:12px; color:#6b7280;">Input: <code id="inputPath"></code></div>
      <div style="margin-top:2px; font-size:12px; color:#6b7280;">Graph: <span id="graphSummary"></span></div>
      <div style="margin-top:2px; font-size:12px; color:#6b7280;">Source: <span id="sourceLabel"></span></div>
    </div>

    <div class="card">
      <div class="row">
        <div>
          <label for="sourceFilter">Source</label>
          <select id="sourceFilter"></select>
        </div>
        <div>
          <label for="kindFilter">SISO kind</label>
          <select id="kindFilter"></select>
        </div>
        <div>
          <label for="modeFilter">Detect mode</label>
          <select id="modeFilter"></select>
        </div>
        <div>
          <label for="entryPredFilter">Entry predicate</label>
          <select id="entryPredFilter"></select>
        </div>
        <div>
          <label for="exitPredFilter">Exit predicate</label>
          <select id="exitPredFilter"></select>
        </div>
        <button id="prevBtn">Previous</button>
        <div id="position">0/0</div>
        <button id="nextBtn">Next</button>
      </div>
    </div>

    <div class="card meta">
      <pre id="regionMeta"></pre>
      <pre id="regionPredMeta"></pre>
    </div>

    <div class="card">
      <svg id="graph" viewBox="0 0 1400 700" preserveAspectRatio="xMidYMid meet"></svg>
      <div style="margin-top:8px; font-size:12px; color:#6b7280;">
        Legend:
        <span class="pill">entry: green</span>
        <span class="pill">exit: red</span>
        <span class="pill">fact: blue</span>
        <span class="pill">derived: gray</span>
        <span class="pill">rule node: square</span>
        <span class="pill">probabilistic edge: orange</span>
      </div>
    </div>

    <div class="card">
      <div style="margin-bottom:8px;"><strong>Region rules</strong></div>
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>edge_id</th>
              <th>head</th>
              <th>inputs</th>
              <th>negated</th>
              <th>probability</th>
              <th>prob-edge</th>
            </tr>
          </thead>
          <tbody id="edgeRows"></tbody>
        </table>
      </div>
    </div>
  </div>

  <script id="payloadB64" type="application/octet-stream">{payload_b64}</script>
  <script>
    const payload = JSON.parse(atob(document.getElementById("payloadB64").textContent.trim()));
    const sources = payload.sources || {{}};
    const sourceKeys = Object.keys(sources);
    let currentSourceKey = payload.selected_source || (sourceKeys.length > 0 ? sourceKeys[0] : "");
    const state = {{ index: 0 }};

    const els = {{
      inputPath: document.getElementById("inputPath"),
      graphSummary: document.getElementById("graphSummary"),
      sourceLabel: document.getElementById("sourceLabel"),
      sourceSel: document.getElementById("sourceFilter"),
      kind: document.getElementById("kindFilter"),
      mode: document.getElementById("modeFilter"),
      entryPred: document.getElementById("entryPredFilter"),
      exitPred: document.getElementById("exitPredFilter"),
      prev: document.getElementById("prevBtn"),
      next: document.getElementById("nextBtn"),
      pos: document.getElementById("position"),
      regionMeta: document.getElementById("regionMeta"),
      regionPredMeta: document.getElementById("regionPredMeta"),
      edgeRows: document.getElementById("edgeRows"),
      svg: document.getElementById("graph"),
    }};

    function uniqSorted(values) {{
      return [...new Set(values)].sort((a, b) => String(a).localeCompare(String(b)));
    }}

    function predOf(atomName) {{
      const i = atomName.indexOf("(");
      return i < 0 ? atomName : atomName.slice(0, i);
    }}

    function currentSource() {{
      return sources[currentSourceKey] || {{ label: currentSourceKey || "unknown", regions: [] }};
    }}

    function currentRegions() {{
      const src = currentSource();
      return Array.isArray(src.regions) ? src.regions : [];
    }}

    function fillSourceSelect() {{
      els.sourceSel.innerHTML = "";
      for (const key of sourceKeys) {{
        const src = sources[key] || {{ label: key, regions: [] }};
        const cnt = Array.isArray(src.regions) ? src.regions.length : 0;
        const op = document.createElement("option");
        op.value = key;
        op.textContent = `${{key}} (${{src.label}}, n=${{cnt}})`;
        els.sourceSel.appendChild(op);
      }}
      if (!sourceKeys.includes(currentSourceKey) && sourceKeys.length > 0) {{
        currentSourceKey = sourceKeys[0];
      }}
      els.sourceSel.value = currentSourceKey;
    }}

    function fillSelect(sel, values, allLabel) {{
      sel.innerHTML = "";
      const first = document.createElement("option");
      first.value = "__all__";
      first.textContent = allLabel;
      sel.appendChild(first);
      for (const v of values) {{
        const op = document.createElement("option");
        op.value = String(v);
        op.textContent = String(v);
        sel.appendChild(op);
      }}
    }}

    function currentFilter() {{
      return {{
        kind: els.kind.value || "__all__",
        mode: els.mode.value || "__all__",
        entryPred: els.entryPred.value || "__all__",
        exitPred: els.exitPred.value || "__all__",
      }};
    }}

    function filteredRegions() {{
      const regs = currentRegions();
      const f = currentFilter();
      return regs.filter(r => {{
        if (f.kind !== "__all__" && String(r.kind) !== f.kind) return false;
        if (f.mode !== "__all__" && String(r.mode) !== f.mode) return false;
        if (f.entryPred !== "__all__" && predOf(String(r.entry)) !== f.entryPred) return false;
        if (f.exitPred !== "__all__" && predOf(String(r.exit)) !== f.exitPred) return false;
        return true;
      }});
    }}

    function clearNode(el) {{
      while (el.firstChild) el.removeChild(el.firstChild);
    }}

    function drawRegion(region) {{
      const svg = els.svg;
      clearNode(svg);
      const graph = region.region_graph || {{ nodes: [], edges: [] }};
      const atoms = graph.nodes || [];
      const rules = graph.edges || [];
      const byName = new Map(atoms.map(n => [n.name, n]));

      const succ = new Map(atoms.map(n => [n.name, new Set()]));
      const atomInDegree = new Map(atoms.map(n => [n.name, 0]));
      for (const e of rules) {{
        if (!byName.has(e.head)) continue;
        const inputs = e.inputs || [];
        for (const inp of inputs) {{
          if (!byName.has(inp)) continue;
          succ.get(inp).add(e.head);
          atomInDegree.set(e.head, (atomInDegree.get(e.head) || 0) + 1);
        }}
      }}

      const layer = new Map();
      const q = [];
      if (byName.has(region.entry)) {{
        layer.set(region.entry, 0);
        q.push(region.entry);
      }} else {{
        for (const n of atoms) {{
          if ((atomInDegree.get(n.name) || 0) === 0) {{
            layer.set(n.name, 0);
            q.push(n.name);
          }}
        }}
        if (q.length === 0 && atoms.length > 0) {{
          layer.set(atoms[0].name, 0);
          q.push(atoms[0].name);
        }}
      }}

      while (q.length > 0) {{
        const u = q.shift();
        const d = layer.get(u) || 0;
        const nexts = succ.get(u) || [];
        for (const v of nexts) {{
          if (!byName.has(v)) continue;
          if (!layer.has(v)) {{
            layer.set(v, d + 1);
            q.push(v);
          }} else if ((layer.get(v) || 0) < d + 1) {{
            // Keep farther layer for readability in fan-out/fan-in areas.
            layer.set(v, d + 1);
          }}
        }}
      }}

      let maxLayer = 0;
      for (const n of atoms) {{
        const d = layer.has(n.name) ? (layer.get(n.name) || 0) : 0;
        maxLayer = Math.max(maxLayer, d);
      }}
      const fallbackLayer = maxLayer + 1;
      for (const n of atoms) {{
        if (!layer.has(n.name)) layer.set(n.name, fallbackLayer);
      }}
      maxLayer = Math.max(maxLayer, fallbackLayer);

      const groups = new Map();
      for (const n of atoms) {{
        const d = layer.get(n.name) || 0;
        if (!groups.has(d)) groups.set(d, []);
        groups.get(d).push(n.name);
      }}
      for (const arr of groups.values()) arr.sort();

      const width = 1400;
      const height = 700;
      const padX = 110;
      const padY = 45;
      const spanX = Math.max(1, width - 2 * padX);
      const spanY = Math.max(1, height - 2 * padY);
      const denom = Math.max(1, maxLayer);

      const atomPos = new Map();
      const orderedLayers = [...groups.keys()].sort((a, b) => a - b);
      for (const d of orderedLayers) {{
        const items = groups.get(d) || [];
        const x = padX + (d / denom) * spanX;
        for (let i = 0; i < items.length; i++) {{
          const y = padY + ((i + 1) / (items.length + 1)) * spanY;
          atomPos.set(items[i], {{ x, y }});
        }}
      }}

      function el(name, attrs = {{}}, text = null) {{
        const x = document.createElementNS("http://www.w3.org/2000/svg", name);
        for (const [k, v] of Object.entries(attrs)) x.setAttribute(k, String(v));
        if (text !== null) x.textContent = text;
        return x;
      }}

      const defs = el("defs");
      const marker = el("marker", {{
        id: "arrow", markerWidth: 8, markerHeight: 8, refX: 7, refY: 3.5, orient: "auto",
      }});
      marker.appendChild(el("path", {{ d: "M0,0 L8,3.5 L0,7 z", fill: "#6b7280" }}));
      defs.appendChild(marker);
      svg.appendChild(defs);

      const rulePos = new Map();
      for (const e of rules) {{
        const headPos = atomPos.get(e.head);
        if (!headPos) continue;
        const inputs = (e.inputs || []).map(name => atomPos.get(name)).filter(Boolean);
        const edgeId = Number(e.edge_id || 0);
        let x = headPos.x - 60;
        if (inputs.length > 0) {{
          const maxInX = Math.max(...inputs.map(p => p.x));
          x = maxInX + Math.max(30, (headPos.x - maxInX) * 0.55);
        }}
        const baseY = inputs.length > 0
          ? ((inputs.reduce((s, p) => s + p.y, 0) + headPos.y) / (inputs.length + 1))
          : headPos.y;
        const y = baseY + ((edgeId % 11) - 5) * 3;
        rulePos.set(edgeId, {{ x, y }});
      }}

      for (const e of rules) {{
        const headPos = atomPos.get(e.head);
        const edgeId = Number(e.edge_id || 0);
        const ep = rulePos.get(edgeId);
        if (!headPos || !ep) continue;
        const inputs = e.inputs || [];
        const negated = e.negated_inputs || [];
        for (let i = 0; i < inputs.length; i++) {{
          const inp = inputs[i];
          const srcPos = atomPos.get(inp);
          if (!srcPos) continue;
          const isProb = !!e.is_probabilistic;
          const isNeg = !!negated[i];
          const mx = (srcPos.x + ep.x) / 2;
          const my = (srcPos.y + ep.y) / 2 + ((i % 3) - 1) * 2;
          const d = `M${{srcPos.x}},${{srcPos.y}} Q${{mx}},${{my}} ${{ep.x}},${{ep.y}}`;
          svg.appendChild(el("path", {{
            d,
            fill: "none",
            stroke: isProb ? "#b45309" : "#9ca3af",
            "stroke-width": isProb ? 2.0 : 1.2,
            "stroke-dasharray": isNeg ? "5,4" : "",
            "marker-end": "url(#arrow)",
            opacity: 0.9,
          }}));
        }}

        const d2 = `M${{ep.x}},${{ep.y}} L${{headPos.x}},${{headPos.y}}`;
        svg.appendChild(el("path", {{
          d: d2,
          fill: "none",
          stroke: (!!e.is_probabilistic) ? "#b45309" : "#6b7280",
          "stroke-width": (!!e.is_probabilistic) ? 2.0 : 1.5,
          "marker-end": "url(#arrow)",
          opacity: 0.95,
        }}));

        const ruleFill = (!!e.is_probabilistic) ? "#f59e0b" : "#f3f4f6";
        svg.appendChild(el("rect", {{
          x: ep.x - 6, y: ep.y - 6, width: 12, height: 12,
          rx: 2, ry: 2, fill: ruleFill, stroke: "#374151", "stroke-width": 0.8,
        }}));
      }}

      for (const n of atoms) {{
        const p = atomPos.get(n.name);
        if (!p) continue;
        let fill = "#6b7280";
        if (n.is_entry) fill = "#15803d";
        else if (n.is_exit) fill = "#b91c1c";
        else if (n.is_fact) fill = "#1d4ed8";

        svg.appendChild(el("circle", {{
          cx: p.x, cy: p.y, r: 8, fill,
          stroke: "#111827", "stroke-width": 0.5,
        }}));
        svg.appendChild(el("text", {{
          x: p.x + 10, y: p.y + 4,
          "font-size": 11, fill: "#111827",
        }}, n.name));
      }}
    }}

    function renderRegionTable(region) {{
      clearNode(els.edgeRows);
      const edges = (region.region_graph && region.region_graph.edges) ? region.region_graph.edges : [];
      for (const e of edges) {{
        const tr = document.createElement("tr");
        const cells = [
          e.edge_id,
          e.head,
          (e.inputs || []).join(", "),
          (e.negated_inputs || []).map(x => x ? "1" : "0").join(", "),
          e.probability,
          e.is_probabilistic ? "yes" : "no",
        ];
        for (const c of cells) {{
          const td = document.createElement("td");
          td.textContent = String(c);
          tr.appendChild(td);
        }}
        els.edgeRows.appendChild(tr);
      }}
    }}

    function render() {{
      const rows = filteredRegions();
      if (state.index >= rows.length) state.index = Math.max(0, rows.length - 1);
      if (rows.length === 0) {{
        els.pos.textContent = "0/0";
        els.regionMeta.textContent = "No region matches current filter.";
        els.regionPredMeta.textContent = "";
        clearNode(els.svg);
        clearNode(els.edgeRows);
        return;
      }}

      const r = rows[state.index];
      els.pos.textContent = `${{state.index + 1}}/${{rows.length}}`;
      const regionNodes = (r.region_graph && r.region_graph.nodes) ? r.region_graph.nodes : [];
      const regionEdges = (r.region_graph && r.region_graph.edges) ? r.region_graph.edges : [];
      const preds = (r.region_graph && r.region_graph.predicates) ? r.region_graph.predicates : [];
      const strataStats = (r.region_graph && r.region_graph.strata_stats) ? r.region_graph.strata_stats : {{}};
      const subStrata = (strataStats.strata_count === null || strataStats.strata_count === undefined)
        ? "n/a"
        : strataStats.strata_count;
      const subNegStrata = (strataStats.negation_strata_count === null || strataStats.negation_strata_count === undefined)
        ? "n/a"
        : strataStats.negation_strata_count;
      const subStratifiable = (strataStats.stratifiable === undefined)
        ? "n/a"
        : (strataStats.stratifiable ? 1 : 0);

      els.regionMeta.textContent = [
        `entry=${{r.entry}}`,
        `exit=${{r.exit}}`,
        `kind=${{r.kind}}`,
        `mode=${{r.mode}}`,
        `nodes=${{r.node_count}}`,
        `edges=${{r.edge_count}}`,
        `random_vars=${{r.random_vars}}`,
        `entry_pred_count=${{r.entry_pred_count}}`,
        `candidate_exit=${{r.candidate_exit}}`,
        `cone_nodes=${{r.cone_nodes || 0}}`,
        `cone_edges=${{r.cone_edges || 0}}`,
        `cone_truncated=${{!!r.cone_truncated}}`,
        `subgraph_predicates=${{strataStats.predicate_count || preds.length || 0}}`,
        `subgraph_strata=${{subStrata}}`,
        `subgraph_recursive_scc=${{strataStats.predicate_recursive_scc_count || 0}}`,
        `subgraph_cond_layers=${{strataStats.predicate_condensation_layers || 0}}`,
        `subgraph_neg_strata=${{subNegStrata}}`,
        `subgraph_stratifiable=${{subStratifiable}}`,
        `subgraph_neg_dep_edges=${{strataStats.negative_dependency_edges || 0}}`,
      ].join("\\n");

      els.regionPredMeta.textContent = [
        `entry_predicate=${{predOf(String(r.entry))}}`,
        `exit_predicate=${{predOf(String(r.exit))}}`,
        `predicates_in_region=${{preds.length}}`,
        "",
        preds.join("\\n"),
        "",
        `node_rows=${{regionNodes.length}}`,
        `edge_rows=${{regionEdges.length}}`,
      ].join("\\n");

      drawRegion(r);
      renderRegionTable(r);
    }}

    function setup() {{
      els.inputPath.textContent = String(payload.input_file || "");
      const s = payload.graph_stats || {{}};
      const strata = (s.strata_count === null || s.strata_count === undefined) ? "n/a" : s.strata_count;
      const negStrata = (s.negation_strata_count === null || s.negation_strata_count === undefined) ? "n/a" : s.negation_strata_count;
      const stratifiable = (s.stratifiable === undefined) ? "n/a" : (s.stratifiable ? 1 : 0);
      els.graphSummary.textContent =
        `nodes=${{s.nodes || 0}}, edges=${{s.edges || 0}}, disjunction=${{s.disjunction_nodes || 0}}, random_vars=${{s.random_variables || 0}}, predicates=${{s.predicate_count || 0}}, strata=${{strata}}, recursive_scc=${{s.predicate_recursive_scc_count || 0}}, cond_layers=${{s.predicate_condensation_layers || 0}}, neg_strata=${{negStrata}}, stratifiable=${{stratifiable}}`;
      fillSourceSelect();

      const refreshFiltersForSource = () => {{
        const regs = currentRegions();
        fillSelect(els.kind, uniqSorted(regs.map(r => r.kind || "unknown")), "all kinds");
        fillSelect(els.mode, uniqSorted(regs.map(r => r.mode || "unknown")), "all modes");
        fillSelect(els.entryPred, uniqSorted(regs.map(r => predOf(String(r.entry)))), "all entry predicates");
        fillSelect(els.exitPred, uniqSorted(regs.map(r => predOf(String(r.exit)))), "all exit predicates");
        const src = currentSource();
        els.sourceLabel.textContent = `${{currentSourceKey}} | ${{src.label || "unknown"}}`;
      }};
      refreshFiltersForSource();

      els.sourceSel.addEventListener("change", () => {{
        currentSourceKey = els.sourceSel.value || currentSourceKey;
        state.index = 0;
        refreshFiltersForSource();
        render();
      }});

      els.prev.addEventListener("click", () => {{
        if (state.index > 0) {{
          state.index -= 1;
          render();
        }}
      }});
      els.next.addEventListener("click", () => {{
        const rows = filteredRegions();
        if (state.index + 1 < rows.length) {{
          state.index += 1;
          render();
        }}
      }});

      for (const sel of [els.kind, els.mode, els.entryPred, els.exitPred]) {{
        sel.addEventListener("change", () => {{
          state.index = 0;
          render();
        }});
      }}
      render();
    }}

    setup();
  </script>
</body>
</html>
"""
    out_html.parent.mkdir(parents=True, exist_ok=True)
    out_html.write_text(html, encoding="utf-8")


def main() -> int:
    args = parse_args()
    path = args.input_json.resolve()
    if not path.exists():
        raise SystemExit(f"input JSON does not exist: {path}")

    raw = json.loads(path.read_text(encoding="utf-8"))
    report: Dict[str, object] = {
        "input_file": str(path),
        "generated_at_ms": _now_ms(),
    }

    # Lightweight graph summary file from --dumpstat
    if isinstance(raw, dict) and "rules" not in raw and "facts" not in raw and "nodes" in raw and "edges" in raw:
        report["input_kind"] = "graph_summary"
        report["graph_summary"] = raw
        if args.interactive:
            print("[warn] interactive viewer skipped: graph summary input has no per-region subgraph")
        print_report(report, top_regions=0)
        if args.out_json:
            args.out_json.parent.mkdir(parents=True, exist_ok=True)
            args.out_json.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
            print(f"[output] wrote report JSON: {args.out_json}")
        return 0

    if not isinstance(raw, dict) or "rules" not in raw or "facts" not in raw:
        raise SystemExit("unsupported JSON format: expected derivation dump with 'facts' and 'rules'")

    report["input_kind"] = "derivation"
    graph = DerivationHyperGraph.from_derivation_json(raw)
    report["graph_stats"] = graph.basic_stats()
    report["delta_stats"] = summarize_delta(raw)
    mutable_graph = MutableDerivationGraph.from_immutable(graph)

    t_scc = _now_ms()
    succ = graph.projected_adjacency()
    scc_stats = kosaraju_scc(succ)
    report["scc_stats"] = scc_stats
    report["timing_ms"] = {"scc": _now_ms() - t_scc}

    include_general_graph = bool(
        args.interactive and (args.interactive_source == "general" or args.include_all_sources)
    )
    include_supported_graph = bool(
        args.interactive
        and args.supported_siso_mode == "on"
        and (args.interactive_source == "supported" or args.include_all_sources)
    )
    include_rewrite_graph = bool(
        args.interactive
        and args.rewrite_fixpoint
        and (args.interactive_source in ("rewrite-final", "rewrite-iter") or args.include_all_sources)
    )
    # If explore mode is enabled in interactive runs, always include region_graph payloads
    # so users can switch between full-graph and explore within one HTML.
    include_explore_graph = bool(args.interactive and args.explore_mode != "off")
    include_full_graph = bool(
        args.interactive
        and (args.interactive_source == "full-graph" or args.include_full_graph_source or args.include_all_sources)
    )

    maybe_confirm_full_graph_render(
        enabled=include_full_graph,
        graph_stats=report["graph_stats"],
        warn_nodes=args.full_graph_warn_nodes,
        warn_edges=args.full_graph_warn_edges,
        force=args.full_graph_force,
    )

    gs = analyze_general_siso(
        graph,
        mode=args.general_siso_mode,
        max_dom_nodes=args.max_dom_nodes,
        max_candidates=args.max_candidates,
        min_random_vars=args.min_random_vars,
        cone_depth=args.cone_depth,
        cone_node_limit=args.cone_node_limit,
        region_limit=args.region_limit,
        include_region_graph=include_general_graph,
    )
    report["general_siso"] = gs

    if args.supported_siso_mode == "on":
        report["supported_siso"] = analyze_supported_siso(
            mutable_graph.clone(),
            include_region_graph=include_supported_graph,
        )

    if args.rewrite_fixpoint:
        report["rewrite_sim"] = run_rewrite_fixpoint_simulation(
            mutable_graph,
            split_mode=args.rewrite_split_mode,
            max_iterations=args.rewrite_max_iterations,
            enable_compaction=(not args.rewrite_no_compaction),
            include_region_graph=include_rewrite_graph,
            split_max_new_nodes_per_pass=args.rewrite_split_max_new_nodes_per_pass,
            split_max_new_edges_per_pass=args.rewrite_split_max_new_edges_per_pass,
            split_max_groups_per_node=args.rewrite_split_max_groups_per_node,
            split_min_group_edges=args.rewrite_split_min_group_edges,
            capture_interactive_states=bool(args.interactive),
            interactive_state_include_region_graph=bool(args.interactive),
            interactive_general_siso_mode=args.general_siso_mode,
            interactive_max_dom_nodes=args.max_dom_nodes,
            interactive_max_candidates=args.max_candidates,
            interactive_min_random_vars=args.min_random_vars,
            interactive_cone_depth=args.cone_depth,
            interactive_cone_node_limit=args.cone_node_limit,
            interactive_region_limit=args.region_limit,
            interactive_explore_mode=args.explore_mode,
            interactive_explore_samples=args.explore_samples,
            interactive_explore_seed=args.explore_seed,
            interactive_explore_seed_policy=args.explore_seed_policy,
            interactive_explore_depth=args.explore_depth,
            interactive_explore_max_nodes=args.explore_max_nodes,
            interactive_explore_max_edges=args.explore_max_edges,
            interactive_explore_min_nodes=args.explore_min_nodes,
            interactive_explore_min_edges=args.explore_min_edges,
            interactive_explore_max_attempts=args.explore_max_attempts,
        )

    if args.explore_mode != "off":
        report["explore_samples"] = analyze_random_explore_samples(
            graph,
            mode=args.explore_mode,
            sample_count=args.explore_samples,
            random_seed=args.explore_seed,
            seed_policy=args.explore_seed_policy,
            depth_limit=args.explore_depth,
            max_nodes=args.explore_max_nodes,
            max_edges=args.explore_max_edges,
            min_nodes=args.explore_min_nodes,
            min_edges=args.explore_min_edges,
            max_attempts=args.explore_max_attempts,
            include_region_graph=include_explore_graph,
        )

    if include_full_graph:
        report["full_graph_view"] = build_full_graph_view(
            graph,
            include_region_graph=True,
        )

    if args.interactive:
        if args.interactive_source in ("rewrite-final", "rewrite-iter") and not args.rewrite_fixpoint:
            raise SystemExit("interactive-source rewrite-* requires --rewrite-fixpoint")
        if args.interactive_source == "supported" and args.supported_siso_mode != "on":
            raise SystemExit("interactive-source supported requires --supported-siso-mode on")
        if args.interactive_source == "explore" and args.explore_mode == "off":
            raise SystemExit("interactive-source explore requires --explore-mode != off")

    print_report(report, top_regions=args.print_top_regions)

    if args.out_json:
        args.out_json.parent.mkdir(parents=True, exist_ok=True)
        args.out_json.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
        print(f"[output] wrote report JSON: {args.out_json}")

    if args.interactive:
        out_html = args.interactive_html
        if out_html is None:
            if path.suffix:
                out_html = path.with_name(path.name[: -len(path.suffix)] + ".interactive.html")
            else:
                out_html = path.with_name(path.name + ".interactive.html")
        write_interactive_html(report, out_html, source=args.interactive_source)
        print(f"[output] wrote interactive HTML: {out_html}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
