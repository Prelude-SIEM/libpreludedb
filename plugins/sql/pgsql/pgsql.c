/*****
*
* Copyright (C) 2001, 2002 Vandoorselaere Yoann <yoann@mandrakesoft.com>
* Copyright (C) 2002 Nicolas Delon <delon.nicolas@wanadoo.fr>
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
#include <libprelude/idmef.h>
#include <libprelude/plugin-common.h>
#include <libprelude/config-engine.h>

#include <libprelude/prelude-io.h>
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-getopt.h>

#include <libpq-fe.h>

#include "config.h"

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"



typedef enum {
	st_allocated = 1,
	st_connected,
	st_connection_failed,
	st_query
} session_status_t;



typedef struct {
	session_status_t status;
	int dberrno;
	char *dbhost;
	char *dbport;
	char *dbname;
	char *dbuser;
	char *dbpass;
	PGconn *pgsql;

	/* query dependant variable */
	int row;
} session_t;



static plugin_sql_t plugin;



static void *db_query(void *, const char *);


static void *db_setup(const char *dbhost, const char *dbport, const char *dbname, 
                      const char *dbuser, const char *dbpass)
{
        session_t *session;
	
        session = malloc(sizeof(*session));
	if ( ! session ) {
                log(LOG_ERR, "memory exhausted.\n");
		return NULL;
        }
	
	session->status = st_allocated;
	session->dberrno = 0;
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
 * start transaction
 */
static int db_begin(void *s) 
{
        session_t *session = s;

	session->dberrno = 0;
       
	db_query(session, "BEGIN;");
		
	return -session->dberrno;
}



/*
 * commit transaction
 */
static int db_commit(void *s)
{
        session_t *session = s;

	session->dberrno = 0;

	session->status = st_connected;
        	
	db_query(session, "COMMIT;");

	return -session->dberrno;
}



/*
 * end transaction
 */
static int db_rollback(void *s)
{
        session_t *session = s;

	session->dberrno = 0;

	session->status = st_connected;
        		
	db_query(session, "ROLLBACK;");
	
	return -session->dberrno;
}



/*
 * closes the DB connection.
 */
static void db_close(void *s)
{
        session_t *session = s;
		
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



/*
 * Execute SQL query, return table
 */
static void *db_query(void *s, const char *query)
{
        int ret;
        PGresult *res;
        session_t *session = s;

	session->dberrno = 0;

	switch ( session->status ) {

	case st_connected:
		break;
		
	case st_query:
		session->dberrno = ERR_PLUGIN_DB_DOUBLE_QUERY;
		return NULL;

	default:
		session->dberrno = ERR_PLUGIN_DB_NOT_CONNECTED;
		return NULL;
	}

#ifdef DEBUG
	log(LOG_INFO, "pgsql: %s\n", query);
#endif

	res = PQexec(session->pgsql, query);
	if ( ! res ) {
		log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, PQerrorMessage(session->pgsql));
		session->dberrno = ERR_PLUGIN_DB_QUERY_ERROR;
		return NULL;
	}

	ret = PQresultStatus(res);
	switch ( ret ) {
		
	case PGRES_COMMAND_OK:
		PQclear(res);
		return NULL;

	case PGRES_TUPLES_OK:
		session->status = st_query;
		session->row = 0;
		return res;

	default:
		log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, PQerrorMessage(session->pgsql));
		session->dberrno = ERR_PLUGIN_DB_QUERY_ERROR;
		PQclear(res);
	}

	return NULL;
}




/*
 * Connect to the PgSQL database
 */
static int db_connect(void *s)
{
        session_t *session = s;

	session->dberrno = 0;
	
	if ( session->status != st_allocated &&
             session->status != st_connection_failed ) {
		session->dberrno = ERR_PLUGIN_DB_ALREADY_CONNECTED;
		return -session->dberrno;
	}
	
        session->pgsql = PQsetdbLogin(session->dbhost, session->dbport, 
                                      NULL, NULL, session->dbname, session->dbuser, session->dbpass);

        if ( PQstatus(session->pgsql) == CONNECTION_BAD ) {
                log(LOG_INFO, "PgSQL connection failed: %s", PQerrorMessage(session->pgsql));
                PQfinish(session->pgsql);
                session->status = st_connection_failed;
		session->dberrno = ERR_PLUGIN_DB_CONNECTION_FAILED;
		return -session->dberrno;
        }

	session->status = st_connected;
        
        return 0;
}



static int db_errno(void *s)
{
	session_t *session = s;

	return session->dberrno;
}


