# Correlation-Driven Variable Ordering Roadmap

This note captures how we can extend `BDDForceHeuristics` so that the static variable ordering reflects correlations between sub-formulas. The goal is to keep highly correlated portions together (with their shared set centered) while discouraging unrelated pieces from interleaving.

## 1. Observations From The Current Implementation

- `GraphHeuristics.h` currently produces only *attractive* hyperedges (AND/OR/BRIDGE) and minimizes weighted span during FORCE iterations (`forceFree()` at `src/include/souffle/problog/formula/GraphHeuristics.h:528`).
- The AND construction flattens all variables collected from a rule body into one event (`buildEvents()` around lines `362-408`), so FORCE has no awareness of how much two body literals overlap or diverge.
- Because the objective is purely attractive, independent literals can end up packed together, which performs poorly on smokers-style programs where the key is to pull correlated people–smoking–cancer chains apart unless they share evidence.

## 2. Correlation Signal Extraction

1. **Literal Blocks**  
   While building an AND event, keep per-body-literal summaries instead of immediately flattening everything into `vars`. A `LiteralBlock` contains `(rule_or_fact_id, std::vector<int> expanded_vars)` and preserves sorted deduplicated var IDs (similar to how `push_event()` normalizes inputs).

2. **Pairwise Analysis**  
   For each pair `(A, B)` of literal blocks inside one rule:
   - Compute `I = A ∩ B` (correlated set), `A_only = A \ I`, `B_only = B \ I`.
   - If `I` is non-empty we flag a *correlated pair*, otherwise mark it as *independent*.

3. **Correlation Events**  
   Emit three kinds of events per correlated pair:
   - **Intersection anchor:** `I` alone with a moderate positive weight (`Params::w_corr_anchor`) so that FORCE tries to keep shared vars contiguous.
   - **Outer-to-center pulls:** `(A_only ∪ I)` and `(I ∪ B_only)` using a stronger weight (`w_corr_pull`). They encourage ordering `A_only – I – B_only` while still letting either side move relative to the shared nucleus.
   - **Soft union:** `(A_only ∪ I ∪ B_only)` with a small weight (`w_corr_group`) so the whole trio does not drift too far away from the rest of the rule.

4. **Anti Events For Side Blocks**  
   Even if `I` is non-empty we still want the unique sides to avoid interleaving, so create a repulsive event on `A_only ∪ B_only`. When `I` happens to be empty (fully independent literals) this reduces to the same construction. The weight magnitude is `w_anti` and the event is tagged as repulsive, penalizing side blocks that weave into each other unless some other attractive event counters that pressure.

## 3. FORCE Integration

1. **Event Metadata**  
   Extend `struct Event` (defined near line `93`) with
   - `enum class Kind { Attractive, Repulsive };`
   - `Kind kind;`
   The existing AND/OR/BRIDGE events are attractive; correlation anti-events are repulsive.

2. **Iteration Update**  
   During `forceFree()` and `forceAnchored()` we currently fold all weights into `sumW` / `sumWC`. Replace it with two accumulators:
   - Attractive contributions build the usual weighted mean.
   - Repulsive contributions compute their center but store it separately. When determining the target position of a variable we first obtain the attractive center (if any) and then shift it away from each repulsive center using a configurable scaling factor (`Params::repel_push`). A simple variant is:
     ```
     target = attr_center;
     for each repulsive contribution r:
         target += repel_push * (attr_center - r.center);
     ```
   - When a variable is only part of repulsive events we fall back to its current position to avoid oscillation.

3. **Objective Reporting**  
   Update `totalSpan()` so that repulsive weights are subtracted instead of added (mirroring the shift in FORCE). This keeps the debug span value meaningful.

## 4. Parameter Surface

Add the following fields to `BDDForceHeuristics::Params` (`GraphHeuristics.h:18-45`):
- `double w_corr_anchor`, `double w_corr_pull`, `double w_corr_group` – magnitude of the three attractive correlation events.
- `double w_anti` – base weight for independent repulsion events (stored as a positive scalar; the `Event` records its kind).
- `double repel_push` – how aggressively repulsive centers move variables (default 0.35–0.5).
- `bool enable_corr_events` / `bool enable_anti_events` – toggles for ablation studies.

These parameters must also be hooked into the verbose logging so experiments can track which combinations helped.

## 5. Implementation Steps

1. **Data structure updates**
   - Extend `Params` plus its constructor defaults.
   - Add the `Event::Kind` flag and optional helper factories (`makeAttractive`, `makeRepulsive`).
   - Introduce a small `LiteralBlock` helper struct inside `buildEvents()` to retain per-body information.

