BEGIN;
UPDATE _format SET version='14.6';

ALTER TABLE Prelude_Alertident ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Alertident SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Alertident DROP COLUMN _index;
ALTER TABLE Prelude_Alertident RENAME temp_useless_column to _index;

COMMIT;
