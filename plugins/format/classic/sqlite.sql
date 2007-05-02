
CREATE TABLE _format (
 name TEXT NOT NULL,
 version TEXT NOT NULL
);
INSERT INTO _format (name, version) VALUES('classic', '14.6');


CREATE TABLE Prelude_Alert (
 _ident INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
 messageid TEXT NULL
) ;

CREATE INDEX prelude_alert_messageid ON Prelude_Alert (messageid);



CREATE TABLE Prelude_Alertident (
 _message_ident INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 alertident TEXT NOT NULL,
 analyzerid TEXT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _index)
) ;




CREATE TABLE Prelude_ToolAlert (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 name TEXT NOT NULL,
 command TEXT NULL
) ;




CREATE TABLE Prelude_CorrelationAlert (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 name TEXT NOT NULL
) ;




CREATE TABLE Prelude_OverflowAlert (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 program TEXT NOT NULL,
 size INTEGER NULL,
 buffer BLOB NULL
) ;




CREATE TABLE Prelude_Heartbeat (
 _ident INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
 messageid TEXT NULL,
 heartbeat_interval INTEGER NULL
) ;




CREATE TABLE Prelude_Analyzer (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _index INTEGER NOT NULL,
 analyzerid TEXT NULL,
 name TEXT NULL,
 manufacturer TEXT NULL,
 model TEXT NULL,
 version TEXT NULL,
 class TEXT NULL,
 ostype TEXT NULL,
 osversion TEXT NULL,
 PRIMARY KEY (_parent_type,_message_ident,_index)
) ;

CREATE INDEX prelude_analyzer_analyzerid ON Prelude_Analyzer (_parent_type,_index,analyzerid);
CREATE INDEX prelude_analyzer_index_model ON Prelude_Analyzer (_parent_type,_index,model);




CREATE TABLE Prelude_Classification (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 ident TEXT NULL,
 text TEXT NOT NULL
) ;

CREATE INDEX prelude_classification_index_text ON Prelude_Classification (text);




CREATE TABLE Prelude_Reference (
 _message_ident INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 origin TEXT NOT NULL,
 name TEXT NOT NULL,
 url TEXT NOT NULL,
 meaning TEXT NULL,
 PRIMARY KEY (_message_ident, _index)
) ;

CREATE INDEX prelude_reference_index_name ON Prelude_Reference (name);




CREATE TABLE Prelude_Source (
 _message_ident INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 ident TEXT NULL,
 spoofed TEXT NOT NULL,
 interface TEXT NULL,
 PRIMARY KEY (_message_ident, _index)
) ;




CREATE TABLE Prelude_Target (
 _message_ident INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 ident TEXT NULL,
 decoy TEXT NOT NULL,
 interface TEXT NULL,
 PRIMARY KEY (_message_ident, _index)
) ;




CREATE TABLE Prelude_File (
 _message_ident INTEGER NOT NULL,
 _parent0_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 ident TEXT NULL,
 path TEXT NOT NULL,
 name TEXT NOT NULL,
 category TEXT NULL,
 create_time DATETIME NULL,
 create_time_gmtoff INTEGER NULL,
 modify_time DATETIME NULL,
 modify_time_gmtoff INTEGER NULL,
 access_time DATETIME NULL,
 access_time_gmtoff INTEGER NULL,
 data_size INTEGER NULL,
 disk_size INTEGER NULL,
 fstype TEXT NULL,
 file_type TEXT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _index)
) ;




CREATE TABLE Prelude_FileAccess (
 _message_ident INTEGER NOT NULL,
 _parent0_index INTEGER NOT NULL,
 _parent1_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) ;




CREATE TABLE Prelude_FileAccess_Permission (
 _message_ident INTEGER NOT NULL,
 _parent0_index INTEGER NOT NULL,
 _parent1_index INTEGER NOT NULL,
 _parent2_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 permission TEXT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _parent2_index, _index)
) ;




CREATE TABLE Prelude_Linkage (
 _message_ident INTEGER NOT NULL,
 _parent0_index INTEGER NOT NULL,
 _parent1_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 category TEXT NOT NULL,
 name TEXT NOT NULL,
 path TEXT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) ;




CREATE TABLE Prelude_Inode (
 _message_ident INTEGER NOT NULL,
 _parent0_index INTEGER NOT NULL,
 _parent1_index INTEGER NOT NULL,
 change_time DATETIME NULL,
 change_time_gmtoff INTEGER NULL, 
 number INTEGER NULL,
 major_device INTEGER NULL,
 minor_device INTEGER NULL,
 c_major_device INTEGER NULL,
 c_minor_device INTEGER NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index)
) ;