2. **Correlation-aware event generation**
   - While iterating rule bodies inside `buildEvents()` keep both the flattened `vars` (for legacy AND events) and the vector of literal blocks.
   - After emitting the classic AND event (`push_event(vars, P_.w_and)`), run a new `emitCorrelationEvents(literal_blocks)` helper that carries out the pairwise analysis described in Section 2.

3. **Repulsion-aware FORCE**
   - Refactor `forceFree()` and `forceAnchored()` to read the `Event::Kind` flag, maintain two accumulators, and adjust the target computation.
   - Make sure their fallback ordering (used when sums are zero) is deterministic by still breaking ties with the stable key.

4. **Span accounting and debugging**
   - Let `totalSpan()` subtract repulsive spans (or equivalently add `w` with `Kind::Repulsive`).
   - Extend verbose prints to expose how many correlation / anti events were generated per rule and their average sizes, which helps tuning.

5. **Testing hooks**
   - Add a debug-only knob (e.g., `Params::log_corr_examples`) to dump detailed info on a handful of rules in smokers. This speeds up verifying whether the intersections/repulsions match expectations before running full benchmarks.

With this roadmap we separate the mechanical work (data plumbing and FORCE math) from heuristic tuning, which should make it easier to iterate until the smokers benchmark improves.

## 6. Repulsive Event Implementation Details

To keep the plan concrete, here is how to realize the repulsion mechanism end-to-end.

1. **Representation**
   - Extend `struct Event` with `enum class Kind { Attractive, Repulsive }; Kind kind;`.
   - Provide helpers:
     ```c++
     Event makeAttractive(std::vector<int> vars, double w, CanonicalKey key);
     Event makeRepulsive(std::vector<int> vars, double w, CanonicalKey key);
     ```
     `w` stays positive; the kind flag dictates how FORCE interprets it.

2. **Event builders**
   - Keep the existing `push_event()` for attractive events and add `push_repulsive_event()` that shares all canonicalization logic but sets `kind = Repulsive`.
   - Whenever `A_only ∪ B_only` is non-empty, call the new helper. If the set has fewer than two variables we skip it (repulsion between a single var and nothing makes no sense).

3. **FORCE iteration**
   - Maintain separate accumulators: `sum_attr_weight`, `sum_attr_weighted_center`, plus `sum_rep_weight`, `sum_rep_weighted_center`.
   - After computing centers for every event:
     ```c++
     double attr_center = sum_attr_weight ? sum_attr_weighted_center / sum_attr_weight : pos[v];
     double repel_shift = 0.0;
     if (sum_rep_weight) {
         double rep_center = sum_rep_weighted_center / sum_rep_weight;
         repel_shift = P_.repel_push * (attr_center - rep_center);
     }
     target = attr_center + repel_shift;
     ```
   - If a variable has no attractive contribution, `attr_center` defaults to its current index so the shift still moves it away without losing stability.

4. **Objective & logging**
   - `totalSpan()` subtracts the span for repulsive events: `S += (mx - mn) * (e.kind == Attractive ? e.weight : -e.weight);`.
   - Verbose mode prints counts of attractive vs. repulsive events so we can see how many anti-constraints influence FORCE.

5. **Parameter tuning**
   - `w_anti` controls how strong the raw repulsive event is, whereas `repel_push` determines how aggressively a variable reacts to repulsive centers. Small `w_anti` but high `repel_push` exaggerates anti-interleaving; increasing `w_anti` instead raises the influence of the repulsive span on both FORCE and `totalSpan()`.

With these mechanics clarified we can implement and tune repulsion without guessing how it should flow through the pipeline.

## 7. GraphHeuristics Structure & Intuition

This is the mental model I now use when editing `src/include/souffle/problog/formula/GraphHeuristics.h`.

1. **Universe construction**  
   Only probabilistic nodes matter: input facts with `p<1` (e.g., learned evidence) and probabilistic rules/edges. `buildVariables()` sorts them deterministically by tuple/key so FORCE has a stable starting order. Deterministic predicates such as `smokes/1` or `cancer/1` never become variables; they merely contribute context when we propagate dependency sets through them.

2. **Event building**  
   - `OR` events bind multiple rule flips that derive the same head.  
   - `AND` events read the precomputed support set of a rule (its own flip plus the full dependency closure of its body) so FORCE knows everything that “travels together” when that rule fires.  
   - `BRIDGE` events keep “shared infrastructure” close (probabilistic rules that feed the same derived fact, or rules that use the same probabilistic input fact).  
   - `LiteralBlock` snapshots enable correlation analysis: every body literal yields a sorted sub-universe, and pairwise comparisons emit (a) attractive anchors/pulls for intersections and (b) repulsive anti-events on the exclusive parts. The goal is to land on layouts of the form `A_only | A∩B | B_only`.

