/*****
*
* Copyright (C) 2005-2015 CS-SI. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "preludedb.h"
#include "preludedb-plugin-sql.h"


#define PRELUDEDB_ENOTSUP(x) preludedb_error_verbose(prelude_error_code_from_errno(ENOSYS), "Database backend does not support '%s' operation", x)


struct preludedb_plugin_sql {
        PRELUDE_PLUGIN_GENERIC;

        preludedb_plugin_sql_open_func_t open;
        preludedb_plugin_sql_close_func_t close;
        preludedb_plugin_sql_escape_func_t escape;
        preludedb_plugin_sql_escape_binary_func_t escape_binary;
        preludedb_plugin_sql_unescape_binary_func_t unescape_binary;
        preludedb_plugin_sql_query_func_t query;
        preludedb_plugin_sql_get_column_count_func_t get_column_count;
        preludedb_plugin_sql_get_row_count_func_t get_row_count;
        preludedb_plugin_sql_get_column_name_func_t get_column_name;
        preludedb_plugin_sql_get_column_num_func_t get_column_num;
        preludedb_plugin_sql_field_destroy_func_t field_destroy;
        preludedb_plugin_sql_row_destroy_func_t row_destroy;
        preludedb_plugin_sql_table_destroy_func_t table_destroy;
        preludedb_plugin_sql_fetch_row_func_t fetch_row;
        preludedb_plugin_sql_fetch_field_func_t fetch_field;
        preludedb_plugin_sql_build_time_extract_string_func_t build_time_extract_string;
        preludedb_plugin_sql_build_time_constraint_string_func_t build_time_constraint_string;
        preludedb_plugin_sql_build_time_interval_string_func_t build_time_interval_string;
        preludedb_plugin_sql_build_limit_offset_string_func_t build_limit_offset_string;
        preludedb_plugin_sql_build_constraint_string_func_t build_constraint_string;
        preludedb_plugin_sql_get_operator_string_func_t get_operator_string;
        preludedb_plugin_sql_build_timestamp_string_func_t build_timestamp_string;
        preludedb_plugin_sql_get_server_version_func_t get_server_version;
        preludedb_plugin_sql_get_last_insert_ident_func_t get_last_insert_ident;
};



static void bin2hex(const unsigned char *in, size_t inlen, char *out)
{
        static const char hex[] = "0123456789ABCDEF";

        while ( inlen-- ) {
                *(out++) = hex[*in >> 4];
                *(out++) = hex[*in++ & 0x0f];
        }
}



void preludedb_plugin_sql_set_open_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_open_func_t func)
{
        plugin->open = func;
}


int _preludedb_plugin_sql_open(preludedb_plugin_sql_t *plugin,
                               preludedb_sql_settings_t *settings, void **session)
{
        return plugin->open(settings, session);
}



void preludedb_plugin_sql_set_close_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_close_func_t func)
{
        plugin->close = func;
}


void _preludedb_plugin_sql_close(preludedb_plugin_sql_t *plugin, void *session)
{
        plugin->close(session);
}


void preludedb_plugin_sql_set_escape_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_escape_func_t func)
{
        plugin->escape = func;
}


int _preludedb_plugin_sql_escape(preludedb_plugin_sql_t *plugin, void *session, const char *input, size_t input_size, char **output)
{
        if ( ! plugin->escape )
                return _preludedb_plugin_sql_escape_binary(plugin, session, (const unsigned char *) input, input_size, output);

        return plugin->escape(session, input, input_size, output);
}


void preludedb_plugin_sql_set_escape_binary_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_escape_binary_func_t func)
{
        plugin->escape_binary = func;
}


int _preludedb_plugin_sql_escape_binary(preludedb_plugin_sql_t *plugin, void *session,
                                        const unsigned char *input, size_t input_size, char **output)
{
        size_t outsize;

        if ( plugin->escape_binary )
                return plugin->escape_binary(session, input, input_size, output);

        outsize = (input_size * 2) + 4;
        if ( outsize <= input_size )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        *output = malloc(outsize);
        if ( ! *output )
                return preludedb_error_from_errno(errno);

        (*output)[0] = 'X';
        (*output)[1] = '\'';

        bin2hex(input, input_size, *output + 2);

        (*output)[outsize - 2] = '\'';
        (*output)[outsize - 1] = '\0';

        return 0;
}


void preludedb_plugin_sql_set_unescape_binary_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_unescape_binary_func_t func)
{
        plugin->unescape_binary = func;
}


int _preludedb_plugin_sql_unescape_binary(preludedb_plugin_sql_t *plugin, void *session, const char *input,
                                          size_t input_size, unsigned char **output, size_t *output_size)
{
        if ( plugin->unescape_binary )
                return plugin->unescape_binary(session, input, output, output_size);

        *output = malloc(input_size);
        if ( ! *output )
                return preludedb_error_from_errno(errno);

        memcpy(*output, input, input_size);
        *output_size = input_size;

        return 0;
}


void preludedb_plugin_sql_set_get_last_insert_ident_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_last_insert_ident_func_t func)
{
        plugin->get_last_insert_ident = func;
}


int _preludedb_plugin_sql_get_last_insert_ident(preludedb_plugin_sql_t *plugin, void *session, uint64_t *ident)
{
        return plugin->get_last_insert_ident(session, ident);
}


void preludedb_plugin_sql_set_query_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_query_func_t func)
{
        plugin->query = func;
}


int _preludedb_plugin_sql_query(preludedb_plugin_sql_t *plugin, void *session, const char *query, preludedb_sql_table_t **res)
{
        return plugin->query(session, query, res);
}


void preludedb_plugin_sql_set_get_column_count_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_count_func_t func)
{
        plugin->get_column_count = func;
}


unsigned int _preludedb_plugin_sql_get_column_count(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table)
{
        return plugin->get_column_count(session, table);
}


void preludedb_plugin_sql_set_get_row_count_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_row_count_func_t func)
{
        plugin->get_row_count = func;
}


unsigned int _preludedb_plugin_sql_get_row_count(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table)
{
        if ( ! plugin->get_row_count )
                return PRELUDEDB_ENOTSUP("get_row_count");

        return plugin->get_row_count(session, table);
}


void preludedb_plugin_sql_set_get_column_name_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_name_func_t func)
{
        plugin->get_column_name = func;
}


const char *_preludedb_plugin_sql_get_column_name(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table, unsigned int column_num)
{
        return plugin->get_column_name(session, table, column_num);
}


void preludedb_plugin_sql_set_get_column_num_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_num_func_t func)
{
        plugin->get_column_num = func;
}


int _preludedb_plugin_sql_get_column_num(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table, const char *column_name)
{
        return plugin->get_column_num(session, table, column_name);
}


void preludedb_plugin_sql_set_field_destroy_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_field_destroy_func_t func)
{
        plugin->field_destroy = func;
}


void _preludedb_plugin_sql_field_destroy(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table, preludedb_sql_row_t *row, preludedb_sql_field_t *field)
{
        if ( plugin->field_destroy )
                plugin->field_destroy(session, table, row, field);
}


void preludedb_plugin_sql_set_row_destroy_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_row_destroy_func_t func)
{
        plugin->row_destroy = func;
}


void _preludedb_plugin_sql_row_destroy(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table, preludedb_sql_row_t *row)
{
        if ( plugin->row_destroy )
                plugin->row_destroy(session, table, row);
}


void preludedb_plugin_sql_set_table_destroy_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_table_destroy_func_t func)
{
        plugin->table_destroy = func;
}


void _preludedb_plugin_sql_table_destroy(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table)
{
        plugin->table_destroy(session, table);
}


void preludedb_plugin_sql_set_fetch_row_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_fetch_row_func_t func)
{
        plugin->fetch_row = func;
}


int _preludedb_plugin_sql_fetch_row(preludedb_plugin_sql_t *plugin, void *session, preludedb_sql_table_t *table, unsigned int row_index, preludedb_sql_row_t **row)
{
        if ( ! plugin->fetch_row )
                return PRELUDEDB_ENOTSUP("fetch_row");

        return plugin->fetch_row(session, table, row_index, row);
}


void preludedb_plugin_sql_set_fetch_field_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_fetch_field_func_t func)
{
        plugin->fetch_field = func;
}


int _preludedb_plugin_sql_fetch_field(preludedb_plugin_sql_t *plugin,
                                      void *session, preludedb_sql_table_t *table, void *row,
                                      unsigned int column_num, preludedb_sql_field_t **field)
{
        if ( ! plugin->fetch_field )
                return PRELUDEDB_ENOTSUP("fetch_field");

        return plugin->fetch_field(session, table, row, column_num, field);
}


void preludedb_plugin_sql_set_build_time_extract_string_func(preludedb_plugin_sql_t *plugin,
                                                             preludedb_plugin_sql_build_time_extract_string_func_t func)
{
        plugin->build_time_extract_string = func;
}



int _preludedb_plugin_sql_build_time_extract_string(preludedb_plugin_sql_t *plugin,
                                                    prelude_string_t *output, const char *field,
                                                    preludedb_sql_time_constraint_type_t type,
                                                   int gmt_offset)
{
        if ( ! plugin->build_time_extract_string )
                return PRELUDEDB_ENOTSUP("build_time_extract_string");

        return plugin->build_time_extract_string(output, field, type, gmt_offset);
}



void preludedb_plugin_sql_set_build_time_constraint_string_func(preludedb_plugin_sql_t *plugin,
                                                                preludedb_plugin_sql_build_time_constraint_string_func_t func)
{
        plugin->build_time_constraint_string = func;
}


int _preludedb_plugin_sql_build_time_constraint_string(preludedb_plugin_sql_t *plugin,
                                                       prelude_string_t *output, const char *field,
                                                       preludedb_sql_time_constraint_type_t type,
                                                       idmef_criterion_operator_t operator, int value, int gmt_offset)
{
        if ( ! plugin->build_time_constraint_string )
                return PRELUDEDB_ENOTSUP("build_time_constraint_string");

        return plugin->build_time_constraint_string(output, field, type, operator, value, gmt_offset);
}



void preludedb_plugin_sql_set_build_time_interval_string_func(preludedb_plugin_sql_t *plugin,
                                                              preludedb_plugin_sql_build_time_interval_string_func_t func)
{
        plugin->build_time_interval_string = func;
}


int _preludedb_plugin_sql_build_time_interval_string(preludedb_plugin_sql_t *plugin, prelude_string_t *output, const char *field, const char *value, preludedb_selected_object_interval_t unit)
{
        if ( ! plugin->build_time_interval_string )
                return PRELUDEDB_ENOTSUP("build_time_interval_string");

        return plugin->build_time_interval_string(output, field, value, unit);
}


void preludedb_plugin_sql_set_build_limit_offset_string_func(preludedb_plugin_sql_t *plugin,
                                                             preludedb_plugin_sql_build_limit_offset_string_func_t func)
{
        plugin->build_limit_offset_string = func;
}


int _preludedb_plugin_sql_build_limit_offset_string(preludedb_plugin_sql_t *plugin,
                                                    void *session, int limit, int offset, prelude_string_t *output)
{
        if ( ! plugin->build_limit_offset_string )
                return PRELUDEDB_ENOTSUP("build_limit_offset_string");

        return plugin->build_limit_offset_string(session, limit, offset, output);
}


void preludedb_plugin_sql_set_build_constraint_string_func(preludedb_plugin_sql_t *plugin,
                                                           preludedb_plugin_sql_build_constraint_string_func_t func)
{
        plugin->build_constraint_string = func;
}


int _preludedb_plugin_sql_build_constraint_string(preludedb_plugin_sql_t *plugin,
                                                  prelude_string_t *out, const char *field,
                                                  idmef_criterion_operator_t operator, const char *value)
{
        if ( ! plugin->build_constraint_string )
                return PRELUDEDB_ENOTSUP("build_constraint_string");

        return plugin->build_constraint_string(out, field, operator, value);
}


void preludedb_plugin_sql_set_get_operator_string_func(preludedb_plugin_sql_t *plugin,
                                                       preludedb_plugin_sql_get_operator_string_func_t func)
{
        plugin->get_operator_string = func;
}

const char *_preludedb_plugin_sql_get_operator_string(preludedb_plugin_sql_t *plugin, idmef_criterion_operator_t operator)
{
        return plugin->get_operator_string(operator);
}


void preludedb_plugin_sql_set_build_timestamp_string_func(preludedb_plugin_sql_t *plugin,
                                                          preludedb_plugin_sql_build_timestamp_string_func_t func)
{
        plugin->build_timestamp_string = func;
}


int _preludedb_plugin_sql_build_timestamp_string(preludedb_plugin_sql_t *plugin, const struct tm *lt, char *out, size_t size)
{
        int ret;

        if ( plugin->build_timestamp_string )
                return plugin->build_timestamp_string(lt, out, size);

        ret = snprintf(out, size, "'%d-%.2d-%.2d %.2d:%.2d:%.2d'",
                       lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                       lt->tm_hour, lt->tm_min, lt->tm_sec);

        return (ret < 0 || (size_t) ret >= size) ? -1 : 0;
}


void preludedb_plugin_sql_set_get_server_version_func(preludedb_plugin_sql_t *plugin,
                                                      preludedb_plugin_sql_get_server_version_func_t func)
{
        plugin->get_server_version = func;
}


long _preludedb_plugin_sql_get_server_version(preludedb_plugin_sql_t *plugin, void *session)
{
        if ( ! plugin->get_server_version )
                return PRELUDEDB_ENOTSUP("get_server_version");

        return plugin->get_server_version(session);
}


int preludedb_plugin_sql_new(preludedb_plugin_sql_t **plugin)
{
        *plugin = calloc(1, sizeof(**plugin));
        if ( ! *plugin )
                return prelude_error_from_errno(errno);

        return 0;
}
