/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_PLUGIN_DB_H
#define _LIBPRELUDEDB_PLUGIN_DB_H

#define DB_INSERT_END ((void *)0x1)

/* used in plugins */

#define	ERR_PLUGIN_DB_SUCCESS 0
#define ERR_PLUGIN_DB_PLUGIN_NOT_ENABLED 1
#define ERR_PLUGIN_DB_SESSIONS_EXHAUSTED 2
#define ERR_PLUGIN_DB_NOT_CONNECTED 3
#define ERR_PLUGIN_DB_QUERY_ERROR 4
#define ERR_PLUGIN_DB_NO_TRANSACTION 5
#define ERR_PLUGIN_DB_CONNECTION_FAILED 6
#define ERR_PLUGIN_DB_RESULT_TABLE_ERROR 7
#define ERR_PLUGIN_DB_RESULT_ROW_ERROR 8
#define ERR_PLUGIN_DB_RESULT_FIELD_ERROR 9
#define ERR_PLUGIN_DB_INCORRECT_PARAMETERS 10
#define ERR_PLUGIN_DB_ALREADY_CONNECTED 11
#define	ERR_PLUGIN_DB_MEMORY_EXHAUSTED 12
#define	ERR_PLUGIN_DB_ILLEGAL_FIELD_NUM 13
#define	ERR_PLUGIN_DB_ILLEGAL_FIELD_NAME 14
#define	ERR_PLUGIN_DB_CONNECTION_LOST 15
#define	ERR_PLUGIN_DB_DOUBLE_QUERY 16

/* used in library code */

#define ERR_DB_PLUGIN_NOT_FOUND 100

typedef struct {
        PLUGIN_GENERIC;
	/* general functions */
        void *(*db_setup)(const char *dbhost, const char *dbport, const char *dbname, 
                          const char *dbuser, const char *dbpass);
        int (*db_connect) (void *session);
        char *(*db_escape)(void *session, const char *input);
        void *(*db_query)(void *session, const char *query);
        int (*db_begin)(void *session);
        int (*db_commit)(void *session);
        int (*db_rollback)(void *session);
        void (*db_close)(void *session);
	int (*db_errno)(void *session);

	/* tables related functions */
	void (*db_table_free)(void *session, void *table);
	const char *(*db_field_name)(void *session, void *table, unsigned int i);
	int (*db_field_num)(void *session, void *table, const char *name);
	prelude_sql_field_type_t (*db_field_type)(void *session, void *table, unsigned int i);
	prelude_sql_field_type_t (*db_field_type_by_name)(void *session, void *table, const char *name);
	unsigned int (*db_fields_num)(void *session, void *table);
	unsigned int (*db_rows_num)(void *session, void *table);
	void *(*db_row_fetch)(void *session, void *table);

	/* rows related functions */
	void *(*db_field_fetch)(void *session, void *table, void *row, unsigned int i);
	void *(*db_field_fetch_by_name)(void *session, void *table, void *row, const char *name);

	/* fields related functions */
	const char *(*db_field_value)(void *session, void *table, void *row, void *field);

	/* 
	 * we could also add functions db_field_value_type (where type could be int32, uint32,
	 * int64, and so on) to support database's API storing values in their native type (I mean
	 * not converting them into string type before storing them) and thus avoid useless double
	 * type conversion
	 */

} plugin_sql_t;



#define plugin_setup_func(p) (p)->db_setup
#define plugin_connect_func(p) (p)->db_connect
#define plugin_escape_func(p) (p)->db_escape
#define plugin_query_func(p) (p)->db_query
#define plugin_begin_func(p) (p)->db_begin
#define plugin_commit_func(p) (p)->db_commit
#define plugin_rollback_func(p) (p)->db_rollback
#define plugin_close_func(p) (p)->db_close
#define	plugin_errno_func(p) (p)->db_errno
#define	plugin_table_free_func(p) (p)->db_table_free
#define	plugin_field_name_func(p) (p)->db_field_name
#define	plugin_field_num_func(p) (p)->db_field_num
#define	plugin_field_type_func(p) (p)->db_field_type
#define	plugin_field_type_by_name_func(p) (p)->db_field_type_by_name
#define	plugin_fields_num_func(p) (p)->db_fields_num
#define	plugin_rows_num_func(p) (p)->db_rows_num
#define	plugin_row_fetch_func(p) (p)->db_row_fetch
#define	plugin_field_fetch_func(p) (p)->db_field_fetch
#define	plugin_field_fetch_by_name_func(p) (p)->db_field_fetch_by_name
#define	plugin_field_value_func(p) (p)->db_field_value

#define plugin_set_setup_func(p, f) plugin_setup_func(p) = (f)
#define plugin_set_connect_func(p, f) plugin_connect_func(p) = (f)
#define plugin_set_escape_func(p, f) plugin_escape_func(p) = (f)
#define plugin_set_query_func(p, f) plugin_query_func(p) = (f)
#define plugin_set_begin_func(p, f) plugin_begin_func(p) = (f)
#define plugin_set_commit_func(p, f) plugin_commit_func(p) = (f)
#define plugin_set_rollback_func(p, f) plugin_rollback_func(p) = (f)
#define plugin_set_closing_func(p, f) plugin_close_func(p) = (f)
#define	plugin_set_errno_func(p, f) plugin_errno_func(p) = (f)
#define	plugin_set_table_free_func(p, f) plugin_table_free_func(p) = (f)
#define	plugin_set_field_name_func(p, f) plugin_field_name_func(p) = (f)
#define	plugin_set_field_num_func(p, f) plugin_field_num_func(p) = (f)
#define	plugin_set_field_type_func(p, f) plugin_field_type_func(p) = (f)
#define	plugin_set_field_type_by_name_func(p, f) plugin_field_type_by_name_func(p) = (f)
#define	plugin_set_fields_num_func(p, f) plugin_fields_num_func(p) = (f)
#define	plugin_set_rows_num_func(p, f) plugin_rows_num_func(p) = (f)
#define	plugin_set_row_fetch_func(p, f) plugin_row_fetch_func(p) = (f)
#define	plugin_set_field_fetch_func(p, f) plugin_field_fetch_func(p) = (f)
#define	plugin_set_field_fetch_by_name_func(p, f) plugin_field_fetch_by_name_func(p) = (f)
#define	plugin_set_field_value_func(p, f) plugin_field_value_func(p) = (f)

int sql_plugins_init(const char *dirname);

plugin_generic_t *plugin_init(int argc, char **argv);

#endif /* _LIBPRELUDEDB_PLUGIN_DB_H */




