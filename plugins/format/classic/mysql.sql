DROP TABLE IF EXISTS _format;

CREATE TABLE _format (
 name VARCHAR(255) NOT NULL
);
INSERT INTO _format (name) VALUES("classic");

DROP TABLE IF EXISTS Prelude_Alert;

CREATE TABLE Prelude_Alert (
 _ident BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 messageid VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Alertident;

CREATE TABLE Prelude_Alertident (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _index TINYINT NOT NULL,
 _parent_type ENUM('T','C') NOT NULL, # T=ToolAlert C=CorrelationAlert
 PRIMARY KEY (_parent_type, _message_ident, _index),
 alertident VARCHAR(255) NOT NULL,
 analyzerid VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_ToolAlert;

CREATE TABLE Prelude_ToolAlert (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 name VARCHAR(255) NOT NULL,
 command VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_CorrelationAlert;

CREATE TABLE Prelude_CorrelationAlert (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 name VARCHAR(255) NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_OverflowAlert;

CREATE TABLE Prelude_OverflowAlert (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 program VARCHAR(255) NOT NULL,
 size INTEGER UNSIGNED NULL,
 buffer BLOB NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Heartbeat;

CREATE TABLE Prelude_Heartbeat (
 _ident BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 messageid VARCHAR(255) NULL,
 heartbeat_interval INTEGER NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Analyzer;

CREATE TABLE Prelude_Analyzer (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H') NOT NULL, # A=Alert H=Hearbeat
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type,_message_ident,_index),
 analyzerid VARCHAR(255) NULL,
 INDEX (_parent_type,analyzerid),
 name VARCHAR(255) NULL,
 manufacturer VARCHAR(255) NULL,
 model VARCHAR(255) NULL,
 INDEX (_parent_type,model),
 version VARCHAR(255) NULL,
 class VARCHAR(255) NULL,
 ostype VARCHAR(255) NULL,
 osversion VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Classification;

CREATE TABLE Prelude_Classification (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 ident VARCHAR(255) NULL,
 text VARCHAR(255) NOT NULL,
 INDEX (text)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Reference;

CREATE TABLE Prelude_Reference (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _index),
 origin ENUM("unknown","vendor-specific","user-specific","bugtraqid","cve","osvdb") NOT NULL,
 name VARCHAR(255) NOT NULL,
 INDEX(name),
 url VARCHAR(255) NOT NULL,
 meaning VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Source;

CREATE TABLE Prelude_Source (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _index),
 ident VARCHAR(255) NULL,
 spoofed ENUM("unknown","yes","no") NOT NULL,
 interface VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Target;

CREATE TABLE Prelude_Target (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _index),
 ident VARCHAR(255) NULL,
 decoy ENUM("unknown","yes","no") NOT NULL,
 interface VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_File;

CREATE TABLE Prelude_File (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent0_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _index),
 ident VARCHAR(255) NULL,
 path VARCHAR(255) NOT NULL,
 name VARCHAR(255) NOT NULL,
 category ENUM("current", "original") NULL,
 create_time DATETIME NULL,
 create_time_gmtoff INTEGER NULL,
 modify_time DATETIME NULL,
 modify_time_gmtoff INTEGER NULL,
 access_time DATETIME NULL,
 access_time_gmtoff INTEGER NULL,
 data_size INT UNSIGNED NULL,
 disk_size INT UNSIGNED NULL,
 fstype ENUM("ufs", "efs", "nfs", "afs", "ntfs", "fat16", "fat32", "pcfs", "joliet", "iso9660") NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_FileAccess;

CREATE TABLE Prelude_FileAccess (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent0_index TINYINT NOT NULL,
 _parent1_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_FileAccess_Permission;

CREATE TABLE Prelude_FileAccess_Permission (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent0_index TINYINT NOT NULL,
 _parent1_index TINYINT NOT NULL,
 _parent2_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _parent2_index, _index),
 permission VARCHAR(255) NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Linkage;

CREATE TABLE Prelude_Linkage (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent0_index TINYINT NOT NULL,
 _parent1_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index),
 category ENUM("hard-link","mount-point","reparse-point","shortcut","stream","symbolic-link") NOT NULL,
 name VARCHAR(255) NOT NULL,
 path VARCHAR(255) NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Inode;

CREATE TABLE Prelude_Inode (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent0_index TINYINT NOT NULL,
 _parent1_index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index),
 change_time DATETIME NULL,
 change_time_gmtoff INTEGER NULL, 
 number INT UNSIGNED NULL,
 major_device INT UNSIGNED NULL,
 minor_device INT UNSIGNED NULL,
 c_major_device INT UNSIGNED NULL,
 c_minor_device INT UNSIGNED NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Checksum;

CREATE TABLE Prelude_Checksum (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent0_index BIGINT NOT NULL,
 _parent1_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index),
 algorithm ENUM("md4", "md5", "sha1", "sha2-256", "sha2-384", "sha2-512", "crc-32", "haval", "tiger", "gost") NOT NULL,
 value VARCHAR(255) NOT NULL,
 checksum_key VARCHAR(255) NULL # key is a reserved word
) TYPE=InnoDB;


DROP TABLE IF EXISTS Prelude_Impact;

CREATE TABLE Prelude_Impact (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 description VARCHAR(255) NULL,
 severity ENUM("low","medium","high") NULL,
 INDEX(severity),
 completion ENUM("failed", "succeeded") NULL,
 INDEX(completion),
 type ENUM("admin", "dos", "file", "recon", "user", "other") NOT NULL,
 INDEX(type)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Action;

CREATE TABLE Prelude_Action (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_message_ident, _index),
 description VARCHAR(255) NULL,
 category ENUM("block-installed", "notification-sent", "taken-offline", "other") NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Confidence;

CREATE TABLE Prelude_Confidence (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 confidence FLOAT NULL,
 rating ENUM("low", "medium", "high", "numeric") NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Assessment;

CREATE TABLE Prelude_Assessment (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_AdditionalData;

CREATE TABLE Prelude_AdditionalData (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A', 'H') NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _index),
 type ENUM("boolean","byte","character","date-time","integer","ntpstamp","portlist","real","string","byte-string","xml") NOT NULL,
 meaning VARCHAR(255) NULL,
 data BLOB NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_CreateTime;

CREATE TABLE Prelude_CreateTime (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H') NOT NULL, # A=Alert H=Hearbeat
 PRIMARY KEY (_parent_type,_message_ident),
 time DATETIME NOT NULL,
 usec INTEGER UNSIGNED NOT NULL,
 gmtoff INTEGER NOT NULL,
 INDEX (_parent_type,time)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_DetectTime;

CREATE TABLE Prelude_DetectTime (
 _message_ident BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 time DATETIME NOT NULL,
 usec INTEGER UNSIGNED NOT NULL,
 gmtoff INTEGER NOT NULL,	
 INDEX (time)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_AnalyzerTime;

CREATE TABLE Prelude_AnalyzerTime (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H') NOT NULL, # A=Alert H=Hearbeat
 PRIMARY KEY (_parent_type, _message_ident),
 time DATETIME NOT NULL,
 usec INTEGER UNSIGNED NOT NULL,
 gmtoff INTEGER NOT NULL,
 INDEX (_parent_type,time)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Node;

CREATE TABLE Prelude_Node (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H','S','T') NOT NULL, # A=Analyzer T=Target S=Source H=Heartbeat
 _parent0_index TINYINT NOT NULL,
 PRIMARY KEY(_parent_type, _message_ident, _parent0_index),
 ident VARCHAR(255) NULL,
 category ENUM("unknown","ads","afs","coda","dfs","dns","hosts","kerberos","nds","nis","nisplus","nt","wfw") NULL,
 location VARCHAR(255) NULL,
 INDEX(_parent_type,location),
 name VARCHAR(255) NULL,
 INDEX(_parent_type,name)
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Address;

CREATE TABLE Prelude_Address (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H','S','T') NOT NULL, # A=Analyser T=Target S=Source H=Heartbeat
 _parent0_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index),
 ident VARCHAR(255) NULL,
 category ENUM("unknown","atm","e-mail","lotus-notes","mac","sna","vm","ipv4-addr","ipv4-addr-hex","ipv4-net","ipv4-net-mask","ipv6-addr","ipv6-addr-hex","ipv6-net","ipv6-net-mask") NOT NULL,
 vlan_name VARCHAR(255) NULL,
 vlan_num INTEGER UNSIGNED NULL,
 address VARCHAR(255) NOT NULL,
 INDEX (_parent_type,address),
 netmask VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_User;

CREATE TABLE Prelude_User (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('S','T') NOT NULL, # T=Target S=Source
 _parent0_index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index),
 ident VARCHAR(255) NULL,
 category ENUM("unknown","application","os-device") NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_UserId;

CREATE TABLE Prelude_UserId (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('S','T', 'F') NOT NULL, # T=Target User S=Source User F=File Access 
 _parent0_index TINYINT NOT NULL,
 _parent1_index TINYINT NOT NULL,
 _parent2_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _parent1_index, _parent2_index, _index), # _parent_index1 and _parent2_index will always be zero if parent_type = 'F'
 ident VARCHAR(255) NULL,
 type ENUM("current-user","original-user","target-user","user-privs","current-group","group-privs","other-privs") NOT NULL,
 name VARCHAR(255) NULL,
 tty VARCHAR(255) NULL,
 number INTEGER UNSIGNED NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Process;

CREATE TABLE Prelude_Process (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H','S','T') NOT NULL, # A=Analyzer T=Target S=Source H=Heartbeat
 _parent0_index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index),
 ident VARCHAR(255) NULL,
 name VARCHAR(255) NOT NULL,
 pid INTEGER UNSIGNED NULL,
 path VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_ProcessArg;

CREATE TABLE Prelude_ProcessArg (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H','S','T') NOT NULL DEFAULT 'A', # A=Analyser T=Target S=Source
 _parent0_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index),
 arg VARCHAR(255) NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_ProcessEnv;

CREATE TABLE Prelude_ProcessEnv (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('A','H','S','T') NOT NULL, # A=Analyser T=Target S=Source
 _parent0_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index, _index),
 env VARCHAR(255) NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_Service;

CREATE TABLE Prelude_Service (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('S','T') NOT NULL, # T=Target S=Source
 _parent0_index BIGINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index),
 ident VARCHAR(255) NULL,
 ip_version TINYINT UNSIGNED NULL,
 name VARCHAR(255) NULL,
 port SMALLINT UNSIGNED NULL,
 iana_protocol_number TINYINT UNSIGNED NULL,
 iana_protocol_name VARCHAR(255) NULL,
 portlist VARCHAR (255) NULL,
 protocol VARCHAR(255) NULL,
 INDEX (_parent_type,protocol(10),port),
 INDEX (_parent_type,protocol(10),name(10))
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_WebService;

CREATE TABLE Prelude_WebService (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('S','T') NOT NULL, # T=Target S=Source
 _parent0_index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index),
 url VARCHAR(255) NOT NULL,
 cgi VARCHAR(255) NULL,
 http_method VARCHAR(255) NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_WebServiceArg;

CREATE TABLE Prelude_WebServiceArg (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('S','T') NOT NULL, # T=Target S=Source
 _parent0_index TINYINT NOT NULL,
 _index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index),
 arg VARCHAR(255) NOT NULL
) TYPE=InnoDB;



DROP TABLE IF EXISTS Prelude_SNMPService;

CREATE TABLE Prelude_SNMPService (
 _message_ident BIGINT UNSIGNED NOT NULL,
 _parent_type ENUM('S','T') NOT NULL, # T=Target S=Source
 _parent0_index TINYINT NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _parent0_index),
 snmp_oid VARCHAR(255) NULL, # oid is a reserved word in PostgreSQL 
 community VARCHAR(255) NULL,
 security_name VARCHAR(255) NULL,
 context_name VARCHAR(255) NULL,
 context_engine_id VARCHAR(255) NULL,
 command VARCHAR(255) NULL
) TYPE=InnoDB;
