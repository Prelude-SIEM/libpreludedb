/*****
*
* Copyright (C) 2001-2004 Vandoorselaere Yoann <yoann@prelude-ids.org>
* Copyright (C) 2001 Sylvain GIL <prelude@tootella.org>
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
#include <libprelude/idmef-util.h>

#include <libprelude/prelude-io.h>
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-getopt.h>

#include <mysql/mysql.h>

#include "config.h"

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"

#if ! defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID < 32224
 #define mysql_field_count mysql_num_fields
#endif /* ! MYSQL_VERSION_ID */


/*
 * NOTE: We assume that:
 *        (1) backend is by default in auto-commit mode
 *        (2) if backend does not handle transactions, it will report
 *            no error on BEGIN; COMMIT; ROLLBACK; commands. 
 */

typedef enum {
	st_allocated,
	st_connection_failed,
	st_connected,
	st_query
} session_status_t;



typedef struct {
	session_status_t status;
	int transaction;
	int dberrno;
	char *dbhost;
	unsigned int dbport;
	char *dbname;
	char *dbuser;
	char *dbpass;
	MYSQL *mysql;
} session_t;


static plugin_sql_t plugin;


MYSQL_FIELD *get_field(session_t *, MYSQL_RES *, unsigned int);
MYSQL_FIELD *get_field_by_name(session_t *, MYSQL_RES *, const char *);
prelude_sql_field_type_t get_field_type(MYSQL_FIELD *);

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
	session->dbport = (dbport) ? (unsigned int) atoi(dbport) : 0;
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

        if ( session->dbname )
                free(session->dbname);

        if ( session->dbuser )
                free(session->dbuser);

        if ( session->dbpass )
                free(session->dbpass);
        
	free(session);
}



/*
 * closes the DB connection.
 */
static void db_close(void *s)
{
        session_t *session = s;
        
        mysql_close(session->mysql);
        
        cleanup(s);
}



/*
 * Takes a string and create a legal SQL string from it.
 * returns the escaped string.
 */
static char *db_escape(void *s, const char *buf, size_t len)
{
        size_t rlen;
        char *escaped;
	session_t *session = s;

	session->dberrno = 0;
	
        if ( ! string )
                return strdup("NULL");
        
        /*
         * MySQL documentation say :
         * The string pointed to by from must be length bytes long. You must
         * allocate the to buffer to be at least length*2+1 bytes long. (In the
         * worse case, each character may need to be encoded as using two bytes,
         * and you need room for the terminating null byte.)
         */
        rlen = len * 2 + 3;
        if ( rlen <= len )
                return NULL;
        
        escaped = malloc(rlen);
        if ( ! escaped ) {
                log(LOG_ERR, "memory exhausted.\n");
		session->dberrno = ERR_PLUGIN_DB_MEMORY_EXHAUSTED;
                return NULL;
        }

        escaped[0] = '\'';
        
#ifdef HAVE_MYSQL_REAL_ESCAPE_STRING
        len = mysql_real_escape_string(session->mysql, escaped + 1, string, len);
#else
        len = mysql_escape_string(escaped + 1, string, len);
#endif

        escaped[len + 1] = '\'';
        escaped[len + 2] = '\0';

        return escaped;
}


static const char *db_limit_offset(void *s, int limit, int offset)
{
	static char buffer[64];
	int ret;

	if ( limit >= 0 ) {
		if ( offset >= 0 )
			ret = snprintf(buffer, sizeof (buffer), "LIMIT %d, %d", offset, limit);
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
	MYSQL_RES *res;
	session_t *session = s;

	session->dberrno = 0;
	
	if ( session->status < st_connected ) {
		session->dberrno = ERR_PLUGIN_DB_NOT_CONNECTED;
		return NULL;
	}

#ifdef DEBUG
	log(LOG_INFO, "mysql: %s\n", query);
#endif

	if ( mysql_query(session->mysql, query) != 0) {
		log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, mysql_error(session->mysql));
		session->dberrno = ERR_PLUGIN_DB_QUERY_ERROR;
		return NULL;
	}

	res = mysql_store_result(session->mysql);
	if ( res ) {
		if ( mysql_num_rows(res) == 0 ) {
			mysql_free_result(res);
			return NULL;
		}
		session->status = st_query;
	} else {
		if ( mysql_errno(session->mysql) ) {
			log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, mysql_error(session->mysql));
			session->dberrno = ERR_PLUGIN_DB_QUERY_ERROR;
		}
	}

	return res;
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
 * Connect to the MySQL database
 */
