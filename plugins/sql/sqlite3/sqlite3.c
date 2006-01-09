/*****
 *
 * Copyright (C) 2005 PreludeIDS Technologies. All Rights Reserved.
 * Author: Rob Holland <rob@inversepath.com>
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
#include <sys/types.h>
#include <assert.h>
#include <limits.h>

#include <sqlite3.h>
#include <libprelude/prelude.h>

#include "preludedb.h"
#include "preludedb-plugin-sql.h"


#define SQLITE_BUSY_TIMEOUT INT_MAX 


typedef struct {
        size_t len;
        void *data;
} sqlite3_field_t;


typedef struct {
        prelude_list_t list;
        sqlite3_field_t *fields;
} sqlite3_row_t;


typedef struct {
        prelude_list_t rows;
        sqlite3_stmt *statement;
        unsigned int nrow;
        unsigned int ncolumn;
        sqlite3_row_t *current_row;
} sqlite3_resource_t;



int sqlite3_LTX_prelude_plugin_version(void);
int sqlite3_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data);



static int sql_open(preludedb_sql_settings_t *settings, void **session, char *errbuf, size_t size)
{
        int ret;
        const char *dbfile;

        dbfile = preludedb_sql_settings_get_file(settings);
        if ( ! dbfile )
                return preludedb_error(PRELUDEDB_ERROR_CONNECTION);
        
        ret = sqlite3_open(dbfile, (sqlite3 **) session);
        if ( ret != SQLITE_OK ) {
                snprintf(errbuf, size, "%s", sqlite3_errmsg(*session));
                sqlite3_close(*session);
                return preludedb_error(PRELUDEDB_ERROR_CONNECTION);
        }

        sqlite3_busy_timeout(*session, SQLITE_BUSY_TIMEOUT);
        
        return 0;
}



static void sql_close(void *session)
{
        sqlite3_close(session);
}



static const char *sql_get_error(void *session)
{
        /*
         * In case the last SQLite API call was successful, sqlite3_errmsg() 
         * will return "not an error". We need to return NULL in this specific case.
         */
        return (sqlite3_errcode(session) != SQLITE_OK) ? sqlite3_errmsg(session) : NULL;
}


static int sql_escape(void *session, const char *input, size_t input_size, char **output)
{
        char *buffer, *copy;

        buffer = sqlite3_mprintf("'%q'", input); 
        if ( ! buffer )
                return preludedb_error_from_errno(errno);

        copy = strdup(buffer);
        if ( ! copy ) {
                sqlite3_free(buffer);
                return preludedb_error_from_errno(errno);
        }

        sqlite3_free(buffer);

        *output = copy;

        return 0;
}



static void bin2hex(const unsigned char *in, size_t inlen, char *out)
{
        static const char hex[] = "0123456789ABCDEF";

        while ( inlen-- ) {
                *(out++) = hex[*in >> 4];
                *(out++) = hex[*in++ & 0x0f];
        }
}



