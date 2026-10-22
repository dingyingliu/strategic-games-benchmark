# Validation Report

*Fortune Fight is an original, unpublished two-player card game (rules: `rules.md`); this report validates it as a measurement instrument, using an exact solve of the complete 10-round game.*

*Tiers used throughout: **[EXACT]** = deterministic solver output. **[MC/EXACT]** = statistics sampled along fixed-seed trajectories of exact policies. **[FUTURE]** = planned model measurements.*

## 1. Exact solve and validation battery [EXACT]

| Check | Result |
|---|---|
| Complete states / LP solves | 30,380,185 / 817,651,685 |
| Runtime | 118.0 s, one 2.1 GHz core |
| Root value (symmetric game; must be 0) | 4.4 × 10⁻¹⁶ |
| Max antisymmetry violation | 1.3 × 10⁻¹² |
| Best-response self-check | 0 violations in 817,651,685 solves |
| Exact best response vs the oracle's own equilibrium | 1.3 × 10⁻¹⁴ pts/game (must be ≈ 0) |

Independent checks: a Python/SciPy implementation reproduces the stage sweep exactly; the LP battery passes matching pennies, RPS, saddle points, and 20,000 random matrices. The battery also caught a real failure — 58 degenerate LP solves in the first run (max violation 0.77 pts), eliminated by perturbation-retry with re-verification; the final solve is clean. Solver: single-file C++, regenerates the ground truth in ~2 minutes (`g++ -O3 -o simple_oracle simple_oracle.cpp && ./simple_oracle solve`).

## 2. Strategic structure [EXACT]

Immediate action game at revealed numbers (r, b), Red's score difference as utility:

| Red \ Blue | Hold | Steal | Trap |
|---|---|---|---|
| **Hold** | r − b | −r | r |
| **Steal** | b | b − r | −(r+b) |
| **Trap** | −b | r + b | 0 |

- **0 / 100** revealed-number states have a pure-strategy Nash equilibrium. In particular, no state offers a dominant action to use as a default: a dominant action would induce a pure equilibrium, and none exists.
- **64 / 100** use all three actions in equilibrium; 36 / 100 use two.
- Equilibrium deforms with the stakes: 1 vs 1 → (H,S,T) = (.50, .25, .25); 1 vs 10 → (.677, .323, 0); 10 vs 1 → (.387, 0, .613). Trap mass grows with number advantage, Steal in the mirror, Hold along the diagonal (full maps: `nash_heatmaps.png`).
- Continuations tax number advantage ~**89%**: after 10 vs 1, the exact subgame value is only **+1.03**.
- Exact opening theory is sparse: play **3 with 77.2%**, 10 with 12.7%, 2 with 10.1%, all else zero.

## 3. Planning depth pays monotonically [MC/EXACT]

3,000 equilibrium self-play games; 54,000 number and 51,808 action decisions; every priced option exact.

| Planning depth | Action regret (pts/dec) | Number regret | Loss vs equilibrium (pts/game) |
|---|---|---|---|
| 1 round | 0.461 | 0.118 | 2.73 |
| 2 rounds | 0.152 | 0.094 | 1.51 |
| 3 rounds | 0.046 | 0.060 | 1.15 |
| full | 0 | 0 | 0 |

*Retired metric:* an earlier diagnostic used agreement rates; exact analysis showed number-phase disagreement **rising** (48.4 → 52.8 → 54.9%) while regret falls — near-indifferent labels churn without value loss. Regret is the instrument.

## 4. Strategic sacrifice and shadow prices

Strict sacrifice = the full-horizon best action has lower immediate EV than every myopically best action, yet higher final value (both vs the equilibrium opponent, 10⁻⁶ tolerance). **9.3%** of non-forced action decisions overall (4,000 games); conditional cost **3.59** immediate pts for **4.39** final pts. [MC/EXACT]

```
round   1    2    3    4    5    6    7     8     9     10
SSR     ▁    ▁    ▁    ▂    ▂    ▃    ▄     ▅     █     forced
%      0.0  0.5  2.4  4.7  7.2  8.8  14.2  20.3  33.2   0
```

Mechanism [EXACT]: a Trap in hand is worth **+1 to +2 pts more than a Steal** for most of the game (+2.11 round 2, +0.86 round 6, +3.40 round 10), and ~+1.4 over a Hold early, converging to ~0 by rounds 7–8.

## 5. Flexibility: shallow play creates its own rigidity [MC/EXACT]

Probability of holding only one action type (random baseline is exact combinatorics):

| Round | Equilibrium | Myopic | Random |
|---|---|---|---|
| 8 | 5.5% | 21.1% | 5.0% |
| 9 | 30.6% | 60.4% | 26.7% |

