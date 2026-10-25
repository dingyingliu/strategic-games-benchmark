"""Build results.csv (one row per game) from the game logs and agent records in games/,
and print the results table and the per-model summary used in README.md.

usage: python3 make_results.py
"""
import json, os, csv, glob, statistics, sys

HERE = os.path.dirname(os.path.abspath(__file__))
GAMES = os.path.join(HERE, 'games')
os.environ['FF_GAMES_DIR'] = GAMES
sys.path.insert(0, HERE)
from score_games import summarize, load  # noqa: E402

NAMES = {'claude-opus-5-5': 'Opus 5.5', 'claude-fable-5-1': 'Fable 5.1'}

rows = []
for d in sorted(glob.glob(os.path.join(GAMES, 'G-*'))):
    game = os.path.basename(d)
    s = summarize(game)
    if not s.get('complete'):
        continue
    rec = json.load(open(os.path.join(d, 'agent_record.json')))
    real = [x for x in load(game) if not x['forced']]
    num = [x for x in real if x['phase'] == 'number']
    act = [x for x in real if x['phase'] == 'action']
    ok = lambda xs: sum(1 for x in xs if x['regret'] < 1e-6)
    rows.append({
        'game': game,
        'model_id': '+'.join(rec['model_ids_reported']),
        'seed': open(os.path.join(d, 'state.txt')).read().split()[10],
        'points_lost': s['points_lost_vs_perfect_play'],
        'number_phase_points_lost': s['number_phase_points_lost'],
        'action_phase_points_lost': s['action_phase_points_lost'],
        'choices_made': len(real),
        'optimal_choices': ok(real),
        'number_choices': len(num), 'number_choices_optimal': ok(num),
        'action_choices': len(act), 'action_choices_optimal': ok(act),
        'mixed_states': s['mixed_states'], 'mixed_states_played_in_support': s['mixed_states_played_in_support'],
        'final_score_red': s['final_score'][0], 'final_score_blue': s['final_score'][1],
        'only_play_commands_for_this_game': rec['only_play_commands_for_this_game'],
        'instructions_identical_to_agent_prompt': rec['instructions_identical_to_agent_prompt'],
        'output_tokens': rec['usage_tokens'].get('output_tokens'),
        'minutes': rec['minutes'],
        'largest_mistakes': json.dumps(s.get('largest_mistakes', [])),
    })

with open(os.path.join(HERE, 'results.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]))
    w.writeheader()
    w.writerows(rows)

print('| Game | Model | Seed | Points lost | Number phase | Action phase | Optimal choices | Final score | Only the play command | Output tokens | Minutes |')
print('|---|---|---|---|---|---|---|---|---|---|---|')
for r in rows:
    print('| %s | %s | %s | %.3f | %.3f | %.3f | %d of %d | %d to %d | %s | %s | %s |' % (
        r['game'], r['model_id'], r['seed'], r['points_lost'], r['number_phase_points_lost'],
        r['action_phase_points_lost'], r['optimal_choices'], r['choices_made'], r['final_score_red'],
        r['final_score_blue'], 'yes' if r['only_play_commands_for_this_game'] else 'no',
        '{:,}'.format(r['output_tokens']), r['minutes']))
print()
for mid in sorted({r['model_id'] for r in rows}):
    rs = [r for r in rows if r['model_id'] == mid]
    lost = [r['points_lost'] for r in rs]
    print('%s (%s), %d game%s, points lost per game %s, mean %.3f, number phase %.3f, action phase %.3f, '
          'number choices optimal %d of %d, action choices optimal %d of %d' % (
              NAMES.get(mid, mid), mid, len(rs), '' if len(rs) == 1 else 's', lost, statistics.mean(lost),
              sum(r['number_phase_points_lost'] for r in rs), sum(r['action_phase_points_lost'] for r in rs),
              sum(r['number_choices_optimal'] for r in rs), sum(r['number_choices'] for r in rs),
              sum(r['action_choices_optimal'] for r in rs), sum(r['action_choices'] for r in rs)))
