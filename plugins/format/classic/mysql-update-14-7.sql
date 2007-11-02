BEGIN;

UPDATE _format SET version="14.7";
ALTER TABLE Prelude_Impact CHANGE description description TEXT NULL;

COMMIT;
