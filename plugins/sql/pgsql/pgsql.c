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

#include <libpq-fe.h>

#include <libprelude/list.h>

#include "sql-table.h"

#include "config.h"
#include "db.h"



#define MAX_QUERY_LENGTH 8192

#define PGSQL_MAX_SESSIONS 256

typedef enum {
	available = 0,
	allocated = 1,
	connected = 2,
	connection_failed = 3,
	connection_closed = 4,
	transaction = 5
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

static session_t sessions[PGSQL_MAX_SESSIONS];

static int is_enabled = 0; 
static plugin_sql_t plugin;

static int find_available_session(void)
{
	int i;

	i = 0;
	while (++i < PGSQL_MAX_SESSIONS) {
		if (sessions[i].status == available)
			return i;
	}
	
	return -ERR_PLUGIN_DB_SESSIONS_EXHAUSTED;
}

static int db_setup(const char *dbhost, const char *dbport, const char *dbname, 
   		 const char *dbuser, const char *dbpass)
{
	int s;

	if (is_enabled == 0) {
		log(LOG_ERR, "pgsql plugin not enabled\n");
		return -1;
	}
	
	s = find_available_session();
	if (s < 0) {
		log(LOG_ERR, "all pgsql sessions exhausted\n");
		return -ERR_PLUGIN_DB_SESSIONS_EXHAUSTED;
	}
	
	sessions[s].status = allocated;
	sessions[s].dbhost = (dbhost) ? strdup(dbhost) : NULL;
	sessions[s].dbport = (dbport) ? strdup(dbport) : NULL;
	sessions[s].dbname = (dbname) ? strdup(dbname) : NULL;
	sessions[s].dbuser = (dbuser) ? strdup(dbuser) : NULL;
	sessions[s].dbpass = (dbpass) ? strdup(dbpass) : NULL;
	
	return s;
}

static int db_cleanup(int session_id)
{
	if ((sessions[session_id].status != connection_closed) &&
	    (sessions[session_id].status != connection_failed))
		return -ERR_PLUGIN_DB_NOT_CONNECTED;
	
	free(sessions[session_id].dbhost);
	free(sessions[session_id].dbport);
	free(sessions[session_id].dbname);
	free(sessions[session_id].dbuser);
	free(sessions[session_id].dbpass);
	sessions[session_id].status = available;
	
	return 0;
}

/* called when plugin is unsubscribed, terminate all connections */
static void db_shutdown(void)
{
	int i, ret;
	
	log(LOG_INFO, "plugin unsubscribed, terminating active connections\n");
	for (i=0;i<PGSQL_MAX_SESSIONS;i++) {
		if (sessions[i].status == transaction) {
			ret = db_rollback(i);
			if (ret < 0)
				log(LOG_ERR, "ROLLBACK error on connection %d\n", i);
		}
		
		if (sessions[i].status == connected) 
			db_close(i);
		
		if (sessions[i].status != available)
			db_cleanup(i);
	}
	
	
}

/*
 * Escape string with single quote
 */
static char *db_escape(const char *str)
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
 * Execute SQL query, do not return table
 */
static int db_command(int session_id, const char *query)
{
        PGresult *ret;

	if ((!is_enabled) && (strcasecmp(query, "ROLLBACK") != 0))
		return -ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED;
		
	if ((sessions[session_id].status != connected) && 
	    (sessions[session_id].status != transaction))
		return -ERR_PLUGIN_DB_NOT_CONNECTED; 

#ifdef DEBUG
	log(LOG_INFO, "pgsql: %s\n", query);
#endif
        
        ret = PQexec(sessions[session_id].pgsql, query);
        if ( ! ret || 
        	((PQresultStatus(ret) != PGRES_COMMAND_OK) &&
        	(PQresultStatus(ret) != PGRES_TUPLES_OK)) ) {
                log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, 
                	PQerrorMessage(sessions[session_id].pgsql));
                return -ERR_PLUGIN_DB_QUERY_ERROR;
        }

        return 0;
}

