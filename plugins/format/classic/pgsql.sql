DROP TABLE _format;

CREATE TABLE _format (
 name VARCHAR(255) NOT NULL
);
INSERT INTO _format (name) VALUES('classic');

DROP TABLE Prelude_Alert;

CREATE TABLE Prelude_Alert (
 _ident BIGSERIAL PRIMARY KEY,
 messageid NUMERIC(20) NOT NULL
);



DROP TABLE Prelude_AlertIdent;

CREATE TABLE Prelude_AlertIdent (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=ToolAlert C=CorrelationAlert */
 alertident NUMERIC(20) NOT NULL,
 analyzerid NUMERIC(20) NULL
);

CREATE INDEX prelude_alertident_index ON Prelude_AlertIdent (_parent_type, _message_ident);



DROP TABLE Prelude_ToolAlert;

CREATE TABLE Prelude_ToolAlert (
 _message_ident NUMERIC(20) PRIMARY KEY,
 name VARCHAR(255) NOT NULL,
 command VARCHAR(255) NULL
);



DROP TABLE Prelude_CorrelationAlert;

CREATE TABLE Prelude_CorrelationAlert (
 _message_ident NUMERIC(20) PRIMARY KEY,
 name VARCHAR(255) NOT NULL
);



DROP TABLE Prelude_OverflowAlert;

CREATE TABLE Prelude_OverflowAlert (
 _message_ident NUMERIC(20) PRIMARY KEY,
 program VARCHAR(255) NOT NULL,
 size NUMERIC(8) NULL,
 buffer BYTEA NULL
);



DROP TABLE Prelude_Heartbeat;

CREATE TABLE Prelude_Heartbeat (
 _ident BIGSERIAL PRIMARY KEY,
 messageid NUMERIC(20) NOT NULL
);



DROP TABLE Prelude_Analyzer;

CREATE TABLE Prelude_Analyzer (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Alert H=Hearbeat */
 _depth NUMERIC(1) NOT NULL,
 PRIMARY KEY (_parent_type,_message_ident,_depth),
 analyzerid NUMERIC(20) NOT NULL,
 name VARCHAR(255) NULL,
 manufacturer VARCHAR(255) NULL,
 model VARCHAR(255) NULL,
 version VARCHAR(255) NULL,
 class VARCHAR(255) NULL,
 ostype VARCHAR(255) NULL,
 osversion VARCHAR(255) NULL
);

CREATE INDEX prelude_analyzer_index_model ON Prelude_Analyzer (_parent_type,model);
CREATE INDEX prelude_analyzer_index_analyzerid ON Prelude_Analyzer (_parent_type,analyzerid);



DROP TABLE Prelude_Classification;

CREATE TABLE Prelude_Classification (
 _message_ident NUMERIC(20) PRIMARY KEY,
 ident NUMERIC(20) NOT NULL,
 text VARCHAR(255) NOT NULL
);

CREATE INDEX prelude_classification_index ON Prelude_Classification (text);



DROP TABLE Prelude_Reference;

CREATE TABLE Prelude_Reference (
 _message_ident NUMERIC(20) NOT NULL,
 origin VARCHAR(32) NOT NULL,
 name VARCHAR(255) NOT NULL,
 url VARCHAR(255) NOT NULL,
 meaning VARCHAR(255) NULL
);

CREATE INDEX prelude_reference_index_message_ident ON Prelude_Reference (_message_ident);
CREATE INDEX prelude_reference_index_name on Prelude_Reference (name);



DROP TABLE Prelude_Source;

CREATE TABLE Prelude_Source (
 _message_ident NUMERIC(20) NOT NULL,
 _index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_message_ident, _index),
 ident NUMERIC(20) NOT NULL,
 spoofed VARCHAR(32) NOT NULL,
 interface VARCHAR(255) NULL
);



DROP TABLE Prelude_Target;