Equilibrium preserves flexibility at the random-shuffle baseline; myopic play burns it 2–4× faster. The 4/3/3 inventory creates little forced play until the late rounds; shallow play accelerates its own depletion.

## 6. Exploitability: the skill ladder and the wedge [EXACT]

```
loss to an exact best responder (pts/game)

uniform random    ██████████████████████████▌  26.50    0% of skill recovered
depth-1 planner   ████████████████████████     23.98    9.5%
depth-2 planner   ███████████████████████      22.94   13.4%
equilibrium                                     0.00   100%
```

```
                 vs equilibrium          vs exact best response
depth-1 planner  ██▌ 2.73                ████████████████████████ 23.98    8.8×
depth-2 planner  █▌  1.51                ███████████████████████  22.94   15.2×
```

Local decision quality and unexploitability can diverge sharply: a round of extra lookahead halves the first and barely dents the second.

## 7. Decision landscape and scoring [MC/EXACT landscape; EXACT metrics]

Near-flat decisions (gap < 0.05 pts): **91.6%** number, **92.3%** action. Decisive (> 0.5): 1.2% / 7.5%. Mean equilibrium support 1.95 / 2.24. Decisiveness is earned through depletion:

```
round      1     2     3     4     5     6     7     8     9     10
act gap    ▁     ▁     ▁     ▁     ▁     ▂     ▄     ▆     █     forced
pts      0.000 0.000 0.001 0.007 0.049 0.230 0.696 1.474 1.961   0
```

**Per-decision EV regret:** `Regret(s, a) = max_a' Q(s, a') − Q(s, a)`, with Q the exact final-score-difference value. Two scored decisions per round (number, then action).

**Why distributions must also be scored.** In a mixed equilibrium every in-support action ties exactly, so sampled-action regret can certify a predictably mis-mixed policy as perfect. Hand-checkable at 10 vs 1 (matrix rows H (9, −10, 10), S (1, −9, −11), T (−1, 11, 0); value 89/31 ≈ 2.87):

- Announce **Hold always** → best reply Steal → guarantee gap **12.87**.
- Announce **50/50 Hold/Trap** (right support, wrong ratio) → worst case 0.5 → gap **2.37**.
- Announce the equilibrium mix → gap **0**. Yet sampled-action regret of the 50/50 policy is **0, forever**.

**Worst-case guarantee gap:** `Gap(p) = V(s) − min over opponent replies of Value(s, p, reply)` — zero exactly for maximin (equilibrium) mixtures, continuous, selection-free. Policy-scale confirmation: the depth-1 planner loses 2.73 pts/game to the equilibrium opponent but 23.98 to an exact best responder.

**Item design:** flat states catch out-of-support blunders (via regret) and predictable mis-mixing (via guarantee gap); decisive states (rounds 6–9 bank) rank mid-skill via sampled-action regret; distribution scoring separates the top. The evaluation battery — in the psychometric sense: a fixed set of test items administered identically to every subject — is oracle-labeled; the same states serve paired text and image conditions for perception-tax measurement.

## 8. Planned next steps [FUTURE]

Frontier-model measurement on the fixed battery; the distribution-elicitation pilot (both scoring modes, agreement reported); the paired text/vision factorial; instance-family scaling; the community game-admission pipeline using this same battery.

| Instance family | States | Solve cost |
|---|---|---|
| 10/10/4-3-3 (canonical) | 3.04 × 10⁷ | ~2 min, one core |
| 12/12/5-4-3 | 7.45 × 10⁸ | ~1.1 core-hours |
| 14/14/6-4-4 | 1.83 × 10¹⁰ | ~37 core-hours |

## 9. Limitations

- [MC/EXACT] statistics are trajectory-conditional on equilibrium play; model play drifts off-equilibrium, where gaps widen.
- Equilibrium values are unique; mixtures are not. Values, gaps, and exploitability are selection-free; support statistics reflect the LP's selection, documented per item.
- The objective is expected score difference, not win probability (tie rate 2.0%, reported for completeness).
- Depth tags use the max-probability pick convention; alternatives shift only near-indifferent items.
- Guarantee-gap scoring partially measures distribution articulation; the elicitation pilot compares both scoring modes before either is used alone.
- All sampling uses fixed seeds; reruns are bit-reproducible.

---

*Beside this report in the repository are `README.md`, `rules.md`, `sample_battery.csv` (500 oracle-labeled items, with the full battery held out), `example_episode.json`, `simple_oracle.cpp`, `validation_summary.csv`, `horizon_results.csv`, `stage_nash_100_states.csv` and `nash_heatmaps.png`.*
