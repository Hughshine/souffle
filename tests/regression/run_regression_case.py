#!/usr/bin/env python3
"""
Regression runner for the maintained Souffle fork test suite.

Each ctest case executes one scenario end-to-end:
- generate a small program + inputs
- compile with the repo-built souffle binary
- run full/incremental modes
- assert semantic and artifact contracts
"""

from __future__ import annotations

import argparse
import math
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

PROB_LINE_RE = re.compile(r"^\s*(.*?)\s*:\s*([+\-]?\d+(?:\.\d+)?(?:[eE][+\-]?\d+)?)\s*$")
GRAPH_QUERY_RESULT_RE = re.compile(
    r"^\[result\]\s+(.*?)\s+=\s+([+\-]?\d+(?:\.\d+)?(?:[eE][+\-]?\d+)?)\s*$"
)
REWRITE_ITER_DETECT_RE = re.compile(
    r"^\[GraphRewriter\] Iteration (\d+) : detected (\d+) SISO region\(s\)\."
)
REWRITE_SISO_MODE_RE = re.compile(
    r"^\[siso-detect\] mode=(\S+) fallback=(\d+) "
    r"seeds\(nodes=(\d+),edges=(\d+)\) frontier\(nodes=(\d+),edges=(\d+)\)$"
)
CASES_ROOT = Path(__file__).resolve().parent / "cases"
REPO_ROOT = Path(__file__).resolve().parents[2]


