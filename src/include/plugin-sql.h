/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_PLUGIN_DB_H
#define _LIBPRELUDEDB_PLUGIN_DB_H

#define DB_INSERT_END ((void *)0x1)

/* used in plugins */

#define ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED 1
#define ERR_PLUGIN_DB_SESSIONS_EXHAUSTED 2
#define ERR_PLUGIN_DB_NOT_CONNECTED 3
#define ERR_PLUGIN_DB_QUERY_ERROR 4
#define ERR_PLUGIN_DB_NO_TRANSACTION 5
#define ERR_PLUGIN_DB_CONNECTION_FAILED 6
#define ERR_PLUGIN_DB_RESULT_TABLE_ERROR 7
#define ERR_PLUGIN_DB_RESULT_ROW_ERROR 8
#define ERR_PLUGIN_DB_RESULT_FIELD_ERROR 9

/* used in library code */

#define ERR_DB_PLUGIN_NOT_FOUND 100

typedef struct {
        PLUGIN_GENERIC;
        int (*db_setup) (const char *dbhost, const char *dbport, const char *dbname, 
        		 const char *dbuser, const char *dbpass);
        int (*db_connect) (int session_id);
        char *(*db_escape)(const char *input);
        int (*db_insert)(int session_id, const char *query);
        sql_table_t *(*db_query)(int session_id, const char *query);
        int (*db_begin)(int session_id);
        int (*db_commit)(int session_id);
        int (*db_rollback)(int session_id);
        void (*db_close)(int session_id);
} plugin_sql_t;

typedef struct {
	plugin_sql_t *plugin;
	int session;	
} sql_connection_t;



#define plugin_setup_func(p) (p)->db_setup

#define plugin_connect_func(p) (p)->db_connect

#define plugin_escape_func(p) (p)->db_escape

#define plugin_insert_func(p) (p)->db_insert

#define plugin_query_func(p) (p)->db_query

#define plugin_begin_func(p) (p)->db_begin

#define plugin_commit_func(p) (p)->db_commit

#define plugin_rollback_func(p) (p)->db_rollback

#define plugin_close_func(p) (p)->db_close


#define plugin_set_setup_func(p, f) plugin_setup_func(p) = (f)

#define plugin_set_connect_func(p, f) plugin_connect_func(p) = (f)

#define plugin_set_escape_func(p, f) plugin_escape_func(p) = (f)

#define plugin_set_insert_func(p, f) plugin_insert_func(p) = (f)

#define plugin_set_query_func(p, f) plugin_query_func(p) = (f)

#define plugin_set_begin_func(p, f) plugin_begin_func(p) = (f)

#define plugin_set_commit_func(p, f) plugin_commit_func(p) = (f)

#define plugin_set_rollback_func(p, f) plugin_rollback_func(p) = (f)

#define plugin_set_closing_func(p, f) plugin_close_func(p) = (f)



int sql_plugins_available(void);

sql_connection_t *sql_connect(const char *dbtype, const char *dbhost, const char *dbport, 
	const char *dbname, const char *dbuser, const char *dbpass);

int sql_plugins_init(const char *dirname, int argc, char **argv);

char *sql_plugin_escape(sql_connection_t *conn, const char *string);

int sql_plugin_insert(sql_connection_t *conn, const char *table, const char *fields, const char *fmt, ...);

sql_table_t *sql_plugin_query(sql_connection_t *conn, const char *fmt, ...);

int sql_plugin_begin(sql_connection_t *conn);

int sql_plugin_commit(sql_connection_t *conn);

int sql_plugin_rollback(sql_connection_t *conn);

void sql_plugins_close(sql_connection_t *conn);

plugin_generic_t *plugin_init(int argc, char **argv);

#endif /* _LIBPRELUDEDB_PLUGIN_DB_H */




