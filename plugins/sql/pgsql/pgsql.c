/*****
*
* Copyright (C) 2001, 2002 Vandoorselaere Yoann <yoann@mandrakesoft.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>
#include <libprelude/idmef-tree.h>
#include <libprelude/plugin-common.h>
#include <libprelude/config-engine.h>

#include <libprelude/prelude-io.h>
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-getopt.h>

#include <libpq-fe.h>

#include "config.h"

#include "sql-table.h"

#include "sql-connection-data.h"
#include "plugin-sql.h"



typedef enum {
	allocated = 1,
	connected = 2,
	connection_failed = 3,
	transaction = 4
} session_status_t;



typedef struct {
	session_status_t status;
	char *dbhost;
	char *dbport;
	char *dbname;
	char *dbuser;
	char *dbpass;
	PGconn *pgsql;
} session_t;


static plugin_sql_t plugin;



static void *db_setup(const char *dbhost, const char *dbport, const char *dbname, 
                      const char *dbuser, const char *dbpass)
{
        session_t *session;

        session = malloc(sizeof(*session));
        if ( ! session ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
	
	session->status = allocated;
	session->dbhost = (dbhost) ? strdup(dbhost) : NULL;
	session->dbport = (dbport) ? strdup(dbport) : NULL;
	session->dbname = (dbname) ? strdup(dbname) : NULL;
	session->dbuser = (dbuser) ? strdup(dbuser) : NULL;
	session->dbpass = (dbpass) ? strdup(dbpass) : NULL;
	
	return session;
}




static void cleanup(void *s)
{
        session_t *session = s;
	
	free(session->dbhost);
	free(session->dbport);
	free(session->dbname);
	free(session->dbuser);
	free(session->dbpass);
        free(session);
}




/*
 * Execute SQL query, do not return table
 */
static int db_command(void *s, const char *query)
{
	session_t *session = s;
        PGresult *ret;
        
	if ( session->status != connected && session->status != transaction )
		return -ERR_PLUGIN_DB_NOT_CONNECTED; 
        
#ifdef DEBUG
	log(LOG_INFO, "pgsql[%p]: %s\n", session, query);
#endif
        
        ret = PQexec(session->pgsql, query);

        if ( ! ret || (PQresultStatus(ret) != PGRES_COMMAND_OK && PQresultStatus(ret) != PGRES_TUPLES_OK) ) {
                log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, PQerrorMessage(session->pgsql));
                return -ERR_PLUGIN_DB_QUERY_ERROR;
        }

        return 0;
}



/*
 * end transaction
 */
static int db_rollback(void *s)
{
        session_t *session = s;
        int ret;
        
	if ( session->status != transaction )
		return -ERR_PLUGIN_DB_NO_TRANSACTION;
		
	ret = db_command(session, "ROLLBACK;");
	
	if (ret == 0)
		session->status = connected;
		
	return ret;
}



/*
 * closes the DB connection.
 */
static void db_close(void *s)
{
        session_t *session = s;
		
	if ( session->status == transaction )
		db_rollback(s);

        PQfinish(session->pgsql);
        
        cleanup(s);
}





/*
 * Escape string with single quote
 */
static char *db_escape(void *s, const char *str)
{
        char *ptr;
        int i, ok, len = strlen(str);

        ptr = malloc((len * 2) + 1);
        if ( ! ptr ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }

        for ( i = 0, ok = 0; i < len; i++ ) {

                if ( str[i] == '\'' ) {
                        ptr[ok++] = '\\';
                        ptr[ok++] = str[i];
                } else
                        ptr[ok++] = str[i];
        }

        ptr[ok] = '\0';
        
        return ptr;
}




static int get_query_result(const char *query, PGresult *res, prelude_sql_table_t *table) 
{
        int i, j;
        prelude_sql_row_t *row;
        const char *name;
        prelude_sql_field_t *field;
        int tuples, fields;
        prelude_sql_field_type_t type;
        
        tuples = PQntuples(res);
        fields = PQnfields(res);
        	
        for ( i = 0; i < tuples; i++ ) {
                
                row = prelude_sql_row_new(fields);
                if ( ! row ) {
                        log(LOG_ERR, "Query \"%s\": could not create table row\n", query);
                        errno = -ERR_PLUGIN_DB_RESULT_ROW_ERROR;
                        return -1;
                }
        		
                prelude_sql_table_add_row(table, row);
                
                for ( j = 0; j < fields; j++ ) {

                        /*
                         * determine and convert column type.
                         *
                         * FIXME: values are taken from pgsql/server/catalog/pg_type.h
                         * but maybe there is a more elegant way to do that
                         */

                        switch ( PQftype(res, j) ) {

                        case 20:
                                type = type_int64;
                                break;

                        case 23:
                                type = type_int32;
                                break;
                                
                        case 700:
                                type = type_float;
                                break;
                                
                        case 701:
                                type = type_double;
                                break;

                        default:
                                type = type_string;
                                break;
                        }

                        name = PQfname(res, j);
                        
                        field = prelude_sql_field_new(name, type, PQgetvalue(res, i, j));
                        if ( ! field ) {
                                log(LOG_ERR, "Query \"%s\": couldn't create result field \"%s\"\n", query, name);
                                errno = -ERR_PLUGIN_DB_RESULT_FIELD_ERROR;
                                return -1;
                        }
                        
                        prelude_sql_row_set_field(row, j, field);
        	}
        }

        return 0;
}



