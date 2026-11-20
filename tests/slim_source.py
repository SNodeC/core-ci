from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
production = [p for directory in ('core', 'persistence', 'adapters', 'app')
              for p in (root / directory).rglob('*') if p.is_file()]
forbidden = ('CORECI_' + 'GITHUB_TOKEN', 'CORECI_' + 'ADMIN_TOKEN', 'Environment' + 'TokenProvider',
             'Job' + 'Cache', 'presentation' + 'Body', 'status' + 'Interval',
             'Dispatch' + 'Build', 'Dispatch' + 'Publish', 'Feed' + 'State', 'maxConcurrent' + 'Builds',
             'build-' + 'timeout', 'publish-' + 'timeout', 'max-concurrent-' + 'builds', 'coreci_graph_' + 'feeds',
             'Event' + 'Store', 'Event' + 'Row', 'Event' + 'Sql', 'Event' + 'Queue', 'load' + 'All', 're' + 'play',
             'get' + 'env', 'CORECI_' + 'WEBHOOK_SECRET', 'CORECI_' + 'CALLBACK_SECRET', 'CORECI_' + 'DB_PASSWORD')
for path in production:
    text = path.read_text()
    assert all(word not in text for word in forbidden), str(path)
    if path.suffix in ('.h', '.cpp'):
        assert not re.search(r'\"Bearer [^\"]+\"', text), str(path)
assert not any('InMemory' in p.name for p in (root / 'persistence').iterdir())
assert not any('Jobs' in p.name for p in (root / 'adapters/github').iterdir())
symbols = subprocess.check_output(['nm', '-C', sys.argv[1]], text=True)
assert not any(word in symbols for word in ('coreci::legacy', 'EventRow', 'EventSql', 'EventQueue', 'EventStore', 'loadAll'))
assert not re.search(r'SELECT[^;]*coreci_log', (root / 'adapters/mariadb/MariaDBStateStore.cpp').read_text())
unit = (root / 'packaging/core-ci.service.in').read_text()
assert 'EnvironmentFile' not in unit and 'CORECI_' not in unit
assert all(name in unit for name in ('XDG_CONFIG_HOME', 'XDG_STATE_HOME', 'XDG_RUNTIME_DIR'))
source_suffixes = {'.cpp', '.h', '.js', '.html', '.css', '.py'}
lines = sum(len(path.read_text().splitlines()) for path in production if path.suffix in source_suffixes)
assert lines <= 5850, ('owner-approved daemon source ceiling exceeded', lines)
print('Daemon source and linked binary contain no replay, legacy persistence or configuration environment lookup')
