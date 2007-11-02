BEGIN;

UPDATE _format SET version='14.7';
ALTER TABLE Prelude_Impact ALTER COLUMN description TYPE TEXT NULL;

COMMIT;
