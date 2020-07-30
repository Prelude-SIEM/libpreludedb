/*****
*
* Copyright (C) 2003-2020 CS GROUP - France. All Rights Reserved.
* Author: Nicolas Delon <nicolas.delon@prelude-ids.com>
*
* This file is part of the PreludeDB library.
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb.h"

#include "classic-delete.h"


static int delete_message(preludedb_sql_t *sql, unsigned int count, const char **queries, const char *idents)
{
        unsigned int i;
        int ret, tmp;

        ret = preludedb_sql_transaction_start(sql);
        if ( ret < 0 )
                return ret;

        for ( i = 0; i < count; i++ ) {

                ret = preludedb_sql_query_sprintf(sql, NULL, queries[i], idents);
                if ( ret < 0 )
                        goto error;
        }

        return preludedb_sql_transaction_end(sql);

 error:
        tmp = preludedb_sql_transaction_abort(sql);

        return (tmp < 0) ? tmp : ret;

}


static int do_delete_alert(preludedb_sql_t *sql, const char *idents)
{
        static const char *queries[] = {
                "DELETE FROM Prelude_Action WHERE _message_ident %s",
                "DELETE FROM Prelude_AdditionalData WHERE _message_ident %s AND _parent_type = 'A'",
                "DELETE FROM Prelude_Address WHERE _message_ident %s AND _parent_type != 'H'",
                "DELETE FROM Prelude_Alert WHERE _ident %s",
                "DELETE FROM Prelude_Alertident WHERE _message_ident %s",
                "DELETE FROM Prelude_Analyzer WHERE _message_ident %s AND _parent_type = 'A'",
                "DELETE FROM Prelude_AnalyzerTime WHERE _message_ident %s AND _parent_type = 'A'",
                "DELETE FROM Prelude_Assessment WHERE _message_ident %s",
                "DELETE FROM Prelude_Classification WHERE _message_ident %s",
                "DELETE FROM Prelude_Reference WHERE _message_ident %s",
                "DELETE FROM Prelude_Confidence WHERE _message_ident %s",
                "DELETE FROM Prelude_CorrelationAlert WHERE _message_ident %s",
                "DELETE FROM Prelude_CreateTime WHERE _message_ident %s AND _parent_type = 'A'",
                "DELETE FROM Prelude_DetectTime WHERE _message_ident %s",
                "DELETE FROM Prelude_File WHERE _message_ident %s",
                "DELETE FROM Prelude_FileAccess WHERE _message_ident %s",
                "DELETE FROM Prelude_FileAccess_Permission WHERE _message_ident %s",
                "DELETE FROM Prelude_Impact WHERE _message_ident %s",
                "DELETE FROM Prelude_Inode WHERE _message_ident %s",
                "DELETE FROM Prelude_Checksum WHERE _message_ident %s",
                "DELETE FROM Prelude_Linkage WHERE _message_ident %s",
                "DELETE FROM Prelude_Node WHERE _message_ident %s AND _parent_type != 'H'",
                "DELETE FROM Prelude_OverflowAlert WHERE _message_ident %s",
                "DELETE FROM Prelude_Process WHERE _message_ident %s AND _parent_type != 'H'",
                "DELETE FROM Prelude_ProcessArg WHERE _message_ident %s AND _parent_type != 'H'",
                "DELETE FROM Prelude_ProcessEnv WHERE _message_ident %s AND _parent_type != 'H'",
                "DELETE FROM Prelude_SnmpService WHERE _message_ident %s",
                "DELETE FROM Prelude_Service WHERE _message_ident %s",
                "DELETE FROM Prelude_Source WHERE _message_ident %s",
                "DELETE FROM Prelude_Target WHERE _message_ident %s",
                "DELETE FROM Prelude_ToolAlert WHERE _message_ident %s",
                "DELETE FROM Prelude_User WHERE _message_ident %s",
                "DELETE FROM Prelude_UserId WHERE _message_ident %s",
                "DELETE FROM Prelude_WebService WHERE _message_ident %s",
                "DELETE FROM Prelude_WebServiceArg WHERE _message_ident %s"
        };

        return delete_message(sql, sizeof(queries) / sizeof(*queries), queries, idents);
}



static int do_delete_heartbeat(preludedb_sql_t *sql, const char *idents)
{
        static const char *queries[] = {
                "DELETE FROM Prelude_AdditionalData WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_Address WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_Analyzer WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_AnalyzerTime WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_CreateTime WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_Node WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_Process WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_ProcessArg WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_ProcessEnv WHERE _parent_type = 'H' AND _message_ident %s",
                "DELETE FROM Prelude_Heartbeat WHERE _ident %s",
        };

        return delete_message(sql, sizeof(queries) / sizeof(*queries), queries, idents);
}


static ssize_t get_string_from_ident_list(prelude_string_t **out, uint64_t *ident, size_t size)
{
        int ret;
        size_t i;
        prelude_bool_t need_sep = FALSE;

        ret = prelude_string_new(out);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_cat(*out, "IN (");
        if ( ret < 0 )
                goto err;

        for ( i = 0; i < size; i++ ) {

                ret = prelude_string_sprintf(*out, "%s%" PRELUDE_PRIu64, (need_sep) ? ", " : "", ident[i]);
                if ( ret < 0 )
                        goto err;

                need_sep = TRUE;
        }

        ret = prelude_string_cat(*out, ")");
        if ( ret < 0 )
                goto err;

        return i;

 err:
        prelude_string_destroy(*out);
        return ret;
}



static ssize_t get_string_from_result_ident(prelude_string_t **out, preludedb_result_idents_t *res)
{
        int ret;
        uint64_t ident;
        unsigned int i = 0;
        prelude_bool_t need_sep = FALSE;

        ret = prelude_string_new(out);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_cat(*out, "IN (");
        if ( ret < 0 )
                goto err;

        while ( preludedb_result_idents_get(res, i, &ident) ) {

                ret = prelude_string_sprintf(*out, "%s%" PRELUDE_PRIu64, (need_sep) ? ", " : "", ident);
                if ( ret < 0 )
                        goto err;

                i++;
                need_sep = TRUE;
        }

        if ( i == 0 ) {
                ret = 0;
                goto err;
        }

        ret = prelude_string_cat(*out, ")");
        if ( ret < 0 )
                goto err;

        return i;

 err:
        prelude_string_destroy(*out);
        return ret;
}



int classic_delete_alert(preludedb_t *db, uint64_t ident)
{
        char buf[32];

        snprintf(buf, sizeof(buf), "= %" PRELUDE_PRIu64, ident);

        return do_delete_alert(preludedb_get_sql(db), buf);
}



int classic_delete_heartbeat(preludedb_t *db, uint64_t ident)
{
        char buf[32];

        snprintf(buf, sizeof(buf), "= %" PRELUDE_PRIu64, ident);

        return do_delete_heartbeat(preludedb_get_sql(db), buf);
}



ssize_t classic_delete_alert_from_result_idents(preludedb_t *db, preludedb_result_idents_t *results)
{
        int ret;
        ssize_t count;
        prelude_string_t *buf;

        count = get_string_from_result_ident(&buf, results);
        if ( count <= 0 )
                return count;

        ret = do_delete_alert(preludedb_get_sql(db), prelude_string_get_string(buf));
        prelude_string_destroy(buf);

        return (ret < 0) ? ret : count;
}


ssize_t classic_delete_alert_from_list(preludedb_t *db, uint64_t *ident, size_t size)
{
        int ret;
        ssize_t count;
        prelude_string_t *buf;

        count = get_string_from_ident_list(&buf, ident, size);
        if ( count < 0 )
                return count;

        ret = do_delete_alert(preludedb_get_sql(db), prelude_string_get_string(buf));
        prelude_string_destroy(buf);

        return (ret < 0) ? ret : count;
}



ssize_t classic_delete_heartbeat_from_result_idents(preludedb_t *db, preludedb_result_idents_t *results)
{
        int ret;
        ssize_t count;
        prelude_string_t *buf;

        count = get_string_from_result_ident(&buf, results);
        if ( count <= 0 )
                return count;

        ret = do_delete_heartbeat(preludedb_get_sql(db), prelude_string_get_string(buf));
        prelude_string_destroy(buf);

        return (ret < 0) ? ret : count;
}



ssize_t classic_delete_heartbeat_from_list(preludedb_t *db, uint64_t *ident, size_t size)
{
        int ret;
        ssize_t count;
        prelude_string_t *buf;

        count = get_string_from_ident_list(&buf, ident, size);
        if ( count < 0 )
                return count;

        ret = do_delete_heartbeat(preludedb_get_sql(db), prelude_string_get_string(buf));
        prelude_string_destroy(buf);

        return (ret < 0) ? ret : count;
}
