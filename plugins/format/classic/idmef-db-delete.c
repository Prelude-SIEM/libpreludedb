/*****
*
* Copyright (C) 2003-2005 Nicolas Delon <nicolas@prelude-ids.org>
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

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "idmef-db-delete.h"


static int delete_message(preludedb_sql_t *sql, uint64_t ident, unsigned int count, const char *queries[])
{
	int i;
	int ret;
	int tmp;

	ret = preludedb_sql_transaction_start(sql);
	if ( ret < 0 )
		return ret;

	for ( i = 0; i < count; i++ ) {
		ret = preludedb_sql_query_sprintf(sql, NULL, queries[i], ident);
		if ( ret < 0 )
			goto error;
	}

	return preludedb_sql_transaction_end(sql);

 error:
	tmp = preludedb_sql_transaction_abort(sql);

	return (tmp < 0) ? tmp : ret;
	
}



int delete_alert(preludedb_sql_t *sql, uint64_t ident)
{
	static const char *queries[] = {
		"DELETE FROM Prelude_Action WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_AdditionalData WHERE _parent_type = 'A' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Address WHERE _parent_type != 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Alert WHERE _ident = %" PRIu64,
		"DELETE FROM Prelude_Alertident WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Analyzer WHERE _parent_type = 'A' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_AnalyzerTime WHERE _parent_type = 'A' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Assessment WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Classification WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Reference WHERE _message_ident = %" PRIu64 "",
		"DELETE FROM Prelude_Confidence WHERE _message_ident = %" PRIu64 "",
		"DELETE FROM Prelude_CorrelationAlert WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_CreateTime WHERE _parent_type = 'A' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_DetectTime WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_File WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_FileAccess WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_FileAccess_Permission WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Impact WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Inode WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Checksum WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Linkage WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Node WHERE _parent_type != 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_OverflowAlert WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Process WHERE _parent_type != 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_ProcessArg WHERE _parent_type != 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_ProcessEnv WHERE _parent_type != 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_SNMPService WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Service WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Source WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Target WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_ToolAlert WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_User WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_UserId WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_WebService WHERE _message_ident = %" PRIu64,
		"DELETE FROM Prelude_WebServiceArg WHERE _message_ident = %" PRIu64
	};

	return delete_message(sql, ident, sizeof (queries) / sizeof (queries[0]), queries);
}



int delete_heartbeat(preludedb_sql_t *sql, uint64_t ident)
{
	static const char *queries[] = {
		"DELETE FROM Prelude_AdditionalData WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Address WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Analyzer WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_AnalyzerTime WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_CreateTime WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Node WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Process WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_ProcessArg WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_ProcessEnv WHERE _parent_type = 'H' AND _message_ident = %" PRIu64,
		"DELETE FROM Prelude_Heartbeat WHERE _ident = %" PRIu64,
	};

	return delete_message(sql, ident, sizeof (queries) / sizeof (queries[0]), queries);
}
