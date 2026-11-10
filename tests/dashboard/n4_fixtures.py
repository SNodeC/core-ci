"""Express captured index, declaration commit and publication times in the N4 model."""
from datetime import datetime
import json
from pathlib import Path

data = Path(__file__).resolve().parents[1] / 'data/dashboard'
for record in json.loads((data / 'reference/screenshots.json').read_text()):
    captured = json.loads((data / 'reference/fixtures' / (record['fixtureSha256'] + '.json')).read_text())['graph']
    path = data / 'models' / record['file'].replace('.png', '.json')
    model = json.loads(path.read_text())
    for package in model['packages']:
        source = package['sourceRepo']
        package['commit'] = captured['repositories'][source['owner'] + '/' + source['repo']]['commit']
    for unit in model['units']:
        old = next(work for work in captured['executions'].values()
                   if work['source']['package'] == unit['package'] and work['target']['id'] == unit['target'])
        unit['order'] = old['index'] + 1
        unit['assigned'] = old['active']['assigned']
        for field, value in [('publishDispatchedAt', old['active'].get('verification_started_at')),
                             ('publishedAt', old['active'].get('proof', {}).get('verified_at'))]:
            unit[field] = int(datetime.fromisoformat(value.replace('Z', '+00:00')).timestamp()) if value else None
    path.write_text(json.dumps(model, indent=2) + '\n')
