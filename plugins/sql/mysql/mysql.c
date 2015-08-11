/*****
*
* Copyright (C) 2003-2015 CS-SI. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#include "config.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>

#include <libprelude/idmef.h>
#include <libprelude/common.h>
#include <libprelude/prelude-error.h>
#include <libprelude/prelude-string.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"
#include "preludedb-plugin-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"


#define WAIT_TIMEOUT_VALUE "31536000" /* 365 days */


#if ! defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID < 32224
# define mysql_field_count mysql_num_fields
#endif /* ! MYSQL_VERSION_ID */



typedef struct {
        MYSQL_ROW *row;
        unsigned long lengths[1];
} mysql_row_data_t;



int mysql_LTX_prelude_plugin_version(void);
int mysql_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data);


static prelude_bool_t is_connection_broken(void *session)
{
        switch (mysql_errno(session)) {

        case CR_CONNECTION_ERROR:
        case CR_SERVER_GONE_ERROR:
        case CR_SERVER_LOST:
        case CR_CONN_HOST_ERROR:
        case CR_IPSOCK_ERROR:
        case ER_SERVER_SHUTDOWN:
                return TRUE;

        default:
                return FALSE;
        }
}



static int handle_error(void *session, prelude_error_code_t code)
{
        int ret;

        if ( is_connection_broken(session) )
                code = PRELUDEDB_ERROR_CONNECTION;

        if ( mysql_errno(session) )
                ret = preludedb_error_verbose(code, "%s", mysql_error(session));
        else
                ret = preludedb_error(code);

        return ret;
}



static int sql_open(preludedb_sql_settings_t *settings, void **session)
{
        int ret;
        unsigned int port = 0;

        if ( preludedb_sql_settings_get_port(settings) )
                port = atoi(preludedb_sql_settings_get_port(settings));

        *session = mysql_init(NULL);
        if ( ! *session )
                return preludedb_error_from_errno(errno);

        if ( ! mysql_real_connect(*session,
                                  preludedb_sql_settings_get_host(settings),
                                  preludedb_sql_settings_get_user(settings),
                                  preludedb_sql_settings_get_pass(settings),
                                  preludedb_sql_settings_get_name(settings),
                                  port, NULL, 0) ) {

                ret = handle_error(*session, PRELUDEDB_ERROR_CONNECTION);
                mysql_close(*session);

                return ret;
        }

        mysql_query(*session, "SET SESSION wait_timeout=" WAIT_TIMEOUT_VALUE);

        return 0;
}



static void sql_close(void *session)
{
        mysql_close((MYSQL *) session);
}


static int sql_escape_binary(void *session, const unsigned char *input, size_t input_size, char **output)
{
        size_t rsize;

        /*
         * MySQL documentation say :
         * The string pointed to by from must be length bytes long. You must
         * allocate the to buffer to be at least length*2+1 bytes long. (In the
         * worse case, each character may need to be encoded as using two bytes,
         * and you need room for the terminating null byte.)
         */
        rsize = input_size * 2 + 3;
        if ( rsize <= input_size )
                return -1;

        *output = malloc(rsize);
        if ( ! *output )
                return preludedb_error_from_errno(errno);

        (*output)[0] = '\'';

#ifdef HAVE_MYSQL_REAL_ESCAPE_STRING
        rsize = mysql_real_escape_string((MYSQL *) session, (*output) + 1, (const char *) input, input_size);
#else
        rsize = mysql_escape_string((*output) + 1, (const char *) input, input_size);
#endif

        (*output)[rsize + 1] = '\'';
        (*output)[rsize + 2] = '\0';

        return 0;
}



static int sql_build_limit_offset_string(void *session, int limit, int offset, prelude_string_t *output)
{
        if ( limit >= 0 ) {
                if ( offset >= 0 )
                        return prelude_string_sprintf(output, " LIMIT %d, %d", offset, limit);

                return prelude_string_sprintf(output, " LIMIT %d", limit);
        }

        else if ( offset >= 0 )
                return prelude_string_sprintf(output, " LIMIT %d, %" PRELUDE_PRIu64, offset, PRELUDE_UINT64_MAX);

        return 0;
}




