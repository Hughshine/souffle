#!/usr/bin/env python3
"""
Regression runner for the maintained Souffle fork test suite.

Each ctest case executes one scenario end-to-end:
- generate a small program + inputs
- compile with the repo-built souffle binary
- run exact probabilistic inference
- assert semantic and artifact contracts
"""

from __future__ import annotations

import argparse
import json
import math
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

PROB_LINE_RE = re.compile(r"^\s*(.*?)\s*:\s*([+\-]?\d+(?:\.\d+)?(?:[eE][+\-]?\d+)?)\s*$")
CASES_ROOT = Path(__file__).resolve().parent / "cases"
COMMAND_TIMEOUT_SECONDS = 300
PROBABILITY_TOLERANCE = 1e-8


class CaseFailure(RuntimeError):
    """Raised when a regression case fails."""


def format_cmd(cmd: Sequence[str]) -> str:
    return " ".join(cmd)


def run_cmd(
    cmd: Sequence[str],
    cwd: Path,
    *,
    stdin_text: str | None = None,
    timeout: int = COMMAND_TIMEOUT_SECONDS,
) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        list(cmd),
        cwd=str(cwd),
        input=stdin_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )
    if proc.returncode != 0:
        raise CaseFailure(
            f"command failed (exit={proc.returncode})\n"
            f"cwd: {cwd}\n"
            f"cmd: {format_cmd(cmd)}\n"
            f"stdout:\n{proc.stdout}\n"
            f"stderr:\n{proc.stderr}"
        )
    return proc


