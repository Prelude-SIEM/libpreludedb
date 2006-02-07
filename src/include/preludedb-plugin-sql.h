/*****
*
* Copyright (C) 2005 PreludeIDS Technologies. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoann.v@prelude-ids.com>
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

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct preludedb_plugin_sql preludedb_plugin_sql_t;

typedef int (*preludedb_plugin_sql_open_func_t)(preludedb_sql_settings_t *settings, void **session);         
typedef void (*preludedb_plugin_sql_close_func_t)(void *session);
typedef int (*preludedb_plugin_sql_escape_func_t)(void *session, const char *input, size_t input_size, char **output);
typedef int (*preludedb_plugin_sql_escape_binary_func_t)(void *session, const unsigned char *input, size_t input_size, char **output);
typedef int (*preludedb_plugin_sql_unescape_binary_func_t)(void *session, const char *input, unsigned char **output, size_t *output_size);
typedef int (*preludedb_plugin_sql_query_func_t)(void *session, const char *query, void **res);
typedef unsigned int (*preludedb_plugin_sql_get_column_count_func_t)(void *session, void *resource);
typedef unsigned int (*preludedb_plugin_sql_get_row_count_func_t)(void *session, void *resource);
typedef const char *(*preludedb_plugin_sql_get_column_name_func_t)(void *session, void *resource, unsigned int column_num);
typedef int (*preludedb_plugin_sql_get_column_num_func_t)(void *session, void *resource, const char *column_name);
typedef void (*preludedb_plugin_sql_resource_destroy_func_t)(void *session, void *resource);
typedef int (*preludedb_plugin_sql_fetch_row_func_t)(void *session, void *resource, void **row);
typedef int (*preludedb_plugin_sql_fetch_field_func_t)(void *session, void *resource, void *row,
                                                       unsigned int column_num, const char **value, size_t *len);

typedef int (*preludedb_plugin_sql_build_time_constraint_string_func_t)(prelude_string_t *output, const char *field,
                                                                        preludedb_sql_time_constraint_type_t type,
                                                                        idmef_criterion_operator_t operator, int value, int gmt_offset);

typedef int (*preludedb_plugin_sql_build_time_interval_string_func_t)(preludedb_sql_time_constraint_type_t type, int value,
                                                                      char *buf, size_t size);

typedef int (*preludedb_plugin_sql_build_limit_offset_string_func_t)(void *session, int limit, int offset, prelude_string_t *output);
typedef int (*preludedb_plugin_sql_build_constraint_string_func_t)(prelude_string_t *out, const char *field,
                                                                   idmef_criterion_operator_t operator, const char *value);

typedef const char *(*preludedb_plugin_sql_get_operator_string_func_t)(idmef_criterion_operator_t operator);
typedef int (*preludedb_plugin_sql_build_timestamp_string_func_t)(const struct tm *t, char *out, size_t size);




void preludedb_plugin_sql_set_open_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_open_func_t func);

int _preludedb_plugin_sql_open(preludedb_plugin_sql_t *plugin,
                               preludedb_sql_settings_t *settings, void **session);

void preludedb_plugin_sql_set_close_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_close_func_t func);

void _preludedb_plugin_sql_close(preludedb_plugin_sql_t *plugin, void *session);

void preludedb_plugin_sql_set_escape_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_escape_func_t func);

int _preludedb_plugin_sql_escape(preludedb_plugin_sql_t *plugin, void *session, const char *input, size_t input_size, char **output);

void preludedb_plugin_sql_set_escape_binary_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_escape_binary_func_t func);

int _preludedb_plugin_sql_escape_binary(preludedb_plugin_sql_t *plugin, void *session,
                                        const unsigned char *input, size_t input_size, char **output);

void preludedb_plugin_sql_set_unescape_binary_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_unescape_binary_func_t func);

int _preludedb_plugin_sql_unescape_binary(preludedb_plugin_sql_t *plugin, void *session, const char *input,
                                          size_t input_size, unsigned char **output, size_t *output_size);

void preludedb_plugin_sql_set_query_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_query_func_t func);

int _preludedb_plugin_sql_query(preludedb_plugin_sql_t *plugin, void *session, const char *query, void **res);

void preludedb_plugin_sql_set_get_column_count_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_count_func_t func);

unsigned int _preludedb_plugin_sql_get_column_count(preludedb_plugin_sql_t *plugin, void *session, void *resource);

void preludedb_plugin_sql_set_get_row_count_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_row_count_func_t func);

unsigned int _preludedb_plugin_sql_get_row_count(preludedb_plugin_sql_t *plugin, void *session, void *resource);

void preludedb_plugin_sql_set_get_column_name_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_name_func_t func);

const char *_preludedb_plugin_sql_get_column_name(preludedb_plugin_sql_t *plugin, void *session, void *resource, unsigned int column_num);

void preludedb_plugin_sql_set_get_column_num_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_num_func_t func);

int _preludedb_plugin_sql_get_column_num(preludedb_plugin_sql_t *plugin, void *session, void *resource, const char *column_name);

void preludedb_plugin_sql_set_resource_destroy_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_resource_destroy_func_t func);

void _preludedb_plugin_sql_resource_destroy(preludedb_plugin_sql_t *plugin, void *session, void *resource);

void preludedb_plugin_sql_set_fetch_row_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_fetch_row_func_t func);

int _preludedb_plugin_sql_fetch_row(preludedb_plugin_sql_t *plugin, void *session, void *resource, void **row);

void preludedb_plugin_sql_set_fetch_field_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_fetch_field_func_t func);

int _preludedb_plugin_sql_fetch_field(preludedb_plugin_sql_t *plugin,
                                      void *session, void *resource, void *row,
                                      unsigned int column_num, const char **value, size_t *len);

void preludedb_plugin_sql_set_build_time_constraint_string_func(preludedb_plugin_sql_t *plugin,
                                                                preludedb_plugin_sql_build_time_constraint_string_func_t func);

int _preludedb_plugin_sql_build_time_constraint_string(preludedb_plugin_sql_t *plugin,
                                                       prelude_string_t *output, const char *field,
                                                       preludedb_sql_time_constraint_type_t type,
                                                       idmef_criterion_operator_t operator, int value, int gmt_offset);

void preludedb_plugin_sql_set_build_time_interval_string_func(preludedb_plugin_sql_t *plugin,
                                                              preludedb_plugin_sql_build_time_interval_string_func_t func);

int _preludedb_plugin_sql_build_time_interval_string(preludedb_plugin_sql_t *plugin,
                                                     preludedb_sql_time_constraint_type_t type, int value, char *buf, size_t size);

void preludedb_plugin_sql_set_build_limit_offset_string_func(preludedb_plugin_sql_t *plugin,
                                                             preludedb_plugin_sql_build_limit_offset_string_func_t func);

int _preludedb_plugin_sql_build_limit_offset_string(preludedb_plugin_sql_t *plugin,
                                                    void *session, int limit, int offset, prelude_string_t *output);

void preludedb_plugin_sql_set_build_constraint_string_func(preludedb_plugin_sql_t *plugin,
                                                           preludedb_plugin_sql_build_constraint_string_func_t func);

int _preludedb_plugin_sql_build_constraint_string(preludedb_plugin_sql_t *plugin,
                                                  prelude_string_t *out, const char *field,
                                                  idmef_criterion_operator_t operator, const char *value);

void preludedb_plugin_sql_set_get_operator_string_func(preludedb_plugin_sql_t *plugin,
                                                       preludedb_plugin_sql_get_operator_string_func_t func);

const char *_preludedb_plugin_sql_get_operator_string(preludedb_plugin_sql_t *plugin, idmef_criterion_operator_t operator);

void preludedb_plugin_sql_set_build_timestamp_string_func(preludedb_plugin_sql_t *plugin,
                                                          preludedb_plugin_sql_build_timestamp_string_func_t func);

int _preludedb_plugin_sql_build_timestamp_string(preludedb_plugin_sql_t *plugin, const struct tm *t, char *out, size_t size);

int preludedb_plugin_sql_new(preludedb_plugin_sql_t **plugin);

#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_PLUGIN_SQL_H */
