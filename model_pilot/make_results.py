"""Build results.csv (one row per game) from the game logs and agent records in games/,
and write the two results tables into README.md between the results markers.

usage: python3 make_results.py
"""
import json, os, csv, glob, statistics, sys, re

HERE = os.path.dirname(os.path.abspath(__file__))
GAMES = os.path.join(HERE, 'games')
os.environ['FF_GAMES_DIR'] = GAMES
sys.path.insert(0, HERE)
from score_games import summarize, load  # noqa: E402

# display names, in the order the models are listed
NAMES = {'claude-fable-5-1': 'Fable 5.1', 'claude-opus-5-5': 'Opus 5.5',
         'claude-sonnet-5-5': 'Sonnet 5.5', 'claude-haiku-5-5': 'Haiku 5.5'}
order = lambda mid: list(NAMES).index(mid) if mid in NAMES else len(NAMES)


def interruption_text(i):
    """Short text for the interruptions in an agent record (see record_agent.py)."""
    parts = []
    if i.get('reply_over_output_cap_then_resumed'):
        parts.append('cap %d' % i['reply_over_output_cap_then_resumed'])
    if i.get('context_compressed'):
        parts.append('compressed %d' % i['context_compressed'])
    stopped = sum(v for k, v in i.items() if k.startswith('agent_stopped_by_error'))
    if stopped:
        parts.append('stopped %d' % stopped)
    if i.get('organizer_message'):
        parts.append('organizer %d' % i['organizer_message'])
    return ', '.join(parts) or 'none'

rows = []
for d in glob.glob(os.path.join(GAMES, 'G-*')):
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
        'interruptions': interruption_text(rec.get('interruptions', {})),
        'largest_mistakes': json.dumps(s.get('largest_mistakes', [])),
    })
rows.sort(key=lambda r: (order(r['model_id']), r['game']))

with open(os.path.join(HERE, 'results.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]))
    w.writeheader()
    w.writerows(rows)

out = ['| Game | Model | Seed | Points lost | Number phase | Action phase | Optimal choices | Final score | Only the play command | Interruptions | Output tokens | Minutes |',
       '|---|---|---|---|---|---|---|---|---|---|---|---|']
for r in rows:
    out.append('| %s | %s | %s | %.3f | %.3f | %.3f | %d of %d | %d to %d | %s | %s | %s | %s |' % (
        r['game'], r['model_id'], r['seed'], r['points_lost'], r['number_phase_points_lost'],
        r['action_phase_points_lost'], r['optimal_choices'], r['choices_made'], r['final_score_red'],
        r['final_score_blue'], 'yes' if r['only_play_commands_for_this_game'] else 'no',
        r['interruptions'], '{:,}'.format(r['output_tokens']), r['minutes']))
out += ['', '| Model | Games | Points lost in each game | Mean points lost per game | Number phase | Action phase | Number choices optimal | Action choices optimal |',
        '|---|---|---|---|---|---|---|---|']
for mid in sorted({r['model_id'] for r in rows}, key=order):
    rs = [r for r in rows if r['model_id'] == mid]
    out.append('| %s | %d | %s | %.3f | %.3f | %.3f | %d of %d | %d of %d |' % (
        NAMES.get(mid, mid), len(rs), ', '.join('%.3f' % r['points_lost'] for r in rs),
        statistics.mean(r['points_lost'] for r in rs),
        statistics.mean(r['number_phase_points_lost'] for r in rs),
        statistics.mean(r['action_phase_points_lost'] for r in rs),
        sum(r['number_choices_optimal'] for r in rs), sum(r['number_choices'] for r in rs),
        sum(r['action_choices_optimal'] for r in rs), sum(r['action_choices'] for r in rs)))
table = '\n'.join(out)
print(table)

readme = os.path.join(HERE, 'README.md')
text = open(readme).read()
start, end = '<!-- results start -->', '<!-- results end -->'
if start in text and end in text:
    text = re.sub(re.escape(start) + '.*?' + re.escape(end), start + '\n' + table + '\n' + end, text, flags=re.S)
    open(readme, 'w').write(text)
    print('README tables updated')
