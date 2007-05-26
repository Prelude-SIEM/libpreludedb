DROP TABLE _format;

CREATE TABLE _format (
 name VARCHAR(255) NOT NULL,
 version VARCHAR(255) NOT NULL
);
INSERT INTO _format (name, version) VALUES('classic', '14.7');

DROP TABLE Prelude_Alert;

CREATE TABLE Prelude_Alert (
 _ident BIGSERIAL PRIMARY KEY,
 messageid VARCHAR(255) NULL
) ;

CREATE INDEX prelude_alert_messageid ON Prelude_Alert (messageid);


DROP TABLE Prelude_Alertident;

CREATE TABLE Prelude_Alertident (
 _message_ident INT8 NOT NULL,
 _index INT4 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('T','C')) NOT NULL, 
 alertident VARCHAR(255) NOT NULL,
 analyzerid VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type, _message_ident, _index)
) ;



DROP TABLE Prelude_ToolAlert;

CREATE TABLE Prelude_ToolAlert (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 name VARCHAR(255) NOT NULL,
 command VARCHAR(255) NULL
) ;



DROP TABLE Prelude_CorrelationAlert;

CREATE TABLE Prelude_CorrelationAlert (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 name VARCHAR(255) NOT NULL
) ;



DROP TABLE Prelude_OverflowAlert;

CREATE TABLE Prelude_OverflowAlert (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 program VARCHAR(255) NOT NULL,
 size INT8 NULL,
 buffer BYTEA NULL
) ;



DROP TABLE Prelude_Heartbeat;

CREATE TABLE Prelude_Heartbeat (
 _ident BIGSERIAL PRIMARY KEY,
 messageid VARCHAR(255) NULL,
 heartbeat_interval INT4 NULL
) ;



DROP TABLE Prelude_Analyzer;

CREATE TABLE Prelude_Analyzer (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H')) NOT NULL, 
 _index INT2 NOT NULL,
 analyzerid VARCHAR(255) NULL,
 name VARCHAR(255) NULL,
 manufacturer VARCHAR(255) NULL,
 model VARCHAR(255) NULL,
 version VARCHAR(255) NULL,
 class VARCHAR(255) NULL,
 ostype VARCHAR(255) NULL,
 osversion VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type,_message_ident,_index)
) ;

CREATE INDEX prelude_analyzer_analyzerid ON Prelude_Analyzer (_parent_type,_index,analyzerid);
CREATE INDEX prelude_analyzer_index_model ON Prelude_Analyzer (_parent_type,_index,model);



DROP TABLE Prelude_Classification;

CREATE TABLE Prelude_Classification (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 ident VARCHAR(255) NULL,
 text VARCHAR(255) NOT NULL
) ;

CREATE INDEX prelude_classification_index_text ON Prelude_Classification (text);



DROP TABLE Prelude_Reference;

CREATE TABLE Prelude_Reference (
 _message_ident INT8 NOT NULL,
 _index INT2 NOT NULL,
 origin VARCHAR(32) CHECK ( origin IN ('unknown','vendor-specific','user-specific','bugtraqid','cve','osvdb')) NOT NULL,
 name VARCHAR(255) NOT NULL,
 url VARCHAR(255) NOT NULL,
 meaning VARCHAR(255) NULL,
 PRIMARY KEY (_message_ident, _index)
) ;

CREATE INDEX prelude_reference_index_name ON Prelude_Reference (name);



DROP TABLE Prelude_Source;

CREATE TABLE Prelude_Source (
 _message_ident INT8 NOT NULL,
 _index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 spoofed VARCHAR(32) CHECK ( spoofed IN ('unknown','yes','no')) NOT NULL,
 interface VARCHAR(255) NULL,
 PRIMARY KEY (_message_ident, _index)
) ;



DROP TABLE Prelude_Target;

CREATE TABLE Prelude_Target (
 _message_ident INT8 NOT NULL,
 _index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 decoy VARCHAR(32) CHECK ( decoy IN ('unknown','yes','no')) NOT NULL,
 interface VARCHAR(255) NULL,
 PRIMARY KEY (_message_ident, _index)
) ;



DROP TABLE Prelude_File;

CREATE TABLE Prelude_File (
 _message_ident INT8 NOT NULL,
 _parent0_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 path VARCHAR(255) NOT NULL,
 name VARCHAR(255) NOT NULL,
 category VARCHAR(32) CHECK ( category IN ('current', 'original')) NULL,
 create_time TIMESTAMP NULL,
 create_time_gmtoff INT4 NULL,
 modify_time TIMESTAMP NULL,
 modify_time_gmtoff INT4 NULL,
 access_time TIMESTAMP NULL,
 access_time_gmtoff INT4 NULL,
 data_size INT8 NULL,
 disk_size INT8 NULL,
 fstype VARCHAR(32) CHECK ( fstype IN ('ufs', 'efs', 'nfs', 'afs', 'ntfs', 'fat16', 'fat32', 'pcfs', 'joliet', 'iso9660')) NULL,
 file_type VARCHAR(255) NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _index)
) ;



