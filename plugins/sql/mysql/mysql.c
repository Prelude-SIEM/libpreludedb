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


typedef enum {
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


static plugin_sql_t plugin;



static void *db_setup(const char *dbhost, const char *dbport,
                      const char *dbname, const char *dbuser, const char *dbpass)
{
        session_t *session;

        session = malloc(sizeof(*session));
        if ( ! session ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
        
	session->status = allocated;
	session->dbhost = (dbhost) ? strdup(dbhost) : NULL;
	session->dbname = (dbname) ? strdup(dbname) : NULL;
	session->dbuser = (dbuser) ? strdup(dbuser) : NULL;
	session->dbpass = (dbpass) ? strdup(dbpass) : NULL;
	
	return session;
}




static int db_cleanup(void *s)
{
        session_t *session = s;
        
	if ( session->status != connection_closed && session->status != connection_failed )
		return -ERR_PLUGIN_DB_NOT_CONNECTED;
	
	free(session->dbhost);
	free(session->dbname);
	free(session->dbuser);
	free(session->dbpass);
	free(session);
	
	return 0;
}




static int db_command(void *s, const char *query)
{
        int ret = 0;
        session_t *session = s;
        
        if ( session->status != connected && session->status != transaction )
                return -ERR_PLUGIN_DB_NOT_CONNECTED; 

#ifdef DEBUG
	log(LOG_INFO, "MySQL[%d]: %s\n", session_id, query);
#endif
               
        ret = mysql_query(&session->mysql, query);
        if ( ret ) {
                log(LOG_ERR, "Query \"%s\" returned %d:%s\n", query, ret, mysql_error(&session->mysql));
                ret = -ERR_PLUGIN_DB_QUERY_ERROR;
        }
        
        return ret;
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
        
        mysql_close(session->connection);
        session->status = connection_closed;
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
        mysql_real_escape_string(&session->mysql, escaped, string, len);
#else
        mysql_escape_string(escaped, string, len);
#endif
        
        return escaped;
}




/*
 * Execute SQL query, return table
 */
static sql_table_t *db_query(void *s, const char *query)
{
	log(LOG_ERR, "db_query() is not implemented in MySQL plugin.\n");	
	return NULL;
}




/*
 * Connect to the MySQL database
 */
static int db_connect(void *s)
{
        int state;
        session_t *session = s;
        
	if ( session->status != allocated && 
             session->status != connection_failed &&
             session->status != connection_closed )
		return -ERR_PLUGIN_DB_ALREADY_CONNECTED;

        
        /*
         * connect to the mySQL database
         */
        session->connection = mysql_connect(&session->mysql, session->dbhost, 
                                            session->dbuser, session->dbpass);
        
        if ( ! session->connection ) {
                log(LOG_INFO, "MySQL error: %s\n", mysql_error(&session->mysql));
                session->status = connection_failed;
                return -ERR_PLUGIN_DB_CONNECTION_FAILED;
        }

        /*
         * select which database to use on the server
         */
        state = mysql_select_db(session->connection, session->dbname);
        if ( state < 0 ) {
                log(LOG_INFO, "%s\n", mysql_error(session->connection));
                mysql_close(session->connection);
                session->status = connection_failed;
                return -ERR_PLUGIN_DB_CONNECTION_FAILED;
        }

	session->status = connected;

        return 0;
}



plugin_generic_t *plugin_init(int argc, char **argv)
{               
	/* system-wide options for the plugin should go in here */
	
        plugin_set_name(&plugin, "MySQL");
        plugin_set_desc(&plugin, "Will log all alert to a MySQL database.");
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




