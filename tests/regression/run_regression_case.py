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
REWRITE_ITER_DETECT_RE = re.compile(
    r"^\[GraphRewriter\] Iteration (\d+) : detected (\d+) SISO region\(s\)\."
)
REWRITE_SISO_MODE_RE = re.compile(
    r"^\[siso-detect\] mode=(\S+) fallback=(\d+) "
    r"seeds\(nodes=(\d+),edges=(\d+)\) frontier\(nodes=(\d+),edges=(\d+)\)$"
)
CASES_ROOT = Path(__file__).resolve().parent / "cases"


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
        extra_args=["--dumpjson", "--dumpdot", "--dumpstat", "--logfile", "reglog"],
    )

    expected_dot_before = out_full / "derivation-full-before-prune1.dot"
    expected_dot_after = out_full / "derivation-full-after-prune1.dot"
    expected_prob = out_full / "fact-iter1-full.prob"
    for expected in (expected_dot_before, expected_dot_after, expected_prob):
        if not expected.exists():
            raise CaseFailure(f"dump contract: missing expected artifact {expected}")

    assert_glob_nonempty(
        out_full,
        "derivation-full-after-prune1-*.json",
        label="dump contract json after prune",
    )
    assert_glob_nonempty(out_full, "reglog_*.json", label="dump contract debugger logs")
    assert_glob_nonempty(out_full, "graph-*.json", label="dump contract graph stats")


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
