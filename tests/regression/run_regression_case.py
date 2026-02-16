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
import re
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

PROB_LINE_RE = re.compile(r"^\s*(.*?)\s*:\s*([+\-]?\d+(?:\.\d+)?(?:[eE][+\-]?\d+)?)\s*$")


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


def write_relation(
    input_dir: Path,
    relation: str,
    tuples: Iterable[Tuple[int, ...]],
    *,
    probs: Iterable[float] | None = None,
) -> None:
    tuple_list = list(tuples)
    facts_path = input_dir / f"{relation}.facts"
    with facts_path.open("w", encoding="utf-8") as f:
        for row in tuple_list:
            f.write("\t".join(str(v) for v in row))
            f.write("\n")

    if probs is None:
        return

    prob_list = list(probs)
    if len(prob_list) != len(tuple_list):
        raise CaseFailure(
            f"probability count mismatch for relation {relation}: "
            f"{len(prob_list)} probs vs {len(tuple_list)} tuples"
        )

    prob_path = input_dir / f"{relation}.prob"
    with prob_path.open("w", encoding="utf-8") as f:
        for p in prob_list:
            f.write(f"{p:.12g}\n")


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
    timeout: int = 180,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(compute_bin), "-F", str(input_dir), "-D", str(output_dir)]
    if extra_args:
        cmd.extend(extra_args)
    run_cmd(cmd, cwd=compute_bin.parent, timeout=timeout)


def assert_glob_nonempty(base_dir: Path, pattern: str, *, label: str) -> None:
    matches = sorted(base_dir.glob(pattern))
    if not matches:
        raise CaseFailure(f"{label}: expected files matching {base_dir / pattern}")


def make_dred_mix_program() -> str:
    return textwrap.dedent(
        """
        .decl edge(x:number, y:number)
        .input edge
        .decl bridge(x:number, y:number)
        .input bridge

        .decl hop(x:number, y:number)
        hop(x,y) :- edge(x,y).
        hop(x,y) :- bridge(x,y).

        .decl path(x:number, y:number)
        path(x,y) :- hop(x,y).
        path(x,z) :- path(x,y), hop(y,z).

        .decl alarm(y:number)
        .output alarm
        alarm(y) :- path(1,y).
        alarm(y) :- path(2,y).
        """
    ).strip() + "\n"


def make_detopt_program() -> str:
    return textwrap.dedent(
        """
        .decl trust(x:number, y:number)
        .input trust
        .decl chance(x:number, y:number)
        .input chance

        .decl det_path(x:number, y:number)
        det_path(x,y) :- trust(x,y).
        det_path(x,z) :- det_path(x,y), trust(y,z).

        .decl risk(y:number)
        risk(y) :- chance(1,y).

        .decl alert(y:number)
        .output alert
        alert(y) :- det_path(1,y).
        alert(y) :- risk(y).
        """
    ).strip() + "\n"


def make_path_program() -> str:
    return textwrap.dedent(
        """
        .decl edge(x:number, y:number)
        .input edge

        .decl path(x:number, y:number)
        .output path
        path(x,y) :- edge(x,y).
        path(x,z) :- path(x,y), edge(y,z).
        """
    ).strip() + "\n"