DROP TABLE Prelude_FileAccess;

CREATE TABLE Prelude_FileAccess (
 _message_ident INT8 NOT NULL,
 _parent0_index INT2 NOT NULL,
 _parent1_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) ;



DROP TABLE Prelude_FileAccess_Permission;

CREATE TABLE Prelude_FileAccess_Permission (
 _message_ident INT8 NOT NULL,
 _parent0_index INT2 NOT NULL,
 _parent1_index INT2 NOT NULL,
 _parent2_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 permission VARCHAR(255) NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _parent2_index, _index)
) ;



DROP TABLE Prelude_Linkage;

CREATE TABLE Prelude_Linkage (
 _message_ident INT8 NOT NULL,
 _parent0_index INT2 NOT NULL,
 _parent1_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 category VARCHAR(32) CHECK ( category IN ('hard-link','mount-point','reparse-point','shortcut','stream','symbolic-link')) NOT NULL,
 name VARCHAR(255) NOT NULL,
 path VARCHAR(255) NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) ;



DROP TABLE Prelude_Inode;

CREATE TABLE Prelude_Inode (
 _message_ident INT8 NOT NULL,
 _parent0_index INT2 NOT NULL,
 _parent1_index INT2 NOT NULL,
 change_time TIMESTAMP NULL,
 change_time_gmtoff INT4 NULL, 
 number INT8 NULL,
 major_device INT8 NULL,
 minor_device INT8 NULL,
 c_major_device INT8 NULL,
 c_minor_device INT8 NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index)
) ;



DROP TABLE Prelude_Checksum;

CREATE TABLE Prelude_Checksum (
 _message_ident INT8 NOT NULL,
 _parent0_index INT2 NOT NULL,
 _parent1_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 algorithm VARCHAR(32) CHECK ( algorithm IN ('MD4', 'MD5', 'SHA1', 'SHA2-256', 'SHA2-384', 'SHA2-512', 'CRC-32', 'Haval', 'Tiger', 'Gost')) NOT NULL,
 value VARCHAR(255) NOT NULL,
 checksum_key VARCHAR(255) NULL, 
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) ;


DROP TABLE Prelude_Impact;

CREATE TABLE Prelude_Impact (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 description VARCHAR(255) NULL,
 severity VARCHAR(32) CHECK ( severity IN ('info', 'low','medium','high')) NULL,
 completion VARCHAR(32) CHECK ( completion IN ('failed', 'succeeded')) NULL,
 type VARCHAR(32) CHECK ( type IN ('admin', 'dos', 'file', 'recon', 'user', 'other')) NOT NULL
) ;

CREATE INDEX prelude_impact_index_severity ON Prelude_Impact (severity);
CREATE INDEX prelude_impact_index_completion ON Prelude_Impact (completion);
CREATE INDEX prelude_impact_index_type ON Prelude_Impact (type);



DROP TABLE Prelude_Action;

CREATE TABLE Prelude_Action (
 _message_ident INT8 NOT NULL,
 _index INT2 NOT NULL,
 description VARCHAR(255) NULL,
 category VARCHAR(32) CHECK ( category IN ('block-installed', 'notification-sent', 'taken-offline', 'other')) NOT NULL,
 PRIMARY KEY (_message_ident, _index)
) ;



DROP TABLE Prelude_Confidence;

CREATE TABLE Prelude_Confidence (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 confidence FLOAT NULL,
 rating VARCHAR(32) CHECK ( rating IN ('low', 'medium', 'high', 'numeric')) NOT NULL
) ;



DROP TABLE Prelude_Assessment;

CREATE TABLE Prelude_Assessment (
 _message_ident INT8 NOT NULL PRIMARY KEY
) ;



DROP TABLE Prelude_AdditionalData;

CREATE TABLE Prelude_AdditionalData (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A', 'H')) NOT NULL,
 _index INT2 NOT NULL,
 type VARCHAR(32) CHECK ( type IN ('boolean','byte','character','date-time','integer','ntpstamp','portlist','real','string','byte-string','xml')) NOT NULL,
 meaning VARCHAR(255) NULL,
 data BYTEA NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _index)
) ;



DROP TABLE Prelude_CreateTime;

CREATE TABLE Prelude_CreateTime (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H')) NOT NULL, 
 time TIMESTAMP NOT NULL,
 usec INT8 NOT NULL,
 gmtoff INT4 NOT NULL,
 PRIMARY KEY (_parent_type,_message_ident)
) ;

CREATE INDEX prelude_createtime_index ON Prelude_CreateTime (_parent_type,time);


DROP TABLE Prelude_DetectTime;

CREATE TABLE Prelude_DetectTime (
 _message_ident INT8 NOT NULL PRIMARY KEY,
 time TIMESTAMP NOT NULL,
 usec INT8 NOT NULL,
 gmtoff INT4 NOT NULL
) ;

