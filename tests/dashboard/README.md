# Dashboard functional checks

S22–S24 exercise authenticated streams, operations and session enforcement against disposable MariaDB, GitHub and IDP fixtures. M_DashboardEndToEnd exercises the running application through its browser interface. G6_DashboardStates executes the dashboard against model fixtures and checks status handling, publication timestamps, ordering and browser errors. Screenshots produced by G6 are diagnostic evidence and are not compared with historical images.

Set `CORECI_PLAYWRIGHT_PATH` to the installed playwright-core directory and provide `CORECI_TEST_DB_HOST`, `CORECI_TEST_DB_PORT`, `CORECI_TEST_DB_USER` and `CORECI_TEST_DB_NAME` for the disposable test database. Optional `CORECI_TEST_DB_PASSWORD` stays in the test environment. Run these checks through CTest; never point fixtures at a production database.