static int sql_query(void *session, const char *query, preludedb_sql_table_t **table)
{
        int ret;
        MYSQL_RES *result;

        if ( mysql_query(session, query) != 0 )
                return handle_error(session, PRELUDEDB_ERROR_QUERY);

        result = mysql_store_result(session);
        if ( ! result )
                return mysql_errno(session) ? handle_error(session, PRELUDEDB_ERROR_QUERY) : 0;

        if ( mysql_num_rows(result) == 0 ) {
                mysql_free_result(result);
                return 0;
        }

        ret = preludedb_sql_table_new(table, result);
        if ( ret < 0 ) {
                mysql_free_result(result);
                return ret;
        }

        return 1;
}



static int sql_get_last_insert_ident(void *session, uint64_t *ident)
{
        *ident = mysql_insert_id(session);
        if ( *ident == 0 )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "could not retrieve last insert ID");

        return 0;
}


static void sql_table_destroy(void *session, preludedb_sql_table_t *table)
{
        mysql_free_result(preludedb_sql_table_get_data(table));
}



static MYSQL_FIELD *get_field(MYSQL_RES *res, unsigned int column_num)
{
        return (column_num >= mysql_num_fields(res)) ? NULL : mysql_fetch_field_direct(res, column_num);
}



static const char *sql_get_column_name(void *session, preludedb_sql_table_t *table, unsigned int column_num)
{
        MYSQL_FIELD *field;

        field = get_field(preludedb_sql_table_get_data(table), column_num);

        return field ? field->name : NULL;
}



static int sql_get_column_num(void *session, preludedb_sql_table_t *table, const char *column_name)
{
        int fields_num, i;
        MYSQL_FIELD *fields;
        MYSQL_RES *result = preludedb_sql_table_get_data(table);

        fields = mysql_fetch_fields(result);
        if ( ! fields )
                return -1;

        fields_num = mysql_num_fields(result);
        for ( i = 0; i < fields_num; i++ ) {
                if ( strcmp(column_name, fields[i].name) == 0 )
                        return i;
        }

        return -1;
}



static unsigned int sql_get_column_count(void *session, preludedb_sql_table_t *table)
{
        return mysql_num_fields(preludedb_sql_table_get_data(table));
}



static unsigned int sql_get_row_count(void *session, preludedb_sql_table_t *table)
{
        return (unsigned int) mysql_num_rows(preludedb_sql_table_get_data(table));
}


static void sql_destroy_row(void *session, preludedb_sql_table_t *table, preludedb_sql_row_t *row)
{
        free(preludedb_sql_row_get_data(row));
}


static int sql_fetch_row(void *session, preludedb_sql_table_t *table, unsigned int row_index, preludedb_sql_row_t **rrow)
{
        int ret;
        void *row;
        mysql_row_data_t *myrow;
        unsigned long *lengths;
        unsigned int column_count, i;
        MYSQL_RES *result = preludedb_sql_table_get_data(table);

        column_count = preludedb_sql_table_get_column_count(table);

        while ( preludedb_sql_table_get_fetched_row_count(table) <= row_index ) {
                row = mysql_fetch_row(result);
                if ( ! row ) {
                        ret = mysql_errno(session);
                        if ( ret )
                                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, mysql_error(session));

                        return 0;
                }

                lengths = mysql_fetch_lengths(result);
                if ( ! lengths )
                        return preludedb_error(PRELUDEDB_ERROR_GENERIC);

                ret = preludedb_sql_table_new_row(table, rrow, preludedb_sql_table_get_fetched_row_count(table));
                if ( ret < 0 )
                        return ret;

                myrow = malloc(offsetof(mysql_row_data_t, lengths) + column_count * (sizeof(unsigned long)));
                if ( ! myrow ) {
                        preludedb_sql_row_destroy(*rrow);
                        return preludedb_error_from_errno(errno);
                }

                for ( i = 0; i < column_count; i++ )
                        myrow->lengths[i] = lengths[i];

                myrow->row = row;
                preludedb_sql_row_set_data(*rrow, myrow);
        }

        return 1;
}



