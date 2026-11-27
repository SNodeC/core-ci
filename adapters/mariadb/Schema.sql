CREATE TABLE IF NOT EXISTS coreci_schema (version INT NOT NULL PRIMARY KEY);

CREATE TABLE IF NOT EXISTS coreci_state (
  singleton INT NOT NULL,
  sequence BIGINT UNSIGNED NULL,
  clock BIGINT NULL,
  quiet_until BIGINT NULL,
  latest VARCHAR(64) NULL,
  dispatch_paused INT NOT NULL,
  PRIMARY KEY (singleton)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_packages (
  package VARCHAR(64) NOT NULL,
  owner VARCHAR(100) NULL,
  repo VARCHAR(100) NULL,
  branch VARCHAR(255) NULL,
  revision_base INT UNSIGNED NULL,
  ci_commit CHAR(40) NOT NULL,
  ci_tree CHAR(40) NOT NULL,
  title TEXT NOT NULL,
  PRIMARY KEY (package)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_targets (
  target VARCHAR(64) NOT NULL PRIMARY KEY
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_target_builds (
  wave_id VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  build_id VARCHAR(255) NOT NULL,
  succeeded INT NOT NULL,
  PRIMARY KEY (wave_id, target, build_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_memberships (
  package VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  PRIMARY KEY (package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_prerequisites (
  package VARCHAR(64) NOT NULL,
  prerequisite VARCHAR(64) NOT NULL,
  version_range VARCHAR(255) NOT NULL,
  PRIMARY KEY (package, prerequisite)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_tags (
  package VARCHAR(64) NOT NULL,
  owner VARCHAR(100) NULL,
  repo VARCHAR(100) NULL,
  branch VARCHAR(255) NULL,
  version VARCHAR(100) NULL,
  tag VARCHAR(255) NULL,
  sha CHAR(40) NULL,
  PRIMARY KEY (package)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_units (
  package VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  status VARCHAR(16) NULL,
  wave_id VARCHAR(64) NULL,
  allocation_order BIGINT UNSIGNED NULL,
  artifact VARCHAR(255) NULL,
  build_start BIGINT NULL,
  build_end BIGINT NULL,
  publish_end BIGINT NULL,
  requested INT NULL,
  retry_after BIGINT NULL,
  auto_version VARCHAR(100) NULL,
  PRIMARY KEY (package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_published (
  package VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  version VARCHAR(100) NULL,
  revision BIGINT UNSIGNED NULL,
  owner VARCHAR(100) NULL,
  repo VARCHAR(100) NULL,
  branch VARCHAR(255) NULL,
  wave_id VARCHAR(64) NULL,
  published_at BIGINT NULL,
  feed_commit CHAR(40) NULL,
  run_id BIGINT UNSIGNED NULL,
  PRIMARY KEY (package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_failures (
  package VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  version VARCHAR(100) NULL,
  reason TEXT NULL,
  revision BIGINT UNSIGNED NULL,
  failed_at BIGINT NULL,
  build_failed INT NULL,
  PRIMARY KEY (package, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_revisions (
  package VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  version VARCHAR(100) NOT NULL,
  revision BIGINT UNSIGNED NULL,
  PRIMARY KEY (package, target, version)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_inputs (
  context VARCHAR(16) NOT NULL,
  owner_id VARCHAR(64) NOT NULL,
  package VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  prerequisite VARCHAR(64) NOT NULL,
  version VARCHAR(100) NULL,
  revision BIGINT UNSIGNED NULL,
  PRIMARY KEY (context, owner_id, package, target, prerequisite)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_waves (
  id VARCHAR(64) NOT NULL,
  feed_branch VARCHAR(255) NULL,
  owner VARCHAR(100) NULL,
  repo VARCHAR(100) NULL,
  branch VARCHAR(255) NULL,
  phase VARCHAR(16) NULL,
  owner_run_id BIGINT UNSIGNED NULL,
  run_attempt INT UNSIGNED NULL,
  attempt INT UNSIGNED NULL,
  retry_at BIGINT NULL,
  dispatched_at BIGINT NULL,
  deadline BIGINT NULL,
  start_deadline BIGINT NULL,
  accepted INT NULL,
  run_finished INT NULL,
  expired INT NULL,
  started BIGINT NULL,
  finished BIGINT NULL,
  conclusion VARCHAR(64) NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_plan (
  wave_id VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  position INT UNSIGNED NOT NULL,
  package VARCHAR(64) NULL,
  owner VARCHAR(100) NULL,
  repo VARCHAR(100) NULL,
  branch VARCHAR(255) NULL,
  version VARCHAR(100) NULL,
  tag VARCHAR(255) NULL,
  sha CHAR(40) NULL,
  revision BIGINT UNSIGNED NULL,
  build_id VARCHAR(255) NULL,
  build INT NULL,
  ci_commit CHAR(40) NOT NULL,
  ci_tree CHAR(40) NOT NULL,
  PRIMARY KEY (wave_id, target, position)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_wave_targets (
  wave_id VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  status VARCHAR(16) NULL,
  artifact VARCHAR(255) NULL,
  finished INT NULL,
  available INT NULL,
  publication_id VARCHAR(64) NULL,
  PRIMARY KEY (wave_id, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_publications (
  id VARCHAR(64) NOT NULL,
  feed_branch VARCHAR(255) NULL,
  owner VARCHAR(100) NULL,
  repo VARCHAR(100) NULL,
  branch VARCHAR(255) NULL,
  phase VARCHAR(16) NULL,
  owner_run_id BIGINT UNSIGNED NULL,
  run_attempt INT UNSIGNED NULL,
  attempt INT UNSIGNED NULL,
  retry_at BIGINT NULL,
  dispatched_at BIGINT NULL,
  deadline BIGINT NULL,
  start_deadline BIGINT NULL,
  accepted INT NULL,
  run_finished INT NULL,
  expired INT NULL,
  started BIGINT NULL,
  finished BIGINT NULL,
  conclusion VARCHAR(64) NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_publication_items (
  publication_id VARCHAR(64) NOT NULL,
  wave_id VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  artifact VARCHAR(255) NULL,
  PRIMARY KEY (publication_id, wave_id, target)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_publication_units (
  publication_id VARCHAR(64) NOT NULL,
  wave_id VARCHAR(64) NOT NULL,
  target VARCHAR(64) NOT NULL,
  build_id VARCHAR(255) NOT NULL,
  succeeded INT NULL,
  PRIMARY KEY (publication_id, wave_id, target, build_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS coreci_log (
  seq BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  logged_at BIGINT NOT NULL,
  description TEXT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