def reset_dir(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def prepare_case_workspace(case_id: str, work_root: Path) -> Path:
    case_src = CASES_ROOT / case_id
    if not case_src.exists():
        raise CaseFailure(f"missing regression case directory: {case_src}")

    case_dir = work_root / case_id
    reset_dir(case_dir)

    for entry in sorted(case_src.iterdir()):
        dst = case_dir / entry.name
        if entry.is_dir():
            shutil.copytree(entry, dst)
        elif entry.is_file():
            shutil.copy2(entry, dst)

    program = case_dir / "compute.dl"
    input_dir = case_dir / "input"
    if not program.exists():
        raise CaseFailure(f"case {case_id} did not provide compute.dl")
    if not input_dir.exists():
        raise CaseFailure(f"case {case_id} did not provide input/")
    return case_dir


def parse_prob_file(path: Path) -> Dict[str, float]:
    if not path.exists():
        raise CaseFailure(f"missing probability output: {path}")
    results: Dict[str, float] = {}
    with path.open("r", encoding="utf-8") as f:
        for idx, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue
            m = PROB_LINE_RE.match(line)
            if not m:
                raise CaseFailure(f"malformed probability line at {path}:{idx}: {line}")
            tuple_key = m.group(1)
            prob_value = float(m.group(2))
            results[tuple_key] = prob_value
    return results


def assert_prob_close(lhs: Path, rhs: Path, *, tol: float = PROBABILITY_TOLERANCE, label: str) -> None:
    left = parse_prob_file(lhs)
    right = parse_prob_file(rhs)
    left_keys = set(left.keys())
    right_keys = set(right.keys())
    if left_keys != right_keys:
        missing = sorted(left_keys - right_keys)
        extra = sorted(right_keys - left_keys)
        raise CaseFailure(
            f"{label}: tuple key mismatch\n"
            f"lhs={lhs}\nrhs={rhs}\n"
            f"missing_in_rhs={missing[:8]}\nextra_in_rhs={extra[:8]}"
        )

    diffs: List[str] = []
    for key in sorted(left_keys):
        lv = left[key]
        rv = right[key]
        if not math.isclose(lv, rv, rel_tol=0.0, abs_tol=tol):
            diffs.append(f"{key}: lhs={lv:.12g} rhs={rv:.12g}")
            if len(diffs) >= 8:
                break
    if diffs:
        raise CaseFailure(
            f"{label}: probability mismatch (tol={tol})\n"
            f"lhs={lhs}\nrhs={rhs}\n"
            + "\n".join(diffs)
        )


def compile_compute(
    *,
    souffle_bin: Path,
    case_dir: Path,
    compile_args: Sequence[str] | None = None,
) -> Tuple[Path, Path, Path]:
    input_dir = case_dir / "input"
    output_dir = case_dir / "output_compile"
    build_dir = case_dir / "build"
    build_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    compute_dl = case_dir / "compute.dl"
    compute_bin = build_dir / "compute"

    cmd = [str(souffle_bin)]
    if compile_args:
        cmd.extend(compile_args)
    cmd.extend(
        [
            "-F",
            str(input_dir),
            "-D",
            str(output_dir),
            str(compute_dl),
            "-o",
            str(compute_bin),
        ]
    )
    run_cmd(cmd, cwd=case_dir, timeout=300)
    if not compute_bin.exists():
        raise CaseFailure(f"compile did not create binary: {compute_bin}")
    return compute_bin, input_dir, output_dir


def run_exact_once(
    *,
    compute_bin: Path,
    input_dir: Path,
    output_dir: Path,
    extra_args: Sequence[str] | None = None,
    timeout: int = COMMAND_TIMEOUT_SECONDS,
) -> subprocess.CompletedProcess[str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(compute_bin), "-F", str(input_dir), "-D", str(output_dir)]
    if extra_args:
        cmd.extend(extra_args)
    return run_cmd(cmd, cwd=compute_bin.parent, timeout=timeout)


def assert_glob_nonempty(base_dir: Path, pattern: str, *, label: str) -> None:
    matches = sorted(base_dir.glob(pattern))
    if not matches:
        raise CaseFailure(f"{label}: expected files matching {base_dir / pattern}")


def read_json(path: Path) -> object:
    if not path.exists():
        raise CaseFailure(f"missing JSON file: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as err:
        raise CaseFailure(f"malformed JSON file: {path}\n{err}") from err


def single_json_file(base_dir: Path, pattern: str, *, label: str) -> Path:
    matches = sorted(base_dir.glob(pattern))
    if len(matches) != 1:
        raise CaseFailure(
            f"{label}: expected exactly one file matching {base_dir / pattern}, found {len(matches)}"
        )
    return matches[0]


def assert_valid_debug_log(path: Path) -> dict:
    payload = read_json(path)
    if not isinstance(payload, dict):
        raise CaseFailure(f"debug log is not a JSON object: {path}")
    turns = payload.get("turns")
    if not isinstance(turns, list) or not turns:
        raise CaseFailure(f"debug log has no turns: {path}")
    first = turns[0]
    if not isinstance(first, dict) or first.get("mode") != "EXACT":
        raise CaseFailure(f"debug log first turn is not exact inference: {path}")
    stages = first.get("stages")
    if not isinstance(stages, list) or not stages:
        raise CaseFailure(f"debug log has no stages: {path}")
    return payload


def find_stage(log_payload: dict, stage_name: str, *, label: str) -> dict:
    for turn in log_payload.get("turns", []):
        for stage in turn.get("stages", []):
            if isinstance(stage, dict) and stage.get("name") == stage_name:
                return stage
    raise CaseFailure(f"{label}: missing debug stage {stage_name}")


def assert_tuple_json(value: object, *, label: str) -> None:
    if not isinstance(value, dict):
        raise CaseFailure(f"{label}: tuple is not a JSON object")
    rel = value.get("rel")
    fields = value.get("fields")
    fields_raw = value.get("fieldsRaw")
    if not isinstance(rel, str):
        raise CaseFailure(f"{label}: tuple relation name is not a string")
    if not isinstance(fields, list):
        raise CaseFailure(f"{label}: tuple fields are not an array")
    if not isinstance(fields_raw, list):
        raise CaseFailure(f"{label}: raw tuple fields are not an array")
    if len(fields) != len(fields_raw):
        raise CaseFailure(f"{label}: decoded and raw tuple fields have different arity")
    for field in fields_raw:
        if not isinstance(field, dict):
            raise CaseFailure(f"{label}: raw tuple field is not a JSON object")


def assert_valid_graph_dump_json(path: Path, *, label: str, require_nonempty: bool) -> dict:
    payload = read_json(path)
    if not isinstance(payload, dict):
        raise CaseFailure(f"{label}: graph dump JSON is not an object")
    facts = payload.get("facts")
    rules = payload.get("rules")
    if not isinstance(facts, list):
        raise CaseFailure(f"{label}: graph dump JSON has no facts array")
    if not isinstance(rules, list):
        raise CaseFailure(f"{label}: graph dump JSON has no rules array")
    if require_nonempty and not facts and not rules:
        raise CaseFailure(f"{label}: graph dump JSON is empty")
    for entry in facts:
        if not isinstance(entry, dict) or not {"name", "tuple", "probability"}.issubset(entry):
            raise CaseFailure(f"{label}: malformed fact entry in graph dump JSON")
        if not isinstance(entry["name"], str):
            raise CaseFailure(f"{label}: fact name is not a string in graph dump JSON")
        assert_tuple_json(entry["tuple"], label=f"{label}: fact tuple")
        if not isinstance(entry["probability"], (int, float)):
            raise CaseFailure(f"{label}: fact probability is not numeric in graph dump JSON")
    for entry in rules:
        if not isinstance(entry, dict) or not {"head", "headTuple", "probability", "bodies"}.issubset(entry):
            raise CaseFailure(f"{label}: malformed rule entry in graph dump JSON")
        if not isinstance(entry["head"], str):
            raise CaseFailure(f"{label}: rule head is not a string in graph dump JSON")
        assert_tuple_json(entry["headTuple"], label=f"{label}: rule head tuple")
        if not isinstance(entry["probability"], (int, float)):
            raise CaseFailure(f"{label}: rule probability is not numeric in graph dump JSON")
        bodies = entry.get("bodies")
        if not isinstance(bodies, list):
            raise CaseFailure(f"{label}: malformed rule bodies in graph dump JSON")
        for body in bodies:
            if not isinstance(body, dict) or not {"negation", "name", "tuple"}.issubset(body):
                raise CaseFailure(f"{label}: malformed rule body in graph dump JSON")
            if not isinstance(body["negation"], bool):
                raise CaseFailure(f"{label}: rule body negation is not boolean in graph dump JSON")
            if not isinstance(body["name"], str):
                raise CaseFailure(f"{label}: rule body name is not a string in graph dump JSON")
            assert_tuple_json(body["tuple"], label=f"{label}: rule body tuple")
    return payload


def assert_rewrite_executed(
    output_dir: Path,
    *,
    expected_impl: str,
    expected_probabilistic_rules: int | None = None,
    label: str,
) -> None:
    rewrite_graph = output_dir / "rewrite_final.json"
    assert_valid_graph_dump_json(rewrite_graph, label=f"{label} rewrite_final.json", require_nonempty=True)

    log_matches = [
        path
        for path in sorted(output_dir.glob("*.json"))
        if path.name not in {"derivation.json", "rewrite_final.json"} and not path.name.startswith("graph-")
    ]
    if len(log_matches) != 1:
        raise CaseFailure(f"{label}: expected exactly one runtime JSON log, found {len(log_matches)}")
    log_path = log_matches[0]
    log_payload = assert_valid_debug_log(log_path)
    stage = find_stage(log_payload, "FC_WMC_HYBRID", label=label)
    info = stage.get("info")
    if not isinstance(info, dict):
        raise CaseFailure(f"{label}: FC_WMC_HYBRID stage has no info map")
    if info.get("rewrite_impl") != expected_impl:
        raise CaseFailure(
            f"{label}: rewrite dispatcher selected the wrong implementation.\n"
            f"expected={expected_impl} actual={info.get('rewrite_impl')}"
        )
    if info.get("rewrite_strategy") != "default":
        raise CaseFailure(f"{label}: unexpected rewrite strategy {info.get('rewrite_strategy')}")
    if "rewrite_dispatch_total_rules" not in info or "rewrite_reason" not in info:
        raise CaseFailure(f"{label}: rewrite dispatch metadata is incomplete: {sorted(info.keys())}")
    if expected_probabilistic_rules is not None:
        actual = info.get("rewrite_dispatch_probabilistic_rules")
        if actual != str(expected_probabilistic_rules):
            raise CaseFailure(
                f"{label}: unexpected probabilistic-rule count.\n"
                f"expected={expected_probabilistic_rules} actual={actual}"
            )


def assert_valid_derivation_json(path: Path, *, label: str) -> None:
    payload = assert_valid_graph_dump_json(path, label=label, require_nonempty=True)
    facts = payload.get("facts")
    rules = payload.get("rules")
    if not facts:
        raise CaseFailure(f"{label}: derivation JSON has no facts")
    if not rules:
        raise CaseFailure(f"{label}: derivation JSON has no rules")


def assert_valid_graph_stats_json(path: Path, *, label: str) -> None:
    payload = read_json(path)
    if not isinstance(payload, dict):
        raise CaseFailure(f"{label}: graph stats JSON is not an object")
    required_numeric = {
        "nodes",
        "edges",
        "queries",
        "avg_in_degree",
        "max_in_degree",
        "avg_out_degree",
        "max_out_degree",
        "avg_hyperedge_inputs",
        "max_hyperedge_inputs",
    }
    missing = sorted(required_numeric - payload.keys())
    if missing:
        raise CaseFailure(f"{label}: graph stats JSON is missing keys {missing}")
    for key in required_numeric:
        if not isinstance(payload[key], (int, float)):
            raise CaseFailure(f"{label}: graph stats field {key} is not numeric")
    if payload["nodes"] <= 0:
        raise CaseFailure(f"{label}: graph stats reports an empty graph")


def assert_valid_dot(path: Path, *, label: str) -> None:
    if not path.exists():
        raise CaseFailure(f"{label}: missing DOT file {path}")
    text = path.read_text(encoding="utf-8")
    if "digraph" not in text or "->" not in text:
        raise CaseFailure(f"{label}: DOT file does not look like a non-empty graph: {path}")


def case_smoke_exact_inference(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("smoke_exact_inference", work_root)
    input_dir = case_dir / "input"
    output_dir = case_dir / "output_run"
    output_dir.mkdir(parents=True, exist_ok=True)

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    run_exact_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=output_dir)

    facts_prob = output_dir / "facts.prob"
    vals = parse_prob_file(facts_prob)
    if not vals:
        raise CaseFailure(f"smoke test produced empty probability output: {facts_prob}")


def case_rewrite_dispatch_equiv(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("rewrite_dispatch_equiv", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_base = case_dir / "out_base"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
        extra_args=["--rewrite", "--det-opt"],
    )
    base_prob = out_base / "facts.prob"

    out_rewrite = case_dir / "out_rewrite"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_rewrite,
        extra_args=["--rewrite", "--dumpjson", "--logfile", "rewrite-dispatch"],
    )
    assert_prob_close(out_rewrite / "facts.prob", base_prob, label="rewrite_dispatch_equiv")
    assert_rewrite_executed(
        out_rewrite,
        expected_impl="graph_rewrite",
        expected_probabilistic_rules=0,
        label="rewrite_dispatch_equiv",
    )


def case_exact_det_modes(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("exact_det_modes", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_base = case_dir / "out_base"
    out_detopt = case_dir / "out_detopt"

    run_exact_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_base)
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_detopt,
        extra_args=["--det-opt"],
    )

    base_prob = out_base / "facts.prob"
    detopt_prob = out_detopt / "facts.prob"
    assert_prob_close(detopt_prob, base_prob, label="det-opt_exact_inference_equivalence")


def run_language_example_case(
    *,
    case_id: str,
    expected: Dict[str, float],
    expected_rewrite_impl: str,
    expected_probabilistic_rules: int,
    souffle_bin: Path,
    work_root: Path,
) -> None:
    case_dir = prepare_case_workspace(case_id, work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_plain = case_dir / "out_plain"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_plain,
        extra_args=["--det-opt", "--dumpjson", "--logfile", "language-plain"],
    )

    out_rewrite = case_dir / "out_rewrite"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_rewrite,
        extra_args=["--det-opt", "--rewrite", "--dumpjson", "--logfile", "language-rewrite"],
    )

    plain_prob = out_plain / "facts.prob"
    rewrite_prob = out_rewrite / "facts.prob"
    probs = parse_prob_file(plain_prob)
    if set(probs.keys()) != set(expected.keys()):
        raise CaseFailure(
            f"{case_id}: full-relation query produced the wrong tuple set.\n"
            f"expected={sorted(expected.keys())}\nactual={sorted(probs.keys())}"
        )
    for key, value in expected.items():
        if not math.isclose(probs[key], value, rel_tol=0.0, abs_tol=1e-8):
            raise CaseFailure(
                f"{case_id}: unexpected plain probability for documented example.\n"
                f"tuple={key} expected={value:.12g} actual={probs[key]:.12g}"
            )
    assert_valid_derivation_json(out_plain / "derivation.json", label=f"{case_id} plain")
    assert_valid_derivation_json(out_rewrite / "derivation.json", label=f"{case_id} rewrite")

    assert_prob_close(rewrite_prob, plain_prob, tol=1e-8, label=f"{case_id}_rewrite_equivalence")
    assert_rewrite_executed(
        out_rewrite,
        expected_impl=expected_rewrite_impl,
        expected_probabilistic_rules=expected_probabilistic_rules,
        label=f"{case_id}_rewrite",
    )


def case_language_side_channel_mini(souffle_bin: Path, work_root: Path) -> None:
    run_language_example_case(
        case_id="language_side_channel_mini",
        expected={
            'explained("cache-hit")': 0.6552,
            'explained("cache-miss")': 0.153,
        },
        expected_rewrite_impl="graph_rewrite",
        expected_probabilistic_rules=0,
        souffle_bin=souffle_bin,
        work_root=work_root,
    )


def case_language_taint_mini(souffle_bin: Path, work_root: Path) -> None:
    run_language_example_case(
        case_id="language_taint_mini",
        expected={
            'alarm("cache")': 0.2904,
            'alarm("network")': 0.26129241,
        },
        expected_rewrite_impl="implicit_split",
        expected_probabilistic_rules=1,
        souffle_bin=souffle_bin,
        work_root=work_root,
    )


def case_language_symbolization_mini(souffle_bin: Path, work_root: Path) -> None:
    run_language_example_case(
        case_id="language_symbolization_mini",
        expected={
            'class_total_bytes("other",10)': 0.51,
            'class_total_bytes("widget",15)': 0.435666,
        },
        expected_rewrite_impl="graph_rewrite",
        expected_probabilistic_rules=0,
        souffle_bin=souffle_bin,
        work_root=work_root,
    )


def case_problog_string_roundtrip(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_string_roundtrip", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_dir,
        extra_args=["--dumpjson", "--logfile", "reglog"],
    )
    expected_key = 'reach("src","dst","path")'
    probs = parse_prob_file(out_dir / "facts.prob")
    if set(probs.keys()) != {expected_key}:
        raise CaseFailure(
            "problog_string_roundtrip: unexpected probability keys.\n"
            f"keys={sorted(probs.keys())}"
        )
    if not math.isclose(probs[expected_key], 1.0, rel_tol=0.0, abs_tol=1e-12):
        raise CaseFailure(
            "problog_string_roundtrip: expected conditioned query probability 1.0.\n"
            f"value={probs[expected_key]}"
        )

    derivation_json = out_dir / "derivation.json"
    if not derivation_json.exists():
        raise CaseFailure(f"problog_string_roundtrip: missing {derivation_json}")
    payload = json.loads(derivation_json.read_text(encoding="utf-8"))
    fact_names = {entry["name"] for entry in payload.get("facts", [])}
    if 'edge("src","mid")' not in fact_names:
        raise CaseFailure(
            "problog_string_roundtrip: derivation.json did not preserve decoded string fact names.\n"
            f"sample={sorted(fact_names)[:8]}"
        )
    rule_heads = {entry["head"] for entry in payload.get("rules", [])}
    if expected_key not in rule_heads:
        raise CaseFailure(
            "problog_string_roundtrip: derivation.json did not preserve decoded string rule heads.\n"
            f"sample={sorted(rule_heads)[:8]}"
        )
    tuple_fields = [
        tuple_obj["fields"]
        for entry in payload.get("facts", [])
        if (tuple_obj := entry.get("tuple")) and tuple_obj.get("rel") == "edge"
    ]
    if ["src", "mid"] not in tuple_fields:
        raise CaseFailure(
            "problog_string_roundtrip: structured tuple fields did not preserve decoded strings.\n"
            f"sample={tuple_fields[:8]}"
        )


def case_problog_symbol_aggregate_roundtrip(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_symbol_aggregate_roundtrip", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_dir,
        extra_args=["--dumpjson", "--logfile", "reglog"],
    )

    expected_key = 'rich("src","many")'
    probs = parse_prob_file(out_dir / "facts.prob")
    if set(probs.keys()) != {expected_key}:
        raise CaseFailure(
            "problog_symbol_aggregate_roundtrip: unexpected probability keys.\n"
            f"keys={sorted(probs.keys())}"
        )
    if not math.isclose(probs[expected_key], 0.8, rel_tol=0.0, abs_tol=1e-12):
        raise CaseFailure(
            "problog_symbol_aggregate_roundtrip: unexpected aggregate probability.\n"
            f"value={probs[expected_key]}"
        )

    derivation_json = out_dir / "derivation.json"
    if not derivation_json.exists():
        raise CaseFailure(f"problog_symbol_aggregate_roundtrip: missing {derivation_json}")
    payload = json.loads(derivation_json.read_text(encoding="utf-8"))

    fact_names = {entry["name"] for entry in payload.get("facts", [])}
    if 'bucket("many")' not in fact_names or 'edge("src","mid")' not in fact_names:
        raise CaseFailure(
            "problog_symbol_aggregate_roundtrip: derivation.json lost decoded symbolic facts.\n"
            f"sample={sorted(fact_names)[:8]}"
        )

    rich_rules = [entry for entry in payload.get("rules", []) if entry.get("head") == expected_key]
    if len(rich_rules) != 2:
        raise CaseFailure(
            "problog_symbol_aggregate_roundtrip: expected two derivations for the aggregate result.\n"
            f"count={len(rich_rules)}"
        )

    head_tuples = [entry.get("headTuple") for entry in rich_rules]
    if not all(tuple_obj and tuple_obj.get("fields") == ["src", "many"] for tuple_obj in head_tuples):
        raise CaseFailure(
            "problog_symbol_aggregate_roundtrip: decoded symbolic head tuple mismatch.\n"
            f"headTuples={head_tuples}"
        )

    body_name_sets = [{body["name"] for body in entry.get("bodies", [])} for entry in rich_rules]
    expected_bodies = [
        {'bucket("many")', 'edge("src","mid")'},
        {'bucket("many")', 'edge("src","dst")'},
    ]
    if sorted(body_name_sets, key=lambda s: sorted(s)) != sorted(expected_bodies, key=lambda s: sorted(s)):
        raise CaseFailure(
            "problog_symbol_aggregate_roundtrip: decoded aggregate derivations mismatch.\n"
            f"bodies={body_name_sets}"
        )


def case_problog_fact_prob_alignment(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_fact_prob_alignment", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_dir)

    probs = parse_prob_file(out_dir / "facts.prob")
    expected = {
        'seen("a","y")': 0.2,
        'seen("b","z")': 0.1,
        'seen("c","x")': 0.3,
    }
    if set(probs.keys()) != set(expected.keys()):
        raise CaseFailure(
            "problog_fact_prob_alignment: unexpected tuple keys.\n"
            f"expected={sorted(expected.keys())}\n"
            f"actual={sorted(probs.keys())}"
        )
    for key, value in expected.items():
        if not math.isclose(probs[key], value, rel_tol=0.0, abs_tol=1e-12):
            raise CaseFailure(
                "problog_fact_prob_alignment: fact/prob pairing was not preserved.\n"
                f"key={key} expected={value} actual={probs[key]}"
            )


def case_problog_large_numeric_tuple_roundtrip(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_large_numeric_tuple_roundtrip", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_dir,
        extra_args=["--dumpjson", "--logfile", "reglog"],
    )

    graph_json = json.loads((out_dir / "derivation.json").read_text(encoding="utf-8"))
    big_fields = [
        tuple_obj["fieldsRaw"]
        for entry in graph_json.get("facts", [])
        if (tuple_obj := entry.get("tuple")) and tuple_obj.get("rel") in {"big", "keep"}
    ]
    if [{"ram": "2147483647"}] not in big_fields and [2147483647] not in big_fields:
        raise CaseFailure(
            "problog_large_numeric_tuple_roundtrip: structured tuple JSON did not preserve the exact RamDomain value.\n"
            f"sample={big_fields[:4]}"
        )


def case_problog_query_named_variable_equality(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_query_named_variable_equality", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_dir)

    probs = parse_prob_file(out_dir / "facts.prob")
    expected = {
        'q("loop","loop")': 0.9,
        'q("other","other")': 0.7,
    }
    if set(probs.keys()) != set(expected.keys()):
        raise CaseFailure(
            "problog_query_named_variable_equality: repeated-variable query matched the wrong tuples.\n"
            f"keys={sorted(probs.keys())}"
        )
    for key, value in expected.items():
        if not math.isclose(probs[key], value, rel_tol=0.0, abs_tol=1e-12):
            raise CaseFailure(
                "problog_query_named_variable_equality: unexpected probability.\n"
                f"tuple={key} value={probs[key]}"
            )


def case_problog_constraint_variable_equality_chain(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_constraint_variable_equality_chain", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_dir)

    keep_csv = (out_dir / "keep.csv").read_text(encoding="utf-8").strip().splitlines()
    if keep_csv != ["10"]:
        raise CaseFailure(
            "problog_constraint_variable_equality_chain: chained variable equality did not propagate bound values.\n"
            f"keep={keep_csv}"
        )

def case_problog_sum_exact_roundtrip(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("problog_sum_exact_roundtrip", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_dir = case_dir / "out_exact"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_dir,
        extra_args=["--dumpjson", "--logfile", "reglog"],
    )

    expected_key = 'total(1,5)'
    probs = parse_prob_file(out_dir / "facts.prob")
    if set(probs.keys()) != {expected_key}:
        raise CaseFailure(
            "problog_sum_exact_roundtrip: unexpected probability keys.\n"
            f"keys={sorted(probs.keys())}"
        )
    if not math.isclose(probs[expected_key], 0.3, rel_tol=0.0, abs_tol=1e-12):
        raise CaseFailure(
            "problog_sum_exact_roundtrip: unexpected aggregate probability.\n"
            f"value={probs[expected_key]}"
        )

    derivation_json = out_dir / "derivation.json"
    if not derivation_json.exists():
        raise CaseFailure(f"problog_sum_exact_roundtrip: missing {derivation_json}")
    payload = json.loads(derivation_json.read_text(encoding="utf-8"))

    fact_names = {entry["name"] for entry in payload.get("facts", [])}
    required_facts = {'base(1)', 'w(1,2)', 'w(1,3)'}
    if not required_facts.issubset(fact_names):
        raise CaseFailure(
            "problog_sum_exact_roundtrip: derivation.json lost decoded input facts.\n"
            f"missing={sorted(required_facts - fact_names)}"
        )

    total_rules = [entry for entry in payload.get("rules", []) if entry.get("head") == expected_key]
    if len(total_rules) != 1:
        raise CaseFailure(
            "problog_sum_exact_roundtrip: expected exactly one derivation for total(1,5).\n"
            f"count={len(total_rules)}"
        )

    total_bodies = {body["name"] for body in total_rules[0].get("bodies", [])}
    if 'base(1)' not in total_bodies or not any(name.startswith("__agg_sum_state(") for name in total_bodies):
        raise CaseFailure(
            "problog_sum_exact_roundtrip: expected total(1,5) to depend on base and an aggregate state.\n"
            f"bodies={sorted(total_bodies)}"
        )

    agg_rules = [
        entry
        for entry in payload.get("rules", [])
        if entry.get("head", "").startswith("__agg_sum_state(")
    ]
    if len(agg_rules) != 2:
        raise CaseFailure(
            "problog_sum_exact_roundtrip: expected two aggregate-state derivations.\n"
            f"count={len(agg_rules)}"
        )
    agg_witnesses = sorted(
        body["name"]
        for entry in agg_rules
        for body in entry.get("bodies", [])
        if body["name"].startswith('w(1,')
    )
    if agg_witnesses != ['w(1,2)', 'w(1,3)']:
        raise CaseFailure(
            "problog_sum_exact_roundtrip: aggregate witness replay mismatch.\n"
            f"witnesses={agg_witnesses}"
        )

def case_dump_outputs_contract(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("dump_outputs_contract", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_exact = case_dir / "out_exact"

    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_exact,
        extra_args=["--dumpjson", "--dumpdot", "--dumpstat", "--logfile", "reglog"],
    )

    expected_dot_before = out_exact / "before_prune.dot"
    expected_dot_after = out_exact / "after_prune.dot"
    expected_prob = out_exact / "facts.prob"
    assert_valid_dot(expected_dot_before, label="dump contract before_prune")
    assert_valid_dot(expected_dot_after, label="dump contract after_prune")
    if not parse_prob_file(expected_prob):
        raise CaseFailure(f"dump contract: empty probability output {expected_prob}")

    derivation_json = single_json_file(out_exact, "derivation*.json", label="dump contract derivation JSON")
    assert_valid_derivation_json(derivation_json, label="dump contract derivation JSON")
    log_json = single_json_file(out_exact, "reglog_*.json", label="dump contract debugger logs")
    log_payload = assert_valid_debug_log(log_json)
    for stage_name in ("SEMINAIVE", "CREATE_GRAPH", "PRUNING", "FORWARD_COMPILATION", "IO_DUMP"):
        find_stage(log_payload, stage_name, label="dump contract debugger logs")

    stats_jsons = sorted(out_exact.glob("graph-*.json"))
    if not stats_jsons:
        raise CaseFailure("dump contract graph stats: expected graph-*.json from --dumpstat")
    for stats_json in stats_jsons:
        assert_valid_graph_stats_json(stats_json, label=f"dump contract graph stats {stats_json.name}")


def case_lifted_partial_outputs(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("lifted_partial_outputs", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_base = case_dir / "out_base"
    out_partial = case_dir / "out_partial"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
        extra_args=["--rewrite", "--det-opt"],
    )
    partial_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_partial,
        extra_args=[
            "--rewrite",
            "--det-opt",
            "--lifted-wmc",
            "--lifted-threshold=1",
            "--dumpdot",
            "--logfile",
            "partial",
        ],
    )

    assert_prob_close(
        out_partial / "facts.prob",
        out_base / "facts.prob",
        label="lifted_partial_outputs",
    )
    expected_log_parts = [
        "[lifted-wmc] handled=1",
        "reason=partial_pointwise_lifted",
        "lifted_output_tuples=2",
        "concrete_output_tuples=2",
        "handled_outputs=1",
        "rejected_outputs=1",
    ]
    for part in expected_log_parts:
        if part not in partial_run.stdout:
            raise CaseFailure(
                f"lifted_partial_outputs: missing runtime diagnostic {part!r}\n"
                f"stdout:\n{partial_run.stdout}"
            )

    lifted_pos = partial_run.stdout.find("[lifted-wmc] handled=1")
    rewrite_pos = partial_run.stdout.find("[pipeline] rewrite dispatch")
    if lifted_pos < 0 or rewrite_pos < 0 or lifted_pos >= rewrite_pos:
        raise CaseFailure(
            "lifted_partial_outputs: expected lifted evaluation before "
            f"residual rewrite\nstdout:\n{partial_run.stdout}"
        )

    abstract_dot = out_partial / "abstract-derivation-graph.dot"
    assert_valid_dot(abstract_dot, label="lifted partial abstract graph")
    abstract_text = abstract_dot.read_text(encoding="utf-8")
    if "good($0)" not in abstract_text or "a($0)" not in abstract_text:
        raise CaseFailure(
            "lifted_partial_outputs: abstract graph is missing the handled good/a region"
        )
    if "bad(" in abstract_text or "edge(" in abstract_text:
        raise CaseFailure(
            "lifted_partial_outputs: abstract graph contains the concrete bad/edge region"
        )

    for dot_name in ("before_prune.dot", "after_prune.dot"):
        concrete_dot = out_partial / dot_name
        assert_valid_dot(concrete_dot, label=f"lifted partial concrete graph {dot_name}")
        concrete_text = concrete_dot.read_text(encoding="utf-8")
        if "bad(" not in concrete_text or "edge(" not in concrete_text:
            raise CaseFailure(
                "lifted_partial_outputs: concrete graph is missing the unsupported "
                f"bad/edge region in {dot_name}"
            )
        if "good(" in concrete_text or "a(" in concrete_text:
            raise CaseFailure(
                "lifted_partial_outputs: handled good/a region was instantiated in "
                f"the concrete graph {dot_name}"
            )


def case_lifted_unique_witness(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("lifted_unique_witness", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_base = case_dir / "out_base"
    out_lifted = case_dir / "out_lifted"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
        extra_args=["--rewrite", "--det-opt"],
    )
    lifted_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_lifted,
        extra_args=[
            "--rewrite",
            "--det-opt",
            "--lifted-wmc",
            "--lifted-threshold=1",
            "--dumpdot",
            "--logfile",
            "lifted",
        ],
    )

    assert_prob_close(
        out_lifted / "facts.prob",
        out_base / "facts.prob",
        label="lifted_unique_witness",
    )
    expected_log_parts = [
        "[lifted-wmc] handled=1",
        "reason=pointwise_lifted",
        "output_tuples=2",
        "lifted_output_tuples=2",
        "concrete_output_tuples=0",
        "handled_outputs=1",
        "rejected_outputs=0",
    ]
    for part in expected_log_parts:
        if part not in lifted_run.stdout:
            raise CaseFailure(
                f"lifted_unique_witness: missing runtime diagnostic {part!r}\n"
                f"stdout:\n{lifted_run.stdout}"
            )

    if "[pipeline] rewrite dispatch" in lifted_run.stdout:
        raise CaseFailure(
            "lifted_unique_witness: complete lifted evaluation should return "
            "before rewrite because no residual graph remains"
        )

    abstract_dot = out_lifted / "abstract-derivation-graph.dot"
    assert_valid_dot(abstract_dot, label="lifted unique-witness abstract graph")
    abstract_text = abstract_dot.read_text(encoding="utf-8")
    for expected in ("out($0)", "seed($0)", "map($0,$e0v1)"):
        if expected not in abstract_text:
            raise CaseFailure(
                "lifted_unique_witness: abstract graph is missing "
                f"{expected!r}\n{abstract_text}"
            )


def case_lifted_multi_witness_fallback(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("lifted_multi_witness_fallback", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_base = case_dir / "out_base"
    out_fallback = case_dir / "out_fallback"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
        extra_args=["--rewrite", "--det-opt"],
    )
    fallback_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_fallback,
        extra_args=[
            "--rewrite",
            "--det-opt",
            "--lifted-wmc",
            "--lifted-threshold=1",
            "--dumpdot",
            "--logfile",
            "fallback",
        ],
    )

    assert_prob_close(
        out_fallback / "facts.prob",
        out_base / "facts.prob",
        label="lifted_multi_witness_fallback",
    )
    expected_log_parts = [
        "[lifted-wmc] handled=0",
        "reason=multiple runtime witnesses for rule:",
        "lifted_output_tuples=0",
        "concrete_output_tuples=2",
        "handled_outputs=0",
        "rejected_outputs=1",
    ]
    for part in expected_log_parts:
        if part not in fallback_run.stdout:
            raise CaseFailure(
                f"lifted_multi_witness_fallback: missing runtime diagnostic {part!r}\n"
                f"stdout:\n{fallback_run.stdout}"
            )

    lifted_pos = fallback_run.stdout.find("[lifted-wmc] handled=0")
    rewrite_pos = fallback_run.stdout.find("[pipeline] rewrite dispatch")
    if lifted_pos < 0 or rewrite_pos < 0 or lifted_pos >= rewrite_pos:
        raise CaseFailure(
            "lifted_multi_witness_fallback: expected lifted rejection before "
            f"rewrite fallback\nstdout:\n{fallback_run.stdout}"
        )

    if (out_fallback / "abstract-derivation-graph.dot").exists():
        raise CaseFailure(
            "lifted_multi_witness_fallback: fallback unexpectedly emitted an abstract graph"
        )
    for dot_name in ("before_prune.dot", "after_prune.dot"):
        assert_valid_dot(
            out_fallback / dot_name,
            label=f"lifted multi-witness concrete graph {dot_name}",
        )


def case_lifted_multi_witness_direct(
    souffle_bin: Path, work_root: Path
) -> None:
    case_dir = prepare_case_workspace("lifted_multi_witness_direct", work_root)
    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir
    )

    out_base = case_dir / "out_base"
    out_lifted = case_dir / "out_lifted"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
    )
    lifted_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_lifted,
        extra_args=["--lifted-wmc", "--lifted-threshold=1", "--dumpdot"],
    )

    assert_prob_close(
        out_lifted / "facts.prob",
        out_base / "facts.prob",
        label="lifted_multi_witness_direct",
    )
    if "reason=multi_witness_lifted" not in lifted_run.stdout:
        raise CaseFailure(
            "lifted_multi_witness_direct: expected multi-witness lifted path\n"
            f"stdout:\n{lifted_run.stdout}"
        )
    if (
        "execution_mode=witness_indexed_template" not in lifted_run.stdout
        or "witness_templates=1" not in lifted_run.stdout
        or "witness_rows=2" not in lifted_run.stdout
        or "witness_tuple_formulas=1" not in lifted_run.stdout
        or "witness_template_dd_nodes=6" not in lifted_run.stdout
    ):
        raise CaseFailure(
            "lifted_multi_witness_direct: expected witness-indexed IR counters\n"
            f"stdout:\n{lifted_run.stdout}"
        )
    if not (out_lifted / "abstract-derivation-graph.tsv").exists():
        raise CaseFailure("lifted_multi_witness_direct: missing abstract graph TSV")
    template_path = out_lifted / "witness-indexed-templates.tsv"
    witness_path = out_lifted / "witness-indexed-witnesses.tsv"
    relation_stats_path = out_lifted / "witness-indexed-relations.tsv"
    tuple_stats_path = out_lifted / "witness-indexed-tuple-stats.tsv"
    if not template_path.exists():
        raise CaseFailure("lifted_multi_witness_direct: missing witness-indexed template TSV")
    if not witness_path.exists():
        raise CaseFailure("lifted_multi_witness_direct: missing witness-indexed witness TSV")
    if not relation_stats_path.exists():
        raise CaseFailure("lifted_multi_witness_direct: missing witness-indexed relation stats TSV")
    if not tuple_stats_path.exists():
        raise CaseFailure("lifted_multi_witness_direct: missing witness-indexed tuple stats TSV")
    template_text = template_path.read_text(encoding="utf-8")
    witness_text = witness_path.read_text(encoding="utf-8")
    relation_stats_text = relation_stats_path.read_text(encoding="utf-8")
    tuple_stats_text = tuple_stats_path.read_text(encoding="utf-8")
    if "OrOver" not in template_text or "typeFilter#rule" not in template_text:
        raise CaseFailure(
            "lifted_multi_witness_direct: witness-indexed template TSV does not describe "
            "the expected OrOver template"
        )
    if witness_text.count("typeFilter#rule") < 2:
        raise CaseFailure(
            "lifted_multi_witness_direct: witness table should contain two runtime witnesses"
        )
    if "typeFilter\t1\t1\t2\t6\t" not in relation_stats_text:
        raise CaseFailure(
            "lifted_multi_witness_direct: relation stats should report one template, "
            "one tuple formula, two witness rows, and six template DD nodes"
        )
    if 'typeFilter\ttypeFilter("a","o")\t1\t2\t' not in tuple_stats_text:
        raise CaseFailure(
            "lifted_multi_witness_direct: tuple stats should report two witnesses for typeFilter(a,o)"
        )
    assert_valid_dot(
        out_lifted / "abstract-derivation-graph.dot",
        label="lifted multi-witness dot",
    )


def case_lifted_disconnected_witness_fallback(
    souffle_bin: Path, work_root: Path
) -> None:
    case_dir = prepare_case_workspace(
        "lifted_disconnected_witness_fallback", work_root
    )
    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir
    )

    out_base = case_dir / "out_base"
    out_fallback = case_dir / "out_fallback"
    run_exact_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_base)
    fallback_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_fallback,
        extra_args=[
            "--rewrite",
            "--det-opt",
            "--lifted-wmc",
            "--lifted-threshold=1",
            "--logfile",
            "fallback",
        ],
    )

    assert_prob_close(
        out_fallback / "facts.prob",
        out_base / "facts.prob",
        label="lifted_disconnected_witness_fallback",
    )
    expected_log_parts = [
        "[lifted-wmc] handled=0",
        "reason=body has a disconnected free-variable component for rule:",
        "lifted_output_tuples=0",
        "concrete_output_tuples=2",
    ]
    for part in expected_log_parts:
        if part not in fallback_run.stdout:
            raise CaseFailure(
                "lifted_disconnected_witness_fallback: missing runtime "
                f"diagnostic {part!r}\nstdout:\n{fallback_run.stdout}"
            )


