"""Disposable-test fixture loading through Store, never a separate persistence codec."""
import hashlib
import json
import os
from pathlib import Path
import sys

from persistence import run


def sql(socket, statement):
    return run(['mariadb', '--no-defaults', f'--socket={socket}', '-uroot', '-N', '-B'], input=statement)


def fingerprint(socket, database):
    statements = []
    for table, identity in [('repository', 'name'), ('execution', 'id'), ('attempt', 'id'), ('content', 'hash'), ('graph_root', 'id')]:
        extra = ',ordinal' if table == 'execution' else ',execution,ordinal' if table == 'attempt' else ''
        refs = '' if table == 'graph_root' else ',SHA2(refs,256)'
        statements.append(f'SELECT HEX({identity}),SHA2(body,256){refs}{extra} FROM {database}.{table} ORDER BY {identity};')
    return hashlib.sha256(sql(socket, ''.join(statements)).encode()).hexdigest()


def seed(build, root, socket, database, value):
    """Reset only the caller's isolated fixture; the running fixture is idle here."""
    sql(socket, f'USE {database}; DROP TABLE IF EXISTS attempt,execution,repository,content,graph_root;')
    source = Path(root) / 'rows-input.json'
    source.write_text(json.dumps(value))
    config = Path(root) / 'empty.conf'
    config.touch()
    environment = dict(os.environ, CI_TEST_SOCKET=str(socket), CI_TEST_DATABASE=database, CI_ROWS_INPUT=str(source))
    return run([str(Path(build) / 'rows-contract-test'), '--config-file', str(config),
                '--log-file', str(Path(root) / 'rows.log'), '--quiet=true'], env=environment)


if __name__ == '__main__':
    print(seed(*sys.argv[1:], json.load(sys.stdin)))
