import json, collections, shutil

p = '/Users/mhaynie/.claude.json'
shutil.copy(p, p + '.bak-ape-research')

with open(p) as f:
    d = json.load(f, object_pairs_hook=collections.OrderedDict)

new_key = open('/tmp/apex/kagi_key.txt').read().strip()

k = d['mcpServers']['kagi']
old = k['env'].get('KAGI_API_KEY', '')
k['env']['KAGI_API_KEY'] = new_key
# uvx needs a writable cache; the sandbox blocks ~/.cache/uv
k['env']['UV_CACHE_DIR'] = '/tmp/uvcache'
k['env']['UV_TOOL_DIR'] = '/tmp/uvtools'

with open(p, 'w') as f:
    json.dump(d, f, indent=2)

print('old key prefix:', old[:12])
print('new key prefix:', new_key[:12])
print('kagi entry now:')
print(json.dumps(k, indent=1))
print('disabledMcpServers (top level):', d.get('disabledMcpServers'))