def resolve_benchmark_root() -> Path:
    candidates = [
        REPO_ROOT / "work" / "benchmarks" / "problog-benchmark",
        REPO_ROOT / "problog-benchmark",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


BENCHMARK_ROOT = resolve_benchmark_root()
SIDE_CHANNEL_FULL_SCRIPT = BENCHMARK_ROOT / "benchmarks" / "side_channel" / "cli" / "side_channel_full.py"
SIDE_CHANNEL_INC_SCRIPT = BENCHMARK_ROOT / "benchmarks" / "side_channel" / "cli" / "side_channel_inc.py"
TAINT_INC_SCRIPT = BENCHMARK_ROOT / "taint_inc.py"
TAINT_BUNDLE = "v2_semantic_subsetprob_noderv0"
TAINT_CASE = "andors-trail"
PIPELINE_CARRY_EXTS = {".facts", ".csv", ".tsv", ".prob"}
PIPELINE_SKIP_OUTPUT_NAMES = {
    "facts.prob",
    "det-relations.txt",
    "det-scc.txt",
    "initial-input-relations-iter0.txt",
}


class CaseFailure(RuntimeError):
    """Raised when a regression case fails."""


def format_cmd(cmd: Sequence[str]) -> str:
    return " ".join(cmd)


def run_cmd(
    cmd: Sequence[str],
    cwd: Path,
    *,
    stdin_text: str | None = None,
    timeout: int = 240,
    env: Dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    proc_env = os.environ.copy()
    if env:
        proc_env.update(env)
    proc = subprocess.run(
        list(cmd),
        cwd=str(cwd),
        input=stdin_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=proc_env,
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


def run_cmd_expect_fail(
    cmd: Sequence[str],
    cwd: Path,
    *,
    expected_substring: str,
    stdin_text: str | None = None,
    timeout: int = 240,
    env: Dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    proc_env = os.environ.copy()
    if env:
        proc_env.update(env)
    proc = subprocess.run(
        list(cmd),
        cwd=str(cwd),
        input=stdin_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=proc_env,
        timeout=timeout,
        check=False,
    )
    if proc.returncode == 0:
        raise CaseFailure(
            f"command unexpectedly succeeded\ncwd: {cwd}\ncmd: {format_cmd(cmd)}\nstdout:\n{proc.stdout}"
        )
    combined = proc.stdout + proc.stderr
    if expected_substring not in combined:
        raise CaseFailure(
            f"command failed without expected message\ncwd: {cwd}\ncmd: {format_cmd(cmd)}\n"
            f"expected substring: {expected_substring}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
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
        if entry.name == "generate.py":
            continue
        dst = case_dir / entry.name
        if entry.is_dir():
            shutil.copytree(entry, dst)
        elif entry.is_file():
            shutil.copy2(entry, dst)

    generator = case_src / "generate.py"
    if generator.exists():
        run_cmd(
            [sys.executable, str(generator), "--out-dir", str(case_dir)],
            cwd=case_src,
            timeout=180,
        )

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


def normalize_tuple_key(value: str) -> str:
    return re.sub(r"\s+", "", value)


def parse_graph_query_results(stdout: str) -> Dict[str, float]:
    results: Dict[str, float] = {}
    for raw in stdout.splitlines():
        line = raw.strip()
        m = GRAPH_QUERY_RESULT_RE.match(line)
        if not m:
            continue
        results[normalize_tuple_key(m.group(1))] = float(m.group(2))
    return results


def assert_prob_close(lhs: Path, rhs: Path, *, tol: float = 1e-9, label: str) -> None:
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


def normalize_table_lines(path: Path) -> List[str]:
    return sorted(line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip())


def assert_csv_outputs_match(lhs_dir: Path, rhs_dir: Path, *, label: str) -> None:
    lhs_files = sorted(path.relative_to(lhs_dir) for path in lhs_dir.rglob("*.csv"))
    rhs_files = sorted(path.relative_to(rhs_dir) for path in rhs_dir.rglob("*.csv"))
    if not lhs_files:
        raise CaseFailure(f"{label}: expected at least one .csv output under {lhs_dir}")
    if lhs_files != rhs_files:
        raise CaseFailure(
            f"{label}: csv file set mismatch\nlhs_dir={lhs_dir}\nrhs_dir={rhs_dir}\n"
            f"lhs_only={sorted(str(p) for p in set(lhs_files) - set(rhs_files))[:8]}\n"
            f"rhs_only={sorted(str(p) for p in set(rhs_files) - set(lhs_files))[:8]}"
        )

    for rel_path in lhs_files:
        lhs_path = lhs_dir / rel_path
        rhs_path = rhs_dir / rel_path
        lhs_lines = normalize_table_lines(lhs_path)
        rhs_lines = normalize_table_lines(rhs_path)
        if lhs_lines != rhs_lines:
            raise CaseFailure(
                f"{label}: csv content mismatch\nlhs={lhs_path}\nrhs={rhs_path}\n"
                f"lhs_sample={lhs_lines[:8]}\nrhs_sample={rhs_lines[:8]}"
            )


def assert_prob_all_ones(path: Path, *, tol: float = 1e-12, label: str) -> None:
    vals = parse_prob_file(path)
    bad = []
    for key, value in sorted(vals.items()):
        if not math.isclose(value, 1.0, rel_tol=0.0, abs_tol=tol):
            bad.append(f"{key}: {value:.12g}")
            if len(bad) >= 8:
                break
    if bad:
        raise CaseFailure(
            f"{label}: det-force output is not all-ones\nfile={path}\n" + "\n".join(bad)
        )


def iter_prob_path(output_dir: Path, iteration: int, suffix: str) -> Path:
    return output_dir / f"fact-iter{iteration}-{suffix}.prob"


def parse_rewrite_detect_counts(stdout: str) -> List[Tuple[int, int]]:
    counts: List[Tuple[int, int]] = []
    for raw in stdout.splitlines():
        line = raw.strip()
        m = REWRITE_ITER_DETECT_RE.match(line)
        if not m:
            continue
        counts.append((int(m.group(1)), int(m.group(2))))
    return counts


def parse_siso_detect_modes(stdout: str) -> List[Tuple[str, int, int, int, int, int]]:
    rows: List[Tuple[str, int, int, int, int, int]] = []
    for raw in stdout.splitlines():
        line = raw.strip()
        m = REWRITE_SISO_MODE_RE.match(line)
        if not m:
            continue
        rows.append(
            (
                m.group(1),
                int(m.group(2)),
                int(m.group(3)),
                int(m.group(4)),
                int(m.group(5)),
                int(m.group(6)),
            )
        )
    return rows


def build_layered_edge_graph(*, num_layers: int, width: int) -> Tuple[List[Tuple[int, int]], List[float]]:
    layers: List[List[int]] = []
    next_id = 1
    for _ in range(num_layers):
        layer = list(range(next_id, next_id + width))
        layers.append(layer)
        next_id += width

    edges: List[Tuple[int, int]] = []
    probs: List[float] = []
    for i in range(len(layers) - 1):
        nxt = layers[i + 1]
        for idx, src in enumerate(layers[i]):
            fan_out = 3 + ((src + idx) % 2)  # 3 or 4
            for dst in nxt[:fan_out]:
                edges.append((src, dst))
                probs.append(0.55 + ((src + dst) % 7) * 0.05)

    return edges, probs


def overwrite_edge_inputs(
    input_dir: Path, *, edges: Sequence[Tuple[int, int]], probs: Sequence[float]
) -> None:
    if len(edges) != len(probs):
        raise CaseFailure(
            f"overwrite_edge_inputs: edge/prob size mismatch ({len(edges)} vs {len(probs)})"
        )

    facts_text = "".join(f"{src}\t{dst}\n" for src, dst in edges)
    prob_text = "".join(f"{prob:.2f}\n" for prob in probs)
    write_text(input_dir / "edge.facts", facts_text)
    write_text(input_dir / "edge.prob", prob_text)


def truncate_facts_prob_pairs(input_dir: Path, *, max_rows: int) -> None:
    if max_rows <= 0:
        return
    for facts_path in sorted(input_dir.glob("*.facts")):
        fact_lines = facts_path.read_text(encoding="utf-8").splitlines()
        fact_lines = fact_lines[:max_rows]
        write_text(facts_path, ("\n".join(fact_lines) + ("\n" if fact_lines else "")))

        prob_path = facts_path.with_suffix(".prob")
        if not prob_path.exists():
            continue
        prob_lines = prob_path.read_text(encoding="utf-8").splitlines()
        prob_lines = prob_lines[: len(fact_lines)]
        write_text(prob_path, ("\n".join(prob_lines) + ("\n" if prob_lines else "")))


def compile_program(
    *,
    souffle_bin: Path,
    source_path: Path,
    input_dir: Path,
    output_dir: Path,
    build_dir: Path,
    binary_name: str,
    online: bool = False,
    full_only: bool = False,
    compile_args: Sequence[str] | None = None,
    timeout: int = 300,
) -> Path:
    build_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)
    binary_path = build_dir / binary_name

    cmd = [str(souffle_bin)]
    if online:
        cmd.append("--online")
    if full_only:
        cmd.append("--full-only")
    if compile_args:
        cmd.extend(compile_args)
    cmd.extend(
        [
            "-F",
            str(input_dir),
            "-D",
            str(output_dir),
            str(source_path),
            "-o",
            str(binary_path),
        ]
    )
    run_cmd(cmd, cwd=source_path.parent, timeout=timeout)
    if not binary_path.exists():
        raise CaseFailure(f"compile did not create binary: {binary_path}")
    return binary_path


def run_binary(
    *,
    binary_path: Path,
    input_dir: Path,
    output_dir: Path,
    extra_args: Sequence[str] | None = None,
    stdin_text: str | None = None,
    timeout: int = 240,
    env: Dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(binary_path), "-F", str(input_dir), "-D", str(output_dir)]
    if extra_args:
        cmd.extend(extra_args)
    return run_cmd(cmd, cwd=binary_path.parent, stdin_text=stdin_text, timeout=timeout, env=env)


def first_commit_script(cli_script: str) -> str:
    lines: List[str] = []
    for raw in cli_script.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line == "q":
            break
        lines.append(line)
        if line == "commit":
            break
    lines.append("q")
    return "\n".join(lines) + "\n"


def count_cli_commits(cli_script: str) -> int:
    return sum(1 for raw in cli_script.splitlines() if raw.strip() == "commit")


def read_stage_list(path: Path) -> List[str]:
    stages: List[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        stages.append(line)
    return stages


def ensure_prob_files(input_dir: Path) -> None:
    for facts_path in sorted(input_dir.glob("*.facts")):
        prob_path = facts_path.with_suffix(".prob")
        if prob_path.exists():
            continue
        count = sum(1 for _ in facts_path.open("r", encoding="utf-8"))
        write_text(prob_path, ("1.0\n" * count) if count > 0 else "")


def copy_pipeline_stage_input(
    *,
    base_input_dir: Path,
    previous_output_dirs: Sequence[Path],
    stage_input_dir: Path,
    max_rows_per_fact: int | None = None,
) -> None:
    reset_dir(stage_input_dir)
    for src in sorted(base_input_dir.glob("*")):
        if src.is_file():
            shutil.copy2(src, stage_input_dir / src.name)
    if max_rows_per_fact is not None:
        truncate_facts_prob_pairs(stage_input_dir, max_rows=max_rows_per_fact)

    for prev_output in previous_output_dirs:
        if not prev_output.exists():
            continue
        for src in sorted(prev_output.glob("*")):
            if not src.is_file():
                continue
            if src.name in PIPELINE_SKIP_OUTPUT_NAMES:
                continue
            if src.name.startswith("log.txt_") and src.suffix == ".json":
                continue
            if src.suffix.lower() not in PIPELINE_CARRY_EXTS:
                continue
            if src.suffix.lower() in {".facts", ".csv", ".tsv"}:
                dst = stage_input_dir / f"{src.stem}.facts"
            else:
                dst = stage_input_dir / src.name
            shutil.copy2(src, dst)
    ensure_prob_files(stage_input_dir)


def assert_output_dir_has_pipeline_artifacts(output_dir: Path, *, label: str) -> None:
    produced = [p for p in sorted(output_dir.glob("*")) if p.is_file()]
    if not produced:
        raise CaseFailure(f"{label}: no stage output files produced in {output_dir}")
    if not any(p.suffix.lower() in PIPELINE_CARRY_EXTS or p.name == "facts.prob" for p in produced):
        names = [p.name for p in produced[:8]]
        raise CaseFailure(f"{label}: missing carry/output artifacts in {output_dir}; saw {names}")


def compile_compute(
    *,
    souffle_bin: Path,
    case_dir: Path,
    full_only: bool = False,
    compile_args: Sequence[str] | None = None,
) -> Tuple[Path, Path, Path]:
    input_dir = case_dir / "input"
    output_dir = case_dir / "output_compile"
    build_dir = case_dir / "build"
    build_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    compute_dl = case_dir / "compute.dl"
    compute_bin = build_dir / "compute"

    cmd = [str(souffle_bin), "--online"]
    if full_only:
        cmd.append("--full-only")
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


def make_cli_script(turns: Sequence[Sequence[str]]) -> str:
    lines: List[str] = []
    for turn_ops in turns:
        lines.extend(turn_ops)
        lines.append("commit")
    lines.append("q")
    return "\n".join(lines) + "\n"


def run_cli_mode(
    *,
    compute_bin: Path,
    input_dir: Path,
    output_dir: Path,
    mode: str,
    turns: Sequence[Sequence[str]],
    extra_args: Sequence[str] | None = None,
    timeout: int = 240,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        str(compute_bin),
        "-F",
        str(input_dir),
        "-D",
        str(output_dir),
        "--setmode",
        mode,
    ]
    if extra_args:
        cmd.extend(extra_args)
    cli_script = make_cli_script(turns)
    write_text(output_dir / "commands.txt", cli_script)
    run_cmd(cmd, cwd=compute_bin.parent, stdin_text=cli_script, timeout=timeout)


def run_full_once(
    *,
    compute_bin: Path,
    input_dir: Path,
    output_dir: Path,
    extra_args: Sequence[str] | None = None,
    env: Dict[str, str] | None = None,
    timeout: int = 180,
) -> subprocess.CompletedProcess[str]:
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(compute_bin), "-F", str(input_dir), "-D", str(output_dir)]
    if extra_args:
        cmd.extend(extra_args)
    return run_cmd(cmd, cwd=compute_bin.parent, timeout=timeout, env=env)


def assert_glob_nonempty(base_dir: Path, pattern: str, *, label: str) -> None:
    matches = sorted(base_dir.glob(pattern))
    if not matches:
        raise CaseFailure(f"{label}: expected files matching {base_dir / pattern}")


def find_single_glob(base_dir: Path, pattern: str, *, label: str) -> Path:
    matches = sorted(base_dir.glob(pattern))
    if not matches:
        raise CaseFailure(f"{label}: expected files matching {base_dir / pattern}")
    if len(matches) != 1:
        raise CaseFailure(f"{label}: expected exactly one match for {base_dir / pattern}, got {len(matches)}")
    return matches[0]


def assert_stdout_contains(stdout: str, needle: str, *, label: str) -> None:
    if needle not in stdout:
        raise CaseFailure(f"{label}: missing stdout substring\nexpected={needle}\nstdout:\n{stdout}")


def case_smoke_full_only(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("smoke_full_only", work_root)
    input_dir = case_dir / "input"
    output_dir = case_dir / "output_run"
    output_dir.mkdir(parents=True, exist_ok=True)

    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )
    run_full_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=output_dir)

    facts_prob = output_dir / "facts.prob"
    vals = parse_prob_file(facts_prob)
    if not vals:
        raise CaseFailure(f"smoke test produced empty probability output: {facts_prob}")


def case_dred_mix_naive_vs_full(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("dred_mix_naive_vs_full", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    turns = [
        [],
        ["insert 0.35::bridge(1,3)", "delete edge(2,4)"],
        ["insert 0.41::edge(3,6)"],
    ]

    out_inc = case_dir / "out_inc_naive"
    out_full = case_dir / "out_full_hard"
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_inc,
        mode="inc-naive",
        turns=turns,
    )
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full,
        mode="full-hard",
        turns=turns,
    )

    for iteration in (1, 2, 3):
        assert_prob_close(
            iter_prob_path(out_inc, iteration, "inc-naive"),
            iter_prob_path(out_full, iteration, "full"),
            label=f"dred_mix iter={iteration}",
        )


def case_dred_hub_rederive_naive_vs_full(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("dred_hub_rederive_naive_vs_full", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)

    delete_ops = [f"delete edge({i},40)" for i in range(1, 7)]
    turns = [
        [],
        delete_ops + ["delete edge(6,7)"],
        ["insert 0.73::edge(6,7)", "insert 0.67::edge(17,40)", "insert 0.55::edge(12,30)"],
    ]

    out_inc = case_dir / "out_inc_naive"
    out_full = case_dir / "out_full_hard"
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_inc,
        mode="inc-naive",
        turns=turns,
        timeout=300,
    )
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full,
        mode="full-hard",
        turns=turns,
        timeout=300,
    )

    for iteration in (1, 2, 3):
        assert_prob_close(
            iter_prob_path(out_inc, iteration, "inc-naive"),
            iter_prob_path(out_full, iteration, "full"),
            label=f"dred_hub_rederive iter={iteration}",
        )


def case_detopt_inc_naive_combo_vs_full(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("detopt_inc_naive_combo_vs_full", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    extra = ["--det-opt", "--post-del", "--no-reuse-var-index", "--no-single-rand-fast"]
    turns = [
        [],
        ["delete trust(1,2)", "delete trust(2,3)"],
        ["insert trust(1,2)", "insert trust(2,3)", "delete chance(1,4)"],
    ]

    out_inc = case_dir / "out_inc_naive"
    out_full = case_dir / "out_full_hard"
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_inc,
        mode="inc-naive",
        turns=turns,
        extra_args=extra,
    )
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full,
        mode="full-hard",
        turns=turns,
        extra_args=extra,
    )

    for iteration in (1, 2, 3):
        assert_prob_close(
            iter_prob_path(out_inc, iteration, "inc-naive"),
            iter_prob_path(out_full, iteration, "full"),
            label=f"detopt_combo iter={iteration}",
        )


def case_detopt_inc_regional_single_round_vs_full(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("detopt_inc_regional_single_round_vs_full", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    turns = [
        [],
        ["delete trust(1,5)", "insert 0.52::chance(1,5)"],
    ]
    extra = ["--det-opt"]

    out_regional = case_dir / "out_inc_regional"
    out_full = case_dir / "out_full_hard"
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_regional,
        mode="inc-regional",
        turns=turns,
        extra_args=extra,
    )
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full,
        mode="full-hard",
        turns=turns,
        extra_args=extra,
    )

    # NOTE: inc-regional is currently validated in single-round form only.
    for iteration in (1, 2):
        assert_prob_close(
            iter_prob_path(out_regional, iteration, "inc-regional"),
            iter_prob_path(out_full, iteration, "full"),
            label=f"detopt_inc_regional iter={iteration}",
        )


def case_detopt_recursive_derivation_guard_vs_full(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("detopt_recursive_derivation_guard_vs_full", work_root)

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    # Two distinct recursive supports derive reach(1,4) initially:
    #   1->2->4 and 1->3->4.
    # This exercises derivation-level delete/rederive behavior under det-opt.
    turns = [
        [],
        ["delete edge(1,2)"],
        ["delete edge(1,3)"],
    ]
    extra = ["--det-opt"]

    out_inc = case_dir / "out_inc_naive"
    out_full = case_dir / "out_full_hard"
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_inc,
        mode="inc-naive",
        turns=turns,
        extra_args=extra,
    )
    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full,
        mode="full-hard",
        turns=turns,
        extra_args=extra,
    )

    for iteration in (1, 2, 3):
        assert_prob_close(
            iter_prob_path(out_inc, iteration, "inc-naive"),
            iter_prob_path(out_full, iteration, "full"),
            label=f"detopt_recursive_derivation_guard iter={iteration}",
        )


def case_rewrite_split_modes_equiv(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("rewrite_split_modes_equiv", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )

    out_base = case_dir / "out_base"
    run_full_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_base)
    base_prob = out_base / "facts.prob"

    split_modes = ["no-split", "naive-split", "complete-split"]
    for split_mode in split_modes:
        out_dir = case_dir / f"out_rewrite_{split_mode}"
        run_full_once(
            compute_bin=compute_bin,
            input_dir=in_dir,
            output_dir=out_dir,
            extra_args=["--rewrite", f"--split-mode={split_mode}"],
        )
        assert_prob_close(
            out_dir / "facts.prob",
            base_prob,
            label=f"rewrite_split_mode={split_mode}",
        )


def case_rewrite_dirty_detect_equiv(souffle_bin: Path, work_root: Path) -> None:
    # Keep fixture source shared, but isolate workspace path so this case can run
    # in parallel with rewrite_split_modes_equiv without rmtree/copy races.
    case_dir = prepare_case_workspace(
        "rewrite_split_modes_equiv", work_root / "rewrite_dirty_detect_equiv_ws"
    )
    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )
    args = ["--rewrite", "--dumpstat", "--split-mode=naive-split"]

    out_dirty = case_dir / "out_rewrite_dirty_detect"
    dirty_run = run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_dirty,
        extra_args=args,
    )

    out_full_detect = case_dir / "out_rewrite_force_full_detect"
    full_run = run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full_detect,
        extra_args=[*args, "--force-complete-siso-detect"],
    )

    dirty_counts = parse_rewrite_detect_counts(dirty_run.stdout)
    full_counts = parse_rewrite_detect_counts(full_run.stdout)
    if not dirty_counts or not full_counts:
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: missing rewrite detect logs.\n"
            "Ensure --dumpstat output includes per-iteration detection lines."
        )
    if dirty_counts != full_counts:
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: per-iteration SISO detection counts differ.\n"
            f"dirty={dirty_counts}\nfull={full_counts}"
        )

    assert_prob_close(
        out_dirty / "facts.prob",
        out_full_detect / "facts.prob",
        label="rewrite_dirty_detect_equiv facts.prob",
    )

    # Mechanism-focused scenario:
    # a layered graph where dirty-frontier detection should be exercised at least once.
    # This validates the mechanism itself (enabled path + forced full suppression path),
    # independent from the small semantic-equivalence fixture above.
    layered_edges, layered_probs = build_layered_edge_graph(num_layers=10, width=8)
    overwrite_edge_inputs(in_dir, edges=layered_edges, probs=layered_probs)

    out_mech_dirty = case_dir / "out_mech_dirty_detect"
    mech_dirty_run = run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_mech_dirty,
        extra_args=args,
    )
    out_mech_full = case_dir / "out_mech_force_full_detect"
    mech_full_run = run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_mech_full,
        extra_args=[*args, "--force-complete-siso-detect"],
    )

    dirty_modes = parse_siso_detect_modes(mech_dirty_run.stdout)
    full_modes = parse_siso_detect_modes(mech_full_run.stdout)
    if not dirty_modes or not full_modes:
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: missing [siso-detect] mode logs in mechanism scenario.\n"
            "Ensure --dumpstat output includes mode/fallback/seeds/frontier lines."
        )

    if not any(row[0] == "dirty-frontier" for row in dirty_modes):
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: mechanism scenario did not exercise dirty-frontier detection.\n"
            f"modes={dirty_modes}"
        )
    if any(row[0] == "dirty-frontier" for row in full_modes):
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: force-complete detect run unexpectedly used dirty-frontier.\n"
            f"modes={full_modes}"
        )
    if not any((row[2] > 0 or row[3] > 0) for row in dirty_modes):
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: dirty-detect run never observed non-zero dirty seeds.\n"
            f"modes={dirty_modes}"
        )
    if any(
        row[1] != 0 or row[2] != 0 or row[3] != 0 or row[4] != 0 or row[5] != 0
        for row in full_modes
    ):
        raise CaseFailure(
            "rewrite_dirty_detect_equiv: force-complete detect run recorded dirty/fallback/frontier state.\n"
            f"modes={full_modes}"
        )