CREATE TABLE Prelude_Target (
 _message_ident NUMERIC(20) NOT NULL,
 _index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_message_ident, _index),
 ident NUMERIC(20) NOT NULL,
 decoy VARCHAR(32) NOT NULL,
 interface VARCHAR(255) NULL
);



DROP TABLE Prelude_File;

CREATE TABLE Prelude_File (
 _message_ident NUMERIC(20) NOT NULL,
 _target_index NUMERIC(1) NOT NULL,
 _index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_message_ident, _target_index, _index),
 ident NUMERIC(20) NOT NULL,
 path VARCHAR(255) NOT NULL,
 name VARCHAR(255) NOT NULL,
 category VARCHAR(32) NULL,
 create_time TIMESTAMP NULL,
 create_time_gmtoff NUMERIC(8) NULL,
 modify_time TIMESTAMP NULL,
 modify_time_gmtoff NUMERIC(8) NULL,
 access_time TIMESTAMP NULL,
 access_time_gmtoff NUMERIC(8) NULL,
 data_size VARCHAR(8) NULL,
 disk_size VARCHAR(8) NULL,
 fstype VARCHAR(32) NULL
);



DROP TABLE Prelude_FileAccess;

CREATE TABLE Prelude_FileAccess (
 _message_ident NUMERIC(20) NOT NULL,
 _target_index NUMERIC(1) NOT NULL,
 _file_index NUMERIC(1) NOT NULL,
 _index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_message_ident, _target_index, _file_index, _index)
);



DROP TABLE Prelude_FileAccess_Permission;

CREATE TABLE Prelude_FileAccess_Permission (
 _message_ident NUMERIC(20) NOT NULL,
 _target_index NUMERIC(1) NOT NULL,
 _file_index NUMERIC(1) NOT NULL,
 _file_access_index NUMERIC(1) NOT NULL,
 perm VARCHAR(255) NOT NULL
);

CREATE INDEX prelude_fileaccess_permission_index ON Prelude_FileAccess_Permission (_message_ident, _target_index, _file_index, _file_access_index);



DROP TABLE Prelude_Linkage;

CREATE TABLE Prelude_Linkage (
 _message_ident NUMERIC(20) NOT NULL,
 _target_index NUMERIC(1) NOT NULL,
 _file_index NUMERIC(1) NOT NULL,
 category VARCHAR(32) NOT NULL,
 name VARCHAR(255) NOT NULL,
 path VARCHAR(255) NOT NULL
);

CREATE INDEX prelude_linkage_index ON Prelude_Linkage (_message_ident, _target_index, _file_index);



DROP TABLE Prelude_Inode;

CREATE TABLE Prelude_Inode (
 _message_ident NUMERIC(20) NOT NULL,
 _target_index NUMERIC(20) NOT NULL,
 _file_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_message_ident, _target_index, _file_index),
 change_time TIMESTAMP NULL,
 change_time_gmtoff NUMERIC(8) NULL, 
 number VARCHAR(8) NULL,
 major_device VARCHAR(8) NULL,
 minor_device VARCHAR(8) NULL,
 c_major_device VARCHAR(8) NULL,
 c_minor_device VARCHAR(8) NULL
);



DROP TABLE Prelude_Checksum;

CREATE TABLE Prelude_Checksum (
 _message_ident NUMERIC(20) NOT NULL,
 _target_index NUMERIC(20) NOT NULL,
 _file_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_message_ident, _target_index, _file_index),
 algorithm VARCHAR(32) NOT NULL,
 value VARCHAR(255) NOT NULL,
 checksum_key VARCHAR(255) NULL /* key is a reserved word */
);


DROP TABLE Prelude_Impact;

CREATE TABLE Prelude_Impact (
 _message_ident NUMERIC(20) PRIMARY KEY,
 description VARCHAR(255) NULL,
 severity VARCHAR(32) NULL,
 completion VARCHAR(32) NULL,
 type VARCHAR(32) NOT NULL
);