def case_lifted_boundary_shared_event(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("lifted_boundary_shared_event", work_root)
    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir
    )

    out_base = case_dir / "out_base"
    out_lifted = case_dir / "out_lifted"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
        extra_args=["--det-opt"],
    )
    lifted_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_lifted,
        extra_args=[
            "--det-opt",
            "--rewrite",
            "--lifted-wmc",
            "--lifted-threshold=1",
            "--dumpdot",
            "--logfile",
            "lifted-boundary",
        ],
    )

    assert_prob_close(
        out_lifted / "facts.prob",
        out_base / "facts.prob",
        label="lifted_boundary_shared_event",
    )
    probs = parse_prob_file(out_lifted / "facts.prob")
    expected = {
        'left("a")': 0.28,
        'right("a")': 0.24,
        'concrete("a")': 0.168,
    }
    for key, value in expected.items():
        if key not in probs or not math.isclose(
            probs[key], value, rel_tol=0.0, abs_tol=1e-8
        ):
            raise CaseFailure(
                "lifted_boundary_shared_event: shared event identity was not "
                f"preserved for {key}; expected={value} actual={probs.get(key)}"
            )

    match = re.search(r"\[lifted-boundary\].*inlined_nodes=(\d+)", lifted_run.stdout)
    if match is None or int(match.group(1)) < 2:
        raise CaseFailure(
            "lifted_boundary_shared_event: boundary pass did not inline both "
            f"helper nodes\nstdout:\n{lifted_run.stdout}"
        )
    boundary_dot = out_lifted / "lifted_boundary_before_rewrite.dot"
    assert_valid_dot(boundary_dot, label="lifted boundary final graph")
    boundary_text = boundary_dot.read_text(encoding="utf-8")
    if 'left("a")' in boundary_text or 'right("a")' in boundary_text:
        raise CaseFailure(
            "lifted_boundary_shared_event: inlined helper instances remain in "
            "the active concrete graph"
        )