def case_full_det_modes(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("full_det_modes", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )

    out_base = case_dir / "out_base"
    out_detopt = case_dir / "out_detopt"
    out_detforce = case_dir / "out_detforce"

    run_full_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=out_base)
    run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_detopt,
        extra_args=["--det-opt"],
    )
    run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_detforce,
        extra_args=["--det-force"],
    )

    base_prob = out_base / "facts.prob"
    detopt_prob = out_detopt / "facts.prob"
    detforce_prob = out_detforce / "facts.prob"
    assert_prob_close(detopt_prob, base_prob, label="det-opt_full_mode_equivalence")
    assert_prob_all_ones(detforce_prob, label="det-force_all_ones")

    base_keys = set(parse_prob_file(base_prob).keys())
    force_keys = set(parse_prob_file(detforce_prob).keys())
    if base_keys != force_keys:
        raise CaseFailure("det-force changed output tuple set compared to baseline")


def case_dump_outputs_contract(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("dump_outputs_contract", work_root)
    input_dir = case_dir / "input"

    compute_bin, in_dir, _ = compile_compute(souffle_bin=souffle_bin, case_dir=case_dir)
    out_full = case_dir / "out_full_hard"

    run_cli_mode(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_full,
        mode="full-hard",
        turns=[[]],
        extra_args=["--dumpjson", "--dumpjson-before-prune", "--dumpdot", "--dumpstat", "--logfile", "reglog"],
    )

    expected_dot_before = out_full / "derivation-full-before-prune1.dot"
    expected_dot_after = out_full / "derivation-full-after-prune1.dot"
    expected_prob = out_full / "fact-iter1-full.prob"
    for expected in (expected_dot_before, expected_dot_after, expected_prob):
        if not expected.exists():
            raise CaseFailure(f"dump contract: missing expected artifact {expected}")

    assert_glob_nonempty(
        out_full,
        "derivation-full-before-prune1-*.json",
        label="dump contract json before prune",
    )
    assert_glob_nonempty(
        out_full,
        "derivation-full-after-prune1-*.json",
        label="dump contract json after prune",
    )
    assert_glob_nonempty(out_full, "reglog_*.json", label="dump contract debugger logs")
    assert_glob_nonempty(out_full, "graph-*.json", label="dump contract graph stats")


def case_full_const_negation_grounding(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("full_const_negation_grounding", work_root)

    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )
    out_dir = case_dir / "out_derv_only"
    run_full_once(
        compute_bin=compute_bin,
        input_dir=in_dir,
        output_dir=out_dir,
        extra_args=["--dumpjson", "--dumpjson-before-prune", "--derv-only", "--no-det-opt"],
    )

    before_prune_json = out_dir / "derivation-before-prune.json"
    if not before_prune_json.exists():
        raise CaseFailure(
            f"const grounding: missing derivation-before-prune.json at {before_prune_json}"
        )
    derivation_json = out_dir / "derivation.json"
    if not derivation_json.exists():
        raise CaseFailure(f"const grounding: missing derivation.json at {derivation_json}")

    result_csv = out_dir / "res.csv"
    if normalize_table_lines(result_csv) != ["1", "2"]:
        raise CaseFailure(
            "const grounding: unexpected res.csv contents\n"
            f"path={result_csv}\n"
            f"lines={normalize_table_lines(result_csv)}"
        )


def case_canonical_compile_defaults_contract(souffle_bin: Path, work_root: Path) -> None:
    baseline_dir = prepare_case_workspace("smoke_full_only", work_root / "canonical_compile_base")
    canonical_dir = prepare_case_workspace("smoke_full_only", work_root / "canonical_compile_canon")

    base_bin, base_in, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=baseline_dir, full_only=True
    )
    canon_bin, canon_in, _ = compile_compute(
        souffle_bin=souffle_bin,
        case_dir=canonical_dir,
        full_only=True,
        compile_args=[
            "--sem-mode=full",
            "--fc-mode=full-soft",
            "--rewrite-engine=off",
            "--rewrite-split=naive",
            "--rewrite-detect=dirty-frontier",
            "--det-mode=auto",
            "--dd-backend=bdd",
            "--dump=json,stat",
            "--profile-stage=wmc",
        ],
    )

    out_base = baseline_dir / "out_base"
    out_canon = canonical_dir / "out_canon"
    run_full_once(compute_bin=base_bin, input_dir=base_in, output_dir=out_base)
    run_full_once(compute_bin=canon_bin, input_dir=canon_in, output_dir=out_canon)

    assert_prob_close(
        out_canon / "facts.prob",
        out_base / "facts.prob",
        label="canonical compile defaults facts.prob",
    )
    derivation_json = out_canon / "derivation.json"
    if not derivation_json.exists():
        raise CaseFailure(f"canonical compile defaults: missing expected artifact {derivation_json}")
    assert_glob_nonempty(out_canon, "graph-*.json", label="canonical compile defaults graph stats")


