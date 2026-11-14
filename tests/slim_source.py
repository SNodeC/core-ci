from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
production = [p for directory in ('core', 'persistence', 'adapters', 'app', 'packaging')
              for p in (root / directory).rglob('*') if p.is_file()]
forbidden = ('CORECI_' + 'GITHUB_TOKEN', 'CORECI_' + 'ADMIN_TOKEN', 'Environment' + 'TokenProvider',
             'Job' + 'Cache', 'presentation' + 'Body', 'status' + 'Interval',
             'Dispatch' + 'Build', 'Dispatch' + 'Publish', 'Feed' + 'State', 'maxConcurrent' + 'Builds',
             'build-' + 'timeout', 'publish-' + 'timeout', 'max-concurrent-' + 'builds', 'coreci_graph_' + 'feeds')
for path in production:
    text = path.read_text()
    assert all(word not in text for word in forbidden), str(path)
    if path.suffix in ('.h', '.cpp'):
        assert not re.search(r'\"Bearer [^\"]+\"', text), str(path)
assert not any('InMemory' in p.name for p in (root / 'persistence').iterdir())
assert not any('Jobs' in p.name for p in (root / 'adapters/github').iterdir())
print('No static GitHub/admin credentials, job lookup or presentation provider in production')