def case_lifted_internal_template_ref(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("lifted_internal_template_ref", work_root)
    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    out_base = case_dir / "out_base"
    out_lifted = case_dir / "out_lifted"
    run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_base,
        extra_args=["--det-opt", "--rewrite"],
    )
    lifted_run = run_exact_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_lifted,
        extra_args=[
            "--det-opt",
            "--rewrite",
            "--lifted-wmc",
            "--lifted-threshold=1",
            "--dumpdot",
            "--logfile",
            "lifted-internal-template-ref",
        ],
    )

    assert_prob_close(
        out_lifted / "facts.prob",
        out_base / "facts.prob",
        label="lifted_internal_template_ref",
    )
    required = [
        "[lifted-wmc] handled=1",
        "handled_outputs=0",
        "witness_templates=1",
        "reason=partial_multi_witness_lifted",
        "[lifted-boundary]",
    ]
    for part in required:
        if part not in lifted_run.stdout:
            raise CaseFailure(
                f"lifted_internal_template_ref: missing runtime diagnostic {part!r}\n"
                f"stdout:\n{lifted_run.stdout}"
            )
    match = re.search(r"\[lifted-boundary\].*inlined_nodes=(\d+)", lifted_run.stdout)
    if match is None or int(match.group(1)) < 1:
        raise CaseFailure(
            "lifted_internal_template_ref: internal template relation was not "
            f"inlined into the residual graph\nstdout:\n{lifted_run.stdout}"
        )
    match = re.search(r"\[lifted-boundary\].*embedded_event_refs=(\d+)", lifted_run.stdout)
    if match is None or int(match.group(1)) < 1:
        raise CaseFailure(
            "lifted_internal_template_ref: probabilistic source rule event was "
            f"not preserved as an embedded event\nstdout:\n{lifted_run.stdout}"
        )
    relation_stats = out_lifted / "witness-indexed-relations.tsv"
    if not relation_stats.exists() or "typeFilter" not in relation_stats.read_text(encoding="utf-8"):
        raise CaseFailure(
            "lifted_internal_template_ref: missing typeFilter witness-indexed "
            "relation stats"
        )