def case_canonical_online_cli_surface(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("smoke_full_only", work_root / "canonical_online_cli_surface_ws")
    compute_bin, input_dir, _ = compile_compute(
        souffle_bin=souffle_bin,
        case_dir=case_dir,
        compile_args=[
            "--sem-mode=inc",
            "--fc-mode=inc-regional",
            "--det-mode=auto",
            "--dd-backend=bdd",
            "--dump=json",
            "--profile-stage=inc,wmc",
        ],
    )

    output_dir = case_dir / "out_canonical_cli"
    output_dir.mkdir(parents=True, exist_ok=True)
    cli_script = "\n".join(
        [
            "show config",
            "set dump stat",
            "set profile-stage fc",
            "unset profile-stage inc",
            "set rewrite-engine legacy",
            "set sem-mode full",
            "set fc-mode full-hard",
            "show config",
            "commit",
            "q",
        ]
    ) + "\n"
    proc = run_cmd(
        [str(compute_bin), "-F", str(input_dir), "-D", str(output_dir)],
        cwd=compute_bin.parent,
        stdin_text=cli_script,
        timeout=240,
    )

    assert_stdout_contains(
        proc.stdout,
        "sem-mode=inc fc-mode=inc-regional full-evaluator=exact rewrite-engine=off "
        "rewrite-split=naive rewrite-detect=dirty-frontier det-mode=auto dd-backend=bdd "
        "dumps=json profile-stages=inc,wmc",
        label="canonical online cli initial config",
    )
    assert_stdout_contains(
        proc.stdout,
        "rewrite-engine is fixed at startup and cannot be changed in the online CLI",
        label="canonical online cli startup-only guard",
    )
    assert_stdout_contains(
        proc.stdout,
        "sem-mode=full fc-mode=full-hard full-evaluator=exact rewrite-engine=off "
        "rewrite-split=naive rewrite-detect=dirty-frontier det-mode=auto dd-backend=bdd "
        "dumps=json,stat profile-stages=fc,wmc",
        label="canonical online cli updated config",
    )

    expected_prob = output_dir / "fact-iter1-full.prob"
    if not expected_prob.exists():
        raise CaseFailure(f"canonical online cli: missing expected artifact {expected_prob}")
    assert_glob_nonempty(
        output_dir,
        "derivation-full-after-prune1-*.json",
        label="canonical online cli json dump",
    )
    assert_glob_nonempty(output_dir, "graph-*.json", label="canonical online cli graph stats")

    elastic_output_dir = case_dir / "out_canonical_cli_elastic"
    elastic_output_dir.mkdir(parents=True, exist_ok=True)
    elastic_proc = run_cmd(
        [str(compute_bin), "-F", str(input_dir), "-D", str(elastic_output_dir)],
        cwd=compute_bin.parent,
        stdin_text="set fc-mode elastic\nshow config\ncommit\nq\n",
        timeout=240,
    )
    assert_stdout_contains(
        elastic_proc.stdout,
        "sem-mode=inc fc-mode=elastic full-evaluator=exact rewrite-engine=off "
        "rewrite-split=naive rewrite-detect=dirty-frontier det-mode=auto dd-backend=bdd "
        "dumps=json profile-stages=inc,wmc",
        label="canonical online cli elastic config",
    )
    assert_stdout_contains(
        elastic_proc.stdout,
        "[cli] fc-mode elastic currently falls back to inc-naive",
        label="canonical online cli elastic fallback notice",
    )
    elastic_iter_prob = elastic_output_dir / "fact-iter1-inc-naive.prob"
    if not elastic_iter_prob.exists():
        raise CaseFailure(f"canonical online cli elastic fallback: missing expected artifact {elastic_iter_prob}")
    assert_prob_close(
        elastic_iter_prob,
        elastic_output_dir / "facts.prob",
        label="canonical online cli elastic fallback facts.prob",
    )


def case_graph_query_canonical_surface(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("smoke_full_only", work_root / "graph_query_canonical_surface_ws")
    graph_query_bin = souffle_bin.parent / "souffle-problog-graph-query"
    if not graph_query_bin.exists():
        raise CaseFailure(f"missing graph-query binary: {graph_query_bin}")

    compute_bin, input_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )
    output_dir = case_dir / "out_graph_query_source"
    run_full_once(
        compute_bin=compute_bin,
        input_dir=input_dir,
        output_dir=output_dir,
        extra_args=["--dumpjson"],
    )

    derivation_json = output_dir / "derivation.json"
    if not derivation_json.exists():
        raise CaseFailure(f"graph-query canonical source json missing: {derivation_json}")
    expected_probs = parse_prob_file(output_dir / "facts.prob")
    query_tuple = "path(1,4)"
    expected_prob = expected_probs.get(query_tuple)
    if expected_prob is None:
        raise CaseFailure(f"graph-query canonical surface: missing {query_tuple} in facts.prob")

    exact_proc = run_cmd(
        [
            str(graph_query_bin),
            "--json",
            str(derivation_json),
            "--query",
            query_tuple,
            "--full-evaluator",
            "exact",
            "--dd-backend",
            "bdd",
        ],
        cwd=case_dir,
        timeout=240,
    )
    rewrite_proc = run_cmd(
        [
            str(graph_query_bin),
            "--json",
            str(derivation_json),
            "--query",
            query_tuple,
            "--full-evaluator",
            "exact",
            "--dd-backend",
            "bdd",
            "--rewrite-engine",
            "legacy",
            "--rewrite-split",
            "naive",
            "--rewrite-detect",
            "dirty-frontier",
        ],
        cwd=case_dir,
        timeout=240,
    )

    exact_results = parse_graph_query_results(exact_proc.stdout)
    rewrite_results = parse_graph_query_results(rewrite_proc.stdout)
    query_key = normalize_tuple_key(query_tuple)
    if query_key not in exact_results:
        raise CaseFailure(f"graph-query canonical surface: missing result for {query_tuple}\n{exact_proc.stdout}")
    if query_key not in rewrite_results:
        raise CaseFailure(
            f"graph-query canonical rewrite surface: missing result for {query_tuple}\n{rewrite_proc.stdout}"
        )
    if not math.isclose(exact_results[query_key], expected_prob, rel_tol=0.0, abs_tol=1e-9):
        raise CaseFailure(
            "graph-query canonical surface: exact replay probability mismatch\n"
            f"expected={expected_prob:.12g} actual={exact_results[query_key]:.12g}"
        )
    if not math.isclose(rewrite_results[query_key], expected_prob, rel_tol=0.0, abs_tol=1e-9):
        raise CaseFailure(
            "graph-query canonical surface: rewrite replay probability mismatch\n"
            f"expected={expected_prob:.12g} actual={rewrite_results[query_key]:.12g}"
        )

    run_cmd_expect_fail(
        [
            str(graph_query_bin),
            "--json",
            str(derivation_json),
            "--query",
            query_tuple,
            "--full-evaluator",
            "scbf",
        ],
        cwd=case_dir,
        expected_substring="graph-query does not support --full-evaluator=scbf",
        timeout=240,
    )


def case_side_channel_full_pipeline_rewrite(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "side_channel_full_pipeline_rewrite_ws"
    reset_dir(case_dir)
    base_dir = case_dir / "side_channel_full"

    run_cmd(
        [
            sys.executable,
            str(SIDE_CHANNEL_FULL_SCRIPT),
            "--base-dir",
            str(base_dir),
            "--quiet",
            "generate",
            "--cases",
            "1",
            "--rule-set",
            "trimmed",
        ],
        cwd=REPO_ROOT,
        timeout=300,
    )

    program_dir = base_dir / "P1"
    source_path = program_dir / "compute.souffle.dl"
    input_dir = program_dir / "input"
    binary_path = compile_program(
        souffle_bin=souffle_bin,
        source_path=source_path,
        input_dir=input_dir,
        output_dir=case_dir / "compile_output",
        build_dir=case_dir / "build",
        binary_name="side_channel_full_compute",
        online=True,
        full_only=True,
        timeout=300,
    )

    out_base = case_dir / "out_base"
    run_binary(binary_path=binary_path, input_dir=input_dir, output_dir=out_base, timeout=240)
    base_prob = out_base / "facts.prob"

    variants = {
        "rewrite_legacy": ["--rewrite", "--split-mode=naive-split"],
        "rewrite_implicit": ["--implicit-rewrite"],
        "rewrite_implicit_iter": ["--implicit-iterate-split-rewrite"],
    }
    for label, args in variants.items():
        out_dir = case_dir / label
        run_binary(
            binary_path=binary_path,
            input_dir=input_dir,
            output_dir=out_dir,
            extra_args=args,
            timeout=240,
        )
        assert_prob_close(out_dir / "facts.prob", base_prob, label=f"side_channel_full {label}")
        assert_csv_outputs_match(out_dir, out_base, label=f"side_channel_full {label} csv")


def case_scbf_rewrite_runtime_lane(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("smoke_full_only", work_root / "scbf_rewrite_runtime_lane_ws")
    compute_bin, input_dir, _ = compile_compute(
        souffle_bin=souffle_bin,
        case_dir=case_dir,
        full_only=True,
    )

    output_dir = case_dir / "out_scbf_rewrite_lane"
    proc = run_full_once(
        compute_bin=compute_bin,
        input_dir=input_dir,
        output_dir=output_dir,
        extra_args=["--scbf", "--rewrite", "--split-mode=naive-split"],
        timeout=240,
    )
    assert_stdout_contains(
        proc.stdout,
        "[pipeline] selected runtime lane=experimental-scbf+rewrite-legacy",
        label="scbf rewrite runtime lane",
    )
    assert_stdout_contains(
        proc.stdout,
        "[pipeline] rewrite took ",
        label="scbf rewrite still executes rewrite lane",
    )
    vals = parse_prob_file(output_dir / "facts.prob")
    if not vals:
        raise CaseFailure("scbf rewrite runtime lane: empty facts.prob")


def case_side_channel_incremental_pipeline_modes(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "side_channel_incremental_pipeline_modes_ws"
    reset_dir(case_dir)
    base_dir = case_dir / "side_channel_inc"

    run_cmd(
        [
            sys.executable,
            str(SIDE_CHANNEL_INC_SCRIPT),
            "--base-dir",
            str(base_dir),
            "--quiet",
            "generate",
            "--cases",
            "1",
            "--rule-set",
            "trimmed",
        ],
        cwd=REPO_ROOT,
        timeout=300,
    )
    run_cmd(
        [
            sys.executable,
            str(SIDE_CHANNEL_INC_SCRIPT),
            "--base-dir",
            str(base_dir),
            "--quiet",
            "delta",
            "--cases",
            "1",
            "--sets",
            "1",
            "--change-spec",
            "inc1=0.001",
            "--change-cap",
            "inc1=2",
        ],
        cwd=REPO_ROOT,
        timeout=300,
    )

    program_dir = base_dir / "P1"
    source_path = program_dir / "compute.souffle.dl"
    input_dir = program_dir / "input"
    delta_script = (program_dir / "delta" / "inc1.txt").read_text(encoding="utf-8")
    commits = count_cli_commits(delta_script)
    if commits < 2:
        raise CaseFailure(f"side_channel_inc: expected two commits in delta script, saw {commits}")

    binary_path = compile_program(
        souffle_bin=souffle_bin,
        source_path=source_path,
        input_dir=input_dir,
        output_dir=case_dir / "compile_output",
        build_dir=case_dir / "build",
        binary_name="side_channel_inc_compute",
        online=True,
        full_only=False,
        timeout=300,
    )

    out_full = case_dir / "out_full_hard"
    out_inc = case_dir / "out_inc_naive"
    run_binary(
        binary_path=binary_path,
        input_dir=input_dir,
        output_dir=out_full,
        extra_args=["--setmode", "full-hard", "--det-opt"],
        stdin_text=delta_script,
        timeout=240,
    )
    run_binary(
        binary_path=binary_path,
        input_dir=input_dir,
        output_dir=out_inc,
        extra_args=["--setmode", "inc-naive", "--det-opt"],
        stdin_text=delta_script,
        timeout=240,
    )

    for iteration in (1, 2):
        assert_prob_close(
            iter_prob_path(out_inc, iteration, "inc-naive"),
            iter_prob_path(out_full, iteration, "full"),
            label=f"side_channel_inc iter={iteration}",
        )

    first_turn = first_commit_script(delta_script)
    out_full_regional = case_dir / "out_full_regional_baseline"
    out_regional = case_dir / "out_inc_regional"
    run_binary(
        binary_path=binary_path,
        input_dir=input_dir,
        output_dir=out_full_regional,
        extra_args=["--setmode", "full-hard", "--det-opt"],
        stdin_text=first_turn,
        timeout=240,
    )
    run_binary(
        binary_path=binary_path,
        input_dir=input_dir,
        output_dir=out_regional,
        extra_args=["--setmode", "inc-regional", "--det-opt"],
        stdin_text=first_turn,
        timeout=240,
    )
    assert_prob_close(
        iter_prob_path(out_regional, 1, "inc-regional"),
        iter_prob_path(out_full_regional, 1, "full"),
        label="side_channel_inc regional iter=1",
    )


def case_taint_stage_pipeline_compile_smoke(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "taint_stage_pipeline_compile_ws"
    reset_dir(case_dir)

    run_cmd(
        [
            sys.executable,
            str(TAINT_INC_SCRIPT),
            "--base-dir",
            str(case_dir / "taint_base"),
            "--bundle",
            TAINT_BUNDLE,
            "--cases",
            TAINT_CASE,
            "--quiet",
            "prepare",
        ],
        cwd=REPO_ROOT,
        timeout=300,
    )

    workspace = case_dir / "taint_base" / TAINT_BUNDLE
    base_input_dir = workspace / "cases" / TAINT_CASE / "input"
    stages = read_stage_list(workspace / "pipeline" / "stages.txt")
    previous_outputs: List[Path] = []

    for stage in stages:
        stage_source = workspace / "cases" / TAINT_CASE / "stages" / stage / "compute.souffle.dl"
        stage_input_dir = case_dir / "runtime" / stage / "input"
        stage_output_dir = case_dir / "runtime" / stage / "output"
        copy_pipeline_stage_input(
            base_input_dir=base_input_dir,
            previous_output_dirs=previous_outputs,
            stage_input_dir=stage_input_dir,
            max_rows_per_fact=12,
        )
        binary_path = compile_program(
            souffle_bin=souffle_bin,
            source_path=stage_source,
            input_dir=stage_input_dir,
            output_dir=case_dir / "runtime" / stage / "compile_output",
            build_dir=case_dir / "runtime" / stage / "build",
            binary_name="compute",
            online=True,
            full_only=True,
            timeout=300,
        )
        run_binary(
            binary_path=binary_path,
            input_dir=stage_input_dir,
            output_dir=stage_output_dir,
            extra_args=["--det-opt"],
            timeout=240,
        )
        assert_output_dir_has_pipeline_artifacts(
            stage_output_dir, label=f"taint stage pipeline {stage}"
        )
        previous_outputs.append(stage_output_dir)


def case_datarace_stage_pipeline_smoke(souffle_bin: Path, work_root: Path) -> None:
    case_dir = prepare_case_workspace("datarace_stage_pipeline_smoke", work_root)
    base_input_dir = case_dir / "input"
    stages = read_stage_list(case_dir / "pipeline" / "stages.txt")
    previous_outputs: List[Path] = []
    final_output_dir: Path | None = None

    for stage in stages:
        stage_source = case_dir / "stages" / stage / "compute.souffle.dl"
        stage_input_dir = case_dir / "runtime" / stage / "input"
        stage_output_dir = case_dir / "runtime" / stage / "output"
        copy_pipeline_stage_input(
            base_input_dir=base_input_dir,
            previous_output_dirs=previous_outputs,
            stage_input_dir=stage_input_dir,
            max_rows_per_fact=None,
        )
        binary_path = compile_program(
            souffle_bin=souffle_bin,
            source_path=stage_source,
            input_dir=stage_input_dir,
            output_dir=case_dir / "runtime" / stage / "compile_output",
            build_dir=case_dir / "runtime" / stage / "build",
            binary_name="compute",
            online=True,
            full_only=True,
            timeout=300,
        )
        run_binary(
            binary_path=binary_path,
            input_dir=stage_input_dir,
            output_dir=stage_output_dir,
            timeout=180,
        )
        assert_output_dir_has_pipeline_artifacts(
            stage_output_dir, label=f"datarace stage pipeline {stage}"
        )
        previous_outputs.append(stage_output_dir)
        final_output_dir = stage_output_dir

    if final_output_dir is None:
        raise CaseFailure("datarace stage pipeline: no stages executed")
    race_pairs = final_output_dir / "racePairs.csv"
    if not race_pairs.exists():
        raise CaseFailure(f"datarace stage pipeline: missing final racePairs.csv at {race_pairs}")


CASES = {
    "smoke_full_only": case_smoke_full_only,
    "dred_mix_naive_vs_full": case_dred_mix_naive_vs_full,
    "dred_hub_rederive_naive_vs_full": case_dred_hub_rederive_naive_vs_full,
    "detopt_inc_naive_combo_vs_full": case_detopt_inc_naive_combo_vs_full,
    "detopt_inc_regional_single_round_vs_full": case_detopt_inc_regional_single_round_vs_full,
    "detopt_recursive_derivation_guard_vs_full": case_detopt_recursive_derivation_guard_vs_full,
    "rewrite_split_modes_equiv": case_rewrite_split_modes_equiv,
    "rewrite_dirty_detect_equiv": case_rewrite_dirty_detect_equiv,
    "full_det_modes": case_full_det_modes,
    "dump_outputs_contract": case_dump_outputs_contract,
    "full_const_negation_grounding": case_full_const_negation_grounding,
    "canonical_compile_defaults_contract": case_canonical_compile_defaults_contract,
    "canonical_online_cli_surface": case_canonical_online_cli_surface,
    "graph_query_canonical_surface": case_graph_query_canonical_surface,
    "side_channel_full_pipeline_rewrite": case_side_channel_full_pipeline_rewrite,
    "scbf_rewrite_runtime_lane": case_scbf_rewrite_runtime_lane,
    "side_channel_incremental_pipeline_modes": case_side_channel_incremental_pipeline_modes,
    "taint_stage_pipeline_compile_smoke": case_taint_stage_pipeline_compile_smoke,
    "datarace_stage_pipeline_smoke": case_datarace_stage_pipeline_smoke,
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
