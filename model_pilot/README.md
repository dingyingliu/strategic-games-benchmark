# Model pilot

Claude models play complete games of Fortune Fight as Red against Blue, which plays the exact equilibrium from [simple_oracle.cpp](../simple_oracle.cpp). The solver prices every decision the models make, so each game gives an exact count of the points their play gave up. This folder holds the game program, the instructions the models received, every logged decision and the scores.

The models are Fable 5.1, Opus 5.5, Sonnet 5.5 and Haiku 5.5, with three games each on the same three seeds, twelve games in all. The results table gives the exact model ID that each agent's transcript reports.

## How a game is run

- Each game is played by a new Claude Code agent. It does not see the organizer's conversation or any other game. It runs with the account's standard agent setup, and its only instructions about the game are those in [agent_prompt.txt](agent_prompt.txt), the same for every game apart from the game name.
- The agent plays by running one command, [play](play), which calls the game program [ff_engine.cpp](ff_engine.cpp). The program shows only what a player can see, which is the score, the cards each side has left and the cards played so far. Blue draws every choice from the exact equilibrium of the full game.
- Before Blue's choice is shown, the program writes Red's decision to a log the agent never sees, with the exact value of every option against Blue's equilibrium mix. A move with only one legal option is played automatically and scores zero.
- Both models play the same seeds. At each decision Blue uses the same random draw in both models' games, so Blue makes the same choice whenever the two games are in the same position.
- After each game, [record_agent.py](record_agent.py) reads the agent's transcript and saves a record in the game's folder with the exact model ID reported for every reply, every command the agent ran and its final reply. The transcript itself stays private because it also holds account details.

## Scoring

- The regret of a decision is the value of the best option minus the value of the option played, both against Blue's equilibrium mix. The values come from the exact solve and are in points of final score difference.
- Points lost in a game is the sum of the regrets of Red's decisions. The game's value is zero, so the expected points lost equals the player's expected loss per game against the equilibrium. This is the unit of the planner results in [horizon_results.csv](../horizon_results.csv).
- Final scores are mostly the luck of the draw. With perfect play on both sides, the final score difference has a standard deviation of 21.4 points per game. Points lost removes that luck. For the three-round planner below its standard deviation is 1.09 points per game, so one game scored by points lost measures that player about as precisely as 390 games scored by final score.
- Points lost has the same blind spot as regret on single items. When the best play is a mix, every move in the mix has zero regret, so a player who always picks the same move from the mix loses nothing to the equilibrium opponent, even though an opponent who knew this could exploit it. The guarantee gap in the [validation report](../validation_report.md) measures that. It needs the player's full mix, which these games do not ask for.

## Reference players

These fixed players are scored in the same way, 20,000 games each against the same equilibrium Blue, by [ff_baseline.cpp](ff_baseline.cpp). Uniform random picks evenly among the options a model is offered. The planners are those of [horizon_results.csv](../horizon_results.csv), and each samples the equilibrium of the game cut off after one, two or three rounds. The last column checks the scoring against the final scores of the same games.

| Player | Points lost per game | Standard error | Number phase | Action phase | Games with no action-phase loss | Loss from final scores |
|---|---|---|---|---|---|---|
| Uniform random | 5.02 | 0.04 | 1.19 | 3.83 | 37% | 4.93 ± 0.15 |
| One-round planner | 2.50 | 0.02 | 0.82 | 1.67 | 64% | 2.62 ± 0.15 |
| Two-round planner | 1.39 | 0.01 | 0.82 | 0.56 | 81% | 1.67 ± 0.15 |
| Three-round planner | 0.83 | 0.01 | 0.63 | 0.21 | 89% | 0.75 ± 0.15 |
| Equilibrium | 0 | 0 | 0 | 0 | 100% | 0.07 ± 0.15 |

The planner losses in horizon_results.csv (2.73, 1.51 and 1.15) come from the final scores of 3,000 games, which carry a standard error of about 0.39 points, and they agree with these within that error. [reference_players.csv](reference_players.csv) has the full results, including the spread of the loss per game.

## Results