CREATE INDEX prelude_impact_index_severity ON Prelude_Impact (severity);
CREATE INDEX prelude_impact_index_completion ON Prelude_Impact (completion);
CREATE INDEX prelude_impact_index_type ON Prelude_Impact (type);



DROP TABLE Prelude_Action;

CREATE TABLE Prelude_Action (
 _message_ident NUMERIC(20) NOT NULL,
 description VARCHAR(255) NULL,
 category VARCHAR(32) NOT NULL
);

CREATE INDEX prelude_action_index ON Prelude_Action (_message_ident);



DROP TABLE Prelude_Confidence;

CREATE TABLE Prelude_Confidence (
 _message_ident NUMERIC(20) PRIMARY KEY,
 confidence FLOAT NULL,
 rating VARCHAR(32) NOT NULL
);



DROP TABLE Prelude_Assessment;

CREATE TABLE Prelude_Assessment (
 _message_ident NUMERIC(20) PRIMARY KEY
);



DROP TABLE Prelude_AdditionalData;

CREATE TABLE Prelude_AdditionalData (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL,
 type VARCHAR(32) NOT NULL,
 meaning VARCHAR(255) NULL,
 data BYTEA NULL
);

CREATE INDEX prelude_additional_data_index ON Prelude_AdditionalData (_parent_type, _message_ident);



DROP TABLE Prelude_CreateTime;

CREATE TABLE Prelude_CreateTime (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Alert H=Hearbeat */
 PRIMARY KEY (_parent_type,_message_ident),
 time TIMESTAMP NOT NULL,
 usec NUMERIC(8) NOT NULL,
 gmtoff NUMERIC(8) NOT NULL
);

CREATE INDEX prelude_createtime_index ON Prelude_CreateTime (_parent_type,time);



DROP TABLE Prelude_DetectTime;

CREATE TABLE Prelude_DetectTime (
 _message_ident NUMERIC(20) PRIMARY KEY,
 time TIMESTAMP NOT NULL,
 usec NUMERIC(8) NOT NULL,
 gmtoff NUMERIC(8) NOT NULL
);

CREATE INDEX prelude_detecttime_index ON Prelude_DetectTime (time);



DROP TABLE Prelude_AnalyzerTime;

CREATE TABLE Prelude_AnalyzerTime (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Alert H=Hearbeat */
 PRIMARY KEY (_parent_type, _message_ident),
 time TIMESTAMP NOT NULL,
 usec NUMERIC(8) NOT NULL,
 gmtoff NUMERIC(8) NOT NULL
);

CREATE INDEX prelude_analyzertime_index ON Prelude_AnalyzerTime (_parent_type,time);



DROP TABLE Prelude_Node;

CREATE TABLE Prelude_Node (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Analyzer T=Target S=Source H=Heartbeat */
 _parent_index NUMERIC(1) NOT NULL,
 PRIMARY KEY(_parent_type, _message_ident, _parent_index),
 ident NUMERIC(20) NOT NULL,
 category VARCHAR(32) NULL,
 location VARCHAR(255) NULL,
 name VARCHAR(255) NULL
);

CREATE INDEX prelude_node_index_name ON Prelude_Node (_parent_type, name);
CREATE INDEX prelude_node_index_location ON Prelude_Node (_parent_type, location);



DROP TABLE Prelude_Address;

CREATE TABLE Prelude_Address (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Analyzer T=Target S=Source H=Heartbeat */
 _parent_index NUMERIC(1) NOT NULL,
 ident NUMERIC(20) NOT NULL,
 category VARCHAR(32) NOT NULL,
 vlan_name VARCHAR(255) NULL,
 vlan_num NUMERIC(8) NULL,
 address VARCHAR(255) NOT NULL,
 netmask VARCHAR(255) NULL
);

CREATE INDEX prelude_address_index ON Prelude_Address (_parent_type, _message_ident, _parent_index);
CREATE INDEX prelude_address_index_address ON Prelude_Address (_parent_type, address);



DROP TABLE Prelude_User;

