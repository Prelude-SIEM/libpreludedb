/*****
*
* Copyright (C) 2001-2004 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2003-2005 Nicolas Delon <nicolas@prelude-ids.org>
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

#ifndef _LIBPRELUDEDB_PLUGIN_SQL_H
#define _LIBPRELUDEDB_PLUGIN_SQL_H

#include <libprelude/prelude-plugin.h>


typedef struct {
        PRELUDE_PLUGIN_GENERIC;

        int (*open)(preludedb_sql_settings_t *settings, void **session);
        void (*close) (void *session);
	const char *(*get_error)(void *session);
        int (*escape)(void *session, const char *input, size_t input_size, char **output);
	int (*escape_binary)(void *session, const unsigned char *input, size_t input_size, char **output);
	int (*unescape_binary)(void *session, const char *input, unsigned char **output, size_t *output_size);
        int (*query)(void *session, const char *query, void **res);
	unsigned int (*get_column_count)(void *session, void *resource);
	unsigned int (*get_row_count)(void *session, void *resource);
	const char *(*get_column_name)(void *session, void *resource, unsigned int column_num);
	int (*get_column_num)(void *session, void *resource, const char *column_name);
	void (*resource_destroy)(void *session, void *resource);
	int (*fetch_row)(void *session, void *resource, void **row);
	int (*fetch_field)(void *session, void *resource, void *row, unsigned int column_num,
			   const char **value, size_t *len);
	int (*build_time_constraint_string)(prelude_string_t *output, const char *field,
					    preludedb_sql_time_constraint_type_t type,
					    idmef_value_relation_t relation, int value, int gmt_offset);
	int (*build_time_interval_string)(preludedb_sql_time_constraint_type_t type, int value,
					  char *buf, size_t size);
	int (*build_limit_offset_string)(void *session, int limit, int offset, prelude_string_t *output);
} preludedb_plugin_sql_t;



#define preludedb_plugin_sql_open_func(p) (p)->open
#define preludedb_plugin_sql_close_func(p) (p)->close
#define preludedb_plugin_sql_get_error_func(p) (p)->get_error
#define preludedb_plugin_sql_escape_func(p) (p)->escape
#define preludedb_plugin_sql_escape_binary_func(p) (p)->escape_binary
#define preludedb_plugin_sql_unescape_binary_func(p) (p)->unescape_binary
#define preludedb_plugin_sql_query_func(p) (p)->query
#define preludedb_plugin_sql_get_column_count_func(p) (p)->get_column_count
#define preludedb_plugin_sql_get_row_count_func(p) (p)->get_row_count
#define preludedb_plugin_sql_get_column_name_func(p) (p)->get_column_name
#define preludedb_plugin_sql_get_column_num_func(p) (p)->get_column_num
#define preludedb_plugin_sql_resource_destroy_func(p) (p)->resource_destroy
#define preludedb_plugin_sql_fetch_row_func(p) (p)->fetch_row
#define preludedb_plugin_sql_fetch_field_func(p) (p)->fetch_field
#define preludedb_plugin_sql_build_time_constraint_string_func(p) (p)->build_time_constraint_string
#define preludedb_plugin_sql_build_time_interval_string_func(p) (p)->build_time_interval_string
#define preludedb_plugin_sql_build_limit_offset_string_func(p) (p)->build_limit_offset_string

#define preludedb_plugin_sql_set_open_func(p, f) preludedb_plugin_sql_open_func(p) = (f)
#define preludedb_plugin_sql_set_close_func(p, f) preludedb_plugin_sql_close_func(p) = (f)
#define preludedb_plugin_sql_set_get_error_func(p, f) preludedb_plugin_sql_get_error_func(p) = (f)
#define preludedb_plugin_sql_set_escape_func(p, f) preludedb_plugin_sql_escape_func(p) = (f)
#define preludedb_plugin_sql_set_escape_binary_func(p, f) preludedb_plugin_sql_escape_binary_func(p) = (f)
#define preludedb_plugin_sql_set_unescape_binary_func(p, f) preludedb_plugin_sql_unescape_binary_func(p) = (f)
#define preludedb_plugin_sql_set_query_func(p, f) preludedb_plugin_sql_query_func(p) = (f)
#define preludedb_plugin_sql_set_get_column_count_func(p, f) preludedb_plugin_sql_get_column_count_func(p) = (f)
#define preludedb_plugin_sql_set_get_row_count_func(p, f) preludedb_plugin_sql_get_row_count_func(p) = (f)
#define preludedb_plugin_sql_set_get_column_name_func(p, f) preludedb_plugin_sql_get_column_name_func(p) = (f)
#define preludedb_plugin_sql_set_get_column_num_func(p, f) preludedb_plugin_sql_get_column_num_func(p) = (f)
#define preludedb_plugin_sql_set_resource_destroy_func(p, f) preludedb_plugin_sql_resource_destroy_func(p) = (f)
#define preludedb_plugin_sql_set_fetch_row_func(p, f) preludedb_plugin_sql_fetch_row_func(p) = (f)
#define preludedb_plugin_sql_set_fetch_field_func(p, f) preludedb_plugin_sql_fetch_field_func(p) = (f)
#define	preludedb_plugin_sql_set_build_time_constraint_string_func(p, f) preludedb_plugin_sql_build_time_constraint_string_func(p) = (f)
#define	preludedb_plugin_sql_set_build_time_interval_string_func(p, f) preludedb_plugin_sql_build_time_interval_string_func(p) = (f)
#define	preludedb_plugin_sql_set_build_limit_offset_string_func(p, f) preludedb_plugin_sql_build_limit_offset_string_func(p) = (f)


#endif /* _LIBPRELUDEDB_PLUGIN_SQL_H */