/*
 * Execute SQL query, return table
 */
sql_table_t *db_query(int session_id, const char *query)
{
        PGresult *res;
        sql_table_t *table;
        sql_row_t *row;
        sql_field_t *field;
	char *name, *val;
	sql_field_type_t type;        
        int tuples, fields;
        int tup, fld;

	if (!is_enabled) {
		errno = -ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED;
		return NULL;
	}

	if ((sessions[session_id].status != connected) &&
	    (sessions[session_id].status != transaction)) {
		errno = -ERR_PLUGIN_DB_NOT_CONNECTED;
		return NULL;
	}

	table = sql_table_create("Results");
	if (!table) {
		log(LOG_ERR, "Query: \"%s\": could not create result table\n", query);
		errno = -ERR_PLUGIN_DB_RESULT_TABLE_ERROR;
		return NULL;
	}

#ifdef DEBUG
	log(LOG_INFO, "pgsql: %s\n", query);
#endif
        
        res = PQexec(sessions[session_id].pgsql, query);
        if ( ! res || 
        	((PQresultStatus(res) != PGRES_COMMAND_OK) &&
        	(PQresultStatus(res) != PGRES_TUPLES_OK)) ) {
                log(LOG_ERR, "Query \"%s\" failed : %s.\n", query, 
                	PQerrorMessage(sessions[session_id].pgsql));
                PQclear(res);
                errno = -ERR_PLUGIN_DB_QUERY_ERROR;
                return NULL;
        }
        
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        	tuples = PQntuples(res);
        	fields = PQnfields(res);
        	
        	for (tup=0;tup<tuples;tup++) {
        		row = sql_row_create(fields);
        		if (!row) {
        			log(LOG_ERR, "Query \"%s\": could not create table row\n", 
        				query);
        			sql_table_destroy(table);
        			PQclear(res);
        			errno = -ERR_PLUGIN_DB_RESULT_ROW_ERROR;
        			return NULL;
        		}
        		
        		sql_table_add_row(table, row);
        		
        		for (fld=0;fld<fields;fld++) {
        			       			
        			/* determine and convert column type */
        			/* FIXME: values are taken from pgsql/server/catalog/pg_type.h
        			   but maybe there is a more elegant way to do that */
        			switch (PQftype(res, fld)) {
					case 20: type = type_int64;
						 break;
        				case 23: type = type_int32;
        					 break;
        				case 700: type = type_float;
        					  break;
        				case 701: type = type_double;
        					  break;
        				default: type = type_string;
        					 break;
        			}
        			
        			name = PQfname(res, fld);
        			val = PQgetvalue(res, tup, fld);
        			
        			field = sql_field_create(name, type, val);
        			if (!field) {
        				log(LOG_ERR, "Query \"%s\": couldn't create result field \"%s\"\n", 
        					query, name);
        				sql_table_destroy(table);
        				PQclear(res);
        				errno = -ERR_PLUGIN_DB_RESULT_FIELD_ERROR;
        				return NULL;
        			}
        		}
        	}
        }

	PQclear(res);

        return table;
}

/*
 * insert data into table
 */
static int db_insert(int session_id, const char *query) 
{
	return db_command(session_id, query);
}


/*
 * start transaction
 */
static int db_begin(int session_id) 
{
	if (sessions[session_id].status == transaction)
		return -ERR_PLUGIN_DB_NO_TRANSACTION;
	
	sessions[session_id].status = transaction;
	return db_command(session_id, "BEGIN;");
}



/*
 * commit transaction
 */
static int db_commit(int session_id) 
{
	if (sessions[session_id].status != transaction)
		return -ERR_PLUGIN_DB_NO_TRANSACTION;
	
	return db_command(session_id, "COMMIT;");
}


/*
 * end transaction
 */
