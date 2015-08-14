-- SQL delta update from rteval-1.2.sql to rteval-1.3.sql

UPDATE rteval_info SET value = '1.3' WHERE key = 'sql_schema_ver';

ALTER TABLE rtevalruns_details ADD COLUMN annotation TEXT;