static int sql_fetch_field(void *session, preludedb_sql_table_t *table, preludedb_sql_row_t *row,
                           unsigned int column_num, preludedb_sql_field_t **field)
{
        mysql_row_data_t *d = preludedb_sql_row_get_data(row);
        void *data;
        size_t dlen = 0;

        if ( column_num >= mysql_num_fields(preludedb_sql_table_get_data(table)) )
                return preludedb_error(PRELUDEDB_ERROR_INVALID_COLUMN_NUM);

        data = d->row[column_num];
        if ( data )
                dlen = d->lengths[column_num];

        return preludedb_sql_row_new_field(row, field, column_num, data, dlen);
}



static const char *get_operator_string(idmef_criterion_operator_t operator)
{
        int i;
        struct tbl {
                idmef_criterion_operator_t operator;
                const char *name;
        } tbl[] = {

                { IDMEF_CRITERION_OPERATOR_EQUAL,             "= BINARY"          },
                { IDMEF_CRITERION_OPERATOR_EQUAL_NOCASE,      "="                 },
                { IDMEF_CRITERION_OPERATOR_NOT_EQUAL,         "!= BINARY"         },
                { IDMEF_CRITERION_OPERATOR_NOT_EQUAL_NOCASE,  "!="                },

                { IDMEF_CRITERION_OPERATOR_GREATER,           ">"                 },
                { IDMEF_CRITERION_OPERATOR_GREATER_OR_EQUAL,  ">="                },
                { IDMEF_CRITERION_OPERATOR_LESSER,            "<"                 },
                { IDMEF_CRITERION_OPERATOR_LESSER_OR_EQUAL,   "<="                },

                { IDMEF_CRITERION_OPERATOR_SUBSTR,            "LIKE BINARY"       },
                { IDMEF_CRITERION_OPERATOR_SUBSTR_NOCASE,     "LIKE"              },
                { IDMEF_CRITERION_OPERATOR_NOT_SUBSTR,        "NOT LIKE BINARY"   },
                { IDMEF_CRITERION_OPERATOR_NOT_SUBSTR_NOCASE, "NOT LIKE "         },

                { IDMEF_CRITERION_OPERATOR_REGEX,             "REGEXP BINARY"     },
                { IDMEF_CRITERION_OPERATOR_REGEX_NOCASE,      "REGEXP"            },
                { IDMEF_CRITERION_OPERATOR_NOT_REGEX,         "NOT REGEXP"        },
                { IDMEF_CRITERION_OPERATOR_NOT_REGEX_NOCASE,  "NOT REGEXP BINARY" },

                { IDMEF_CRITERION_OPERATOR_NULL,              "IS NULL"           },
                { IDMEF_CRITERION_OPERATOR_NOT_NULL,          "IS NOT NULL"       },
                { 0, NULL },
        };

        for ( i = 0; tbl[i].operator != 0; i++ )
                if ( operator == tbl[i].operator )
                        return tbl[i].name;

        return NULL;
}



static int sql_build_constraint_string(prelude_string_t *out, const char *field,
                                       idmef_criterion_operator_t operator, const char *value)
{
        const char *op_str;

        op_str = get_operator_string(operator);
        if ( ! op_str )
                return -1;

        if ( ! value )
                value = "";

        return prelude_string_sprintf(out, "%s %s %s", field, op_str, value);
}