static int db_rollback(int session_id)
{
	if (sessions[session_id].status != transaction)
		return -ERR_PLUGIN_DB_NO_TRANSACTION;
	
	return db_command(session_id, "ROLLBACK;");
}



/*
 * closes the DB connection.
 */
static void db_close(int session_id)
{
        PQfinish(sessions[session_id].pgsql);
        sessions[session_id].status = connection_closed;
}



/*
 * Connect to the PgSQL database
 */
static int db_connect(int session_id)
{        
	if (is_enabled == 0) {
		log(LOG_ERR, "pgsql plugin not enabled\n");
		return -ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED;
	}

	if ((sessions[session_id].status != allocated) && 
	    (sessions[session_id].status != connection_failed) &&
	    (sessions[session_id].status != connection_closed))
		return -1;
	
        sessions[session_id].pgsql = PQsetdbLogin(sessions[session_id].dbhost, 
				        	sessions[session_id].dbport, 
    				    	NULL, NULL, 
				        	sessions[session_id].dbname, 
				        	sessions[session_id].dbuser, 
				        	sessions[session_id].dbpass);


        if ( PQstatus(sessions[session_id].pgsql) == CONNECTION_BAD) {
                log(LOG_INFO, "PgSQL connection failed: %s", 
                	PQerrorMessage(sessions[session_id].pgsql));
                PQfinish(sessions[session_id].pgsql);
                sessions[session_id].status = connection_failed;
                return -ERR_PLUGIN_DB_CONNECTION_FAILED;
        }

	sessions[session_id].status = connected;
        
        return 0;
}


static int set_pgsql_state(prelude_option_t *opt, const char *arg) 
{
        int ret;

	printf("set_pgsql_state: is_enabled = %d\n", is_enabled);
        
        if ( is_enabled == 1 ) {
        	log(LOG_INFO, "disabling pgsql plugin\n");
                db_shutdown();
                
                ret = plugin_unsubscribe((plugin_generic_t *) &plugin);
                if ( ret < 0 )
                        return prelude_option_error;
                is_enabled = 0;
        }

        else {
        	log(LOG_INFO, "enabling pgsql plugin\n");
		bzero(&sessions, PGSQL_MAX_SESSIONS*sizeof(session_t));

                ret = plugin_subscribe((plugin_generic_t *) &plugin);
                if ( ret < 0 )
                        return prelude_option_error;
                
                is_enabled = 1;
        }
        
        return prelude_option_success;
}



static int get_pgsql_state(char *buf, size_t size) 
{
        snprintf(buf, size, "%s", (is_enabled == 1) ? "enabled" : "disabled");
        return prelude_option_success;
}




plugin_generic_t *plugin_init(int argc, char **argv)
{
        prelude_option_t *opt;

	printf("initializing psql plugin\n");
	
        opt = prelude_option_add(NULL, CLI_HOOK|CFG_HOOK|WIDE_HOOK, 0, "pgsql",
                                 "Option for the PgSQL plugin", no_argument,
                                 set_pgsql_state, get_pgsql_state);

	/* system-wide options for the plugin should go in here */
	
        plugin_set_name(&plugin, "PgSQL");
        plugin_set_desc(&plugin, "Will log all alert to a PostgreSQL database.");
        plugin_set_setup_func(&plugin, db_setup);
        plugin_set_connect_func(&plugin, db_connect);
        plugin_set_escape_func(&plugin, db_escape);
        plugin_set_insert_func(&plugin, db_insert);
        plugin_set_query_func(&plugin, db_query);
        plugin_set_begin_func(&plugin, db_begin);
        plugin_set_commit_func(&plugin, db_commit);
        plugin_set_rollback_func(&plugin, db_rollback);
        plugin_set_closing_func(&plugin, db_close);

	set_pgsql_state(NULL, NULL);
       
	return (plugin_generic_t *) &plugin;
}




