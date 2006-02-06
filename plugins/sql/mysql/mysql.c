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

#include "config.h"
#include "libmissing.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>

#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>

#include <libprelude/idmef.h>
#include <libprelude/prelude-error.h>
#include <libprelude/prelude-string.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"
#include "preludedb-plugin-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"


int mysql_LTX_prelude_plugin_version(void);
int mysql_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data);


#if ! defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID < 32224
 #define mysql_field_count mysql_num_fields
#endif /* ! MYSQL_VERSION_ID */


static int sql_open(preludedb_sql_settings_t *settings, void **session, char *errbuf, size_t size)
{
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
                if ( mysql_error(*session) )
                        snprintf(errbuf, size, "%s", mysql_error(*session));

                mysql_close(*session);
                return preludedb_error(PRELUDEDB_ERROR_CONNECTION);
        }

        return 0;
}



static void sql_close(void *session)
{
        mysql_close((MYSQL *) session);
}



static const char *sql_get_error(void *session)
{
        return mysql_error(session);
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
        rsize = mysql_escape_string((*output) + 1, input, input_size);
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

        return 0;
}



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



static int sql_query(void *session, const char *query, void **resource)
{
        if ( mysql_query(session, query) != 0 ) {

                if ( is_connection_broken(session) ) 
                        return preludedb_error(PRELUDEDB_ERROR_CONNECTION);
                
                return preludedb_error(PRELUDEDB_ERROR_QUERY);
        }
        
        *resource = mysql_store_result(session);
        if ( *resource ) {
                if ( mysql_num_rows(*resource) == 0 ) {
                        mysql_free_result(*resource);
                        return 0;
                }

                return 1;
        }

        return mysql_errno(session) ? preludedb_error(PRELUDEDB_ERROR_QUERY) : 0;
}



static void sql_resource_destroy(void *session, void *resource)
{
        if ( resource ) 
                mysql_free_result(resource);
}



static MYSQL_FIELD *get_field(MYSQL_RES *res, unsigned int column_num)
{
        return (column_num >= mysql_num_fields(res)) ? NULL : mysql_fetch_field_direct(res, column_num);
}



static const char *sql_get_column_name(void *session, void *resource, unsigned int column_num)
{
        MYSQL_FIELD *field;

        field = get_field(resource, column_num);

        return field ? field->name : NULL;
}

 

static int sql_get_column_num(void *session, void *resource, const char *column_name)
{
        MYSQL_FIELD *fields;
        int fields_num;
        int i;

        fields = mysql_fetch_fields(resource);
        if ( ! fields )
                return -1;

        fields_num = mysql_num_fields(resource);

        for ( i = 0; i < fields_num; i++ ) {
                if ( strcmp(column_name, fields[i].name) == 0 )
                        return i;
        }

        return -1;
}



static unsigned int sql_get_column_count(void *session, void *resource)
{
        return mysql_num_fields(resource);
}



static unsigned int sql_get_row_count(void *session, void *resource)
{
        return (unsigned int) mysql_num_rows(resource);
}



static int sql_fetch_row(void *session, void *resource, void **row)
{
        *row = mysql_fetch_row(resource);
        if ( ! *row )
                return mysql_errno(session) ? preludedb_error(PRELUDEDB_ERROR_GENERIC) : 0;

        return 1;
}



