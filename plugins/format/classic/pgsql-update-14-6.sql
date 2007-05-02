-- you can safely ignore error if the following action fails:
ALTER TABLE Prelude_Checksum DROP CONSTRAINT prelude_checksum_algorithm_check;

BEGIN;
UPDATE _format SET version='14.6';

ALTER TABLE Prelude_Alertident ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Alertident SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Alertident DROP COLUMN _index;
ALTER TABLE Prelude_Alertident RENAME temp_useless_column to _index;

ALTER TABLE Prelude_Service ADD COLUMN temp_useless_column INT2;
UPDATE Prelude_Service SET temp_useless_column = CAST(_parent0_index AS INT2);
ALTER TABLE Prelude_Service DROP COLUMN _index;
ALTER TABLE Prelude_Service RENAME temp_useless_column to _index;

ALTER TABLE Prelude_Service ADD COLUMN temp_useless_column INT2;
UPDATE Prelude_Service SET temp_useless_column = CAST(ip_version AS INT2);
ALTER TABLE Prelude_Service DROP COLUMN ip_version;
ALTER TABLE Prelude_Service RENAME temp_useless_column to ip_version;

ALTER TABLE Prelude_Service ADD COLUMN temp_useless_column INT2;
UPDATE Prelude_Service SET temp_useless_column = CAST(iana_protocol_number AS INT2);
ALTER TABLE Prelude_Service DROP COLUMN iana_protocol_number; 
ALTER TABLE Prelude_Service RENAME temp_useless_column to iana_protocol_number;

ALTER TABLE Prelude_Service ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Service SET temp_useless_column = CAST(port AS INT4);
ALTER TABLE Prelude_Service DROP COLUMN port;
ALTER TABLE Prelude_Service RENAME temp_useless_column to port;

ALTER TABLE Prelude_Checksum ADD CHECK ( algorithm IN ('MD4', 'MD5', 'SHA1', 'SHA2-256', 'SHA2-384', 'SHA2-512', 'CRC-32', 'Haval', 'Tiger', 'Gost'));

COMMIT;
