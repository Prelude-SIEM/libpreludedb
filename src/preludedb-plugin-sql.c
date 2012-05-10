/*****
*
* Copyright (C) 2005-2012 CS-SI. All Rights Reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "preludedb.h"
#include "preludedb-plugin-sql.h"


#if ! defined(ENOTSUP) && defined(EOPNOTSUPP)
# define ENOTSUP EOPNOTSUPP
#endif

#define PRELUDEDB_ENOTSUP(x) preludedb_error_verbose(prelude_error_code_from_errno(ENOTSUP), "Database backend does not support '%s' operation", x)


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
        preludedb_plugin_sql_resource_destroy_func_t resource_destroy;
        preludedb_plugin_sql_fetch_row_func_t fetch_row;
        preludedb_plugin_sql_fetch_field_func_t fetch_field;
        preludedb_plugin_sql_build_time_constraint_string_func_t build_time_constraint_string;
        preludedb_plugin_sql_build_time_interval_string_func_t build_time_interval_string;
        preludedb_plugin_sql_build_limit_offset_string_func_t build_limit_offset_string;
        preludedb_plugin_sql_build_constraint_string_func_t build_constraint_string;
        preludedb_plugin_sql_get_operator_string_func_t get_operator_string;
        preludedb_plugin_sql_build_timestamp_string_func_t build_timestamp_string;

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


void preludedb_plugin_sql_set_query_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_query_func_t func)
{
        plugin->query = func;
}


int _preludedb_plugin_sql_query(preludedb_plugin_sql_t *plugin, void *session, const char *query, void **res)
{
        return plugin->query(session, query, res);
}


void preludedb_plugin_sql_set_get_column_count_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_count_func_t func)
{
        plugin->get_column_count = func;
}


unsigned int _preludedb_plugin_sql_get_column_count(preludedb_plugin_sql_t *plugin, void *session, void *resource)
{
        return plugin->get_column_count(session, resource);
}


void preludedb_plugin_sql_set_get_row_count_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_row_count_func_t func)
{
        plugin->get_row_count = func;
}


unsigned int _preludedb_plugin_sql_get_row_count(preludedb_plugin_sql_t *plugin, void *session, void *resource)
{
        return plugin->get_row_count(session, resource);
}


void preludedb_plugin_sql_set_get_column_name_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_name_func_t func)
{
        plugin->get_column_name = func;
}


const char *_preludedb_plugin_sql_get_column_name(preludedb_plugin_sql_t *plugin, void *session, void *resource, unsigned int column_num)
{
        return plugin->get_column_name(session, resource, column_num);
}


void preludedb_plugin_sql_set_get_column_num_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_get_column_num_func_t func)
{
        plugin->get_column_num = func;
}


int _preludedb_plugin_sql_get_column_num(preludedb_plugin_sql_t *plugin, void *session, void *resource, const char *column_name)
{
        return plugin->get_column_num(session, resource, column_name);
}


void preludedb_plugin_sql_set_resource_destroy_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_resource_destroy_func_t func)
{
        plugin->resource_destroy = func;
}


void _preludedb_plugin_sql_resource_destroy(preludedb_plugin_sql_t *plugin, void *session, void *resource)
{
        plugin->resource_destroy(session, resource);
}


void preludedb_plugin_sql_set_fetch_row_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_fetch_row_func_t func)
{
        plugin->fetch_row = func;
}


int _preludedb_plugin_sql_fetch_row(preludedb_plugin_sql_t *plugin, void *session, void *resource, void **row)
{
        if ( ! plugin->fetch_row )
                PRELUDEDB_ENOTSUP("fetch_row");

        return plugin->fetch_row(session, resource, row);
}


void preludedb_plugin_sql_set_fetch_field_func(preludedb_plugin_sql_t *plugin, preludedb_plugin_sql_fetch_field_func_t func)
{
        plugin->fetch_field = func;
}


int _preludedb_plugin_sql_fetch_field(preludedb_plugin_sql_t *plugin,
                                      void *session, void *resource, void *row,
                                      unsigned int column_num, const char **value, size_t *len)
{
        if ( ! plugin->fetch_field )
                return PRELUDEDB_ENOTSUP("fetch_field");

        return plugin->fetch_field(session, resource, row, column_num, value, len);
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


int _preludedb_plugin_sql_build_time_interval_string(preludedb_plugin_sql_t *plugin,
                                                     preludedb_sql_time_constraint_type_t type, int value, char *buf, size_t size)
{
        if ( ! plugin->build_time_interval_string )
                return PRELUDEDB_ENOTSUP("build_time_interval_string");

        return plugin->build_time_interval_string(type, value, buf, size);
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



int preludedb_plugin_sql_new(preludedb_plugin_sql_t **plugin)
{
        *plugin = calloc(1, sizeof(**plugin));
        if ( ! *plugin )
                return prelude_error_from_errno(errno);

        return 0;
}
