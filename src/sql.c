/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/time.h>
#include <inttypes.h>
#include <assert.h>

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>
#include <libprelude/plugin-common.h>
#include <libprelude/plugin-common-prv.h>


#include "sql-table.h"
#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"
#include "db-type.h"
#include "db-connection.h"


struct prelude_sql_connection {
        void *session;
	plugin_sql_t *plugin;
	prelude_sql_connection_data_t *data;
};



/*
 * va_copy() was introduced in C99.
 * in C89, it was available throught __va_copy().
 *
 * A safe bet is to fallback on memcpy() in case __va_copy()
 * isn't defined. Thought it won't work with some va_list
 * implementation.
 */
#ifndef va_copy

 #if defined (__va_copy)
  #define va_copy __va_copy
#else
  #warning "Both va_copy (C99) and __va_copy (C89) are undefined: working arround."
  #warning "Result might be undefined depending on your va_list implementation."
  #define va_copy(dst, src) memcpy(&(dst), &(src), sizeof(dst))
 #endif

#endif



/*
 * define what we believe should be enough for most of our query.
 */
#define DB_REQUEST_LENGTH 16384

/*
 * Maximum value of a dynamically allocated buffer in case
 * DB_REQUEST_LENGTH isn't enough (the length is computed taking
 * prelude_string_to_hex() function into account).
 * 1024 : "INSERT INTO table_name (field_names ...)
 * 65536 * (3+21/16) + 1 : a 2^16 length segment logged with its hexa dump based on prelude_string_to_hex()
 * 1 : the ')' at the end of an SQL request
 */
#define DB_MAX_INSERT_QUERY_LENGTH (1024 + 65536 * (3+21/16) + 1 + 1)





static char *generate_dynamic_query(const char *old, size_t olen,
                                    int *nlen, const char *fmt, va_list ap) 
{
        int ret;
        char *query;

        assert(olen < *nlen);
        
        query = malloc(*nlen);
        if ( ! query ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
                
        strncpy(query, old, olen);
        ret = vsnprintf(query + olen, *nlen - olen, fmt, ap);
        
        if ( (ret + 2) > (*nlen - olen) || ret < 0 ) {
                log(LOG_ERR, "query %s doesn't fit in %d bytes.\n", query, *nlen);
                free(query);
                return NULL;
        }

        *nlen = ret + olen;
        
        return query;
}




static char *generate_query(char *buf, size_t size, int *len, const char *fmt, va_list ap)
{
        char *query;
        va_list copy;
        int query_length;

        va_copy(copy, ap);
        
        /*
         * These  functions  return  the number of characters printed
         * (not including the trailing `\0' used  to  end  output  to
         * strings).   snprintf  and vsnprintf do not write more than
         * size bytes (including the trailing '\0'), and return -1 if
         * the  output  was truncated due to this limit.
         */
        query_length = vsnprintf(buf + *len, size - *len, fmt, ap);

        if ( (query_length + 2) <= (size - *len) ) {
                *len = query_length + *len;
                va_end(copy);
                return buf;
        }
        
        if ( query_length < 0 ) 
                query_length = DB_MAX_INSERT_QUERY_LENGTH;
        else 
                query_length += *len + 2;

        query = generate_dynamic_query(buf, *len, &query_length, fmt, copy);
        va_end(copy);
        
        if ( ! query )
                return NULL;

        *len = query_length;
                
        return query;
}



int prelude_sql_insert(prelude_sql_connection_t *conn, const char *table, const char *fields, const char *fmt, ...)
{
        va_list ap;
        int len, ret;
        char query_static[DB_REQUEST_LENGTH], *query;

        len = snprintf(query_static, sizeof(query_static), "INSERT INTO %s (%s) VALUES(", table, fields);
        if ( (len + 1) > sizeof(query_static) || len < 0 ) {
                log(LOG_ERR, "start of query doesn't fit in %d bytes.\n", sizeof(query_static));
                return -1;
        }
        
        va_start(ap, fmt);
        query = generate_query(query_static, sizeof(query_static), &len, fmt, ap);
        va_end(ap);
        
        if ( ! query )
                return -1;

        /*
         * generate_query() allow us to add one byte at the end of the buffer.
         */
        query[len] = ')';
        query[len + 1] = '\0';
	
        ret = conn->plugin->db_command(conn->session, query);
	if (ret < 0)
		log(LOG_ERR, "[%s]->db_command returned error code %d\n", conn->plugin->name, ret);
        
        if ( query != query_static )
                free(query);
        
        return ret;
}



prelude_sql_table_t *prelude_sql_query(prelude_sql_connection_t *conn, const char *fmt, ...)
{
        va_list ap;
        prelude_sql_table_t *ret;
        int query_length = 0;
        char query_static[DB_REQUEST_LENGTH], *query;
        
        va_start(ap, fmt);
        query = generate_query(query_static, sizeof(query_static), &query_length, fmt, ap);
        va_end(ap);
	
        ret = conn->plugin->db_query(conn->session, query);

	if ( query != query_static )
                free(query);

        return ret;
}



char *prelude_sql_escape(prelude_sql_connection_t *conn, const char *string) 
{
	if ( ! string ) 
		return strdup("NULL");
	
	return (conn && conn->plugin) ? 
		conn->plugin->db_escape(conn->session, string) : NULL;
}



int prelude_sql_begin(prelude_sql_connection_t *conn)
{
	return (conn && conn->plugin) ? conn->plugin->db_begin(conn->session) : -1;
}



int prelude_sql_commit(prelude_sql_connection_t *conn)
{
	return (conn && conn->plugin) ? conn->plugin->db_commit(conn->session) : -1;
}



int prelude_sql_rollback(prelude_sql_connection_t *conn)
{
	return (conn && conn->plugin) ? conn->plugin->db_rollback(conn->session) : -1;
}



void prelude_sql_close(prelude_sql_connection_t *conn)
{
	if (conn && conn->plugin)
    		conn->plugin->db_close(conn->session);
		
	free(conn);
}



prelude_sql_connection_t *prelude_sql_connect(prelude_sql_connection_data_t *data) 
{
	int ret;
        void *session;
	plugin_sql_t *plugin;
	prelude_sql_connection_t *conn;
	char *dbtype, *dbhost, *dbport, *dbname, *dbuser, *dbpass;
		
	dbtype = prelude_sql_connection_data_get_type(data);
	dbhost = prelude_sql_connection_data_get_host(data);
	dbport = prelude_sql_connection_data_get_port(data);
	dbname = prelude_sql_connection_data_get_name(data);
	dbuser = prelude_sql_connection_data_get_user(data);
	dbpass = prelude_sql_connection_data_get_pass(data);

	if ( ! dbtype ) {
		log(LOG_ERR, "dbtype not specified\n");
		return NULL;
	}
	
	plugin = (plugin_sql_t *) plugin_search_by_name(dbtype);
	if ( ! plugin ) {
		errno = -ERR_DB_PLUGIN_NOT_FOUND;
		return NULL;
	}
	
	/*
         * any needed parameter translation should go in here
         */
	session = plugin->db_setup(dbhost, dbport, dbname, dbuser, dbpass);
	if ( ! session ) 
		return NULL; /* FIXME: better error detection ? */
	
	ret = plugin->db_connect(session);
	if ( ret < 0 )
		return NULL; /* FIXME: better error detection ? */
	
	conn = calloc(1, sizeof(prelude_sql_connection_t));
	if ( ! conn ) {
		log(LOG_ERR, "out of memory\n");
		return NULL;
	}
	
	conn->plugin = plugin;
	conn->session = session;
	
	return conn;
}

