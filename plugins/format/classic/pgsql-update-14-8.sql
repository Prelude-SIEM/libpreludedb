BEGIN;

UPDATE _format SET version='14.8';
ALTER TABLE _format ADD uuid VARCHAR(23) NULL;

COMMIT;
