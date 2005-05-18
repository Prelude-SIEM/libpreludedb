ALTER TABLE Prelude_Impact CHANGE severity severity ENUM("info", "low", "medium", "high") NULL;
UPDATE _format SET version="14.2";

ALTER TABLE Prelude_Address DROP INDEX _parent_type;
CREATE INDEX prelude_address_index_address ON Prelude_Address (_parent_type,_parent0_index,_index,address(10)); 

ALTER TABLE Prelude_Analyzer DROP INDEX _parent_type;
ALTER TABLE Prelude_Analyzer DROP INDEX _parent_type_2;
CREATE INDEX prelude_analyzer_index_analyzerid ON Prelude_Analyzer (_parent_type, _index, analyzerid); 
CREATE INDEX prelude_analyzer_index_model ON Prelude_Analyzer (_parent_type, _index, model); 

ALTER TABLE Prelude_Classification DROP INDEX text;
CREATE INDEX prelude_classification_index_text ON Prelude_Classification (text(40));  

ALTER TABLE Prelude_File ADD COLUMN file_type VARCHAR(255) NULL;

ALTER TABLE Prelude_Node DROP INDEX _parent_type;
ALTER TABLE Prelude_Node DROP INDEX _parent_type_2;
CREATE INDEX prelude_node_index_location ON Prelude_Node (_parent_type,_parent0_index,location(20));
CREATE INDEX prelude_node_index_name ON Prelude_Node (_parent_type,_parent0_index,name(20)); 

ALTER TABLE Prelude_Reference DROP INDEX name;
CREATE INDEX prelude_reference_index_name ON Prelude_Reference (name(40)); 


ALTER TABLE Prelude_Service  DROP INDEX prelude_service_index_protocol_port;
ALTER TABLE Prelude_Service  DROP INDEX prelude_service_index_protocol_name;
CREATE INDEX prelude_service_index_protocol_port ON Prelude_Service (_parent_type,_parent0_index,protocol(10),port);
CREATE INDEX prelude_service_index_protocol_name ON Prelude_Service (_parent_type,_parent0_index,protocol(10),name(10));  
