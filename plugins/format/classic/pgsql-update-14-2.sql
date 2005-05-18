UPDATE _format SET version='14.2';

ALTER TABLE Prelude_Alertident ADD COLUMN _index INT4;
ALTER TABLE Prelude_Alertident ALTER COLUMN _index SET NOT NULL;
ALTER TABLE Prelude_Alertident ADD PRIMARY KEY (_parent_type, _message_ident, _index);
DROP INDEX prelude_alert_ident;

DROP INDEX prelude_analyzer_index_model;
CREATE INDEX prelude_analyzer_index_model ON Prelude_Analyzer (_parent_type,_index,model);

/* Update the data type of the column _index from Prelude_Analyzer */
ALTER TABLE Prelude_Analyzer ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Analyzer SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Analyzer DROP COLUMN _index;
ALTER TABLE Prelude_Analyzer RENAME temp_useless_column to _index;
/* Prelude_Analyzer->_index  data type updated*/

DROP INDEX prelude_classification_index;
CREATE INDEX prelude_classification_index_text ON Prelude_Classification (text);

ALTER TABLE Prelude_Reference ADD PRIMARY KEY (_message_ident, _index);
DROP INDEX prelude_reference_index_message_ident;

/* Update the data type of the column _index from Prelude_Reference */
ALTER TABLE Prelude_Reference ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Reference SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Reference DROP COLUMN _index;
ALTER TABLE Prelude_Reference RENAME temp_useless_column to _index;
/* Prelude_Reference->_index  data type updated*/

/* Update the data type of the column _index from Prelude_Source */
ALTER TABLE Prelude_Source ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Source SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Source DROP COLUMN _index;
ALTER TABLE Prelude_Source RENAME temp_useless_column to _index;
/* Prelude_Source->_index  data type updated*/

/* Update the data type of the column _index from Prelude_Target */
ALTER TABLE Prelude_Target ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Target SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Target DROP COLUMN _index;
ALTER TABLE Prelude_Target RENAME temp_useless_column to _index;
/* Prelude_Target->_index  data type updated*/

ALTER TABLE Prelude_File ADD COLUMN file_type VARCHAR(255) NULL;
/* Update the data type of the column _parent0_index from Prelude_File */
ALTER TABLE Prelude_File ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_File SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_File DROP COLUMN _parent0_index;
ALTER TABLE Prelude_File RENAME temp_useless_column to _parent0_index;
/* Prelude_File->_parent0_index  data type updated*/
/* Update the data type of the column _index from Prelude_File */
ALTER TABLE Prelude_File ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_File SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_File DROP COLUMN _index;
ALTER TABLE Prelude_File RENAME temp_useless_column to _index;
/* Prelude_File->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_FileAccess */
ALTER TABLE Prelude_FileAccess ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_FileAccess DROP COLUMN _parent0_index;
ALTER TABLE Prelude_FileAccess RENAME temp_useless_column to _parent0_index;
/* Prelude_FileAccess->_parent0_index  data type updated*/
/* Update the data type of the column _parent1_index from Prelude_FileAccess */
ALTER TABLE Prelude_FileAccess ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess SET temp_useless_column = CAST(_parent1_index AS INT4);
ALTER TABLE Prelude_FileAccess DROP COLUMN _parent1_index;
ALTER TABLE Prelude_FileAccess RENAME temp_useless_column to _parent1_index;
/* Prelude_FileAccess->_parent1_index  data type updated*/
/* Update the data type of the column _index from Prelude_FileAccess */
ALTER TABLE Prelude_FileAccess ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_FileAccess DROP COLUMN _index;
ALTER TABLE Prelude_FileAccess RENAME temp_useless_column to _index;
/* Prelude_FileAccess->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_FileAccess_Permission */
ALTER TABLE Prelude_FileAccess_Permission ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess_Permission SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_FileAccess_Permission DROP COLUMN _parent0_index;
ALTER TABLE Prelude_FileAccess_Permission RENAME temp_useless_column to _parent0_index;
/* Prelude_FileAccess_Permission->_parent0_index  data type updated*/
/* Update the data type of the column _parent1_index from Prelude_FileAccess_Permission */
ALTER TABLE Prelude_FileAccess_Permission ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess_Permission SET temp_useless_column = CAST(_parent1_index AS INT4);
ALTER TABLE Prelude_FileAccess_Permission DROP COLUMN _parent1_index;
ALTER TABLE Prelude_FileAccess_Permission RENAME temp_useless_column to _parent1_index;
/* Prelude_FileAccess_Permission->_parent1_index  data type updated*/
/* Update the data type of the column _parent2_index from Prelude_FileAccess_Permission */
ALTER TABLE Prelude_FileAccess_Permission ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess_Permission SET temp_useless_column = CAST(_parent2_index AS INT4);
ALTER TABLE Prelude_FileAccess_Permission DROP COLUMN _parent2_index;
ALTER TABLE Prelude_FileAccess_Permission RENAME temp_useless_column to _parent2_index;
/* Prelude_FileAccess_Permission->_parent2_index  data type updated*/
/* Update the data type of the column _index from Prelude_FileAccess_Permission */
ALTER TABLE Prelude_FileAccess_Permission ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_FileAccess_Permission SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_FileAccess_Permission DROP COLUMN _index;
ALTER TABLE Prelude_FileAccess_Permission RENAME temp_useless_column to _index;
/* Prelude_FileAccess_Permission->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_Linkage */
ALTER TABLE Prelude_Linkage ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Linkage SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_Linkage DROP COLUMN _parent0_index;
ALTER TABLE Prelude_Linkage RENAME temp_useless_column to _parent0_index;
/* Prelude_Linkage->_parent0_index  data type updated*/
/* Update the data type of the column _parent1_index from Prelude_Linkage */
ALTER TABLE Prelude_Linkage ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Linkage SET temp_useless_column = CAST(_parent1_index AS INT4);
ALTER TABLE Prelude_Linkage DROP COLUMN _parent1_index;
ALTER TABLE Prelude_Linkage RENAME temp_useless_column to _parent1_index;
/* Prelude_Linkage->_parent1_index  data type updated*/
/* Update the data type of the column _index from Prelude_Linkage */
ALTER TABLE Prelude_Linkage ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Linkage SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Linkage DROP COLUMN _index;
ALTER TABLE Prelude_Linkage RENAME temp_useless_column to _index;
/* Prelude_Linkage->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_Inode */
ALTER TABLE Prelude_Inode ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Inode SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_Inode DROP COLUMN _parent0_index;
ALTER TABLE Prelude_Inode RENAME temp_useless_column to _parent0_index;
/* Prelude_Inode->_parent0_index  data type updated*/
/* Update the data type of the column _parent1_index from Prelude_Inode */
ALTER TABLE Prelude_Inode ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Inode SET temp_useless_column = CAST(_parent1_index AS INT4);
ALTER TABLE Prelude_Inode DROP COLUMN _parent1_index;
ALTER TABLE Prelude_Inode RENAME temp_useless_column to _parent1_index;
/* Prelude_Inode->_parent1_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_Checksum */
ALTER TABLE Prelude_Checksum ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Checksum SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_Checksum DROP COLUMN _parent0_index;
ALTER TABLE Prelude_Checksum RENAME temp_useless_column to _parent0_index;
/* Prelude_Checksum->_parent0_index  data type updated*/
/* Update the data type of the column _parent1_index from Prelude_Checksum */
ALTER TABLE Prelude_Checksum ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Checksum SET temp_useless_column = CAST(_parent1_index AS INT4);
ALTER TABLE Prelude_Checksum DROP COLUMN _parent1_index;
ALTER TABLE Prelude_Checksum RENAME temp_useless_column to _parent1_index;
/* Prelude_Checksum->_parent1_index  data type updated*/
/* Update the data type of the column _index from Prelude_Checksum */
ALTER TABLE Prelude_Checksum ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Checksum SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Checksum DROP COLUMN _index;
ALTER TABLE Prelude_Checksum RENAME temp_useless_column to _index;
/* Prelude_Checksum->_index  data type updated*/

