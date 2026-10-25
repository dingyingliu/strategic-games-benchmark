"""Reproduce the numbers in the README findings from results.csv, the game logs and the reference players.

The per-game losses of the reference players come from ff_baseline with a fourth argument, for example
    ./ff_baseline depth3 20000 20261029 pergame/depth3.csv
using the seeds in reference_players.csv. Point PERGAME_DIR at the folder that holds them.

usage: PERGAME_DIR=pergame python3 analysis.py
"""
import csv, json, glob, os, random, statistics, itertools, collections

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
    line = '%s mean points lost %.3f' % (mid, m)
    for p, L in ref.items():
        n = 200000
        hit = sum(1 for _ in range(n) if sum(L[rng.randrange(len(L))] for _ in range(3)) / 3 <= m)
        line += ', %s three-game mean this low %.1f%%' % (p, 100 * hit / n)
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
