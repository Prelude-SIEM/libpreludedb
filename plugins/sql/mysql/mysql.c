/*****
*
* Copyright (C) 2001, 2002 Vandoorselaere Yoann <yoann@mandrakesoft.com>
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

#include <libprelude/prelude-io.h>
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-getopt.h>

#include <mysql/mysql.h>

#include "config.h"

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"

#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
# define mysql_field_count mysql_num_fields
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
        	
	free(session->dbhost);
	free(session->dbname);
	free(session->dbuser);
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
static char *db_escape(void *s, const char *string)
{
        int len;
        char *escaped;
        session_t *session = s;

	session->dberrno = 0;
	
        if ( ! string )
                string = "";
        
        /*
         * MySQL documentation say :
         * The string pointed to by from must be length bytes long. You must
         * allocate the to buffer to be at least length*2+1 bytes long. (In the
         * worse case, each character may need to be encoded as using two bytes,
         * and you need room for the terminating null byte.)
         */
        len = strlen(string);
        
        escaped = malloc(len * 2 + 1);
        if ( ! escaped ) {
                log(LOG_ERR, "memory exhausted.\n");
		session->dberrno = ERR_PLUGIN_DB_MEMORY_EXHAUSTED;
                return NULL;
        }

#ifdef HAVE_MYSQL_REAL_ESCAPE_STRING
        mysql_real_escape_string(session->mysql, escaped, string, len);
#else
        mysql_escape_string(escaped, string, len);
#endif
        
        return escaped;
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



plugin_generic_t *plugin_init(int argc, char **argv)
{               
	/* system-wide options for the plugin should go in here */
	
        plugin_set_name(&plugin, "MySQL");
        plugin_set_desc(&plugin, "Will log all alert to a MySQL database.");
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
