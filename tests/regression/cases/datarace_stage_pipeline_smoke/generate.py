#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path


STAGES = [
    "chord_analyses_method_checkExcludedM.dlog",
    "chord_analyses_datarace_parallel_include.dlog",
    "chord_analyses_datarace_escaping_include.dlog",
    "chord_analyses_datarace_datarace.dlog",
]


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    out_dir = Path(args.out_dir).resolve()

    write_text(out_dir / "pipeline" / "stages.txt", "\n".join(STAGES) + "\n")
    write_text(out_dir / "compute.dl", ".decl placeholder(x:number)\n.output placeholder\n")

    write_text(
        out_dir / "input" / "checkExcludedT.facts",
        "1\n",
    )
    write_text(
        out_dir / "input" / "TM.facts",
        "1\t10\n2\t20\n3\t30\n",
    )
    write_text(
        out_dir / "input" / "reachableAM.facts",
        "1\t20\n2\t30\n",
    )
    write_text(
        out_dir / "input" / "ME.facts",
        "20\t100\n30\t200\n",
    )
    write_text(
        out_dir / "input" / "writeE.facts",
        "100\n",
    )
    write_text(
        out_dir / "input" / "EF.facts",
        "100\t300\n200\t300\n",
    )
    write_text(
        out_dir / "input" / "statF.facts",
        "300\n",
    )
    write_text(
        out_dir / "input" / "EH.facts",
        "100\t400\n200\t400\n",
    )
    write_text(
        out_dir / "input" / "mhe.facts",
        "100\t1\t2\n200\t2\t1\n",
    )
    write_text(
        out_dir / "input" / "syncLH.facts",
        "7\t400\n",
    )
    write_text(
        out_dir / "input" / "unlockedE.facts",
        "1\t100\t400\n",
    )

    write_text(
        out_dir / "stages" / STAGES[0] / "compute.souffle.dl",
        """
.decl checkExcludedT(t:number)
.input checkExcludedT
.decl TM(t:number, m:number)
.input TM
.decl checkExcludedM(m:number)
.output checkExcludedM

checkExcludedM(m) :- TM(t, m), checkExcludedT(t).
""".strip()
        + "\n",
    )

    write_text(
        out_dir / "stages" / STAGES[1] / "compute.souffle.dl",
        """
.decl writeE(e:number)
.input writeE
.decl ME(m:number, e:number)
.input ME
.decl EF(e:number, f:number)
.input EF
.decl statF(f:number)
.input statF
.decl reachableAM(a:number, m:number)
.input reachableAM
.decl checkExcludedM(m:number)
.input checkExcludedM
.decl EH(e:number, h:number)
.input EH
.decl mhe(e:number, a1:number, a2:number)
.input mhe

.decl relevantAM(a:number, m:number)
.decl relevantAE(a:number, e:number)
.decl rdOrWrAEF(a:number, e:number, f:number)
.decl onlyWrAEF(a:number, e:number, f:number)
.decl startingRace(a1:number, e1:number, a2:number, e2:number)
.decl escapingRace(a1:number, e1:number, a2:number, e2:number)
.output escapingRace
.decl parallelRace(a1:number, e1:number, a2:number, e2:number)
.output parallelRace

relevantAM(a, m) :- reachableAM(a, m), !checkExcludedM(m).
relevantAE(a, e) :- relevantAM(a, m), ME(m, e).
rdOrWrAEF(a, e, f) :- relevantAE(a, e), EF(e, f).
onlyWrAEF(a, e, f) :- relevantAE(a, e), EF(e, f), writeE(e).

startingRace(a1, e1, a2, e2) :- onlyWrAEF(a1, e1, f), rdOrWrAEF(a2, e2, f), e1 < e2.
startingRace(a1, e1, a2, e2) :- rdOrWrAEF(a1, e1, f), onlyWrAEF(a2, e2, f), e1 < e2.
startingRace(a1, e1, a2, e2) :- onlyWrAEF(a1, e1, f), onlyWrAEF(a2, e2, f), e1 = e2, a1 <= a2.

escapingRace(a1, e1, a2, e2) :- startingRace(a1, e1, a2, e2), EH(e1, h), EH(e2, h).
escapingRace(a1, e1, a2, e2) :- startingRace(a1, e1, a2, e2), EF(e1, f1), EF(e2, f2), statF(f1), statF(f2).

parallelRace(a1, e1, a2, e2) :- escapingRace(a1, e1, a2, e2), mhe(e1, a1, a2), mhe(e2, a2, a1).
""".strip()
        + "\n",
    )

    write_text(
        out_dir / "stages" / STAGES[2] / "compute.souffle.dl",
        """
.decl parallelRace(a1:number, e1:number, a2:number, e2:number)
.input parallelRace
.decl EH(e:number, h:number)
.input EH
.decl syncLH(l:number, h:number)
.input syncLH
.decl unlockedE(t:number, e:number, h:number)
.input unlockedE

.decl syncH(h:number)
.decl guardedE(t:number, e:number, h:number)
.decl unlikelyRace(a1:number, e1:number, a2:number, e2:number)
.output unlikelyRace

syncH(h) :- syncLH(_, h).
guardedE(t, e, h) :- parallelRace(t, e, _, _), EH(e, h), syncH(h), !unlockedE(t, e, h).
guardedE(t, e, h) :- parallelRace(_, _, t, e), EH(e, h), syncH(h), !unlockedE(t, e, h).

unlikelyRace(a1, e1, a2, e2) :- parallelRace(a1, e1, a2, e2), guardedE(a1, e1, h), guardedE(a2, e2, h).
""".strip()
        + "\n",
    )

    write_text(
        out_dir / "stages" / STAGES[3] / "compute.souffle.dl",
        """
.decl parallelRace(a1:number, e1:number, a2:number, e2:number)
.input parallelRace
.decl unlikelyRace(a1:number, e1:number, a2:number, e2:number)
.input unlikelyRace

.decl ultimateRace(a1:number, e1:number, a2:number, e2:number)
.output ultimateRace
.decl racePairs(e1:number, e2:number)
.output racePairs

ultimateRace(a1, e1, a2, e2) :- parallelRace(a1, e1, a2, e2), !unlikelyRace(a1, e1, a2, e2).
racePairs(e1, e2) :- ultimateRace(_, e1, _, e2).
""".strip()
        + "\n",
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
