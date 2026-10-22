<div align="center">

<h1>Strategic Games Benchmark</h1>

**Fortune Fight** · *an original card game with an exact game-theoretic answer for every decision*

[![License MIT](https://img.shields.io/badge/License-MIT-blue)](LICENSE)
[![C++ single file](https://img.shields.io/badge/C%2B%2B-single%20file-blue)](simple_oracle.cpp)
[![Built with Claude Code](https://img.shields.io/badge/Built%20with-Claude%20Code-D97757)](#how-it-was-built)

[Rules](rules.md) • [Validation report](validation_report.md) • [Sample items](sample_battery.csv) • [Kaggle](https://www.kaggle.com/datasets/dingyingliu/novel-strategic-games-benchmark-public-preview) • [Citation](CITATION.cff)

</div>

## Overview

Good play in a strategic game depends on the opponent, and often the best play is a mix of moves rather than one right move. That makes it hard to check whether a model reasons well in a game. The Strategic Games Benchmark uses original games that can be solved exactly, so every decision a model makes can be scored against the game-theoretic answer.

The first game is **Fortune Fight**, a two-player card game designed by Dingying Liu. Over ten rounds, both players secretly pick a number card and reveal it, then secretly pick an action card (Hold, Steal or Trap) and reveal that too. Steal beats Hold, Trap beats Steal and Hold beats Trap, and the points at stake depend on the two numbers. Each card can be played only once, so every choice also changes what a player can do later. The full rules are in [rules.md](rules.md).

## Exact ground truth

Each round is a small zero-sum game, and a match is a sequence of them. A single C++ file, [simple_oracle.cpp](simple_oracle.cpp), solves the complete 10-round game. It works backward from the last round and solves a small linear program at every decision point, which gives the equilibrium strategy and the exact value of every option across all 30,380,185 states. The full solve takes about two minutes on one core.

The solve passes its own checks. The game is symmetric, so its value must be zero, and the solver returns 4.4 × 10⁻¹⁶. None of the 817,651,685 linear programs fails the best-response check. The [validation report](validation_report.md) has the details.

## What the solve shows

- **Often there is no single right move.** In the single-round action game, none of the 100 pairs of revealed numbers has a pure equilibrium. The best play is a mix of moves, and the mix shifts with the numbers at stake ([heatmaps](nash_heatmaps.png)).
- **Planning ahead pays.** Against equilibrium play, a player who plans over one round loses 2.73 points per game, over two rounds 1.51, and over three rounds 1.15.
- **Some of the best moves give up points now.** In 9.3% of action decisions that are not forced, the best move scores less in that round than the move that is best for the round alone, yet leads to a better final score. On average it gives up 3.59 points in the round and gains 4.39 by the end.
- **Doing well against equilibrium play is not the same as being hard to beat.** The player who plans over one round loses only 2.73 points per game against equilibrium play, but 23.98 against an opponent who exploits it exactly.

## Scoring

Each item is a game state with the exact value of every option. A model's move is scored by its **regret**, the points it gives up compared with the best option. When the best play is a mix, every move in the mix has zero regret, so a model that always picks the same one looks perfect even though an opponent could exploit it. For this case the report defines a second score, the **guarantee gap**, which measures how many points a stated mix gives up against an opponent who knows it.

## Files

| File | What it is |
|---|---|
| [rules.md](rules.md) | Full rules of Fortune Fight |
| [simple_oracle.cpp](simple_oracle.cpp) | Exact solver and validation checks |
| [validation_report.md](validation_report.md) | How the solve was checked and what it shows |
| [validation_summary.csv](validation_summary.csv) | Headline numbers from the solve |
| [horizon_results.csv](horizon_results.csv) | Results for players who plan over one, two or three rounds, and for random play |
| [stage_nash_100_states.csv](stage_nash_100_states.csv) | Equilibrium mix of Hold, Steal and Trap for all 100 number pairs |
| [sample_battery.csv](sample_battery.csv) | 500 public test items with exact labels |
| [example_episode.json](example_episode.json) | One full game with both players following the equilibrium |
| [nash_heatmaps.png](nash_heatmaps.png) | Heatmaps of the equilibrium mix |

The 500 items in `sample_battery.csv` are a public sample. The full battery is held out so that it stays out of model training data.

## Rebuild the results

```bash
g++ -O3 -o simple_oracle simple_oracle.cpp
./simple_oracle test      # unit tests for the linear-program solver
./simple_oracle solve     # full solve and checks, about two minutes
./simple_oracle vd 1      # one-round planner, needed for the sample items
./simple_oracle battery   # rebuilds sample_battery.csv
./simple_oracle episode   # rebuilds example_episode.json
```

The solver writes value tables of about 243 MB each to the working folder. Sampling uses fixed seeds, so a rerun gives the same items. The other commands in the file (stage, hr, depth, sims, brU, brM and brE) print the numbers behind the remaining tables in the report.

## Citation and licence

Please cite this repository with [CITATION.cff](CITATION.cff). The code and data are released under the [MIT License](LICENSE).
