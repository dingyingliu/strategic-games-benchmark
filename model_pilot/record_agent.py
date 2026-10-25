"""Write the audit record of one pilot game from the agent's transcript.

Run by the organizer after a game ends. The transcript itself stays private because it also holds
account details. The record keeps what is needed to check the run: the exact model ID the transcript
reports for every reply, every command the agent ran, any other tool it used, whether its
instructions match agent_prompt.txt exactly, its final reply, token usage and timing.

usage: python3 record_agent.py TRANSCRIPT.jsonl GAME OUT.json
"""
import json, re, sys, os
from collections import Counter
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
# how an agent hands its final reply back to the organizer, not a tool for playing
HANDBACK = 'SubagentHandback'


def record(path, game):
    template = open(os.path.join(HERE, 'agent_prompt.txt')).read()
    models, tools, commands, usage, seen = Counter(), Counter(), [], Counter(), set()
    first_user, final_reply, times = None, None, []
    for line in open(path):
        try:
            m = json.loads(line)
        except ValueError:
            continue
        if m.get('timestamp'):
            times.append(m['timestamp'])
        msg = m.get('message') or {}
        if m.get('type') == 'user' and first_user is None:
            c = msg.get('content')
            if isinstance(c, list):
                c = ''.join(x.get('text', '') for x in c if isinstance(x, dict))
            first_user = c
        if m.get('type') != 'assistant':
            continue
        if msg.get('model'):
            models[msg['model']] += 1
        for part in msg.get('content') or []:
            if not (isinstance(part, dict) and part.get('type') == 'tool_use'):
                continue
            if part['name'] == HANDBACK:
                final_reply = part['input'].get('message')
                continue
            tools[part['name']] += 1
            if part['name'] == 'Bash':
                commands.append(part['input'].get('command', ''))
        mid = msg.get('id')
        if mid and mid not in seen and msg.get('usage'):
            seen.add(mid)
            for k in ('input_tokens', 'cache_creation_input_tokens', 'cache_read_input_tokens', 'output_tokens'):
                usage[k] += msg['usage'].get(k) or 0
    play = re.compile(r'^/home/claude/ffpilot/play (state|move) ' + re.escape(game) + r'( \S+)?$')
    other = [c for c in commands if not play.match(c.strip())]
    t = sorted(times)
    to_dt = lambda s: datetime.fromisoformat(s.replace('Z', '+00:00'))
    return {
        'game': game,
        'model_ids_reported': dict(models),
        'instructions_identical_to_agent_prompt': first_user == template.replace('GAMEID', game),
        'commands': commands,
        'play_commands': len(commands) - len(other),
        'other_commands': other,
        'tools_used': dict(tools),
        'only_play_commands_for_this_game': not other and set(tools) <= {'Bash'},
        'final_reply': final_reply,
        'usage_tokens': dict(usage),
        'start_utc': t[0] if t else None,
        'end_utc': t[-1] if t else None,
        'minutes': round((to_dt(t[-1]) - to_dt(t[0])).total_seconds() / 60, 1) if t else None,
    }


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    rec = record(sys.argv[1], sys.argv[2])
    with open(sys.argv[3], 'w') as f:
        json.dump(rec, f, indent=1, ensure_ascii=False)
        f.write('\n')
    print(json.dumps({k: rec[k] for k in ('game', 'model_ids_reported', 'instructions_identical_to_agent_prompt',
                                          'play_commands', 'other_commands', 'tools_used', 'minutes')}))