static void db_table_free(void *s, void *t)
{
	session_t *session = s;
	PGresult *res = t;

	session->dberrno = 0;

	session->status = st_connected;

	if ( res )
		PQclear(res);
}



static const char *db_field_name(void *s, void *t, unsigned int i)
{
	session_t *session = s;
	PGresult *res = t;
	const char *name;

	session->dberrno = 0;

	name = PQfname(res, i);
	if ( ! name )
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM;

	return name;
}



static int db_field_num(void *s, void *t, const char *name)
{
	session_t *session = s;
	PGresult *res = t;
	int num;

	session->dberrno = 0;

	num = PQfnumber(res, name);
	if ( num == -1 )
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;

	return num;
}



/*
 * FIXME: type values are taken from pgsql/server/catalog/pg_type.h
 * but maybe there is a more elegant way to do that
 * 
 * Nicolas: the only "elegant way" to do it that seems to exist, is to query
 * the postgres table named pg_type 
 */
static prelude_sql_field_type_t db_field_type(void *s, void *t, unsigned int i)
{
	session_t *session = s;
	PGresult *res = t;

	session->dberrno = 0;

	switch ( PQftype(res, i) ) {
		
	case InvalidOid:
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM;
		return dbtype_unknown;
		
	case 20:
		return dbtype_int64;

	case 23:
		return dbtype_int32;

	case 700:
		return dbtype_float;

	case 701:
		return dbtype_double;

	default:
		return dbtype_string;
	}

	return dbtype_unknown;	
}



static prelude_sql_field_type_t db_field_type_by_name(void *s, void *t, const char *name)
{
	session_t *session = s;
	PGresult *res = t;
	int i;

	session->dberrno = 0;

	i = PQfnumber(res, name);
	if ( i == -1 ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;
		return dbtype_unknown;
	}

	return db_field_type(session, res, i);	
}



static unsigned int db_fields_num(void *s, void *t)
{
	session_t *session = s;
	PGresult *res = t;

	session->dberrno = 0;

	return PQnfields(res);
}



static unsigned int db_rows_num(void *s, void *t)
{
	session_t *session = s;
	PGresult *res = t;

	session->dberrno = 0;

	return PQntuples(res);
}



static void *db_row_fetch(void *s, void *t)
{
	session_t *session = s;
	PGresult *res = t;

	session->dberrno = 0;

	return ( session->row < PQntuples(res) ) ? ((void *) session->row++ + 1) : NULL;
}



static void *db_field_fetch(void *s, void *t, void *r, unsigned int i)
{
	session_t *session = s;
	PGresult *res = t;

	session->dberrno = 0;
	
	if ( i >= PQnfields(res) ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM;
		return NULL;
	}

	return ((void *) i + 1);
}



static void *db_field_fetch_by_name(void *s, void *t, void *r, const char *name)
{
	session_t *session = s;
	PGresult *res = t;
	int i;

	session->dberrno = 0;

	i = PQfnumber(res, name);
	if ( i == -1 ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;
		return NULL;
	}

	return db_field_fetch(session, res, r, i);
}



static const char *db_field_value(void *s, void *t, void *r, void *f)
{
	session_t *session = s;
	PGresult *res = t;
	unsigned int row = (unsigned int) r - 1;
	unsigned int field = (unsigned int) f - 1;

	session->dberrno = 0;

	return PQgetvalue(res, row, field);
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
        plugin_set_query_func(&plugin, db_query);
        plugin_set_begin_func(&plugin, db_begin);
        plugin_set_commit_func(&plugin, db_commit);
        plugin_set_rollback_func(&plugin, db_rollback);
        plugin_set_closing_func(&plugin, db_close);
        plugin_set_errno_func(&plugin, db_errno);
	plugin_set_table_free_func(&plugin, db_table_free);
	plugin_set_field_name_func(&plugin, db_field_name);
	plugin_set_field_num_func(&plugin, db_field_num);
	plugin_set_field_type_func(&plugin, db_field_type);
	plugin_set_field_type_by_name_func(&plugin, db_field_type_by_name);
	plugin_set_fields_num_func(&plugin, db_fields_num);
	plugin_set_rows_num_func(&plugin, db_rows_num);
	plugin_set_row_fetch_func(&plugin, db_row_fetch);
	plugin_set_field_fetch_func(&plugin, db_field_fetch);
	plugin_set_field_fetch_by_name_func(&plugin, db_field_fetch_by_name);
	plugin_set_field_value_func(&plugin, db_field_value);

	return (plugin_generic_t *) &plugin;
}
