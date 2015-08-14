-- SQL delta update from rteval-1.0.sql to rteval-1.1.sql

CREATE TABLE rteval_info (
       key    varchar(32) NOT NULL,
       value  TEXT NOT NULL,
       rtiid  SERIAL,
       PRIMARY KEY(rtiid)
);
GRANT SELECT ON rteval_info TO rtevparser;
INSERT INTO rteval_info (key, value) VALUES ('sql_schema_ver','1.1');

ALTER TABLE cyclic_statistics ADD COLUMN mean_abs_dev REAL;
ALTER TABLE cyclic_statistics ADD COLUMN variance REAL;

