UPDATE _format SET version="14.4";
ALTER TABLE Prelude_AdditionalData CHANGE data data BLOB NOT NULL;