CREATE TABLE Prelude_User (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=Target S=Source */
 _parent_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent_index),
 ident NUMERIC(20) NOT NULL,
 category VARCHAR(32) NOT NULL
);



DROP TABLE Prelude_UserId;

CREATE TABLE Prelude_UserId (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=Target User S=Source User F=File Access */
 _parent_index NUMERIC(1) NOT NULL,
 _file_index NUMERIC(1) NOT NULL,
 _file_access_index NUMERIC(1) NOT NULL,
 ident NUMERIC(20) NOT NULL,
 type VARCHAR(32) NOT NULL,
 name VARCHAR(255) NULL,
 number NUMERIC(8) NULL
);

CREATE INDEX prelude_user_id_index ON Prelude_UserId (_parent_type, _message_ident, _parent_index, _file_index, _file_access_index); /* _file_index and _file_access_index will always be zero if parent_type = 'F' */



DROP TABLE Prelude_Process;

CREATE TABLE Prelude_Process (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Analyzer T=Target S=Source H=Heartbeat */
 _parent_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent_index),
 ident NUMERIC(20) NOT NULL,
 name VARCHAR(255) NOT NULL,
 pid NUMERIC(8) NULL,
 path VARCHAR(255) NULL
);



DROP TABLE Prelude_ProcessArg;

CREATE TABLE Prelude_ProcessArg (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL DEFAULT 'A', /* A=Analyser T=Target S=Source */
 _parent_index NUMERIC(1) NOT NULL,
 arg VARCHAR(255) NOT NULL
);

CREATE INDEX prelude_process_arg_index ON Prelude_ProcessArg (_parent_type, _message_ident, _parent_index);



DROP TABLE Prelude_ProcessEnv;

CREATE TABLE Prelude_ProcessEnv (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* A=Analyser T=Target S=Source */
 _parent_index NUMERIC(20) NOT NULL,
 env VARCHAR(255) NOT NULL
);

CREATE INDEX prelude_process_env_index ON Prelude_ProcessEnv (_parent_type, _message_ident, _parent_index);



DROP TABLE Prelude_Service;

CREATE TABLE Prelude_Service (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=Target S=Source */
 _parent_index NUMERIC(20) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent_index),
 ident NUMERIC(20) NOT NULL,
 ip_version NUMERIC(1) NULL,
 name VARCHAR(255) NULL,
 port NUMERIC(5) NULL,
 iana_protocol_number NUMERIC(1) NULL,
 iana_protocol_name VARCHAR(255) NULL,
 portlist VARCHAR (255) NULL,
 protocol VARCHAR(255) NULL
);

CREATE INDEX prelude_service_index_protocol_port ON Prelude_Service (_parent_type, protocol, port);
CREATE INDEX prelude_service_index_protocol_name ON Prelude_Service (_parent_type, protocol, name);



DROP TABLE Prelude_WebService;

CREATE TABLE Prelude_WebService (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=Target S=Source */
 _parent_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent_index),
 url VARCHAR(255) NOT NULL,
 cgi VARCHAR(255) NULL,
 http_method VARCHAR(255) NULL
);



DROP TABLE Prelude_WebServiceArg;

CREATE TABLE Prelude_WebServiceArg (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=Target S=Source */
 _parent_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent_index),
 arg VARCHAR(255) NOT NULL
);



DROP TABLE Prelude_SNMPService;

CREATE TABLE Prelude_SNMPService (
 _message_ident NUMERIC(20) NOT NULL,
 _parent_type VARCHAR(1) NOT NULL, /* T=Target S=Source */
 _parent_index NUMERIC(1) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent_index),
 snmp_oid VARCHAR(255) NULL, /* oid is a reserved word in PostgreSQL */
 community VARCHAR(255) NULL,
 security_name VARCHAR(255) NULL,
 context_name VARCHAR(255) NULL,
 context_engine_id VARCHAR(255) NULL,
 command VARCHAR(255) NULL
);
