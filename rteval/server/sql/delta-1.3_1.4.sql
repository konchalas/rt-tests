-- SQL delta update from rteval-1.3.sql to rteval-1.4.sql

UPDATE rteval_info SET value = '1.4' WHERE key = 'sql_schema_ver';

ALTER TABLE rtevalruns_details ADD COLUMN cpu_core_spread INTEGER[];