def case_smoke_full_only(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "smoke_full_only"
    reset_dir(case_dir)

    input_dir = case_dir / "input"
    output_dir = case_dir / "output_run"
    input_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    write_text(
        case_dir / "compute.dl",
        textwrap.dedent(
            """
            .decl edge(x:number, y:number)
            .input edge
            .decl path(x:number, y:number)
            .output path
            path(x,y) :- edge(x,y).
            path(x,z) :- path(x,y), edge(y,z).
            """
        ).strip()
        + "\n",
    )
    edges = [(1, 2), (2, 3), (3, 4), (1, 4)]
    probs = [0.9, 0.8, 0.7, 0.2]
    write_relation(input_dir, "edge", edges, probs=probs)

    compute_bin, in_dir, _ = compile_compute(
        souffle_bin=souffle_bin, case_dir=case_dir, full_only=True
    )
    run_full_once(compute_bin=compute_bin, input_dir=in_dir, output_dir=output_dir)

    facts_prob = output_dir / "facts.prob"
    vals = parse_prob_file(facts_prob)
    if not vals:
        raise CaseFailure(f"smoke test produced empty probability output: {facts_prob}")


def case_dred_mix_naive_vs_full(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "dred_mix"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)
    write_text(case_dir / "compute.dl", make_dred_mix_program())

    edge_rows = [
        (1, 2),
        (2, 3),
        (1, 3),
        (3, 4),
        (2, 4),
        (4, 5),
        (5, 6),
        (2, 6),
        (1, 5),
    ]
    edge_probs = [0.91, 0.72, 0.33, 0.84, 0.61, 0.77, 0.66, 0.42, 0.55]
    write_relation(input_dir, "edge", edge_rows, probs=edge_probs)
    write_relation(input_dir, "bridge", [(2, 5)], probs=[0.44])

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
    case_dir = work_root / "dred_hub_rederive"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)

    write_text(
        case_dir / "compute.dl",
        textwrap.dedent(
            """
            .decl seed(x:number)
            .input seed
            .decl edge(x:number, y:number)
            .input edge

            .decl reach(x:number)
            reach(x) :- seed(x).
            reach(y) :- reach(x), edge(x,y).

            .decl hot(x:number)
            .output hot
            hot(x) :- reach(x).
            hot(x) :- seed(s), edge(s,x).
            """
        ).strip()
        + "\n",
    )

    seed_rows = [(i,) for i in range(1, 9)]
    write_relation(input_dir, "seed", seed_rows)

    edge_rows: List[Tuple[int, int]] = []
    for i in range(1, 25):
        edge_rows.append((i, i + 1))
    for i in range(1, 17):
        edge_rows.append((i, 40))
    edge_rows.extend([(40, 41), (41, 42), (42, 43), (10, 30), (12, 32), (32, 43)])
    # Preserve order while dropping duplicates.
    edge_rows = list(dict.fromkeys(edge_rows))

    edge_probs = []
    for idx, _ in enumerate(edge_rows):
        p = 0.31 + ((idx * 17) % 59) / 100.0
        edge_probs.append(min(p, 0.97))
    write_relation(input_dir, "edge", edge_rows, probs=edge_probs)

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
    case_dir = work_root / "detopt_inc_naive_combo"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)
    write_text(case_dir / "compute.dl", make_detopt_program())

    trust_rows = [(1, 2), (2, 3), (3, 4), (1, 5), (5, 6), (6, 4)]
    chance_rows = [(1, 2), (1, 3), (1, 4), (1, 6)]
    chance_probs = [0.25, 0.60, 0.40, 0.80]
    write_relation(input_dir, "trust", trust_rows)
    write_relation(input_dir, "chance", chance_rows, probs=chance_probs)

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
    case_dir = work_root / "detopt_inc_regional_single_round"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)
    write_text(case_dir / "compute.dl", make_detopt_program())

    trust_rows = [(1, 2), (2, 3), (3, 4), (1, 5), (5, 6), (6, 4)]
    chance_rows = [(1, 2), (1, 3), (1, 4), (1, 6)]
    chance_probs = [0.25, 0.60, 0.40, 0.80]
    write_relation(input_dir, "trust", trust_rows)
    write_relation(input_dir, "chance", chance_rows, probs=chance_probs)

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


def case_rewrite_split_modes_equiv(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "rewrite_split_modes"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)

    write_text(case_dir / "compute.dl", make_path_program())
    write_relation(
        input_dir,
        "edge",
        [(1, 2), (2, 3), (1, 3), (3, 4), (2, 4), (4, 5)],
        probs=[0.8, 0.6, 0.3, 0.9, 0.55, 0.5],
    )

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


def case_full_det_modes(souffle_bin: Path, work_root: Path) -> None:
    case_dir = work_root / "full_det_modes"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)
    write_text(case_dir / "compute.dl", make_detopt_program())

    trust_rows = [(1, 2), (2, 3), (3, 4), (1, 5), (5, 6), (6, 4)]
    chance_rows = [(1, 2), (1, 3), (1, 4), (1, 6)]
    chance_probs = [0.25, 0.60, 0.40, 0.80]
    write_relation(input_dir, "trust", trust_rows)
    write_relation(input_dir, "chance", chance_rows, probs=chance_probs)

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
    case_dir = work_root / "dump_outputs_contract"
    reset_dir(case_dir)
    input_dir = case_dir / "input"
    input_dir.mkdir(parents=True, exist_ok=True)
    write_text(case_dir / "compute.dl", make_path_program())

    edge_rows = [(1, 2), (2, 3), (1, 3), (3, 4)]
    edge_probs = [0.9, 0.8, 0.5, 0.7]
    write_relation(input_dir, "edge", edge_rows, probs=edge_probs)

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
    "rewrite_split_modes_equiv": case_rewrite_split_modes_equiv,
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
