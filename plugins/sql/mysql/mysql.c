/*****
*
* Copyright (C) 2001, 2002 Vandoorselaere Yoann <yoann@mandrakesoft.com>
* Copyright (C) 2001 Sylvain GIL <prelude@tootella.org>
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

#include <mysql/mysql.h>

#include "sql-table.h"

#include "config.h"
#include "db.h"

/*
 * NOTE: Modifications for LibpreludeDB are heavily based on relevant PgSQL code 
 * and thus heavily influenced with the way of thinking used there :-)
 */

/*
 * FIXME: The session finite state automata [cool name, eh?] is currently done
 * the same way as in PostgreSQL; i.e. we assume there's full transaction support. 
 * This is not true for standard MySQL, but transactions are again available in
 * MySQL-Max. So we should either detect if we are using MySQL-Max or not and adjust
 * the behavior accordingly or write separate plugins for MySQL and MySQL-Max. 
 *  
 * And still the great question, how to implement 'ROLLBACK' on standard MySQL. 
 * Either make db_begin(), db_commit(), db_rollback() as no-ops or try to emulate 
 * the stuff ourselves...
 *
 * So there is - for now at least - the reason for code duplication between PgSQL
 * and MySQL plugins. Also note that AFAIK Oracle opens transaction automatically 
 * each time you send query to the backend (and requires 'COMMIT') so it must be 
 * managed differently. 
 *
 */


#define MYSQL_MAX_SESSIONS 256

#define MAX_QUERY_LENGTH 8192

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
	char *dbname;
	char *dbuser;
	char *dbpass;
	MYSQL *connection;
	MYSQL mysql;
} session_t;


static int is_enabled = 0;
static plugin_sql_t plugin;

static session_t sessions[MYSQL_MAX_SESSIONS];

static int find_available_session(void)
{
	int i;

	i = 0;
	while (i < MYSQL_MAX_SESSIONS) {
		if (sessions[i].status == available)
			return i;

		i++;
	}
	
	return -ERR_PLUGIN_DB_SESSIONS_EXHAUSTED;
}

/* db_port parameter is ignored, but must be kept for compatibility */

static int db_setup(const char *dbhost, const char *dbport, const char *dbname, 
   		 const char *dbuser, const char *dbpass)
{
	int s;

	if (is_enabled == 0) {
		log(LOG_ERR, "MySQL plugin not enabled\n");
		return -ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED;
	}
	
	s = find_available_session();
	if (s < 0) {
		log(LOG_ERR, "all MySQL sessions exhausted\n");
		return -ERR_PLUGIN_DB_SESSIONS_EXHAUSTED;
	}
	
	if ( ! dbhost || ! dbname ) {
                log(LOG_INFO, "MySQL session not allocated because dbhost / dbname information missing.\n");
                return -ERR_PLUGIN_DB_INCORRECT_PARAMETERS;
        }
	
	sessions[s].status = allocated;
	sessions[s].dbhost = (dbhost) ? strdup(dbhost) : NULL;
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
	
	log(LOG_INFO, "MySQL plugin unsubscribed, terminating active connections\n");
	for (i=0;i<MYSQL_MAX_SESSIONS;i++) {
		if (sessions[i].status == transaction) {
			ret = db_rollback(i);
			if (ret < 0)
				log(LOG_ERR, "ROLLBACK error on connection %d\n", i);
		}
		
		if (sessions[i].status == connected) 
			db_close(i);
		
		/* at this point all sessions should be either closed or in 
		   other inactive state */
		
		if (sessions[i].status != available)
			db_cleanup(i);
	}	
}


/*
 * Takes a string and create a legal SQL string from it.
 * returns the escaped string.
 */
static char *db_escape(int session_id, const char *string)
{
        int len;
        char *escaped;

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
        if (! escaped) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }

#ifdef HAVE_MYSQL_REAL_ESCAPE_STRING
        mysql_real_escape_string(&sessions[session_id].mysql, escaped, string, len);
#else
        mysql_escape_string(escaped, string, len);
#endif
        
        return escaped;
}