CASES = {
    "smoke_exact_inference": case_smoke_exact_inference,
    "rewrite_dispatch_equiv": case_rewrite_dispatch_equiv,
    "problog_string_roundtrip": case_problog_string_roundtrip,
    "problog_symbol_aggregate_roundtrip": case_problog_symbol_aggregate_roundtrip,
    "problog_fact_prob_alignment": case_problog_fact_prob_alignment,
    "problog_large_numeric_tuple_roundtrip": case_problog_large_numeric_tuple_roundtrip,
    "problog_query_named_variable_equality": case_problog_query_named_variable_equality,
    "problog_constraint_variable_equality_chain": case_problog_constraint_variable_equality_chain,
    "problog_sum_exact_roundtrip": case_problog_sum_exact_roundtrip,
    "exact_det_modes": case_exact_det_modes,
    "language_side_channel_mini": case_language_side_channel_mini,
    "language_taint_mini": case_language_taint_mini,
    "language_symbolization_mini": case_language_symbolization_mini,
    "dump_outputs_contract": case_dump_outputs_contract,
    "lifted_partial_outputs": case_lifted_partial_outputs,
    "lifted_unique_witness": case_lifted_unique_witness,
    "lifted_multi_witness_fallback": case_lifted_multi_witness_fallback,
    "lifted_multi_witness_direct": case_lifted_multi_witness_direct,
    "lifted_disconnected_witness_fallback": case_lifted_disconnected_witness_fallback,
    "lifted_boundary_shared_event": case_lifted_boundary_shared_event,
    "lifted_internal_template_ref": case_lifted_internal_template_ref,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run one maintained Souffle regression case")
    parser.add_argument("--case", required=True, choices=sorted(CASES.keys()))
    parser.add_argument("--souffle-bin", required=True, help="Path to repo-built souffle binary")
    parser.add_argument("--work-root", required=True, help="Directory for per-case temporary work")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    souffle_bin = Path(args.souffle_bin).resolve()
    work_root = Path(args.work_root).resolve()
    work_root.mkdir(parents=True, exist_ok=True)

    if not souffle_bin.exists():
        print(f"error: souffle binary does not exist: {souffle_bin}", file=sys.stderr)
        return 2
    if not souffle_bin.is_file():
        print(f"error: souffle binary path is not a file: {souffle_bin}", file=sys.stderr)
        return 2

    try:
        CASES[args.case](souffle_bin, work_root)
    except CaseFailure as err:
        print(f"[regression:{args.case}] FAIL\n{err}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as err:
        print(f"[regression:{args.case}] TIMEOUT: {err}", file=sys.stderr)
        return 1

    print(f"[regression:{args.case}] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