/*
 * Execute SQL query, return table
 */
static prelude_sql_table_t *db_query(void *s, const char *query)
{
        int ret;
        PGresult *res;
        prelude_sql_table_t *table;
        session_t *session = s;
       
	if ( session->status != connected && session->status != transaction ) {
                errno = -ERR_PLUGIN_DB_NOT_CONNECTED;
		return NULL;
	}

	table = prelude_sql_table_new("Results");
	if ( ! table ) {
		log(LOG_ERR, "Query: \"%s\": could not create result table\n", query);
		errno = -ERR_PLUGIN_DB_RESULT_TABLE_ERROR;
		return NULL;
	}

#ifdef DEBUG
	log(LOG_INFO, "pgsql: %s\n", query);
#endif
        
        res = PQexec(session->pgsql, query);
        if ( ! res || (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) ) {
                log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, PQerrorMessage(session->pgsql));
                PQclear(res);
                errno = -ERR_PLUGIN_DB_QUERY_ERROR;
                return NULL;
        }

        if ( PQresultStatus(res) != PGRES_TUPLES_OK ) {
                PQclear(res);
                return table;
        }
        
        ret = get_query_result(query, res, table);
        if ( ret < 0 ) {
                prelude_sql_table_destroy(table);
                table = NULL;
        }
        
	PQclear(res);

        return table;
}





/*
 * start transaction
 */
static int db_begin(void *s) 
{
        session_t *session = s;
        int ret;
        
	if ( session->status == transaction )
		return -ERR_PLUGIN_DB_NO_TRANSACTION;
       
	ret = db_command(session, "BEGIN;");
		
	if (ret == 0)
		session->status = transaction;
		
	return ret;
}




/*
 * commit transaction
 */
static int db_commit(void *s)
{
        session_t *session = s;
        int ret;
        
	if ( session->status != transaction )
		return -ERR_PLUGIN_DB_NO_TRANSACTION;
	
	ret = db_command(session, "COMMIT;");
	
	if (ret == 0)
		session->status = connected;
		
	return ret;
}




/*
 * Connect to the PgSQL database
 */
static int db_connect(void *s)
{
        int ret;
        session_t *session = s;
        
	if ( session->status != allocated &&
             session->status != connection_failed )
		return -ERR_PLUGIN_DB_ALREADY_CONNECTED;
	
        session->pgsql = PQsetdbLogin(session->dbhost, session->dbport, 
                                      NULL, NULL, session->dbname, session->dbuser, session->dbpass);

        ret = PQstatus(session->pgsql);
        
        if ( ret == CONNECTION_BAD) {
                log(LOG_INFO, "PgSQL connection failed: %s", PQerrorMessage(session->pgsql));
                PQfinish(session->pgsql);
                session->status = connection_failed;
                return -ERR_PLUGIN_DB_CONNECTION_FAILED;
        }

	session->status = connected;
        
        return 0;
}



plugin_generic_t *plugin_init(int argc, char **argv)
{
        
	/*
         * system-wide options for the plugin should go in here
         */
	
        plugin_set_name(&plugin, "PgSQL");
        plugin_set_desc(&plugin, "Will log all alert to a PostgreSQL database.");
        plugin_set_setup_func(&plugin, db_setup);
        plugin_set_connect_func(&plugin, db_connect);
        plugin_set_escape_func(&plugin, db_escape);
        plugin_set_command_func(&plugin, db_command);
        plugin_set_query_func(&plugin, db_query);
        plugin_set_begin_func(&plugin, db_begin);
        plugin_set_commit_func(&plugin, db_commit);
        plugin_set_rollback_func(&plugin, db_rollback);
        plugin_set_closing_func(&plugin, db_close);
        
	return (plugin_generic_t *) &plugin;
}