/* Update the data type of the column _index from Prelude_Action */
ALTER TABLE Prelude_Action ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Action SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Action DROP COLUMN _index;
ALTER TABLE Prelude_Action RENAME temp_useless_column to _index;
/* Prelude_Action->_index  data type updated*/

/* Update the data type of the column _index from Prelude_AdditionalData */
ALTER TABLE Prelude_AdditionalData ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_AdditionalData SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_AdditionalData DROP COLUMN _index;
ALTER TABLE Prelude_AdditionalData RENAME temp_useless_column to _index;
/* Prelude_AdditionalData->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_Node */
ALTER TABLE Prelude_Node ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Node SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_Node DROP COLUMN _parent0_index;
ALTER TABLE Prelude_Node RENAME temp_useless_column to _parent0_index;
/* Prelude_Node->_parent0_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_Address */
ALTER TABLE Prelude_Address ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Address SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_Address DROP COLUMN _parent0_index;
ALTER TABLE Prelude_Address RENAME temp_useless_column to _parent0_index;
/* Prelude_Address->_parent0_index  data type updated*/
/* Update the data type of the column _index from Prelude_Address */
ALTER TABLE Prelude_Address ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Address SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_Address DROP COLUMN _index;
ALTER TABLE Prelude_Address RENAME temp_useless_column to _index;
/* Prelude_Address->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_User */
ALTER TABLE Prelude_User ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_User SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_User DROP COLUMN _parent0_index;
ALTER TABLE Prelude_User RENAME temp_useless_column to _parent0_index;
/* Prelude_User->_parent0_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_UserId */
ALTER TABLE Prelude_UserId ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_UserId SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_UserId DROP COLUMN _parent0_index;
ALTER TABLE Prelude_UserId RENAME temp_useless_column to _parent0_index;
/* Prelude_UserId->_parent0_index  data type updated*/
/* Update the data type of the column _parent1_index from Prelude_UserId */
ALTER TABLE Prelude_UserId ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_UserId SET temp_useless_column = CAST(_parent1_index AS INT4);
ALTER TABLE Prelude_UserId DROP COLUMN _parent1_index;
ALTER TABLE Prelude_UserId RENAME temp_useless_column to _parent1_index;
/* Prelude_UserId->_parent1_index  data type updated*/
/* Update the data type of the column _parent2_index from Prelude_UserId */
ALTER TABLE Prelude_UserId ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_UserId SET temp_useless_column = CAST(_parent2_index AS INT4);
ALTER TABLE Prelude_UserId DROP COLUMN _parent2_index;
ALTER TABLE Prelude_UserId RENAME temp_useless_column to _parent2_index;
/* Prelude_UserId->_parent2_index  data type updated*/
/* Update the data type of the column _index from Prelude_UserId */
ALTER TABLE Prelude_UserId ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_UserId SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_UserId DROP COLUMN _index;
ALTER TABLE Prelude_UserId RENAME temp_useless_column to _index;
/* Prelude_UserId->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_Process */
ALTER TABLE Prelude_Process ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_Process SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_Process DROP COLUMN _parent0_index;
ALTER TABLE Prelude_Process RENAME temp_useless_column to _parent0_index;
/* Prelude_Process->_parent0_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_ProcessArg */
ALTER TABLE Prelude_ProcessArg ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_ProcessArg SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_ProcessArg DROP COLUMN _parent0_index;
ALTER TABLE Prelude_ProcessArg RENAME temp_useless_column to _parent0_index;
/* Prelude_ProcessArg->_parent0_index  data type updated*/
/* Update the data type of the column _index from Prelude_ProcessArg */
ALTER TABLE Prelude_ProcessArg ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_ProcessArg SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_ProcessArg DROP COLUMN _index;
ALTER TABLE Prelude_ProcessArg RENAME temp_useless_column to _index;
/* Prelude_ProcessArg->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_ProcessEnv */
ALTER TABLE Prelude_ProcessEnv ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_ProcessEnv SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_ProcessEnv DROP COLUMN _parent0_index;
ALTER TABLE Prelude_ProcessEnv RENAME temp_useless_column to _parent0_index;
/* Prelude_ProcessEnv->_parent0_index  data type updated*/
/* Update the data type of the column _index from Prelude_ProcessEnv */
ALTER TABLE Prelude_ProcessEnv ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_ProcessEnv SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_ProcessEnv DROP COLUMN _index;
ALTER TABLE Prelude_ProcessEnv RENAME temp_useless_column to _index;
/* Prelude_ProcessEnv->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_WebService */
ALTER TABLE Prelude_WebService ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_WebService SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_WebService DROP COLUMN _parent0_index;
ALTER TABLE Prelude_WebService RENAME temp_useless_column to _parent0_index;
/* Prelude_WebService->_parent0_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_WebServiceArg */
ALTER TABLE Prelude_WebServiceArg ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_WebServiceArg SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_WebServiceArg DROP COLUMN _parent0_index;
ALTER TABLE Prelude_WebServiceArg RENAME temp_useless_column to _parent0_index;
/* Prelude_WebServiceArg->_parent0_index  data type updated*/
/* Update the data type of the column _index from Prelude_WebServiceArg */
ALTER TABLE Prelude_WebServiceArg ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_WebServiceArg SET temp_useless_column = CAST(_index AS INT4);
ALTER TABLE Prelude_WebServiceArg DROP COLUMN _index;
ALTER TABLE Prelude_WebServiceArg RENAME temp_useless_column to _index;
/* Prelude_WebServiceArg->_index  data type updated*/

