/*****
*
* Copyright (C) 2003,2004 Nicolas Delon <nicolas@prelude-ids.org>
* All Rights Reserved
*
* This file is part of the Prelude program.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <time.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-message-ident.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object.h"


#include "idmef-db-delete.h"

#define db_log(sql) log(LOG_ERR, "%s\n", prelude_sql_error(sql))

#define db_query(sql, query, ident)			\
	do {						\
		prelude_sql_query(sql, query, ident);	\
							\
		if ( prelude_sql_errno(sql) ) {		\
			db_log(sql);			\
			goto error;			\
		}					\
	} while ( 0 )


int	delete_alert(prelude_db_connection_t *connection, uint64_t ident)
{
	prelude_sql_connection_t *sql;

	sql = prelude_db_connection_get(connection);
	if ( ! sql ) {
		log(LOG_ERR, "not a SQL connection\n");
		return -1;
	}

	if ( prelude_sql_begin(sql) < 0 ) {
		db_log(sql);
		return -3;
	}

	db_query(sql, "DELETE FROM Prelude_Action WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_AdditionalData WHERE _parent_type = 'A' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Address WHERE _parent_type != 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Alert WHERE _ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_AlertIdent WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Analyzer WHERE _parent_type = 'A' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_AnalyzerTime WHERE _parent_type = 'A' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Assessment WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Classification WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Reference WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Confidence WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_CorrelationAlert WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_CreateTime WHERE _parent_type = 'A' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_DetectTime WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_File WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_FileAccess WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_FileAccess_Permission WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Impact WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Inode WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Checksum WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Linkage WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Node WHERE _parent_type != 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_OverflowAlert WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Process WHERE _parent_type != 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessArg WHERE _parent_type != 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessEnv WHERE _parent_type != 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_SNMPService WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Service WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Source WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Target WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_ToolAlert WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_User WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_UserId WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_WebService WHERE _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_WebServiceArg WHERE _message_ident = %" PRIu64 "", ident);

	if ( prelude_sql_commit(sql) < 0 ) {
		db_log(sql);
		return -4;
	}

	return 1;

 error:
	if ( prelude_sql_rollback(sql) < 0 ) {
		log(LOG_ERR, "could not rollback: %s\n", prelude_sql_error(sql));
		return -5;
	}

	log(LOG_ERR, "deletion of alert %" PRIu64 " failed\n", ident);

	return -6;
}


int	delete_heartbeat(prelude_db_connection_t *connection, uint64_t ident)
{
	prelude_sql_connection_t *sql;

	sql = prelude_db_connection_get(connection);
	if ( ! sql ) {
		log(LOG_ERR, "not a SQL connection\n");
		return -1;
	}

	db_query(sql, "DELETE FROM Prelude_AdditionalData WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Address WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Analyzer WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_AnalyzerTime WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_CreateTime WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Node WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Process WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessArg WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessEnv WHERE _parent_type = 'H' AND _message_ident = %" PRIu64 "", ident);
	db_query(sql, "DELETE FROM Prelude_Heartbeat WHERE _ident = %" PRIu64 "", ident);

	if ( prelude_sql_commit(sql) < 0 ) {
		db_log(sql);
		return -4;
	}

	return 0;

 error:
	if ( prelude_sql_rollback(sql) < 0 ) {
		log(LOG_ERR, "could not rollback: %s\n", prelude_sql_error(sql));
		return -5;
	}

	log(LOG_ERR, "deletion of alert %" PRIu64 " failed\n", ident);

	return -6;
}
