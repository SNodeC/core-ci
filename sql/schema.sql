-- Fresh administrative setup only. The runtime account does not execute DDL.
CREATE TABLE schema_version (
    version INT NOT NULL PRIMARY KEY,
    CHECK (version = 1)
) ENGINE=InnoDB;

INSERT INTO schema_version (version) VALUES (1);

CREATE TABLE repositories (
    github_id BIGINT UNSIGNED NOT NULL PRIMARY KEY CHECK (github_id > 0),
    name VARCHAR(128) NOT NULL UNIQUE,
    full_name VARCHAR(256) NOT NULL UNIQUE,
    editing_ref VARCHAR(256) NOT NULL,
    configuration_revision VARCHAR(64) NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE definitions (
    repository_id BIGINT UNSIGNED NOT NULL,
    revision VARCHAR(64) NOT NULL,
    blob_sha VARCHAR(64) NOT NULL,
    document LONGTEXT NOT NULL,
    PRIMARY KEY (repository_id, revision),
    FOREIGN KEY (repository_id) REFERENCES repositories(github_id),
    CHECK (JSON_VALID(document))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE namespace_claims (
    namespace VARCHAR(128) NOT NULL PRIMARY KEY,
    repository_id BIGINT UNSIGNED NOT NULL,
    definition_revision VARCHAR(64) NOT NULL,
    FOREIGN KEY (repository_id, definition_revision) REFERENCES definitions(repository_id, revision)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;


CREATE TABLE graph_revision (
    id TINYINT NOT NULL PRIMARY KEY CHECK (id = 1),
    generation BIGINT UNSIGNED NOT NULL,
    next_revision BIGINT UNSIGNED NOT NULL CHECK (next_revision > 0)
) ENGINE=InnoDB;
INSERT INTO graph_revision VALUES (1, 0, 1);

CREATE TABLE executions (
    id CHAR(64) NOT NULL PRIMARY KEY,
    repository_id BIGINT UNSIGNED NOT NULL,
    definition_revision VARCHAR(64) NOT NULL,
    source LONGTEXT NOT NULL CHECK (JSON_VALID(source)),
    target LONGTEXT NOT NULL CHECK (JSON_VALID(target)),
    cause LONGTEXT NOT NULL CHECK (JSON_VALID(cause)),
    automatic BOOLEAN NOT NULL,
    superseded_by CHAR(64) NULL,
    FOREIGN KEY (superseded_by) REFERENCES executions(id),
    FOREIGN KEY (repository_id, definition_revision) REFERENCES definitions(repository_id, revision),
    CHECK (repository_id = JSON_VALUE(source, '$.repository_id')),
    CHECK (definition_revision = JSON_VALUE(source, '$.definition_revision'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE prerequisites (
    execution_id CHAR(64) NOT NULL,
    prerequisite_id CHAR(64) NOT NULL,
    position INT UNSIGNED NOT NULL,
    PRIMARY KEY (execution_id, prerequisite_id),
    UNIQUE (execution_id, position),
    FOREIGN KEY (execution_id) REFERENCES executions(id),
    FOREIGN KEY (prerequisite_id) REFERENCES executions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE attempts (
    id VARCHAR(96) NOT NULL PRIMARY KEY,
    execution_id CHAR(64) NOT NULL,
    revision BIGINT UNSIGNED NOT NULL UNIQUE,
    state ENUM('waiting','building','publishing','published','failed') NOT NULL,
    assignment LONGTEXT NULL CHECK (assignment IS NULL OR JSON_VALID(assignment)),
    build_run LONGTEXT NULL CHECK (build_run IS NULL OR JSON_VALID(build_run)),
    build_result LONGTEXT NULL CHECK (build_result IS NULL OR JSON_VALID(build_result)),
    publication_run LONGTEXT NULL CHECK (publication_run IS NULL OR JSON_VALID(publication_run)),
    receipt LONGTEXT NULL CHECK (receipt IS NULL OR JSON_VALID(receipt)),
    failure TEXT NOT NULL,
    FOREIGN KEY (execution_id) REFERENCES executions(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
