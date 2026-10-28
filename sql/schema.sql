-- Fresh administrative provisioning; the controller account needs SELECT and UPDATE.
CREATE TABLE graph_state (
    id TINYINT NOT NULL PRIMARY KEY CHECK (id = 1),
    document LONGTEXT NOT NULL CHECK (JSON_VALID(document))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
INSERT INTO graph_state VALUES (1, '{"generation":0,"next_revision":1,"repositories":{},"executions":{}}');
