#!/usr/bin/env python3
"""Weighted ApproxMC demo:
1) weighted CNF -> unweighted CNF (weighted-to-unweighted)
2) approximate count on unweighted CNF (ApproxMC CLI or pyapproxmc)
3) divide by converter factor to recover weighted estimate
"""

import argparse
import pathlib
import re
import subprocess
import sys
import tempfile
from typing import List, Optional, Tuple


WEIGHTED_CNF = """p cnf 2 1
c t wpmc
c p show 1 2 0
1 2 0
c p weight 1 0.9 0
c p weight 2 0.5 0
"""


def run(cmd: List[str]) -> str:
    proc = subprocess.run(cmd, check=True, capture_output=True, text=True)
    return proc.stdout


def parse_divisor(converter_stdout: str) -> int:
    match = re.search(r"divide by: 2\*\*(\d+)", converter_stdout)
    if not match:
        raise RuntimeError("Cannot parse divisor from weighted_to_unweighted output.")
    return int(match.group(1))


def parse_approxmc_cli_count(stdout: str) -> int:
    match = re.search(r"^s mc (\d+)$", stdout, flags=re.MULTILINE)
    if match:
        return int(match.group(1))

    match = re.search(r"Number of solutions is:\s*(\d+)\*2\*\*(\d+)", stdout)
    if match:
        return int(match.group(1)) << int(match.group(2))

    raise RuntimeError("Cannot parse ApproxMC count from CLI output.")


def parse_dimacs_cnf(path: pathlib.Path) -> Tuple[List[List[int]], Optional[List[int]]]:
    clauses: List[List[int]] = []
    projection: Optional[List[int]] = None

    with path.open("r", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("c p show "):
                projection = [int(x) for x in line.split()[3:] if x != "0"]
                continue
            if line.startswith("c") or line.startswith("p"):
                continue
            lits = [int(x) for x in line.split() if x != "0"]
            if lits:
                clauses.append(lits)

    return clauses, projection


def count_with_pyapproxmc(
    unweighted_cnf: pathlib.Path,
    pyapproxmc_path: Optional[pathlib.Path],
    seed: int,
    epsilon: float,
    delta: float,
) -> int:
    if pyapproxmc_path is not None:
        sys.path.insert(0, str(pyapproxmc_path))

    try:
        import pyapproxmc  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "pyapproxmc is not importable. Install it or provide --approxmc-bin."
        ) from exc

    clauses, projection = parse_dimacs_cnf(unweighted_cnf)
    counter = pyapproxmc.Counter(seed=seed, epsilon=epsilon, delta=delta)
    counter.add_clauses(clauses)
    if projection is None:
        cell, hashes = counter.count()
    else:
        cell, hashes = counter.count(projection)
    return int(cell) * (1 << int(hashes))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Demo weighted -> unweighted -> ApproxMC counting pipeline"
    )
    parser.add_argument(
        "--converter",
        type=pathlib.Path,
        required=True,
        help="Path to weighted_to_unweighted.py",
    )
    parser.add_argument(
        "--approxmc-bin",
        type=pathlib.Path,
        default=None,
        help="Path to approxmc binary (optional; if omitted, pyapproxmc is used)",
    )
    parser.add_argument(
        "--pyapproxmc-path",
        type=pathlib.Path,
        default=None,
        help="Optional path added to sys.path before importing pyapproxmc",
    )
    parser.add_argument("--prec", type=int, default=7, help="Converter precision")
    parser.add_argument("--seed", type=int, default=1, help="ApproxMC seed")
    parser.add_argument("--epsilon", type=float, default=0.2, help="ApproxMC epsilon")
    parser.add_argument("--delta", type=float, default=0.05, help="ApproxMC delta")
    args = parser.parse_args()

    if not args.converter.exists():
        raise RuntimeError(f"Converter script not found: {args.converter}")
    if args.approxmc_bin is not None and not args.approxmc_bin.exists():
        raise RuntimeError(f"ApproxMC binary not found: {args.approxmc_bin}")

    with tempfile.TemporaryDirectory(prefix="approxmc-weighted-demo-") as temp_dir:
        temp = pathlib.Path(temp_dir)
        weighted = temp / "weighted.cnf"
        unweighted = temp / "unweighted.cnf"

        weighted.write_text(WEIGHTED_CNF, encoding="utf-8")

        converter_out = run(
            [
                "python3",
                str(args.converter),
                "--prec",
                str(args.prec),
                str(weighted),
                str(unweighted),
            ]
        )
        divisor_exp = parse_divisor(converter_out)

        if args.approxmc_bin is not None:
            approx_out = run(
                [str(args.approxmc_bin), "--seed", str(args.seed), str(unweighted)]
            )
            unweighted_count = parse_approxmc_cli_count(approx_out)
            backend = "approxmc-bin"
        else:
            unweighted_count = count_with_pyapproxmc(
                unweighted_cnf=unweighted,
                pyapproxmc_path=args.pyapproxmc_path,
                seed=args.seed,
                epsilon=args.epsilon,
                delta=args.delta,
            )
            backend = "pyapproxmc"

    weighted_estimate = unweighted_count / float(2**divisor_exp)
    exact_reference = 0.95

    print(f"Backend: {backend}")
    print(f"Unweighted count estimate: {unweighted_count}")
    print(f"Divider from converter: 2**{divisor_exp}")
    print(f"Weighted estimate: {weighted_estimate:.8f}")
    print(f"Reference exact weighted value for this toy CNF: {exact_reference:.8f}")
    print(
        "Absolute error:",
        f"{abs(weighted_estimate - exact_reference):.8f}",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
