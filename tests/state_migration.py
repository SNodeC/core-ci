"""Schema-7 upgrade preserves all existing rows and refuses unsafe/repeated writes."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from database import eventually

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import fixture


def run(build):
    root = Path(__file__).resolve().parents[1]
    cli = ['mariadb', '--no-defaults', '--skip-ssl', '--host=' + os.environ['CORECI_TEST_DB_HOST'],
           '--port=' + os.environ['CORECI_TEST_DB_PORT'], '--user=' + os.environ['CORECI_TEST_DB_USER'],
           '--database=' + os.environ['CORECI_TEST_DB_NAME'], '-NB']
    env = dict(os.environ, MYSQL_PWD=os.environ.get('CORECI_TEST_DB_PASSWORD', ''))
    def sql(statement, check=True):
        result = subprocess.run(cli, input=statement, text=True, env=env, capture_output=True, check=check)
        return result
    migration = (root / 'migration/state-7-to-8.sql').read_text()
    tables = ('state', 'packages', 'prerequisites', 'plan')
    with fixture(build, small=True) as fx:
        fx.ready()
        assert fx.request('/').status == 200
        fx.tags = {'owner/' + name: {'v1.0.0': 'b' * 40} for name in 'abcde'}
        assert fx.post('/api/reconcile', {})[0] == 200
        owner, values = fx.dispatches.get(timeout=10)
        fx.complete(owner, values)
        # The writer guard must reject the upgrade before creating staged tables.
        assert sql(migration, check=False).returncode != 0
        assert sql("SHOW TABLES LIKE 'coreci_v8_%';").stdout == ''
        fx.process.terminate()
        assert fx.process.wait(timeout=10) == 0
        sql('ALTER TABLE coreci_state DROP COLUMN dispatch_paused; '
            'ALTER TABLE coreci_packages DROP COLUMN ci_commit,DROP COLUMN ci_tree,DROP COLUMN title; '
            'ALTER TABLE coreci_prerequisites DROP COLUMN version_range; '
            'ALTER TABLE coreci_plan DROP COLUMN ci_commit,DROP COLUMN ci_tree; '
            'UPDATE coreci_schema SET version=7;')
        columns = {table: sql('SHOW COLUMNS FROM coreci_' + table).stdout.splitlines() for table in tables}
        columns = {table: ','.join(line.split('\t')[0] for line in rows) for table, rows in columns.items()}
        before = {table: sql('SELECT ' + names + ' FROM coreci_' + table + ' ORDER BY 1,2;').stdout
                  for table, names in columns.items()}
        other_tables = sql("SHOW TABLES WHERE Tables_in_" + os.environ['CORECI_TEST_DB_NAME'] + " LIKE 'coreci_%';").stdout.splitlines()
        untouched = {name: sql('SELECT * FROM ' + name + ' ORDER BY 1;').stdout for name in other_tables
                     if name not in {'coreci_schema', *('coreci_' + table for table in tables)}}
        refused = subprocess.run(fx.command, env=fx.env, capture_output=True, timeout=15)
        assert refused.returncode != 0 and b'offline migration' in refused.stderr
        sql(migration)
        assert sql('SELECT version FROM coreci_schema;').stdout.strip() == '8'
        assert sql('SELECT dispatch_paused FROM coreci_state;').stdout.strip() == '1'
        for table, names in columns.items():
            assert sql('SELECT ' + names + ' FROM coreci_' + table + ' ORDER BY 1,2;').stdout == before[table]
            assert sql('SELECT * FROM schema7_coreci_' + table + ' ORDER BY 1,2;').stdout == before[table]
        assert all(sql('SELECT * FROM ' + name + ' ORDER BY 1;').stdout == rows for name, rows in untouched.items())
        assert sql("SELECT COUNT(*) FROM coreci_plan WHERE ci_commit<>sha OR ci_tree<>'';").stdout.strip() == '0'
        assert sql(migration, check=False).returncode != 0
        fx.process = subprocess.Popen(fx.command, env=fx.env, stdout=fx.log, stderr=subprocess.STDOUT)
        def ready():
            response = fx.request('/api/v1/status.json', browser=False)
            value = json.loads(response.read()) if response.status == 200 else None
            return value
        assert eventually(lambda: ready() is not None)
        assert ready()['dispatch_paused']
        time.sleep(1.2)
        assert fx.dispatches.empty(), 'upgraded state dispatched before owner resumed it'
    print('schema 7 upgrade: preserved every old column, histories and revisions; paused; refused writer/repeat')


if __name__ == '__main__':
    run(sys.argv[1])
