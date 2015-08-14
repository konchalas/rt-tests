-- SQL delta update from rteval-1.1.sql to rteval-1.2.sql

UPDATE rteval_info SET value = '1.2' WHERE key = 'sql_schema_ver';

ALTER TABLE rtevalruns ADD COLUMN distro VARCHAR(128);
ALTER TABLE rtevalruns_details ADD COLUMN num_cpu_cores INTEGER;
ALTER TABLE rtevalruns_details ADD COLUMN num_cpu_sockets INTEGER;
ALTER TABLE rtevalruns_details ADD COLUMN numa_nodes INTEGER;