/* Update the data type of the column _parent0_index from Prelude_SNMPService */
ALTER TABLE Prelude_SNMPService ADD COLUMN temp_useless_column INT4;
UPDATE Prelude_SNMPService SET temp_useless_column = CAST(_parent0_index AS INT4);
ALTER TABLE Prelude_SNMPService DROP COLUMN _parent0_index;
ALTER TABLE Prelude_SNMPService RENAME temp_useless_column to _parent0_index;
/* Prelude_SNMPService->_parent0_index  data type updated*/

ALTER TABLE Prelude_Linkage ADD PRIMARY KEY (_message_ident, _parent0_index, _parent1_index, _index);

ALTER TABLE Prelude_Impact ADD CHECK (severity  IN ('info', 'low', 'medium', 'high')); 

DROP INDEX prelude_node_index_name;
DROP INDEX prelude_node_index_location;
CREATE INDEX prelude_node_index_location ON Prelude_Node (_parent_type,_parent0_index,location);
CREATE INDEX prelude_node_index_name ON Prelude_Node (_parent_type,_parent0_index,name);
DROP INDEX prelude_address_index_address;
CREATE INDEX prelude_address_index_address ON Prelude_Address (_parent_type,_parent0_index,_index,address);

DROP INDEX prelude_service_index_protocol_port;
DROP INDEX prelude_service_index_protocol_name;
CREATE INDEX prelude_service_index_protocol_port ON Prelude_Service (_parent_type,_parent0_index,protocol, port);
CREATE INDEX prelude_service_index_protocol_name ON Prelude_Service (_parent_type,_parent0_index,protocol, name);