<!-- results start -->
| Game | Model | Seed | Points lost | Number phase | Action phase | Optimal choices | Final score | Only the play command | Output tokens | Minutes |
|---|---|---|---|---|---|---|---|---|---|---|
| G-fable51-1 | claude-fable-5-1 | 2026 | 0.177 | 0.177 | 0.000 | 16 of 18 | 38 to 30 | yes | 179,582 | 38.8 |
| G-fable51-2 | claude-fable-5-1 | 2027 | 0.376 | 0.376 | 0.000 | 13 of 16 | 38 to 40 | yes | 255,344 | 53.5 |
| G-fable51-3 | claude-fable-5-1 | 2028 | 0.254 | 0.254 | 0.000 | 14 of 18 | 35 to 30 | yes | 192,297 | 44.0 |
| G-fable51-4 | claude-fable-5-1 | 2029 | 0.647 | 0.647 | 0.000 | 13 of 17 | 29 to 54 | yes | 225,402 | 53.0 |
| G-fable51-5 | claude-fable-5-1 | 2030 | 1.348 | 1.348 | 0.000 | 12 of 18 | 28 to 38 | yes | 221,969 | 49.8 |
| G-opus55-1 | claude-opus-5-5 | 2026 | 0.750 | 0.750 | 0.000 | 13 of 18 | 32 to 30 | yes | 345,491 | 47.0 |
| G-opus55-2 | claude-opus-5-5 | 2027 | 1.518 | 1.518 | 0.000 | 13 of 18 | 49 to 27 | yes | 336,105 | 47.1 |
| G-opus55-3 | claude-opus-5-5 | 2028 | 0.487 | 0.487 | 0.000 | 13 of 18 | 42 to 41 | yes | 255,527 | 36.9 |
| G-opus55-4 | claude-opus-5-5 | 2029 | 0.969 | 0.969 | 0.000 | 12 of 16 | 39 to 24 | yes | 262,723 | 36.4 |
| G-opus55-5 | claude-opus-5-5 | 2030 | 0.836 | 0.836 | 0.000 | 12 of 18 | 28 to 40 | yes | 317,578 | 43.0 |
| G-sonnet55-1 | claude-sonnet-5-5 | 2026 | 0.183 | 0.183 | 0.000 | 13 of 18 | 46 to 49 | yes | 344,574 | 39.7 |
| G-sonnet55-2 | claude-sonnet-5-5 | 2027 | 0.151 | 0.151 | 0.000 | 15 of 18 | 43 to 27 | yes | 583,792 | 66.0 |
| G-sonnet55-3 | claude-sonnet-5-5 | 2028 | 1.294 | 1.294 | 0.000 | 10 of 18 | 24 to 33 | yes | 364,962 | 42.6 |
| G-sonnet55-4 | claude-sonnet-5-5 | 2029 | 0.236 | 0.236 | 0.000 | 13 of 16 | 52 to 38 | yes | 531,576 | 61.8 |
| G-haiku55-1 | claude-haiku-5-5 | 2026 | 1.957 | 1.957 | 0.000 | 13 of 18 | 58 to 22 | yes | 305,345 | 21.6 |
| G-haiku55-2 | claude-haiku-5-5 | 2027 | 0.290 | 0.290 | 0.000 | 13 of 18 | 39 to 28 | yes | 179,290 | 13.6 |
| G-haiku55-3 | claude-haiku-5-5 | 2028 | 1.510 | 1.510 | 0.000 | 11 of 18 | 31 to 47 | yes | 276,415 | 19.3 |
| G-haiku55-4 | claude-haiku-5-5 | 2029 | 1.358 | 1.358 | 0.000 | 15 of 18 | 17 to 37 | yes | 232,489 | 17.5 |
| G-haiku55-5 | claude-haiku-5-5 | 2030 | 0.894 | 0.894 | 0.000 | 11 of 18 | 17 to 38 | yes | 190,922 | 13.5 |

