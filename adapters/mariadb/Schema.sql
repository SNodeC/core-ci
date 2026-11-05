CREATE TABLE IF NOT EXISTS coreci_schema (
  version INT NOT NULL PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS coreci_events (
  seq          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  recorded_at  DATETIME(3)  NOT NULL,
  event_type   VARCHAR(32)  NOT NULL,   -- TagObserved, DispatchAccepted, ... (section 4.3)
  package      VARCHAR(128) NULL,
  target       VARCHAR(128) NULL,
  tag          VARCHAR(255) NULL,
  source_sha   CHAR(40)     NULL,
  origin       VARCHAR(16)  NULL,
  build_id     VARCHAR(255) NULL,
  kind         VARCHAR(16)  NULL,
  http_status  INT          NULL,
  message      TEXT         NULL,
  run_id       BIGINT UNSIGNED NULL,
  run_attempt  INT          NULL,
  attempt      INT          NULL,       -- dispatch attempt
  artifact     VARCHAR(255) NULL,
  feed_commit  CHAR(40)     NULL,
  reason       TEXT         NULL,
  event_at     BIGINT       NOT NULL,   -- the event's `at` (UTC seconds), section 4.3
  INDEX idx_build_id (build_id),
  INDEX idx_unit (package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS coreci_graph_feeds (
  event_seq     BIGINT UNSIGNED NOT NULL,
  feed          VARCHAR(64)  NOT NULL,
  owner         VARCHAR(100) NOT NULL,
  repo          VARCHAR(100) NOT NULL,
  workflow_file VARCHAR(255) NOT NULL,
  branch        VARCHAR(255) NOT NULL,
  PRIMARY KEY (event_seq, feed)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_graph_packages (
  event_seq           BIGINT UNSIGNED NOT NULL,
  package             VARCHAR(64)  NOT NULL,
  source_owner        VARCHAR(100) NOT NULL,
  source_repo         VARCHAR(100) NOT NULL,
  source_branch       VARCHAR(255) NOT NULL,
  build_owner         VARCHAR(100) NOT NULL,
  build_repo          VARCHAR(100) NOT NULL,
  build_workflow_file VARCHAR(255) NOT NULL,
  build_branch        VARCHAR(255) NOT NULL,
  accept_prereleases  TINYINT      NOT NULL,
  PRIMARY KEY (event_seq, package)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_graph_targets (
  event_seq BIGINT UNSIGNED NOT NULL,
  package   VARCHAR(64) NOT NULL,
  target    VARCHAR(64) NOT NULL,
  feed      VARCHAR(64) NOT NULL,
  PRIMARY KEY (event_seq, package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_graph_prerequisites (
  event_seq    BIGINT UNSIGNED NOT NULL,
  package      VARCHAR(64) NOT NULL,
  prerequisite VARCHAR(64) NOT NULL,
  PRIMARY KEY (event_seq, package, prerequisite)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
