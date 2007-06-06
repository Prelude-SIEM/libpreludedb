-- you can safely ignore error if the following action fails:
ALTER TABLE Prelude_Checksum DROP CONSTRAINT prelude_checksum_algorithm_check;

BEGIN;
UPDATE _format SET version='14.6';

ALTER TABLE Prelude_Alertident ALTER COLUMN _index TYPE INT4;
ALTER TABLE Prelude_Service ALTER COLUMN _parent0_index TYPE INT2;
ALTER TABLE Prelude_Service ALTER COLUMN ip_version TYPE INT2;
ALTER TABLE Prelude_Service ALTER COLUMN iana_protocol_number TYPE INT2;
ALTER TABLE Prelude_Service ALTER COLUMN port TYPE INT4;


ALTER TABLE Prelude_Checksum ADD CHECK ( algorithm IN ('MD4', 'MD5', 'SHA1', 'SHA2-256', 'SHA2-384', 'SHA2-512', 'CRC-32', 'Haval', 'Tiger', 'Gost'));

ALTER TABLE Prelude_Alertident ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_ToolAlert ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_CorrelationAlert ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_OverflowAlert ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Analyzer ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Classification ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Reference ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Source ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Target ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_File ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_FileAccess ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_FileAccess_Permission ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Linkage ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Inode ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Checksum ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Impact ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Action ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Confidence ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Assessment ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_AdditionalData ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_CreateTime ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_DetectTime ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_AnalyzerTime ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Node ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Address ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_User ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_UserId ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Process ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_ProcessArg ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_ProcessEnv ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_Service ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_WebService ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_WebServiceArg ALTER COLUMN _message_ident TYPE INT8;
ALTER TABLE Prelude_SnmpService ALTER COLUMN _message_ident TYPE INT8;

COMMIT;
