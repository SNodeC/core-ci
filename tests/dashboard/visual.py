"""G4 asset accounting and G5 exact pixel comparison against the selected capture."""
import difflib
import json
from pathlib import Path
import re
import subprocess
import sys
import math
import numpy as np
from PIL import Image, ImageDraw

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
            identity_box = '.execution-content dl dd:nth-of-type(2) { min-height: 2lh; }\n'
            expected = before.replace('.actions {', identity_box + '.actions {', 1) if name == 'style.css' else before
            assert expected == after
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
    print('G4: HTML byte-identical; one exact N4 attempt-box CSS rule; drawing functions unchanged; full asset diff saved')
else:
    subprocess.run(['node', str(root / 'tests/dashboard/browser.cjs'), str(output), 'references'], check=True)
    records = json.loads((data / 'reference/screenshots.json').read_text())
    report = []
    masks = json.loads((output / 'G5-masks.json').read_text())
    def difference(a, b, box=None):
        with Image.open(a) as first, Image.open(b) as second:
            width, height = max(first.width, second.width), max(first.height, second.height)
            left, right = Image.new('RGBA', (width, height)), Image.new('RGBA', (width, height))
            left.paste(first.convert('RGBA'), (0, 0))
            right.paste(second.convert('RGBA'), (0, 0))
            if box:
                rectangle = (math.floor(box['x']), math.floor(box['y']),
                             math.ceil(box['x'] + box['width']) - 1, math.ceil(box['y'] + box['height']) - 1)
                for canvas in (left, right):
                    ImageDraw.Draw(canvas).rectangle(rectangle, fill='black')
            result = int(np.any(np.asarray(left) != np.asarray(right), axis=2).sum())
        return result
    for record in records:
        name = record['file']
        reference = data / 'reference' / name
        assert masks[name]['baseline'] == masks[name]['new'], 'attempt value changed surrounding layout'
        box = masks[name]['baseline']
        if box:
            ink = (masks[name]['baselineInk'], masks[name]['newInk'])
            box = dict(x=math.floor(box['x']), y=math.floor(box['y']),
                       width=math.ceil(max(item['x'] + item['width'] for item in ink)) - math.floor(box['x']),
                       height=math.ceil(box['y'] + box['height']) - math.floor(box['y']))
        baseline = difference(reference, output / ('baseline-' + name))
        assert baseline == 0, 'captured-assets control failed for %s: %d pixels' % (name, baseline)
        pixels = difference(reference, output / name, box)
        with Image.open(reference) as first, Image.open(output / name) as second:
            width, height = max(first.width, second.width), max(first.height, second.height)
            left, right = Image.new('RGBA', (width, height)), Image.new('RGBA', (width, height))
            left.paste(first.convert('RGBA'), (0, 0))
            right.paste(second.convert('RGBA'), (0, 0))
            if box:
                rectangle = (math.floor(box['x']), math.floor(box['y']),
                             math.ceil(box['x'] + box['width']) - 1, math.ceil(box['y'] + box['height']) - 1)
                for canvas in (left, right):
                    ImageDraw.Draw(canvas).rectangle(rectangle, fill='black')
            mask = np.any(np.asarray(left) != np.asarray(right), axis=2)
            diff = np.zeros((height, width, 3), dtype=np.uint8)
            diff[mask] = [255, 0, 255]
            Image.fromarray(diff).save(output / ('diff-' + name))
        report.append(dict(reference=name, pixelDifference=pixels, capturedAssetsControlDifference=baseline,
                           maskBoxes=dict(baseline=box, new=box), measuredValueBoxes=masks[name]))
    (output / 'G5-results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    errors = json.loads((output / 'references-console.json').read_text())
    assert all(row['pixelDifference'] == 0 and row['maskBoxes']['baseline'] == row['maskBoxes']['new'] for row in report) and not errors, \
        'N4 reference equivalence failed; see G5-results.json, diff-*.png and references-console.json'
