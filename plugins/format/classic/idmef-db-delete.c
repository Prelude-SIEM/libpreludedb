/*****
*
* Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
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
#include <libprelude/ntp.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-uident.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object.h"
#include "strbuf.h"

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


int	classic_delete_alert(prelude_db_connection_t *connection,
			     prelude_db_alert_uident_t *uident)
{
	prelude_sql_connection_t *sql;
	prelude_sql_table_t *table;
	uint64_t ident;

	ident = uident->alert_ident;

	sql = prelude_db_connection_get(connection);
	if ( ! sql ) {
		log(LOG_ERR, "not a SQL connection\n");
		return -1;
	}

	table = prelude_sql_query(sql, "SELECT * FROM Prelude_Alert WHERE ident = %llu", ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -2;
		}
		return 0;
	}

	prelude_sql_table_free(table);

	if ( prelude_sql_begin(sql) < 0 ) {
		db_log(sql);
		return -3;
	}

	db_query(sql, "DELETE FROM Prelude_Action WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_AdditionalData WHERE parent_type = 'A' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Address WHERE parent_type != 'H' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Alert WHERE ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Analyzer WHERE parent_type = 'A' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_AnalyzerTime WHERE parent_type = 'A' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Assessment WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Classification WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Confidence WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_CorrelationAlert WHERE ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_CorrelationAlert_Alerts WHERE ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_CreateTime WHERE parent_type = 'A' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_DetectTime WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_File WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_FileAccess WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_FileList WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Impact WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Inode WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Linkage WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Node WHERE parent_type != 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_OverflowAlert WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Process WHERE parent_type != 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessArg WHERE parent_type != 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessEnv WHERE parent_type != 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_SNMPService WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Service WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_ServicePortlist WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Source WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Target WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_ToolAlert WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_User WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_UserId WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_WebService WHERE alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_WebServiceArg WHERE alert_ident = %llu", ident);

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

	log(LOG_ERR, "deletion of alert %llu failed\n", ident);

	return -6;
}


int	classic_delete_heartbeat(prelude_db_connection_t *connection,
				 prelude_db_heartbeat_uident_t *uident)
{
	prelude_sql_connection_t *sql;
	prelude_sql_table_t *table;
	uint64_t ident;

	ident = uident->heartbeat_ident;

	sql = prelude_db_connection_get(connection);
	if ( ! sql ) {
		log(LOG_ERR, "not a SQL connection\n");
		return -1;
	}

	table = prelude_sql_query(sql, "SELECT * FROM Prelude_Heartbeat WHERE ident = %llu", ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -2;
		}
		return 0;
	}

	prelude_sql_table_free(table);

	if ( prelude_sql_begin(sql) < 0 ) {
		db_log(sql);
		return -3;
	}

	db_query(sql, "DELETE FROM Prelude_AdditionalData WHERE parent_type = 'H' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Address WHERE parent_type = 'H' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Analyzer WHERE parent_type = 'H' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_AnalyzerTime WHERE parent_type = 'H' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_CreateTime WHERE parent_type = 'H' AND parent_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Heartbeat WHERE ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Node WHERE parent_type = 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_Process WHERE parent_type = 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessArg WHERE parent_type = 'H' AND alert_ident = %llu", ident);
	db_query(sql, "DELETE FROM Prelude_ProcessEnv WHERE parent_type = 'H' AND alert_ident = %llu", ident);

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

	log(LOG_ERR, "deletion of alert %llu failed\n", ident);

	return -6;
}
