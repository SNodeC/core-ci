"""G4 asset accounting and G5 exact pixel comparison against the selected capture."""
import difflib
import json
from pathlib import Path
import re
import subprocess
import sys
import numpy as np
from PIL import Image

root = Path(__file__).resolve().parents[2]
data = root / 'tests/data/dashboard'
output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)
mode = sys.argv[2]
if mode == 'assets':
    changes = []
    for name in ('index.html', 'style.css', 'app.js'):
        before = (data / 'reference/assets' / name).read_text()
        after = (root / 'app/dashboard' / name).read_text()
        if name != 'app.js':
            assert before == after
        else:
            def functions(text):
                matches = list(re.finditer(r'^(?:async )?function (\w+)\(', text, re.M))
                result = {match[1]: text[match.start():matches[i+1].start()] for i, match in enumerate(matches[:-1])}
                return result
            old, new = functions(before), functions(after)
            for name2, body in old.items():
                if name2 not in ('api', 'connect'):
                    assert body == new[name2], 'drawing function changed: ' + name2
        changes.extend(difflib.unified_diff(before.splitlines(keepends=True), after.splitlines(keepends=True),
                                          fromfile='captured/' + name, tofile='app/dashboard/' + name))
    (output / 'G4-assets.diff').write_text(''.join(changes))
    print('G4: index.html and style.css byte-identical; drawing functions unchanged; full app.js data-layer diff saved')
else:
    subprocess.run(['node', str(root / 'tests/dashboard/browser.cjs'), str(output), 'references'], check=True)
    records = json.loads((data / 'reference/screenshots.json').read_text())
    report = []
    def difference(a, b):
        with Image.open(a) as first, Image.open(b) as second:
            width, height = max(first.width, second.width), max(first.height, second.height)
            left, right = Image.new('RGBA', (width, height)), Image.new('RGBA', (width, height))
            left.paste(first.convert('RGBA'), (0, 0))
            right.paste(second.convert('RGBA'), (0, 0))
            result = int(np.any(np.asarray(left) != np.asarray(right), axis=2).sum())
        return result
    for record in records:
        name = record['file']
        reference = data / 'reference' / name
        baseline = difference(reference, output / ('baseline-' + name))
        assert baseline == 0, 'captured-assets control failed for %s: %d pixels' % (name, baseline)
        pixels = difference(reference, output / name)
        if record['view'] == 'repositories':
            reason = None
            assert pixels == 0
        elif record['view'] == 'repository-definition':
            reason = 'The model has no declaration HEAD commit or full package definition.'
        elif record['view'] == 'work-failed':
            reason = 'The model has no captured evaluation index, creation grouping or original attempt identifiers.'
        else:
            reason = 'The model has no publication verification-start timestamp, captured evaluation index/creation grouping, original attempt identifiers or package-directory URL.'
        report.append(dict(reference=name, pixelDifference=pixels, capturedAssetsControlDifference=baseline, exception=reason))
    (output / 'G5-results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