3. **FORCE with repulsion**  
   Each iteration repeats “compute event centers → accumulate forces → stable-sort by targets”. Attractive weights pull towards the barycenter of their events, repulsive weights push away from their centers with strength `repel_push`. tie-breaking uses the stable key so we get deterministic orders even if forces cancel out. `totalSpan()` now subtracts repulsive span to keep logging meaningful.

4. **Debug/intuitive checkpoints**  
   - When `verbose` is on, log the counts of attractive vs. repulsive events per rule so we can confirm smokers-like programs actually create the expected `stress-smokes-influences` triads.  
   - For deterministic inputs (e.g., smokers where only `stress`/`influences` carry probabilities) make sure literal blocks still form, otherwise FORCE has no signal to reorganize the recursion chain.

## 8. Event & Weight Design For Deterministic-Input Benchmarks

The smokers fragment supplied in the prompt is a good stress case: all base facts are deterministic; randomness lies entirely in the `0.2::stress/1` and `0.3::influences/2` rule flips. That means:

- Every person `P` yields two probabilistic variables (one for `stress(P)`, one for the recursive `smokes` flip that fires once the proof reaches `smokes(P)`), plus a `cancer(P)` *deterministic* literal that never becomes a variable.
- Recursive rule bodies share exactly one probabilistic literal (`smokes(Y)`’s flip); the rest are unique (`influences(Y,X)` and `smokes(X)`’s flip).

To make FORCE honor this structure we should tailor events/weights as follows:

1. **Same-person clustering**  
   Emit explicit “person block” events: `{stress(P) flip, smokes(P)←stress flip}` with high weight (>= `w_and`). Optionally add a light event tying `{smokes(P)←stress flip, smokes(P)←influences flip}`—even though the second flip might not exist yet, this keeps local proofs contiguous.
   Node-level OR events now rely on the same dependency closure, so every derived predicate automatically emits an event covering all probabilistic evidence that can produce it; no extra knob is needed to mimic “person blocks”.

2. **Recursive chain control**  
   For each `(Y,X)` pair in `smokes(X) :- smokes(Y), influences(Y,X)`:
   - Core anchor: `{smokes(Y)←…}` with weight `w_corr_anchor`.
   - Side pulls: `{influences(Y,X), smokes(Y)←…}` and `{smokes(Y)←…, smokes(X)←…}` with `w_corr_pull`. This creates the desired `stress(Y)`→`smokes(Y)`→`influences(Y,X)`→`smokes(X)` ordering.
   - Anti-event: `{influences(Y,X), smokes(X)←…}` using `w_anti`. Because people often have multiple friends, these anti-events stop unrelated `influences` flips from interleaving.

3. **Bridge fine-tuning**  
   In deterministic-input settings, bridge events should have lower weight than correlation anchors; otherwise FORCE might jam together two chains that share a friend but never co-occur in the same proof. Suggested ratios:
   - `w_corr_pull >= w_and >= w_bridge`
   - `w_anti` around half of `w_corr_pull`, with `repel_push ≈ 0.5` so repulsion overcomes weak attraction but not the strong core anchors.

4. **Force adaptations**  
   - Cap `force_iters` at something modest (10–15) and check convergence by looking at span deltas. If repulsion keeps bouncing variables, inject damping by gradually reducing `repel_push` per iteration.
   - Consider computing targets using “attractive priority”: if a variable only has repulsive signals (no attractive weight), anchor it next to its deterministic key (e.g., within its person block) instead of its current index. That guarantees every variable has a stable home even when the recursion graph is sparse.

5. **BDD-aligned heuristics**  
   BDD size explodes when variables from independent subchains interleave. Therefore:
   - Events that bind more than ~5 variables should be down-weighted, otherwise FORCE will try to minimize very large spans and ignore the local pairs we care about.  
   - Track how often a variable participates in repulsive vs. attractive events; if repulsion dominates, it probably belongs near the ends of the ordering (heuristics like “place leaves at extremes” help keep branching factors low).

These guidelines are specific to deterministic-input smokers, but the same reasoning extends to other domains: emphasize events that mirror the causal chain of probabilistic effects, keep shared cores in the middle, and use repulsion to separate independent noise so downstream BDD construction sees low-entropy prefixes first.