static int sql_fetch_field(void *session, void *resource, void *row,
                           unsigned int column_num, const char **value, size_t *len)
{
        unsigned long *lengths;
        
        if ( column_num >= mysql_num_fields(resource) )
                return preludedb_error(PRELUDEDB_ERROR_INVALID_COLUMN_NUM);

        lengths = mysql_fetch_lengths(resource);
        if ( ! lengths )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        if ( ! ((MYSQL_ROW) row)[column_num] )
                return 0;

        *value = ((MYSQL_ROW) row)[column_num];
        *len = lengths[column_num];

        return 1;
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



static int sql_build_time_constraint_string(prelude_string_t *output, const char *field,
                                            preludedb_sql_time_constraint_type_t type,
                                            idmef_criterion_operator_t operator, int value, int gmt_offset)
{
        char buf[128];
        const char *sql_operator;
        int ret;
 
        ret = snprintf(buf, sizeof(buf), "DATE_ADD(%s, INTERVAL %d HOUR)", field, gmt_offset / 3600);
        if ( ret < 0 || ret >= sizeof(buf) )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        sql_operator = get_operator_string(operator);
        if ( ! sql_operator )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);
        
        switch ( type ) {
        case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
                return prelude_string_sprintf(output, "EXTRACT(YEAR FROM %s) %s '%d'",
                                              buf, sql_operator, value);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
                return  prelude_string_sprintf(output, "EXTRACT(MONTH FROM %s) %s '%d'",
                                               buf, sql_operator, value);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY:
                return prelude_string_sprintf(output, "DAYOFYEAR(%s) %s '%d'",
                                              buf, sql_operator, value);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
                return prelude_string_sprintf(output, "DAYOFMONTH(%s) %s '%d'",
                                              buf, sql_operator, value);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY:
                return prelude_string_sprintf(output, "DAYOFWEEK(%s) %s '%d'",
                                              buf, sql_operator, value % 7 + 1);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
                return prelude_string_sprintf(output, "EXTRACT(HOUR FROM %s) %s '%d'",
                                              buf, sql_operator, value);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
                return prelude_string_sprintf(output, "EXTRACT(MINUTE FROM %s) %s '%d'",
                                              buf, sql_operator, value);

        case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
                return prelude_string_sprintf(output, "EXTRACT(SECOND FROM %s) %s '%d'",
                                              buf, sql_operator, value);
        }

        /* not reached */

        return preludedb_error(PRELUDEDB_ERROR_GENERIC);
}



static int sql_build_time_interval_string(preludedb_sql_time_constraint_type_t type, int value,
                                          char *buf, size_t size)
{
        char *type_str;
        int ret;

        switch ( type ) {
        case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
                type_str = "YEAR";
                break;

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
                type_str = "MONTH";
                break;

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
                type_str = "DAY";
                break;

        case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
                type_str = "HOUR";
                break;

        case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
                type_str = "MINUTE";
                break;

        case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
                type_str = "SECOND";
                break;

        default:
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);
        }

        ret = snprintf(buf, size, "INTERVAL %d %s", value, type_str);

        return (ret < 0 || ret >= size) ? preludedb_error(PRELUDEDB_ERROR_GENERIC) : 0;
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
        preludedb_plugin_sql_set_get_error_func(plugin, sql_get_error);
        preludedb_plugin_sql_set_escape_binary_func(plugin, sql_escape_binary);
        preludedb_plugin_sql_set_query_func(plugin, sql_query);
        preludedb_plugin_sql_set_resource_destroy_func(plugin, sql_resource_destroy);
        preludedb_plugin_sql_set_get_column_count_func(plugin, sql_get_column_count);
        preludedb_plugin_sql_set_get_row_count_func(plugin, sql_get_row_count);
        preludedb_plugin_sql_set_get_column_name_func(plugin, sql_get_column_name);
        preludedb_plugin_sql_set_get_column_num_func(plugin, sql_get_column_num);
        preludedb_plugin_sql_set_get_operator_string_func(plugin, get_operator_string);
        preludedb_plugin_sql_set_fetch_row_func(plugin, sql_fetch_row);
        preludedb_plugin_sql_set_fetch_field_func(plugin, sql_fetch_field);
        preludedb_plugin_sql_set_build_constraint_string_func(plugin, sql_build_constraint_string);
        preludedb_plugin_sql_set_build_time_constraint_string_func(plugin, sql_build_time_constraint_string);
        preludedb_plugin_sql_set_build_time_interval_string_func(plugin, sql_build_time_interval_string);
        preludedb_plugin_sql_set_build_limit_offset_string_func(plugin, sql_build_limit_offset_string);
        
        return 0;
}



int mysql_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}