| Model | Games | Points lost in each game | Mean points lost per game | Number phase | Action phase | Number choices optimal | Action choices optimal |
|---|---|---|---|---|---|---|---|
| Fable 5.1 | 5 | 0.177, 0.376, 0.254, 0.647, 1.348 | 0.560 | 0.560 | 0.000 | 26 of 45 | 42 of 42 |
| Opus 5.5 | 5 | 0.750, 1.518, 0.487, 0.969, 0.836 | 0.912 | 0.912 | 0.000 | 20 of 45 | 43 of 43 |
| Sonnet 5.5 | 4 | 0.183, 0.151, 1.294, 0.236 | 0.466 | 0.466 | 0.000 | 17 of 36 | 34 of 34 |
| Haiku 5.5 | 5 | 1.957, 0.290, 1.510, 1.358, 0.894 | 1.202 | 1.202 | 0.000 | 18 of 45 | 45 of 45 |
<!-- results end -->

In all twelve games the agent ran only the play command for its own game.

- **Every action choice was optimal.** All 106 action choices across the four models had zero regret. All of the points lost came from the number cards, meaning which card to play in which round. 51 of the 108 number choices were optimal.
- **The number-card losses sit in the middle of the game.** Rounds 1 to 6 hold 76% of them, with the peak in rounds 5 and 6. Several agents said they solved the game exactly from round 7 on, once few enough cards were left to work backward by hand.
- **Fable 5.1 lost the least**, 0.27 points per game. A three-round planner's average over three games is that low only 3% of the time. Sonnet 5.5 (0.54), Opus 5.5 (0.92) and Haiku 5.5 (1.25) fall within the range of the planners. Each Fable game lost less than each Opus game, which would happen 1 time in 20 if the two played equally well. The other gaps between models are within what three games can show.
- **Final scores say little about play.** Across the twelve games, a bigger win did not go with fewer points lost (correlation 0.24). Haiku's 36-point win was its worst-played game.
- **The agents did not see where they lost points.** None of the twelve final replies names a number-card choice as a mistake. Two blame the score on action choices that the solver scores as optimal.

Three games per model are enough to show these patterns, not to rank the models closely.

[results.csv](results.csv) has one row per game, including the largest mistakes. Each folder in [games](games) holds the game's log (every Red decision with the value of every option, both equilibrium mixes, the choice, its regret and Blue's choice), its final state and the agent record.

## Files

| File | What it is |
|---|---|
| [agent_prompt.txt](agent_prompt.txt) | Exact instructions given to every agent, with GAMEID in place of the game name |
| [ff_engine.cpp](ff_engine.cpp) | Game program the agents play through, which includes simple_oracle.cpp unchanged |
| [play](play) | The one command the agents run |
| [score_games.py](score_games.py) | Scores a game from its log |
| [record_agent.py](record_agent.py) | Writes the audit record of a game from the agent's transcript |
| [make_results.py](make_results.py) | Builds results.csv and the results table |
| [analysis.py](analysis.py) | Reproduces the numbers in the findings |
| [ff_baseline.cpp](ff_baseline.cpp) | Reference players, scored the same way |
| [reference_players.csv](reference_players.csv) | Reference player results |
| [results.csv](results.csv) | One row per finished game |
| [games](games) | Logs and agent records, one folder per game |

## Rerun

```bash
g++ -O3 -o simple_oracle ../simple_oracle.cpp
./simple_oracle solve                          # writes simpleV.bin, about two minutes
./simple_oracle vd 1 && ./simple_oracle vd 2   # tables for the two- and three-round planners
g++ -O3 -o ff_engine ff_engine.cpp
g++ -O3 -o ff_baseline ff_baseline.cpp
./ff_engine new G-example 2026 opus            # sets up a game
./ff_baseline depth3 20000 20261029            # one reference player
mkdir -p pergame && ./ff_baseline depth3 20000 20261029 pergame/depth3.csv   # also writes the loss of every game
python3 make_results.py                        # results from games
PERGAME_DIR=pergame python3 analysis.py        # numbers in the findings
```

The programs use absolute paths from the machine the pilot ran on, so change the paths in ff_engine.cpp, ff_baseline.cpp and play before rebuilding.
