BEGIN;

UPDATE _format SET version="14.5";
ALTER TABLE Prelude_SnmpService RENAME TO Prelude_SnmpServiceOld;

CREATE TABLE Prelude_SnmpService (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 snmp_oid TEXT NULL, 
 message_processing_model INTEGER NULL,
 security_model INTEGER NULL,
 security_name TEXT NULL,
 security_level INTEGER NULL,
 context_name TEXT NULL,
 context_engine_id TEXT NULL,
 command TEXT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;

INSERT INTO Prelude_SnmpService (_message_ident, _parent_type, _parent0_index, snmp_oid, security_name, context_name, context_engine_id, command) SELECT _message_ident, _parent_type, _parent0_index, snmp_oid, security_name, context_name, context_engine_id, command FROM Prelude_SnmpServiceOld;

DROP TABLE Prelude_SnmpServiceOld;

COMMIT;
