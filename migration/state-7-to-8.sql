-- Stop core-ci and back up the database first. Run through the mariadb client
-- without --force. On any error, inspect coreci_schema and retained tables;
-- never blindly repeat an operation whose acknowledgement was lost.
DELIMITER //
BEGIN NOT ATOMIC
    DECLARE locked INT DEFAULT 0;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        DO RELEASE_LOCK(CONCAT('coreci:', DATABASE()));
        RESIGNAL;
    END;
    SELECT GET_LOCK(CONCAT('coreci:', DATABASE()), 0) INTO locked;
    IF locked IS NULL OR locked <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'core-ci writer lock unavailable';
    END IF;
    IF (SELECT COUNT(*) FROM coreci_schema) <> 1 OR
       (SELECT MIN(version) FROM coreci_schema) <> 7 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'exactly one schema-7 version row required';
    END IF;

    CREATE TABLE coreci_v8_schema LIKE coreci_schema;
    INSERT INTO coreci_v8_schema VALUES (8);
    CREATE TABLE coreci_v8_state LIKE coreci_state;
    ALTER TABLE coreci_v8_state ADD COLUMN dispatch_paused INT NOT NULL;
    INSERT INTO coreci_v8_state SELECT singleton,sequence,clock,quiet_until,latest,1 FROM coreci_state;
    CREATE TABLE coreci_v8_packages LIKE coreci_packages;
    ALTER TABLE coreci_v8_packages ADD COLUMN ci_commit CHAR(40) NOT NULL,
        ADD COLUMN ci_tree CHAR(40) NOT NULL, ADD COLUMN title TEXT NOT NULL;
    INSERT INTO coreci_v8_packages
        SELECT p.*,COALESCE(t.sha,''),'',p.package FROM coreci_packages p
        LEFT JOIN coreci_tags t ON p.package=t.package;
    CREATE TABLE coreci_v8_prerequisites LIKE coreci_prerequisites;
    ALTER TABLE coreci_v8_prerequisites ADD COLUMN version_range VARCHAR(255) NOT NULL;
    INSERT INTO coreci_v8_prerequisites SELECT package,prerequisite,'>=0.0.0' FROM coreci_prerequisites;
    CREATE TABLE coreci_v8_plan LIKE coreci_plan;
    ALTER TABLE coreci_v8_plan ADD COLUMN ci_commit CHAR(40) NOT NULL, ADD COLUMN ci_tree CHAR(40) NOT NULL;
    INSERT INTO coreci_v8_plan SELECT p.*,p.sha,'' FROM coreci_plan p;

    RENAME TABLE coreci_schema TO schema7_coreci_schema, coreci_v8_schema TO coreci_schema,
        coreci_state TO schema7_coreci_state, coreci_v8_state TO coreci_state,
        coreci_packages TO schema7_coreci_packages, coreci_v8_packages TO coreci_packages,
        coreci_prerequisites TO schema7_coreci_prerequisites, coreci_v8_prerequisites TO coreci_prerequisites,
        coreci_plan TO schema7_coreci_plan, coreci_v8_plan TO coreci_plan;
    DO RELEASE_LOCK(CONCAT('coreci:', DATABASE()));
END//
DELIMITER ;
