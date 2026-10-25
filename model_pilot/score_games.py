"""Score Fortune Fight pilot games from the private move logs.

For every Red decision the log holds the exact value of each option (against Blue's equilibrium play,
from Dingying Liu's solver) and the regret of the move played: best option value minus the value of
the move played. Summed over a game, regret is the expected number of points the player gave up
against perfect play. Forced moves have zero regret and are counted separately.
"""
import json, sys, glob, os

# folder with one subfolder of logs per game; FF_GAMES_DIR=model_pilot/games scores the games in the repo
PRIV = os.environ.get('FF_GAMES_DIR', '/home/claude/ffpilot/private')

def load(game):
    path = os.path.join(PRIV, game, 'log.jsonl')
    return [json.loads(l) for l in open(path)] if os.path.exists(path) else []

def summarize(game):
    log = load(game)
    real = [d for d in log if not d['forced']]
    out = {'game': game, 'model': log[0]['model'] if log else None, 'decisions_logged': len(log),
           'choices_made': len(real), 'complete': len(log) == 20}
    if not real:
        return out
    reg = [d['regret'] for d in real]
    zero = [r < 1e-6 for r in reg]
    out.update({
        'points_lost_vs_perfect_play': round(sum(reg), 3),
        'share_zero_regret': round(sum(zero) / len(reg), 3),
        'number_phase_points_lost': round(sum(d['regret'] for d in real if d['phase'] == 'number'), 3),
        'action_phase_points_lost': round(sum(d['regret'] for d in real if d['phase'] == 'action'), 3),
    })
    # mixed-equilibrium decisions: more than one option has positive equilibrium weight
    mixed = [d for d in real if sum(1 for p in d['red_eq_mix'] if p > 1e-9) > 1]
    out['mixed_states'] = len(mixed)
    out['mixed_states_played_in_support'] = sum(
        1 for d in mixed if d['red_eq_mix'][d['options'].index(d['choice'])] > 1e-9)
    worst = sorted(real, key=lambda d: -d['regret'])[:3]
    out['largest_mistakes'] = [
        {'round': d['round'], 'phase': d['phase'], 'played': d['choice'],
         'best': d['options'][max(range(len(d['options'])), key=lambda i: d['option_evs'][i])],
         'regret': round(d['regret'], 3), 'revealed': d.get('revealed')} for d in worst if d['regret'] > 1e-6]
    last = log[-1]
    if last['phase'] == 'action' and len(log) == 20:
        out['final_score'] = [last['score_before'][0] + last['points'][0], last['score_before'][1] + last['points'][1]]
    return out

if __name__ == '__main__':
    games = sys.argv[1:] or sorted(os.path.basename(p) for p in glob.glob(PRIV + '/G-*'))
    for g in games:
        print(json.dumps(summarize(g)))
