# Dashboard HTTP checks

S22–S24 exercise SSE snapshots/updates, operations and session enforcement against
a running controller with disposable MariaDB, GitHub and IDP HTTP fixtures.
W_Model tests the dashboard projection and frozen plan schema in C++.
M_LocalSmoke covers public plans, HMAC callbacks, route removal, restart, writer
locking and tag-bound declaration changes.

Provide CORECI_TEST_DB_HOST, CORECI_TEST_DB_PORT, CORECI_TEST_DB_USER and
CORECI_TEST_DB_NAME for the disposable database. Optional
CORECI_TEST_DB_PASSWORD stays in the test environment. Run through CTest;
never use a production database. The shared fixture.py and tests/data/dashboard
files are used by these HTTP checks and smoke tests.