static int db_connect(void *s)
{
        session_t *session = s;
	MYSQL *ret;

	if ( session->status >= st_connected ) {
		session->dberrno = ERR_PLUGIN_DB_ALREADY_CONNECTED;
		return -session->dberrno;
	}

	session->mysql = mysql_init(NULL);
	if ( ! session->mysql ) {
		log(LOG_INFO, "MySQL error: out of memory\n");
		session->status = st_connection_failed;
		session->dberrno = ERR_PLUGIN_DB_MEMORY_EXHAUSTED;
		return -session->dberrno;
	}
        
        /*
         * connect to the mySQL database
         */
	ret = mysql_real_connect(session->mysql, session->dbhost, session->dbuser, session->dbpass,
				 session->dbname, session->dbport, NULL, 0);
        if ( ! ret ) {
                log(LOG_INFO, "MySQL error: %s\n", mysql_error(session->mysql));
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
	MYSQL_RES *res = t;

	session->dberrno = 0;

	session->status = st_connected;

	if ( res )	
		mysql_free_result(res);
}

MYSQL_FIELD *get_field(session_t *session, MYSQL_RES *res, unsigned int i)
{
	if ( i >= mysql_num_fields(res) ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM;
		return NULL;
	}

	return mysql_fetch_field_direct(res, i);
}



MYSQL_FIELD *get_field_by_name(session_t *session, MYSQL_RES *res, const char *name)
{
	MYSQL_FIELD *fields;
	int fields_num;
	int i;

	fields = mysql_fetch_fields(res);
	if ( ! fields )
		return NULL;

	fields_num = mysql_num_fields(res);
	
	for ( i = 0; i < fields_num; i++ ) {
		if (strcmp(name, fields[i].name) == 0)
			return &fields[i];
	}

	session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;

	return NULL;
}



prelude_sql_field_type_t get_field_type(MYSQL_FIELD *field)
{
	switch ( field->type ) {

	case FIELD_TYPE_LONG:
		return ( field->flags & UNSIGNED_FLAG ) ? dbtype_uint32 : dbtype_int32;

	case FIELD_TYPE_LONGLONG:
		return ( field->flags & UNSIGNED_FLAG ) ? dbtype_uint64 : dbtype_int64;

	case FIELD_TYPE_FLOAT:
		return dbtype_float;

	case FIELD_TYPE_DOUBLE:
		return dbtype_double;

	default:
		return dbtype_string;
	}

	return dbtype_unknown;
}



static const char *db_field_name(void *s, void *t, unsigned int i)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_FIELD *field;

	session->dberrno = 0;

	field = get_field(session, res, i);

	return field ? field->name : NULL;
}



static int db_field_num(void *s, void *t, const char *name)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_FIELD *fields;
	int fields_num;
	int i;

	session->dberrno = 0;

	fields = mysql_fetch_fields(res);
	if ( ! fields )
		return -1;

	fields_num = mysql_num_fields(res);

	for ( i = 0; i < fields_num; i++ ) {
		if (strcmp(name, fields[i].name) == 0)
			return i;
	}

	session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;

	return -1;
}


static prelude_sql_field_type_t db_field_type(void *s, void *t, unsigned int i)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_FIELD *field;

	session->dberrno = 0;

	field = get_field(session, res, i);

	return field ? get_field_type(field) : dbtype_unknown;
}



static prelude_sql_field_type_t db_field_type_by_name(void *s, void *t, const char *name)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_FIELD *field;

	session->dberrno = 0;

	field = get_field_by_name(session, res, name);

	return field ? get_field_type(field) : dbtype_unknown;;
}



