UPDATE _format SET version='14.4'; 

BEGIN;

ALTER TABLE Prelude_AdditionalData RENAME TO Prelude_AdditionalDataOld;

CREATE TABLE Prelude_AdditionalData (
 _message_ident INTEGER NOT NULL,
 _parent_type TEXT NOT NULL,
 _index INTEGER NOT NULL,
 type TEXT NOT NULL,
 meaning TEXT NULL,
 data BLOB NOT NULL,
 PRIMARY KEY (_parent_type, _message_ident, _index)
) ;

INSERT INTO Prelude_AdditionalData SELECT * FROM Prelude_AdditionalDataOld;
DROP TABLE Prelude_AdditionalDataOld;

COMMIT;
