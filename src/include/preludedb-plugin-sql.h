/*****
*
* Copyright (C) 2003-2005 PreludeIDS Technologies. All Rights Reserved.
* Author: Nicolas Delon <nicolas.delon@prelude-ids.com>
*
* This file is part of the PreludeDB library.
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

        int (*open)(preludedb_sql_settings_t *settings, void **session, char *errbuf, size_t size);
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
					    idmef_criterion_operator_t operator, int value, int gmt_offset);
	int (*build_time_interval_string)(preludedb_sql_time_constraint_type_t type, int value,
					  char *buf, size_t size);
	int (*build_limit_offset_string)(void *session, int limit, int offset, prelude_string_t *output);
        const char *(*get_operator_string)(idmef_criterion_operator_t operator);
} preludedb_plugin_sql_t;


#define preludedb_plugin_sql_set_open_func(p, f) \
        (p)->open = (f)

#define preludedb_plugin_sql_set_close_func(p, f) \
        (p)->close = (f)

#define preludedb_plugin_sql_set_get_error_func(p, f) \
        (p)->get_error = (f)

#define preludedb_plugin_sql_set_escape_func(p, f) \
        (p)->escape = (f)

#define preludedb_plugin_sql_set_escape_binary_func(p, f) \
        (p)->escape_binary = (f)

#define preludedb_plugin_sql_set_unescape_binary_func(p, f) \
        (p)->unescape_binary = (f)

#define preludedb_plugin_sql_set_query_func(p, f) \
        (p)->query = (f)

#define preludedb_plugin_sql_set_get_column_count_func(p, f) \
        (p)->get_column_count = (f)

#define preludedb_plugin_sql_set_get_row_count_func(p, f) \
        (p)->get_row_count = (f)

#define preludedb_plugin_sql_set_get_column_name_func(p, f) \
        (p)->get_column_name = (f)

#define preludedb_plugin_sql_set_get_column_num_func(p, f) \
        (p)->get_column_num = (f)

#define preludedb_plugin_sql_set_resource_destroy_func(p, f) \
        (p)->resource_destroy = (f)

#define preludedb_plugin_sql_set_fetch_row_func(p, f) \
        (p)->fetch_row = (f)

#define preludedb_plugin_sql_set_fetch_field_func(p, f) \
        (p)->fetch_field = (f)

#define	preludedb_plugin_sql_set_build_time_constraint_string_func(p, f) \
        (p)->build_time_constraint_string = (f)

#define	preludedb_plugin_sql_set_build_time_interval_string_func(p, f) \
        (p)->build_time_interval_string = (f)

#define	preludedb_plugin_sql_set_build_limit_offset_string_func(p, f) \
        (p)->build_limit_offset_string = (f)

#define preludedb_plugin_sql_set_get_operator_string_func(p, f) \
        (p)->get_operator_string = (f)

#endif /* _LIBPRELUDEDB_PLUGIN_SQL_H */