CREATE INDEX prelude_detecttime_index ON Prelude_DetectTime (time);


DROP TABLE Prelude_AnalyzerTime;

CREATE TABLE Prelude_AnalyzerTime (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H')) NOT NULL, 
 time TIMESTAMP NOT NULL,
 usec INT8 NOT NULL,
 gmtoff INT4 NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident)
) ;

CREATE INDEX prelude_analyzertime_index ON Prelude_AnalyzerTime (_parent_type,time);



DROP TABLE Prelude_Node;

CREATE TABLE Prelude_Node (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H','S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 category VARCHAR(32) CHECK ( category IN ('unknown','ads','afs','coda','dfs','dns','hosts','kerberos','nds','nis','nisplus','nt','wfw')) NULL,
 location VARCHAR(255) NULL,
 name VARCHAR(255) NULL,
 PRIMARY KEY(_parent_type, _message_ident, _parent0_index)
) ;

CREATE INDEX prelude_node_index_location ON Prelude_Node (_parent_type,_parent0_index,location);
CREATE INDEX prelude_node_index_name ON Prelude_Node (_parent_type,_parent0_index,name);



DROP TABLE Prelude_Address;

CREATE TABLE Prelude_Address (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H','S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 category VARCHAR(32) CHECK ( category IN ('unknown','atm','e-mail','lotus-notes','mac','sna','vm','ipv4-addr','ipv4-addr-hex','ipv4-net','ipv4-net-mask','ipv6-addr','ipv6-addr-hex','ipv6-net','ipv6-net-mask')) NOT NULL,
 vlan_name VARCHAR(255) NULL,
 vlan_num INT8 NULL,
 address VARCHAR(255) NOT NULL,
 netmask VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;

CREATE INDEX prelude_address_index_address ON Prelude_Address (_parent_type,_parent0_index,_index,address);



DROP TABLE Prelude_User;

CREATE TABLE Prelude_User (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 category VARCHAR(32) CHECK ( category IN ('unknown','application','os-device')) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;



DROP TABLE Prelude_UserId;

CREATE TABLE Prelude_UserId (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('S','T', 'F')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 _parent1_index INT2 NOT NULL,
 _parent2_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 type VARCHAR(32) CHECK ( type IN ('current-user','original-user','target-user','user-privs','current-group','group-privs','other-privs')) NOT NULL,
 name VARCHAR(255) NULL,
 tty VARCHAR(255) NULL,
 number INT8 NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _parent1_index, _parent2_index, _index) 
) ;



DROP TABLE Prelude_Process;

CREATE TABLE Prelude_Process (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H','S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 name VARCHAR(255) NOT NULL,
 pid INT8 NULL,
 path VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;



DROP TABLE Prelude_ProcessArg;

CREATE TABLE Prelude_ProcessArg (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H','S','T')) NOT NULL DEFAULT 'A', 
 _parent0_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 arg VARCHAR(255) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;



DROP TABLE Prelude_ProcessEnv;

CREATE TABLE Prelude_ProcessEnv (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('A','H','S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 env VARCHAR(255) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;



DROP TABLE Prelude_Service;

CREATE TABLE Prelude_Service (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 ident VARCHAR(255) NULL,
 ip_version INT2 NULL,
 name VARCHAR(255) NULL,
 port INT4 NULL,
 iana_protocol_number INT2 NULL,
 iana_protocol_name VARCHAR(255) NULL,
 portlist VARCHAR (255) NULL,
 protocol VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;

CREATE INDEX prelude_service_index_protocol_port ON Prelude_Service (_parent_type,_parent0_index,protocol,port);
CREATE INDEX prelude_service_index_protocol_name ON Prelude_Service (_parent_type,_parent0_index,protocol,name);



DROP TABLE Prelude_WebService;

CREATE TABLE Prelude_WebService (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 url VARCHAR(255) NOT NULL,
 cgi VARCHAR(255) NULL,
 http_method VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;



DROP TABLE Prelude_WebServiceArg;

CREATE TABLE Prelude_WebServiceArg (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 _index INT2 NOT NULL,
 arg VARCHAR(255) NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index)
) ;



DROP TABLE Prelude_SnmpService;

CREATE TABLE Prelude_SnmpService (
 _message_ident INT8 NOT NULL,
 _parent_type VARCHAR(1) CHECK (_parent_type IN ('S','T')) NOT NULL, 
 _parent0_index INT2 NOT NULL,
 snmp_oid VARCHAR(255) NULL, 
 message_processing_model INT8 NULL,
 security_model INT8 NULL,
 security_name VARCHAR(255) NULL,
 security_level INT8 NULL,
 context_name VARCHAR(255) NULL,
 context_engine_id VARCHAR(255) NULL,
 command VARCHAR(255) NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index)
) ;