CREATE TABLE Prelude_Checksum (
 _message_ident INTEGER NOT NULL,
 _parent0_index INTEGER NOT NULL,
 _parent1_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 algorithm TEXT NOT NULL,
 value TEXT NOT NULL,
 checksum_key TEXT NULL, 
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) ;



CREATE TABLE Prelude_Impact (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 description TEXT NULL,
 severity TEXT NULL,
 completion TEXT NULL,
 type TEXT NOT NULL
) ;

CREATE INDEX prelude_impact_index_severity ON Prelude_Impact (severity);
CREATE INDEX prelude_impact_index_completion ON Prelude_Impact (completion);
CREATE INDEX prelude_impact_index_type ON Prelude_Impact (type);




CREATE TABLE Prelude_Action (
 _message_ident INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 description TEXT NULL,
 category TEXT NOT NULL,
 PRIMARY KEY (_message_ident, _index)
) ;




CREATE TABLE Prelude_Confidence (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 confidence FLOAT NULL,
 rating TEXT NOT NULL
) ;




CREATE TABLE Prelude_Assessment (
 _message_ident INTEGER NOT NULL PRIMARY KEY
) ;




CREATE TABLE Prelude_AdditionalData (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL,
 _index INTEGER NOT NULL,
 type TEXT NOT NULL,
 meaning TEXT NULL,
 data BLOB NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _index)
) ;




CREATE TABLE Prelude_CreateTime (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 time DATETIME NOT NULL,
 usec INTEGER NOT NULL,
 gmtoff INTEGER NOT NULL,
 PRIMARY KEY (_parent_type,_message_ident)
) ;

CREATE INDEX prelude_createtime_index ON Prelude_CreateTime (_parent_type,time);



CREATE TABLE Prelude_DetectTime (
 _message_ident INTEGER NOT NULL PRIMARY KEY,
 time DATETIME NOT NULL,
 usec INTEGER NOT NULL,
 gmtoff INTEGER NOT NULL
) ;

CREATE INDEX prelude_detecttime_index ON Prelude_DetectTime (time);



CREATE TABLE Prelude_AnalyzerTime (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 time DATETIME NOT NULL,
 usec INTEGER NOT NULL,
 gmtoff INTEGER NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident)
) ;

CREATE INDEX prelude_analyzertime_index ON Prelude_AnalyzerTime (_parent_type,time);




CREATE TABLE Prelude_Node (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 ident TEXT NULL,
 category TEXT NULL,
 location TEXT NULL,
 name TEXT NULL,
 PRIMARY KEY(_parent_type, _message_ident, _parent0_index)
) ;

CREATE INDEX prelude_node_index_location ON Prelude_Node (_parent_type,_parent0_index,location);
CREATE INDEX prelude_node_index_name ON Prelude_Node (_parent_type,_parent0_index,name);




CREATE TABLE Prelude_Address (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 ident TEXT NULL,
 category TEXT NOT NULL,
 vlan_name TEXT NULL,
 vlan_num INTEGER NULL,
 address TEXT NOT NULL,
 netmask TEXT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;

CREATE INDEX prelude_address_index_address ON Prelude_Address (_parent_type,_parent0_index,_index,address);




CREATE TABLE Prelude_User (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 ident TEXT NULL,
 category TEXT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;




CREATE TABLE Prelude_UserId (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 _parent1_index INTEGER NOT NULL,
 _parent2_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 ident TEXT NULL,
 type TEXT NOT NULL,
 name TEXT NULL,
 tty TEXT NULL,
 number INTEGER NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _parent1_index, _parent2_index, _index) 
) ;




CREATE TABLE Prelude_Process (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 ident TEXT NULL,
 name TEXT NOT NULL,
 pid INTEGER NULL,
 path TEXT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;




CREATE TABLE Prelude_ProcessArg (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL DEFAULT 'A', 
 _parent0_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 arg TEXT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;




CREATE TABLE Prelude_ProcessEnv (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 env TEXT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;




CREATE TABLE Prelude_Service (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 ident TEXT NULL,
 ip_version INTEGER NULL,
 name TEXT NULL,
 port INTEGER NULL,
 iana_protocol_number INTEGER NULL,
 iana_protocol_name TEXT NULL,
 portlist VARCHAR  NULL,
 protocol TEXT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;

CREATE INDEX prelude_service_index_protocol_port ON Prelude_Service (_parent_type,_parent0_index,protocol,port);
CREATE INDEX prelude_service_index_protocol_name ON Prelude_Service (_parent_type,_parent0_index,protocol,name);




CREATE TABLE Prelude_WebService (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 url TEXT NOT NULL,
 cgi TEXT NULL,
 http_method TEXT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;




CREATE TABLE Prelude_WebServiceArg (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL, 
 _parent0_index INTEGER NOT NULL,
 _index INTEGER NOT NULL,
 arg TEXT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;




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
