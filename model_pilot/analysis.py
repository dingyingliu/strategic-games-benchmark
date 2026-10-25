"""Reproduce the numbers in the README findings from results.csv, the game logs and the reference players.

The per-game losses of the reference players come from ff_baseline with a fourth argument, for example
    ./ff_baseline depth3 20000 20261029 pergame/depth3.csv
using the seeds in reference_players.csv. Point PERGAME_DIR at the folder that holds them.

usage: PERGAME_DIR=pergame python3 analysis.py
"""
import csv, json, glob, os, random, statistics, itertools, collections  # noqa: E401

HERE = os.path.dirname(os.path.abspath(__file__))
PERGAME = os.environ.get('PERGAME_DIR', os.path.join(HERE, 'pergame'))
rows = list(csv.DictReader(open(os.path.join(HERE, 'results.csv'))))
by = collections.defaultdict(list)
for r in rows:
    by[r['model_id']].append(r)

# choices and where the number-card losses fall
print('action choices optimal %d of %d' % (sum(int(r['action_choices_optimal']) for r in rows),
                                           sum(int(r['action_choices']) for r in rows)))
print('number choices optimal %d of %d' % (sum(int(r['number_choices_optimal']) for r in rows),
                                           sum(int(r['number_choices']) for r in rows)))
by_round = collections.Counter()
for f in glob.glob(os.path.join(HERE, 'games', 'G-*', 'log.jsonl')):
    for line in open(f):
        d = json.loads(line)
        if d['phase'] == 'number':
            by_round[d['round']] += d['regret']
total = sum(by_round.values())
print('number-card loss by round (share): ' + ', '.join('%d %.0f%%' % (k, 100 * by_round[k] / total) for k in range(1, 11)))
print('rounds 1 to 6 hold %.0f%%' % (100 * sum(by_round[k] for k in range(1, 7)) / total))

# each model's three-game mean against the reference players' three-game means
ref = {}
for p in ('uniform', 'depth1', 'depth2', 'depth3'):
    path = os.path.join(PERGAME, p + '.csv')
    if os.path.exists(path):
        ref[p] = [float(x['points_lost']) for x in csv.DictReader(open(path))]
rng = random.Random(7)
for mid, rs in sorted(by.items(), key=lambda kv: statistics.mean(float(r['points_lost']) for r in kv[1])):
    m = statistics.mean(float(r['points_lost']) for r in rs)
    k = len(rs)
    line = '%s mean points lost %.3f over %d games' % (mid, m, k)
    for p, L in ref.items():
        n = 200000
        hit = sum(1 for _ in range(n) if sum(L[rng.randrange(len(L))] for _ in range(k)) / k <= m)
        line += ', %s %d-game mean this low %.1f%%' % (p, k, 100 * hit / n)
    print(line)

# exact permutation test between each pair of models (difference in mean points lost, one-sided)
for a, b in itertools.combinations(sorted(by), 2):
    A = [float(r['points_lost']) for r in by[a]]
    B = [float(r['points_lost']) for r in by[b]]
    obs = statistics.mean(A) - statistics.mean(B)
    pool, hits, count = A + B, 0, 0
    for idx in itertools.combinations(range(len(pool)), len(A)):
        x = [pool[i] for i in idx]
        y = [pool[i] for i in range(len(pool)) if i not in idx]
        d = statistics.mean(x) - statistics.mean(y)
        count += 1
        hits += (d >= obs - 1e-12) if obs > 0 else (d <= obs + 1e-12)
    print('%s vs %s difference %.3f, one-sided permutation p %d of %d' % (a, b, obs, hits, count))

# final score difference against points lost
fs = [int(r['final_score_red']) - int(r['final_score_blue']) for r in rows]
pl = [float(r['points_lost']) for r in rows]
print('correlation of final score difference with points lost %.2f' % statistics.correlation(fs, pl))

# where the points were lost, and the action choices that were not optimal
logs = {os.path.basename(os.path.dirname(f)): [json.loads(l) for l in open(f)]
        for f in glob.glob(os.path.join(HERE, 'games', 'G-*', 'log.jsonl'))}
total = sum(d['regret'] for log in logs.values() for d in log)
act = [(g, d['round'], d['choice'], round(d['regret'], 3)) for g, log in sorted(logs.items())
       for d in log if d['phase'] == 'action' and d['regret'] > 1e-6]
print('points lost in all games %.3f, of which action choices %.3f' % (total, sum(a[3] for a in act)))
print('action choices that were not optimal: %s' % act)

# number-card patterns
opening = collections.Counter(log[0]['choice'] for log in logs.values())
print('round 1 opening cards: %s' % dict(sorted(opening.items())))
highest = []
lost_on_highest = lost_number = 0.0
for g, log in sorted(logs.items()):
    nums = [d for d in log if d['phase'] == 'number']
    if sum(d['choice'] == max(d['options']) for d in nums[:5]) >= 4:
        highest.append(g)
    lost_on_highest += sum(d['regret'] for d in nums if d['choice'] == max(d['options']))
    lost_number += sum(d['regret'] for d in nums)
print('games that played the highest card in hand in at least 4 of the first 5 rounds: %d %s' % (len(highest), highest))
print('share of number-card loss from playing the highest card in hand %.0f%%' % (100 * lost_on_highest / lost_number))
direction = collections.defaultdict(collections.Counter)
for g, log in logs.items():
    model = g.split('-')[1]
    for d in log:
        if d['phase'] != 'number' or d['regret'] < 1e-6:
            continue
        top = max(d['option_evs'])
        best = [o for o, e in zip(d['options'], d['option_evs']) if e > top - 1e-6]
        side = 'higher' if d['choice'] > max(best) else 'lower' if d['choice'] < min(best) else 'between'
        direction[model][side] += 1
for model, c in sorted(direction.items()):
    print('%s costly number choices, card higher than every best card %d, lower %d, between %d' % (
        model, c['higher'], c['lower'], c['between']))

# rewarding each choice by whether its game was won, instead of by the choice's value
margin = {r['game']: int(r['final_score_red']) - int(r['final_score_blue']) for r in rows}
best_m, costly = [], []
for g, log in logs.items():
    for d in log:
        if d['forced']:
            continue
        if d['regret'] < 1e-6:
            best_m.append(margin[g])
        else:
            costly.append((margin[g], d['regret']))
costly_m = [m for m, _ in costly]
punished = sum(m < 0 for m in best_m)
rewarded = sum(m > 0 for m in costly_m)
print('win-based reward, best moves punished %d of %d (%.0f%%), costly choices rewarded %d of %d (%.0f%%)' % (
    punished, len(best_m), 100 * punished / len(best_m), rewarded, len(costly_m), 100 * rewarded / len(costly_m)))
print('share of points lost to costly choices that came in won games %.0f%%' % (
    100 * sum(r for m, r in costly if m > 0) / sum(r for _, r in costly)))
sign = lambda m: (m > 0) - (m < 0)
p = sum(1.0 if sign(a) > sign(b) else 0.5 if sign(a) == sign(b) else 0.0
        for a in best_m for b in costly_m) / (len(best_m) * len(costly_m))
print('chance that a best move gets a higher win-based reward than a costly choice %.3f' % p)