static int sql_build_time_extract_string(prelude_string_t *output, const char *field, preludedb_sql_time_constraint_type_t type, int gmt_offset)
{
        int ret;
        char buf[128];

        if ( ! gmt_offset )
                ret = snprintf(buf, sizeof(buf), "%s", field);
        else
                ret = snprintf(buf, sizeof(buf), "DATE_ADD(%s, INTERVAL %d HOUR)", field, gmt_offset / 3600);

        if ( ret < 0 || (size_t) ret >= sizeof(buf) )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        switch ( type ) {
        case PRELUDEDB_SQL_TIME_CONSTRAINT_QUARTER:
                return prelude_string_sprintf(output, "QUARTER(%s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
                return prelude_string_sprintf(output, "EXTRACT(YEAR FROM %s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
                return prelude_string_sprintf(output, "EXTRACT(MONTH FROM %s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY:
                return prelude_string_sprintf(output, "DAYOFYEAR(%s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
                return prelude_string_sprintf(output, "DAYOFMONTH(%s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY:
                return prelude_string_sprintf(output, "DAYOFWEEK(%s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
                return prelude_string_sprintf(output, "EXTRACT(HOUR FROM %s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
                return prelude_string_sprintf(output, "EXTRACT(MINUTE FROM %s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
                return prelude_string_sprintf(output, "EXTRACT(SECOND FROM %s)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MSEC:
                return prelude_string_sprintf(output, "(EXTRACT(MICROSECOND FROM %s) / 1000)", buf);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_USEC:
                return prelude_string_sprintf(output, "EXTRACT(MICROSECOND FROM %s)", buf);
        }

        return preludedb_error(PRELUDEDB_ERROR_GENERIC);
}


static int sql_build_time_constraint_string(prelude_string_t *output, const char *field,
                                            preludedb_sql_time_constraint_type_t type,
                                            idmef_criterion_operator_t operator, int value, int gmt_offset)
{
        const char *sql_operator;
        int ret;

        ret = sql_build_time_extract_string(output, field, type, gmt_offset);
        if ( ret < 0 )
                return ret;

        sql_operator = get_operator_string(operator);
        if ( ! sql_operator )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        if ( type == PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY )
                value = value % 7 + 1;

        return prelude_string_sprintf(output, " %s '%d'", sql_operator, value);
}



static int sql_build_time_interval_string(prelude_string_t *output, const char *field, const char *value, preludedb_selected_object_interval_t unit)
{
        const char *sunit;

        switch ( unit ) {
        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_QUARTER:
                sunit = "QUARTER";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_YEAR:
                sunit = "YEAR";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_MONTH:
                sunit = "MONTH";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_DAY:
                sunit = "DAY";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_HOUR:
                sunit = "HOUR";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_MIN:
                sunit = "MINUTE";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_SEC:
                sunit = "SECOND";
                break;

        default:
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);
        }

        return prelude_string_sprintf(output, "(%s + INTERVAL %s %s)", field, value, sunit);
}



static long sql_get_server_version(void *session)
{
        return mysql_get_server_version(session);
}



int mysql_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data)
{
        int ret;
        preludedb_plugin_sql_t *plugin;

        ret = preludedb_plugin_sql_new(&plugin);
        if ( ret < 0 )
                return ret;

        prelude_plugin_set_name((prelude_plugin_generic_t *) plugin, "MySQL");
        prelude_plugin_entry_set_plugin(pe, (void *) plugin);

        preludedb_plugin_sql_set_open_func(plugin, sql_open);
        preludedb_plugin_sql_set_close_func(plugin, sql_close);
        preludedb_plugin_sql_set_escape_binary_func(plugin, sql_escape_binary);
        preludedb_plugin_sql_set_query_func(plugin, sql_query);
        preludedb_plugin_sql_set_get_server_version_func(plugin, sql_get_server_version);
        preludedb_plugin_sql_set_table_destroy_func(plugin, sql_table_destroy);
        preludedb_plugin_sql_set_get_column_count_func(plugin, sql_get_column_count);
        preludedb_plugin_sql_set_get_row_count_func(plugin, sql_get_row_count);
        preludedb_plugin_sql_set_get_column_name_func(plugin, sql_get_column_name);
        preludedb_plugin_sql_set_get_column_num_func(plugin, sql_get_column_num);
        preludedb_plugin_sql_set_get_operator_string_func(plugin, get_operator_string);
        preludedb_plugin_sql_set_fetch_row_func(plugin, sql_fetch_row);
        preludedb_plugin_sql_set_row_destroy_func(plugin, sql_destroy_row);
        preludedb_plugin_sql_set_fetch_field_func(plugin, sql_fetch_field);
        preludedb_plugin_sql_set_build_constraint_string_func(plugin, sql_build_constraint_string);
        preludedb_plugin_sql_set_build_time_extract_string_func(plugin, sql_build_time_extract_string);
        preludedb_plugin_sql_set_build_time_constraint_string_func(plugin, sql_build_time_constraint_string);
        preludedb_plugin_sql_set_build_time_interval_string_func(plugin, sql_build_time_interval_string);
        preludedb_plugin_sql_set_build_limit_offset_string_func(plugin, sql_build_limit_offset_string);
        preludedb_plugin_sql_set_get_last_insert_ident_func(plugin, sql_get_last_insert_ident);

        return 0;
}



int mysql_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}
