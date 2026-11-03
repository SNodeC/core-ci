"""Lossless row persistence and changed-row writes through real Store and SQL boundaries."""
import collections
import copy
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time

from persistence import run
from row_fixture import sql


def main():
    build = Path(sys.argv[1]).resolve()
    parent = Path('/tmp/codex/build/core-ci/db-phase1-20261103/lossless')
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='case-', dir=parent) as temporary:
        root = Path(temporary)
        backend = root / 's'
        run(['mariadb-install-db', '--no-defaults', f'--datadir={root / "data"}',
             '--auth-root-authentication-method=normal', '--skip-test-db'])
        with (root / 'server.log').open('w') as log:
            server = subprocess.Popen(['/usr/sbin/mariadbd', '--no-defaults', f'--datadir={root / "data"}',
                f'--socket={backend}', f'--pid-file={root / "pid"}', '--skip-networking',
                '--max-allowed-packet=16777216', f'--user={os.geteuid()}'], stdout=log, stderr=log)
            try:
                ready = False
                deadline = time.monotonic() + 20
                while not ready and time.monotonic() < deadline and server.poll() is None:
                    try:
                        sql(backend, 'SELECT 1')
                        ready = True
                    except RuntimeError:
                        time.sleep(.05)
                assert ready
                sql(backend, 'CREATE DATABASE fixture; CREATE TABLE fixture.unrelated(id INT)')
                (root / 'empty.conf').touch()
                input_file, output_file = root / 'input.json', root / 'output.json'
                if os.getenv('CI_CORPUS_PATH'):
                    original = json.loads(Path(os.environ['CI_CORPUS_PATH']).read_text())
                else:
                    run([str(build / 'graph-contract-test'), str(input_file)])
                    original = json.loads(input_file.read_text())
                graph = copy.deepcopy(original)
                strange = {'__fk': {'refs': 'a' * 64}, '0': [], '01': {}, '/~': None,
                           'float': [-0.0, 0.125, 1e-50, 1e30], 'order': [3, 2, 1]}
                graph['root/~0'] = strange
                graph['repositories']['name/~0'] = {'definition': {'title': 'strange', 'unknown': strange},
                                                       'ref': None, 'commit': '', 'unknown': strange}
                graph['executions'][0]['unknown'] = strange
                first = graph['executions'][0]['attempts'][0]
                first['attempts'] = {'literal': 'must remain'}
                first['unknown'] = strange
                # Never derive publisher assertions from another field, even when identities differ.
                if 'receipt' in first:
                    first['receipt']['build_job']['independent/~'] = strange
                    first['receipt']['unknown'] = strange
                environment = dict(os.environ, CI_TEST_SOCKET=str(backend), CI_TEST_DATABASE='fixture',
                                   CI_ROWS_OUTPUT=str(output_file), CI_ROWS_ORACLE='1')
                command = [str(build / 'rows-contract-test'), '--config-file', str(root / 'empty.conf'),
                           '--log-file', str(root / 'native.log'), '--quiet=true']

                for name, ddl in [('legacy', 'CREATE TABLE legacy.ci_graph(wrong INT)'),
                                  ('partial', 'CREATE TABLE partial.attempt(wrong INT)'),
                                  ('mixed', 'CREATE TABLE mixed.ci_graph(wrong INT); CREATE TABLE mixed.repository(wrong INT)')]:
                    sql(backend, f'CREATE DATABASE {name}; {ddl}')
                    rejected = subprocess.run(command, env=dict(environment, CI_TEST_DATABASE=name),
                                              capture_output=True, text=True, timeout=10)
                    assert rejected.returncode and 'Legacy or partial controller schema' in rejected.stderr
                    assert sql(backend, f"SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='{name}'").strip() == ('2' if name == 'mixed' else '1')

                def load(value, expected=True):
                    input_file.write_text(json.dumps(value))
                    result = subprocess.run(command, env=dict(environment, CI_ROWS_INPUT=str(input_file)),
                                            text=True, capture_output=True, timeout=90)
                    assert (result.returncode == 0) == expected, result.stdout + result.stderr
                    if expected:
                        assert json.loads(output_file.read_text()) == value
                    return result.stdout + result.stderr

                if os.getenv('CI_CORPUS_PATH'):
                    print(load(original).strip())
                    # Baseline import is measured separately from adversarial unknown fields.
                    sql(backend, 'USE fixture; DROP TABLE attempt,execution,repository,content,graph_root')
                print(load(graph).strip())
                for table in ['repository', 'execution', 'attempt', 'content', 'graph_root']:
                    sql(backend, f'USE fixture; CREATE TABLE IF NOT EXISTS audit(t TEXT,op TEXT); '
                                 f"CREATE TRIGGER audit_{table}_insert AFTER INSERT ON {table} FOR EACH ROW INSERT INTO audit VALUES('{table}','insert'); "
                                 f"CREATE TRIGGER audit_{table}_update AFTER UPDATE ON {table} FOR EACH ROW INSERT INTO audit VALUES('{table}','update');")
                print(load(graph).strip())
                assert sql(backend, 'SELECT COUNT(*) FROM fixture.audit').strip() == '0'
                changed = copy.deepcopy(graph)
                changed['executions'][0]['attempts'][-1]['facts']['internal/~0'] = strange
                print(load(changed).strip())
                assert sql(backend, 'SELECT t,op FROM fixture.audit').strip() == 'attempt\tupdate'
                sql(backend, 'TRUNCATE fixture.audit')
                print(load(changed).strip())
                assert sql(backend, 'SELECT COUNT(*) FROM fixture.audit').strip() == '0'
                rejected = copy.deepcopy(changed)
                rejected['executions'][0]['unknown']['order'].reverse()
                assert 'Immutable execution changed' in load(rejected, False)
                rejected = copy.deepcopy(changed)
                rejected['executions'].reverse()
                assert 'Execution moved' in load(rejected, False)
                rejected = copy.deepcopy(changed)
                rejected['executions'][0]['attempts'].clear()
                assert 'Attempt removed' in load(rejected, False)
                assert sql(backend, 'SELECT COUNT(*) FROM fixture.audit').strip() == '0'
                timings = []
                for index in range(5):
                    result = run(command, env=environment)
                    timings.append(float(result.split('read_ms=')[1].split()[0]))
                sql(backend, 'ANALYZE TABLE fixture.repository,fixture.execution,fixture.attempt,fixture.content,fixture.graph_root')
                output = {'read_ms': timings, 'median_read_ms': statistics.median(timings),
                          'executions': len(graph['executions']),
                          'content_rows': sql(backend, 'SELECT COUNT(*) FROM fixture.content').strip(),
                          'physical_fixture_bytes': sql(backend, "SELECT SUM(DATA_LENGTH+INDEX_LENGTH) FROM information_schema.TABLES WHERE TABLE_SCHEMA='fixture'").strip()}
                (parent / ('corpus-results.json' if os.getenv('CI_CORPUS_PATH') else 'fixture-results.json')).write_text(json.dumps(output, indent=2))
                print(json.dumps(output))
                # A later row exceeds the fixture packet bound after an earlier write succeeded.
                sql(backend, 'SET GLOBAL max_allowed_packet=16384')
                rejected = copy.deepcopy(changed)
                rejected['executions'][0]['attempts'][-1]['facts']['first_row'] = 'must roll back'
                rejected['executions'][-1]['attempts'][-1]['facts']['too_large'] = 'x' * 30000
                environment['CI_ROWS_EXPECT_CLOSED'] = '1'
                assert 'Database write outcome unresolved; no dispatch:' in load(rejected, False)
                del environment['CI_ROWS_EXPECT_CLOSED']
                assert sql(backend, 'SELECT COUNT(*) FROM fixture.audit').strip() == '0', 'No committed subset on packet rejection'
                sql(backend, 'SET GLOBAL max_allowed_packet=16777216')
                run(command, env=environment)
                assert json.loads(output_file.read_text()) == changed
                print('Exact raw roundtrip, unknown fields/floats, independent hashes, one changed row, silence, immutable guards, packet rollback passed')
            finally:
                server.terminate()
                server.wait(timeout=20)


if __name__ == '__main__':
    main()
