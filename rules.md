# Fortune Fight — Rules

Fortune Fight is a two-player strategy game played over 10 rounds.

## Game Components

**20 Number Cards** — two identical sets numbered 1–10.

**20 Action Cards** — two identical sets, each containing:
- 4 Hold
- 3 Steal
- 3 Trap

## Objective

The player with the higher total score after 10 rounds wins.

## Setup

Each player receives:
- one set of Number Cards numbered 1–10
- 4 Hold cards, 3 Steal cards, and 3 Trap cards

Each card can be played only once. Because each player has exactly ten Number Cards and ten Action Cards for ten rounds, **every card will be played exactly once by the end of the game** — when and against what is the entire question.

## Gameplay

The game consists of 10 rounds. Each round has two phases.

**1. Number Phase.** Each player secretly chooses one of their remaining Number Cards. Both players reveal their chosen numbers simultaneously.

**2. Action Phase.** After both numbers have been revealed, each player secretly chooses one of their remaining Action Cards. Both Action Cards are then revealed simultaneously, and points are awarded according to the interaction between the two actions.

The Number Card and Action Card used by each player are removed from play.

## Action Cards

- **Hold** — attempts to secure your own revealed number.
- **Steal** — attempts to take the opponent's revealed number.
- **Trap** — punishes an opponent who attempts to Steal. If Trap catches Steal, the Trap player scores both revealed numbers.

## Action Outcomes

Let Red's revealed number be **r** and Blue's revealed number be **b**. The table below is the complete and authoritative resolution rule; the card descriptions above are summaries of it.

| Red Action | Blue Action | Red Scores | Blue Scores |
|---|---|---|---|
| Hold | Hold | r | b |
| Hold | Steal | 0 | r |
| Hold | Trap | r | 0 |
| Steal | Hold | b | 0 |
| Steal | Steal | b | r |
| Steal | Trap | 0 | r + b |
| Trap | Hold | 0 | b |
| Trap | Steal | r + b | 0 |
| Trap | Trap | 0 | 0 |

In short: **Steal beats Hold. Trap beats Steal. Hold beats Trap.** However, the value of each outcome depends on the two numbers that have been revealed.

## Example

Suppose Red reveals 1 and Blue reveals 5.

- Hold vs. Hold: Red scores 1 and Blue scores 5.
- Hold vs. Steal: Red scores 0 and Blue scores 1.
- Trap vs. Steal: Red scores 1 + 5 = 6 and Blue scores 0.
- Trap vs. Trap: both players score 0.

## Public Information

At any point in the game, both players may see:
- the current score
- all previously played Number Cards and Action Cards
- which Number Cards remain for each player
- how many Hold, Steal, and Trap cards remain for each player

The only hidden information is the choice each player is currently making before the simultaneous reveal.

## Winning the Game

After the tenth round, add each player's points across all rounds. The player with the higher total score wins. If both players have the same score, the game ends in a draw.

*Variant: Fortune Fight can also be played by two teams, each side deciding its moves together.*