static int sql_escape_binary(void *session, const unsigned char *input, size_t input_size, char **output)
{
        size_t outsize;

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



static int sql_build_limit_offset_string(void *session, int limit, int offset, prelude_string_t *output)
{
        if ( limit >= 0 ) {
                if ( offset >= 0 )
                        return prelude_string_sprintf(output, " LIMIT %d, %d", offset, limit);

                return prelude_string_sprintf(output, " LIMIT %d", limit);
        }

        return 0;
}



static sqlite3_row_t *sql_resource_add_row(sqlite3_resource_t *resource, unsigned int cols)
{
        sqlite3_row_t *row;

        row = malloc(sizeof(*row));
        if ( ! row )
                return NULL;

        row->fields = malloc(sizeof(*row->fields) * cols);
        if ( ! row->fields ) {
                free(row);
                return NULL;
        }
        
        resource->nrow++;
        prelude_list_add_tail(&resource->rows, &row->list);
        
        return row;
}



static int sql_resource_field_copy(sqlite3_field_t *field, sqlite3_stmt *statement, unsigned int col)
{
        field->len = sqlite3_column_bytes(statement, col);
        if ( ! field->len ) {
                field->data = NULL;
                return 0;
        }

        if ( field->len + 1 < field->len )
                return -1;
        
        field->data = malloc(field->len + 1);
        if ( ! field->data )
                return preludedb_error_from_errno(errno);

        memcpy(field->data, sqlite3_column_blob(statement, col), field->len);
        ((unsigned char *) field->data)[field->len] = '\0';
        
        return 0;
}


static void sql_resource_destroy(void *session, void *res)
{
        unsigned int i;
        sqlite3_row_t *row;
        sqlite3_field_t *field;
        sqlite3_resource_t *resource = res;
        prelude_list_t *cursor, *safety_cursor;

        if ( ! resource )
                return;
        
        prelude_list_for_each_safe(&resource->rows, cursor, safety_cursor) { 
                row = prelude_list_entry(cursor, sqlite3_row_t, list);
                               
                for ( i = 0; i < resource->ncolumn; i++ ) {
                        field = &row->fields[i];
                        free(field->data);
                }
                
                free(row->fields);
                                
                prelude_list_del(&row->list);
                free(row);
        }
        
        sqlite3_finalize(resource->statement);
        free(resource);
}



static int sql_read_row(sqlite3_stmt *statement, sqlite3_resource_t **resource)
{
        int ret;
        unsigned int i;
        sqlite3_row_t *row;
        unsigned int ncolumn;
        
        ncolumn = sqlite3_column_count(statement);
        if ( ncolumn == 0 )                
                return 0;
        
        *resource = calloc(1, sizeof(**resource));
        if ( ! *resource )
                return preludedb_error_from_errno(errno);
        
        prelude_list_init(&(*resource)->rows);
        
        while ( (ret = sqlite3_step(statement)) ) {
                
                if ( ret == SQLITE_ERROR || ret == SQLITE_MISUSE || ret == SQLITE_BUSY ) {
                        sql_resource_destroy(NULL, *resource);
                        return preludedb_error(PRELUDEDB_ERROR_QUERY);
                }
                
                else if ( ret == SQLITE_DONE )
                        break;

                assert(ret == SQLITE_ROW);
                
                row = sql_resource_add_row(*resource, ncolumn);
                if ( ! row ) {
                        sql_resource_destroy(NULL, *resource);
                        return preludedb_error_from_errno(errno);
                }
                
                for ( i = 0; i < ncolumn; i++ ) {
                        ret = sql_resource_field_copy(&row->fields[i], statement, i);
                        if ( ret < 0 ) {
                                sql_resource_destroy(NULL, *resource);
                                return preludedb_error_from_errno(errno);
                        }
                }
        }
        
        (*resource)->ncolumn = ncolumn;
        (*resource)->statement = statement;
        
        return 1;
}



static int sql_query(void *session, const char *query, void **resource)
{
        int ret;
        sqlite3_stmt *statement;
        const char *unparsed = NULL;

        /*
         * FIXME: we need a better way to know the kind of operation performed.
         */
        if ( strncmp(query, "SELECT", 6) != 0 ) {
                
                ret = sqlite3_exec(session, query, NULL, NULL, 0);
                if ( ret != SQLITE_OK )
                        return preludedb_error(PRELUDEDB_ERROR_QUERY);

        } else {
                ret = sqlite3_prepare(session, query, strlen(query), &statement, &unparsed);
                if ( ret != SQLITE_OK )
                        return preludedb_error(PRELUDEDB_ERROR_QUERY);

                ret = sql_read_row(statement, (sqlite3_resource_t **) resource);        
                if ( ret != 1 ) {
                        sqlite3_finalize(statement);
                        return ret;
                }
        }

        return ret;
}



static const char *sql_get_column_name(void *session, void *resource, unsigned int column_num)
{
        sqlite3_resource_t *res = resource;
        
        if ( column_num >= res->ncolumn )
                return NULL;

        return sqlite3_column_name(res->statement, column_num);
}



static int sql_get_column_num(void *session, void *resource, const char *column_name)
{
        int ret, i;
        sqlite3_resource_t *res = resource;
        
        for ( i = 0; i < res->ncolumn; i++ ) {

                ret = strcmp(column_name, sqlite3_column_name(res->statement, i));
                if ( ret == 0 )
                        return i;
        }

        return -1;
}



static unsigned int sql_get_column_count(void *session, void *resource)
{
        return ((sqlite3_resource_t *) resource)->ncolumn;
}



static unsigned int sql_get_row_count(void *session, void *resource)
{
        return ((sqlite3_resource_t *) resource)->nrow;
}



static int sql_fetch_row(void *session, void *resource, void **row)
{
        sqlite3_resource_t *res = resource;

        *row = prelude_list_get_next(&res->rows, res->current_row, sqlite3_row_t, list);
        if ( ! *row ) {
                res->current_row = NULL;
                return 0;
        }

        res->current_row = *row;

        return 1;
}



static int sql_fetch_field(void *session, void *resource, void *row,
                           unsigned int column_num, const char **value, size_t *len)
{
        sqlite3_field_t *field;
        
        if ( column_num >= ((sqlite3_resource_t *) resource)->ncolumn )
                return preludedb_error(PRELUDEDB_ERROR_INVALID_COLUMN_NUM);

        field = &(((sqlite3_row_t *) row)->fields[column_num]);
                
        *value = field->data;
        *len = field->len;
        
        if ( *len == 0 )
                return 0;

        return 1;
}



static const char *get_operator_string(idmef_criterion_operator_t operator)
{
        int i;
        struct tbl {
                idmef_criterion_operator_t operator;
                const char *name;
        } tbl[] = {

                { IDMEF_CRITERION_OPERATOR_EQUAL,             "="                 },
                { IDMEF_CRITERION_OPERATOR_EQUAL_NOCASE,      "="                 },
                { IDMEF_CRITERION_OPERATOR_NOT_EQUAL,         "!="                },
                { IDMEF_CRITERION_OPERATOR_NOT_EQUAL_NOCASE,  "!="                },

                { IDMEF_CRITERION_OPERATOR_GREATER,           ">"                 },
                { IDMEF_CRITERION_OPERATOR_GREATER_OR_EQUAL,  ">="                },
                { IDMEF_CRITERION_OPERATOR_LESSER,            "<"                 },
                { IDMEF_CRITERION_OPERATOR_LESSER_OR_EQUAL,   "<="                },

                { IDMEF_CRITERION_OPERATOR_SUBSTR,            "LIKE"              },
                { IDMEF_CRITERION_OPERATOR_SUBSTR_NOCASE,     "LIKE"              },
                { IDMEF_CRITERION_OPERATOR_NOT_SUBSTR,        "NOT LIKE"          },
                { IDMEF_CRITERION_OPERATOR_NOT_SUBSTR_NOCASE, "NOT LIKE "         },

                /* { IDMEF_CRITERION_OPERATOR_REGEX,             "REGEXP BINARY"     }, 
                 * { IDMEF_CRITERION_OPERATOR_REGEX_NOCASE,      "REGEXP"            }, 
                 * { IDMEF_CRITERION_OPERATOR_NOT_REGEX,         "NOT REGEXP"        }, 
                 * { IDMEF_CRITERION_OPERATOR_NOT_REGEX_NOCASE,  "NOT REGEXP BINARY" }, */

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



int sqlite3_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data)
{
        static preludedb_plugin_sql_t sql_plugin;

        memset(&sql_plugin, 0, sizeof(sql_plugin));
        prelude_plugin_set_name(&sql_plugin, "sqlite3");
        prelude_plugin_entry_set_plugin(pe, (void *) &sql_plugin);

        preludedb_plugin_sql_set_open_func(&sql_plugin, sql_open);
        preludedb_plugin_sql_set_close_func(&sql_plugin, sql_close);
        preludedb_plugin_sql_set_get_error_func(&sql_plugin, sql_get_error);
        preludedb_plugin_sql_set_escape_func(&sql_plugin, sql_escape);
        preludedb_plugin_sql_set_escape_binary_func(&sql_plugin, sql_escape_binary);
        preludedb_plugin_sql_set_query_func(&sql_plugin, sql_query);
        preludedb_plugin_sql_set_resource_destroy_func(&sql_plugin, sql_resource_destroy);
        preludedb_plugin_sql_set_get_column_count_func(&sql_plugin, sql_get_column_count);
        preludedb_plugin_sql_set_get_row_count_func(&sql_plugin, sql_get_row_count);
        preludedb_plugin_sql_set_get_column_name_func(&sql_plugin, sql_get_column_name);
        preludedb_plugin_sql_set_get_column_num_func(&sql_plugin, sql_get_column_num);
        preludedb_plugin_sql_set_fetch_row_func(&sql_plugin, sql_fetch_row);
        preludedb_plugin_sql_set_fetch_field_func(&sql_plugin, sql_fetch_field);
        preludedb_plugin_sql_set_build_constraint_string_func(&sql_plugin, sql_build_constraint_string);
        preludedb_plugin_sql_set_build_time_constraint_string_func(&sql_plugin, sql_build_time_constraint_string);
        preludedb_plugin_sql_set_build_time_interval_string_func(&sql_plugin, sql_build_time_interval_string);
        preludedb_plugin_sql_set_build_limit_offset_string_func(&sql_plugin, sql_build_limit_offset_string);

        return 0;
}



int sqlite3_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}

		