static int db_command(int session_id, const char *query)
{
        int ret = 0;

	if ((!is_enabled) && (strcasecmp(query, "ROLLBACK") != 0))
		return -ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED;
		
	if ((sessions[session_id].status != connected) && 
	    (sessions[session_id].status != transaction))
		return -ERR_PLUGIN_DB_NOT_CONNECTED; 

#ifdef DEBUG
	log(LOG_INFO, "MySQL[%d]: %s\n", session_id, query);
#endif
               
        ret = mysql_query(&sessions[session_id].mysql, query);
        if ( ret ) {
                log(LOG_ERR, "Query \"%s\" returned %d:%s\n", query, ret, 
                    mysql_error(&sessions[session_id].mysql));
                ret = -ERR_PLUGIN_DB_QUERY_ERROR;
        }
        
        return ret;
}

/*
 * Execute SQL query, return table
 */
sql_table_t *db_query(int session_id, const char *query)
{
	log(LOG_ERR, "db_query() is not implemented in MySQL plugin.\n");
	
	return NULL;
}

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
        mysql_close(sessions[session_id].connection);
        sessions[session_id].status = connection_closed;
}



/*
 * Connect to the MySQL database
 */
static int db_connect(int session_id)
{
        int state;

	if (is_enabled == 0) {
		log(LOG_ERR, "MySQL plugin not enabled\n");
		return -ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED;
	}

	if ((sessions[session_id].status != allocated) && 
	    (sessions[session_id].status != connection_failed) &&
	    (sessions[session_id].status != connection_closed))
		return -ERR_PLUGIN_DB_ALREADY_CONNECTED;

        
        /*
         * connect to the mySQL database
         */
        sessions[session_id].connection = mysql_connect(&sessions[session_id].mysql, 
                                                         sessions[session_id].dbhost, 
                            			     sessions[session_id].dbuser, 
        			                         sessions[session_id].dbpass);
        if ( ! sessions[session_id].connection ) {
                log(LOG_INFO, "MySQL error: %s\n", mysql_error(&sessions[session_id].mysql));
                sessions[session_id].status = connection_failed;
                return -ERR_PLUGIN_DB_CONNECTION_FAILED;
        }

        /*
         * select which database to use on the server
         */
        state = mysql_select_db(sessions[session_id].connection, 
        			sessions[session_id].dbname);

        /* -1 means an error occurred */
        if (state == -1) {
                log(LOG_INFO, "%s\n", mysql_error(sessions[session_id].connection));
                mysql_close(sessions[session_id].connection);
                sessions[session_id].status = connection_failed;
                return -ERR_PLUGIN_DB_CONNECTION_FAILED;
        }

	sessions[session_id].status = connected;

        return 0;
}




static int set_mysql_state(const char *arg) 
{
        int ret;
        
        if ( is_enabled == 1 ) {
                db_shutdown();
                
                ret = plugin_unsubscribe((plugin_generic_t *) &plugin);
                if ( ret < 0 )
                        return prelude_option_error;
                is_enabled = 0;
        }

        else {
		bzero(&sessions, MYSQL_MAX_SESSIONS*sizeof(session_t));
                
                ret = plugin_subscribe((plugin_generic_t *) &plugin);
                if ( ret < 0 )
                        return prelude_option_error;

                is_enabled = 1;
        }
        
        return prelude_option_success;
}



static int get_mysql_state(char *buf, size_t size) 
{
        snprintf(buf, size, "%s", (is_enabled == 1) ? "enabled" : "disabled");
        return prelude_option_success;
}



plugin_generic_t *plugin_init(int argc, char **argv)
{               
	/* system-wide options for the plugin should go in here */
	
        plugin_set_name(&plugin, "MySQL");
        plugin_set_desc(&plugin, "Will log all alert to a MySQL database.");
        plugin_set_setup_func(&plugin, db_setup);
        plugin_set_connect_func(&plugin, db_connect);
        plugin_set_escape_func(&plugin, db_escape);
        plugin_set_insert_func(&plugin, db_insert);
        plugin_set_query_func(&plugin, db_query);
        plugin_set_begin_func(&plugin, db_begin);
        plugin_set_commit_func(&plugin, db_commit);
        plugin_set_rollback_func(&plugin, db_rollback);
        plugin_set_closing_func(&plugin, db_close);

	set_mysql_state(NULL);
        
	return (plugin_generic_t *) &plugin;
}




