/*****
*
* Copyright (C) 2001-2004 Vandoorselaere Yoann <yoann@prelude-ids.org>
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

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include <libpq-fe.h>

#include "config.h"

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"

prelude_plugin_generic_t *pgsql_LTX_prelude_plugin_init(void);


typedef enum {
	st_allocated,
	st_connection_failed,
	st_connected,
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



static void *db_query(void *s, const char *query);



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

        if ( session->dbhost )
                free(session->dbhost);

        if ( session->dbport )
                free(session->dbport);

        if ( session->dbname )
                free(session->dbname);

        if ( session->dbuser )
                free(session->dbuser);

        if ( session->dbpass )
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

	if ( session->status < st_connected ) {
		session->dberrno = ERR_PLUGIN_DB_NOT_CONNECTED;
		return -session->dberrno;
	}
       
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
static char *db_escape(void *s, const char *str, size_t len)
{
        char *ptr;
        size_t rlen;
        int i, ok = 0;

        if ( ! str )
                return strdup("NULL");

        rlen = len * 2 + 3;
        if ( rlen <= len )
                return NULL;
        
        ptr = malloc(rlen);
        if ( ! ptr ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }

        ptr[ok++] = '\'';
        
        for ( i = 0; i < len; i++ ) {

                if ( str[i] == '\'' ) {
                        ptr[ok++] = '\\';
                        ptr[ok++] = str[i];
                } else
                        ptr[ok++] = str[i];
        }

        ptr[ok++] = '\'';
        ptr[ok] = '\0';
        
        return ptr;
}


static const char *db_limit_offset(void *s, int limit, int offset)
{
	static char buffer[64];
	int ret;

	if ( limit >= 0 ) {
		if ( offset >= 0 )
			ret = snprintf(buffer, sizeof (buffer), "LIMIT %d OFFSET %d", limit, offset);
		else
			ret = snprintf(buffer, sizeof (buffer), "LIMIT %d", limit);

		return (ret < 0) ? NULL : buffer;		
	}

	return "";
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

	if ( session->status < st_connected ) {
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
		if ( PQntuples(res) == 0 ) {
			PQclear(res);
			return NULL;
		}

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

	if ( session->status >= st_connected ) {
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



static void *db_field_fetch(void *s, void *t, void *r, unsigned int field)
{
	session_t *session = s;
	PGresult *res = t;
	unsigned int row = (unsigned int) r - 1;

	session->dberrno = 0;
	
	if ( field >= PQnfields(res) ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM;
		return NULL;
	}

	return PQgetisnull(res, row, field) ? NULL : ((void *) field + 1);
}



static void *db_field_fetch_by_name(void *s, void *t, void *r, const char *name)
{
	session_t *session = s;
	PGresult *res = t;
	int field;

	session->dberrno = 0;

	field = PQfnumber(res, name);
	if ( field == -1 ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;
		return NULL;
	}

	return db_field_fetch(session, res, r, field);
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



static int db_build_time_constraint(prelude_string_t *output, const char *field,
				    prelude_sql_time_constraint_type_t type,
				    idmef_value_relation_t relation, int value, int gmt_offset)
{
	char buf[128];
	const char *sql_relation;

	if ( snprintf(buf, sizeof (buf), "%s + INTERVAL '%d HOUR'", field, gmt_offset / 3600) < 0 )
		return -1;

	sql_relation = prelude_sql_idmef_relation_to_string(relation);
	if ( ! sql_relation )
		return -1;

	switch ( type ) {
	case dbconstraint_year:
		return prelude_string_sprintf(output, "EXTRACT(YEAR FROM %s) %s %d",
					      buf, sql_relation, value);

	case dbconstraint_month:
		return  prelude_string_sprintf(output, "EXTRACT(MONTH FROM %s) %s %d",
					       buf, sql_relation, value);

	case dbconstraint_yday:
		return prelude_string_sprintf(output, "EXTRACT(DOY FROM %s) %s %d",
					      buf, sql_relation, value);

	case dbconstraint_mday:
		return prelude_string_sprintf(output, "EXTRACT(DAY FROM %s) %s %d",
					      buf, sql_relation, value);

	case dbconstraint_wday:
		return prelude_string_sprintf(output, "EXTRACT(DOW FROM %s) %s %d",
					      buf, sql_relation, value % 7 + 1);

	case dbconstraint_hour:
		return prelude_string_sprintf(output, "EXTRACT(HOUR FROM %s) %s %d",
					      buf, sql_relation, value);

	case dbconstraint_min:
		return prelude_string_sprintf(output, "EXTRACT(MINUTE FROM %s) %s %d",
					      buf, sql_relation, value);

	case dbconstraint_sec:
		return prelude_string_sprintf(output, "EXTRACT(SECOND FROM %s) %s %d",
					      buf, sql_relation, value);
	}

	/* not reached */

	return -1;
}



static int db_build_time_interval(prelude_sql_time_constraint_type_t type, int value,
				  char *buf, size_t size)
{
	char *type_str;
	int ret;

	switch ( type ) {
	case dbconstraint_year:
		type_str = "YEAR";
		break;

	case dbconstraint_month:
		type_str = "MONTH";
		break;

	case dbconstraint_mday:
		type_str = "DAY";
		break;

	case dbconstraint_hour:
		type_str = "HOUR";
		break;

	case dbconstraint_min:
		type_str = "MINUTE";
		break;

	case dbconstraint_sec:
		type_str = "SECOND";
		break;

	default:
		return -1;
	}

	ret = snprintf(buf, size, "INTERVAL '%d %s'", value, type_str);

	return (ret < 0 || ret >= size) ? -1 : 0;
}



prelude_plugin_generic_t *pgsql_LTX_prelude_plugin_init(void)
{
        
	/*
         * system-wide options for the plugin should go in here
         */
	
        prelude_plugin_set_name(&plugin, "PgSQL");
        prelude_plugin_set_desc(&plugin, "Will log all alert to a PostgreSQL database.");
        plugin_set_setup_func(&plugin, db_setup);
        plugin_set_connect_func(&plugin, db_connect);
        plugin_set_escape_func(&plugin, db_escape);
        plugin_set_limit_offset_func(&plugin, db_limit_offset);
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
	plugin_set_build_time_constraint_func(&plugin, db_build_time_constraint);
	plugin_set_build_time_interval_func(&plugin, db_build_time_interval);

	return (void *) &plugin;
}

