BEGIN;

UPDATE _format SET version='14.5';
ALTER TABLE Prelude_SnmpService DROP COLUMN community;
ALTER TABLE Prelude_SnmpService ADD COLUMN message_processing_model INT8 NULL;
ALTER TABLE Prelude_SnmpService ADD COLUMN security_model INT8 NULL;
ALTER TABLE Prelude_SnmpService ADD COLUMN security_level INT8 NULL;

COMMIT;