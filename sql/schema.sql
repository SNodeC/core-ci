-- Fresh administrative setup only. The runtime account does not execute DDL.
CREATE TABLE schema_version (
    version INT NOT NULL PRIMARY KEY,
    CHECK (version = 1)
) ENGINE=InnoDB;

INSERT INTO schema_version (version) VALUES (1);

CREATE TABLE repositories (
    github_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
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
