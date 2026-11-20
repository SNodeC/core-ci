"""One-shot conversion of the full production-shaped schema-5/6 history."""
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch
from database import database

sys.path.insert(0, str(Path(__file__).parent / 'dashboard'))
from fixture import fixture


def run(build, proxy=None, fault=''):
    build = Path(build)
    root = Path(__file__).resolve().parents[1]
    cli = ['mariadb', '--no-defaults', '--skip-ssl', '--host=' + os.environ['CORECI_TEST_DB_HOST'],
           '--port=' + os.environ['CORECI_TEST_DB_PORT'], '--user=' + os.environ['CORECI_TEST_DB_USER'],
           '--database=' + os.environ['CORECI_TEST_DB_NAME'], '-NB']
    environment = dict(os.environ, MYSQL_PWD=os.environ.get('CORECI_TEST_DB_PASSWORD', ''))

    def sql(statement):
        return subprocess.check_output(cli, input=statement, text=True, env=environment)

    events = subprocess.check_output([build / 'W_ReplayHistory', root / 'tests/data', '--sql'], text=True)
    with fixture(build, small=True) as fx:
        fx.ready()
        fx.process.terminate()
        assert fx.process.wait(timeout=10) == 0
        config = fx.work / 'migration.conf'
        config.write_text('[coreci]\n' + '\n'.join(key + '=' + repr(fx.settings['coreci'][key])
            for key in ('db-host', 'db-port', 'db-user', 'db-name', 'db-password-file')) + '\nwave-delay=5\n')
        for version in ((6,) if proxy else (5, 6)):
            subprocess.run([build / 'coreci-reset-store'], env=fx.env, check=True)
            sql((root / 'migration/legacy/Schema.sql').read_text() + '\nINSERT INTO coreci_schema VALUES (6);\n' + events)
            if version == 5:
                sql('ALTER TABLE coreci_events DROP COLUMN dispatch_deadline; UPDATE coreci_schema SET version=5;')
            columns = ('seq,event_type,package,target,tag,source_sha,origin,build_id,wave_id,http_status,message,'
                       'run_id,run_attempt,attempt,artifact,feed_commit,reason,conclusion,event_at')
            before = sql('SELECT ' + columns + ' FROM coreci_events ORDER BY seq;')
            refused = subprocess.run(fx.command, env=fx.env, capture_output=True, timeout=15)
            assert refused.returncode != 0 and b'coreci-migrate' in refused.stderr
            assert sql('SELECT version FROM coreci_schema;').strip() == str(version)
            command = [build / 'coreci-migrate', '--config-file', config, 'coreci']
            if proxy:
                proxy.arm('=COMMIT' if fault == 'commit' else 'RENAME TABLE coreci_schema', 'after')
            migrated = subprocess.run(command,
                                      env=fx.env, capture_output=True, timeout=30)
            if proxy:
                assert proxy.fired.wait(5) and migrated.returncode != 0
                version_after = sql('SELECT version FROM coreci_schema;').strip()
                assert version_after == ('6' if fault == 'commit' else '7')
                if version_after == '6':
                    refused = subprocess.run(fx.command, env=fx.env, capture_output=True, timeout=15)
                    assert refused.returncode != 0 and b'coreci-migrate' in refused.stderr
                    migrated = subprocess.run(command, env=fx.env, capture_output=True, timeout=30)
                else:
                    # The atomic rename committed; never infer failure from the lost acknowledgement.
                    migrated = None
            assert migrated is None or migrated.returncode == 0, migrated.stderr.decode()
            assert sql('SELECT version FROM coreci_schema;').strip() == '7'
            assert sql('SELECT ' + columns + ' FROM legacy_coreci_events ORDER BY seq;') == before
            assert sql('SELECT COUNT(*) FROM coreci_log;').strip() == str(len(before.splitlines()))
            subprocess.run([build / 'coreci-state-dump', root / 'tests/data/legacy-replay-baseline.json'],
                           env=fx.env, check=True, timeout=20)
            repeated = subprocess.run([build / 'coreci-migrate', '--config-file', config, 'coreci'],
                                      env=fx.env, capture_output=True, timeout=15)
            assert repeated.returncode != 0
            subprocess.run([build / 'coreci-state-dump', root / 'tests/data/legacy-replay-baseline.json'],
                           env=fx.env, check=True, timeout=20)
    print('schema 5/6: revisions, published facts, tags, waves and full display history preserved')


if __name__ == '__main__':
    run(sys.argv[1])
    for boundary in ('commit', 'rename'):
        with database(sys.argv[1]) as (proxy, _, environment, _), patch.dict(os.environ, environment):
            run(sys.argv[1], proxy, boundary)
