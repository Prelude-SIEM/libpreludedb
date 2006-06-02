BEGIN;

UPDATE _format SET version='14.5';
ALTER TABLE Prelude_SnmpService DROP COLUMN community;
ALTER TABLE Prelude_SnmpService ADD COLUMN message_processing_model INT8 NULL;
ALTER TABLE Prelude_SnmpService ADD COLUMN security_model INT8 NULL;
ALTER TABLE Prelude_SnmpService ADD COLUMN security_level INT8 NULL;

ALTER TABLE Prelude_Checksum DROP CONSTRAINT prelude_checksum_algorithm_check;
ALTER TABLE Prelude_Checksum ADD  CONSTRAINT prelude_checksum_algorithm_check CHECK (algorithm IN ('MD4', 'MD5', 'SHA1', 'SHA2-256', 'SHA2-384', 'SHA2-512', 'CRC-32', 'Haval', 'Tiger', 'Gost'));

COMMIT;
