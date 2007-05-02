BEGIN;

UPDATE _format SET version="14.6";

ALTER TABLE Prelude_Alertident CHANGE _index _index INTEGER NOT NULL;
ALTER TABLE Prelude_Source CHANGE _index _index SMALLINT NOT NULL;
ALTER TABLE Prelude_Target CHANGE _index _index SMALLINT NOT NULL;
ALTER TABLE Prelude_File CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_FileAccess CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_FileAccess_Permission CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Linkage CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Inode CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Checksum CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Node CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Address CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_User CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_UserId CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Process CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_ProcessArg CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_ProcessEnv CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_Service CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_WebService CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_WebServiceArg CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;
ALTER TABLE Prelude_SnmpService CHANGE _parent0_index _parent0_index SMALLINT NOT NULL;


ALTER TABLE Prelude_Checksum CHANGE algorithm algorithm ENUM("MD4", "MD5", "SHA1", "SHA2-256", "SHA2-384", "SHA2-512", "CRC-32", "Haval", "Tiger", "Gost") NOT NULL;

COMMIT;