static unsigned int db_fields_num(void *s, void *t)
{
	session_t *session = s;
	MYSQL_RES *res = t;

	session->dberrno = 0;

	return mysql_num_fields(res);
}



static unsigned int db_rows_num(void *s, void *t)
{
	session_t *session = s;
	MYSQL_RES *res = t;

	session->dberrno = 0;

	return (unsigned int) mysql_num_rows(res);
}


static void *db_row_fetch(void *s, void *t)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_ROW row;

	session->dberrno = 0;
	
	row = mysql_fetch_row(res);

	return row;
}



static void *db_field_fetch(void *s, void *t, void *r, unsigned int i)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_ROW row = r;

	session->dberrno = 0;

	if ( i >= mysql_num_fields(res) ) {
		session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM;
		return NULL;
	}

	return row[i];
}



static void *db_field_fetch_by_name(void *s, void *t, void *r, const char *name)
{
	session_t *session = s;
	MYSQL_RES *res = t;
	MYSQL_ROW row = r;
	MYSQL_FIELD *fields = mysql_fetch_fields(res);
	int fields_num = mysql_num_fields(res);
	int i;

	session->dberrno = 0;

	for ( i = 0; i < fields_num; i++ ) {
		if (strcmp(name, fields[i].name) == 0)
			return row[i];
	}

	session->dberrno = ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME;

	return NULL;
}



static const char *db_field_value(void *s, void *t, void *r, void *f)
{
	session_t *session = s;

	session->dberrno = 0;

	return (const char *) f;
}



static int db_build_time_constraint(prelude_strbuf_t *output, const char *field,
				    prelude_sql_time_constraint_type_t type,
				    idmef_relation_t relation, int value, int gmt_offset)
{
	int gmt_offset;
	char buf[128];
	const char *sql_relation;

	if ( prelude_get_gmt_offset(&gmt_offset) < 0 )
		return -1;

	if ( snprintf(buf, sizeof (buf), "DATE_ADD(%s, INTERVAL %d HOUR)", field, gmt_offset / 3600) < 0 )
		return -1;

	sql_relation = prelude_sql_idmef_relation_to_string(relation);
	if ( ! sql_relation )
		return -1;

	switch ( type ) {
	case dbconstraint_year:
		return prelude_strbuf_sprintf(output, "EXTRACT(YEAR FROM %s) %s '%d'",
					      buf, sql_relation, value);

	case dbconstraint_month:
		return  prelude_strbuf_sprintf(output, "EXTRACT(MONTH FROM %s) %s '%d'",
					       buf, sql_relation, value);

	case dbconstraint_yday:
		return prelude_strbuf_sprintf(output, "DAYOFYEAR(%s) %s '%d'",
					      buf, sql_relation, value);

	case dbconstraint_mday:
		return prelude_strbuf_sprintf(output, "DAYOFMONTH(%s) %s '%d'",
					      buf, sql_relation, value);

	case dbconstraint_wday:
		return prelude_strbuf_sprintf(output, "DAYOFWEEK(%s) %s '%d'",
					      buf, sql_relation, value % 7 + 1);

	case dbconstraint_hour:
		return prelude_strbuf_sprintf(output, "EXTRACT(HOUR FROM %s) %s '%d'",
					      buf, sql_relation, value);

	case dbconstraint_min:
		return prelude_strbuf_sprintf(output, "EXTRACT(MINUTE FROM %s) %s '%d'",
					      buf, sql_relation, value);

	case dbconstraint_sec:
		return prelude_strbuf_sprintf(output, "EXTRACT(SECOND FROM %s) %s '%d'",
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

	ret = snprintf(buf, size, "INTERVAL %d %s", value, type_str);

	return (ret < 0 || ret >= size) ? -1 : 0;
}


plugin_generic_t *plugin_init(int argc, char **argv)
{               
	/* system-wide options for the plugin should go in here */
	
        plugin_set_name(&plugin, "MySQL");
        plugin_set_desc(&plugin, "Will log all alert to a MySQL database.");
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

	return (plugin_generic_t *) &plugin;
}
