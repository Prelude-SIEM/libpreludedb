/*****
 *
 * Copyright (C) 2005-2016 CS-SI. All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *****/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <assert.h>
#include <limits.h>
#include <regex.h>

#include <sqlite3.h>
#include <libprelude/prelude.h>

#include "preludedb.h"
#include "preludedb-plugin-sql.h"


#define SQLITE_BUSY_TIMEOUT INT_MAX


/*
 * Up to SQLite 3.2.2, there was no way to create an user defined
 * function for the REGEXP operator. In this situation, we use the
 * GLOB operator in order to define the regexp handler.
 */
#if SQLITE_VERSION_NUMBER >= 3002002
# define SQLITE_REGEX_OPERATOR "REGEXP"
# define SQLITE_REGEX_BIND_OPERATOR "regexp"
#else
# define SQLITE_REGEX_OPERATOR "GLOB"
# define SQLITE_REGEX_BIND_OPERATOR "glob"
#endif




int sqlite3_LTX_prelude_plugin_version(void);
int sqlite3_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data);



static void sqlite3_regexp(sqlite3_context *context, int argc, sqlite3_value **argv)
{
        int ret;
        regex_t regex;

        if ( argc != 2 ) {
                sqlite3_result_error(context, "Invalid argument count", -1);
                return;
        }

        ret = regcomp(&regex, (const char *) sqlite3_value_text(argv[0]), REG_EXTENDED | REG_NOSUB);
        if ( ret != 0 ) {
                sqlite3_result_error(context, "error compiling regular expression", -1);
                return;
        }

        ret = regexec(&regex, (const char *) sqlite3_value_text(argv[1]), 0, NULL, 0);
        regfree(&regex);

        sqlite3_result_int(context, (ret == REG_NOMATCH) ? 0 : 1 );
}



static int sql_open(preludedb_sql_settings_t *settings, void **session)
{
        int ret;
        const char *dbfile;

        dbfile = preludedb_sql_settings_get_file(settings);
        if ( ! dbfile || ! *dbfile )
                return preludedb_error_verbose(PRELUDEDB_ERROR_CONNECTION, "no database file specified");

        ret = access(dbfile, F_OK);
        if ( ret != 0 )
                return preludedb_error_verbose(PRELUDEDB_ERROR_CONNECTION, "database file '%s' does not exist", dbfile);

        ret = sqlite3_open(dbfile, (sqlite3 **) session);
        if ( ret != SQLITE_OK ) {
                ret = preludedb_error_verbose(PRELUDEDB_ERROR_CONNECTION, "%s", sqlite3_errmsg(*session));
                sqlite3_close(*session);
                return ret;
        }

        ret = sqlite3_create_function(*session, SQLITE_REGEX_BIND_OPERATOR, 2, SQLITE_ANY, NULL, sqlite3_regexp, NULL, NULL);
        if ( ret != SQLITE_OK ) {
                ret = preludedb_error_verbose(PRELUDEDB_ERROR_CONNECTION, "%s", sqlite3_errmsg(*session));
                sqlite3_close(*session);
                return ret;
        }

        sqlite3_busy_timeout(*session, SQLITE_BUSY_TIMEOUT);

        return 0;
}



static void sql_close(void *session)
{
        sqlite3_close(session);
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



static int sql_build_limit_offset_string(void *session, int limit, int offset, prelude_string_t *output)
{
        if ( limit >= 0 ) {
                if ( offset >= 0 )
                        return prelude_string_sprintf(output, " LIMIT %d, %d", offset, limit);

                return prelude_string_sprintf(output, " LIMIT %d", limit);
        }

        else if ( offset >= 0 )
                return prelude_string_sprintf(output, " LIMIT %d, -1", offset);

        return 0;
}



static int sql_table_field_copy(preludedb_sql_row_t *row, sqlite3_stmt *statement, unsigned int col)
{
        char *data = NULL;
        preludedb_sql_field_t *field;
        size_t len;

        len = sqlite3_column_bytes(statement, col);
        if ( len ) {
                if ( len + 1 < len )
                        return -1;

                data = malloc(len + 1);
                if ( ! data )
                        return preludedb_error_from_errno(errno);

                memcpy(data, sqlite3_column_blob(statement, col), len);
                data[len] = '\0';
        }

        return preludedb_sql_row_new_field(row, &field, col, data, len);
}


static void sql_field_destroy(void *session, preludedb_sql_table_t *table, preludedb_sql_row_t *row, preludedb_sql_field_t *field)
{
        free(preludedb_sql_field_get_value(field));
}


static void sql_table_destroy(void *session, preludedb_sql_table_t *table)
{
        sqlite3_finalize(preludedb_sql_table_get_data(table));
}


static int sql_query(void *session, const char *query, preludedb_sql_table_t **table)
{
        int ret;
        sqlite3_stmt *statement;
        const char *unparsed = NULL;

        /*
         * FIXME: we need a better way to know the kind of operation performed.
         */
        if ( strncasecmp(query, "SELECT", 6) != 0 ) {

                ret = sqlite3_exec(session, query, NULL, NULL, 0);
                if ( ret != SQLITE_OK )
                        return preludedb_error_verbose(PRELUDEDB_ERROR_QUERY, sqlite3_errmsg(session));

        } else {
                ret = sqlite3_prepare(session, query, strlen(query), &statement, &unparsed);
                if ( ret != SQLITE_OK )
                        return preludedb_error_verbose(PRELUDEDB_ERROR_QUERY, sqlite3_errmsg(session));

                if ( sqlite3_column_count(statement) == 0 )
                        return 0;

                ret = preludedb_sql_table_new(table, statement);
                if ( ret < 0 )
                        return ret;

                ret = 1;
        }

        return ret;
}



static const char *sql_get_column_name(void *session, preludedb_sql_table_t *table, unsigned int column_num)
{
        if ( column_num >= preludedb_sql_table_get_column_count(table) )
                return NULL;

        return sqlite3_column_name(preludedb_sql_table_get_data(table), column_num);
}


static int sql_get_column_num(void *session, preludedb_sql_table_t *table, const char *column_name)
{
        int ret;
        unsigned int i;

        for ( i = 0; i < preludedb_sql_table_get_column_count(table); i++ ) {
                ret = strcmp(column_name, sqlite3_column_name(preludedb_sql_table_get_data(table), i));
                if ( ret == 0 )
                        return i;
        }

        return prelude_error_verbose(PRELUDEDB_ERROR_GENERIC, "unknown column '%s'", column_name);
}



static unsigned int sql_get_column_count(void *session, preludedb_sql_table_t *table)
{
        return sqlite3_column_count(preludedb_sql_table_get_data(table));
}



static int sql_fetch_row(void *session, preludedb_sql_table_t *table, unsigned int row_index, preludedb_sql_row_t **row)
{
        int ret, i;
        sqlite3_stmt *statement = preludedb_sql_table_get_data(table);

        while ( preludedb_sql_table_get_fetched_row_count(table) <= row_index ) {
                ret = sqlite3_step(statement);
                if ( ret == SQLITE_ERROR || ret == SQLITE_MISUSE || ret == SQLITE_BUSY )
                        return preludedb_error_verbose(PRELUDEDB_ERROR_QUERY, "%s", sqlite3_errmsg(session));

                else if ( ret == SQLITE_DONE )
                        return 0;

                assert(ret == SQLITE_ROW);

                ret = preludedb_sql_table_new_row(table, row, preludedb_sql_table_get_fetched_row_count(table));
                if ( ret < 0 )
                        return ret;

                for ( i = 0; i < sqlite3_column_count(statement); i++ ) {
                        ret = sql_table_field_copy(*row, statement, i);
                        if ( ret < 0 )
                                return preludedb_error_from_errno(errno);
                }
        }

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

                { IDMEF_CRITERION_OPERATOR_REGEX,             SQLITE_REGEX_OPERATOR        },
                { IDMEF_CRITERION_OPERATOR_REGEX_NOCASE,      SQLITE_REGEX_OPERATOR        },
                { IDMEF_CRITERION_OPERATOR_NOT_REGEX,         "NOT " SQLITE_REGEX_OPERATOR },
                { IDMEF_CRITERION_OPERATOR_NOT_REGEX_NOCASE,  "NOT " SQLITE_REGEX_OPERATOR },

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

        if ( operator & IDMEF_CRITERION_OPERATOR_NOCASE )
                return prelude_string_sprintf(out, "lower(%s) %s lower(%s)", field, op_str, value);

        return prelude_string_sprintf(out, "%s %s %s", field, op_str, value);
}



static int sql_build_time_extract_string(prelude_string_t *output, const char *field,
                                        preludedb_sql_time_constraint_type_t type, int gmt_offset)
{
        char buf[128];
        int ret;

        if ( ! gmt_offset )
                ret = snprintf(buf, sizeof(buf), "%s", field);
        else
                ret = snprintf(buf, sizeof(buf), "DATETIME(%s, '%d hours')", field, gmt_offset / 3600);

        if ( ret < 0 || (size_t) ret >= sizeof(buf) )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        switch ( type ) {
                case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
                        return prelude_string_sprintf(output, "STRFTIME('%%Y', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_QUARTER:
                        return prelude_string_sprintf(output, "((STRFTIME('%%m', %s) + 2) / 3)", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
                        return  prelude_string_sprintf(output, "STRFTIME('%%m', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY:
                        return prelude_string_sprintf(output, "STRFTIME('%%j', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
                        return prelude_string_sprintf(output, "STRFTIME('%%d', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY:
                        return prelude_string_sprintf(output, "STRFTIME('%%w', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
                        return prelude_string_sprintf(output, "STRFTIME('%%H', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
                        return prelude_string_sprintf(output, "STRFTIME('%%M', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
                        return prelude_string_sprintf(output, "STRFTIME('%%S', %s) + 0", buf);

                case PRELUDEDB_SQL_TIME_CONSTRAINT_MSEC:
                        return prelude_string_sprintf(output, "(STRFTIME('%%f', %s) - STRFTIME('%%S', %s)) * 1000", buf, buf);

                default:
                        return preludedb_error(PRELUDEDB_ERROR_GENERIC);
        }
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
                value = value % 7;

        return prelude_string_sprintf(output, " %s %d", sql_operator, value);
}



static int sql_build_time_interval_string(prelude_string_t *output, const char *field, const char *value, preludedb_selected_object_interval_t unit)
{
        const char *sunit;

        switch (unit) {
        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_YEAR:
                sunit = "years";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_MONTH:
                sunit = "months";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_DAY:
                sunit = "days";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_HOUR:
                sunit = "hours";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_MIN:
                sunit = "minutes";
                break;

        case PRELUDEDB_SELECTED_OBJECT_INTERVAL_SEC:
                sunit = "seconds";
                break;

        default:
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);
        }

        return prelude_string_sprintf(output, "datetime(%s, %s || ' %s')", field, value, sunit);
}



static long sql_get_server_version(void *session)
{
        return SQLITE_VERSION_NUMBER;
}



int sqlite3_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data)
{
        int ret;
        preludedb_plugin_sql_t *plugin;

        ret = preludedb_plugin_sql_new(&plugin);
        if ( ret < 0 )
                return ret;

        prelude_plugin_set_name((prelude_plugin_generic_t *) plugin, "sqlite3");
        prelude_plugin_entry_set_plugin(pe, (void *) plugin);

        preludedb_plugin_sql_set_open_func(plugin, sql_open);
        preludedb_plugin_sql_set_close_func(plugin, sql_close);
        preludedb_plugin_sql_set_escape_func(plugin, sql_escape);
        preludedb_plugin_sql_set_query_func(plugin, sql_query);
        preludedb_plugin_sql_set_get_server_version_func(plugin, sql_get_server_version);
        preludedb_plugin_sql_set_field_destroy_func(plugin, sql_field_destroy);
        preludedb_plugin_sql_set_table_destroy_func(plugin, sql_table_destroy);
        preludedb_plugin_sql_set_get_column_count_func(plugin, sql_get_column_count);
        preludedb_plugin_sql_set_get_column_name_func(plugin, sql_get_column_name);
        preludedb_plugin_sql_set_get_column_num_func(plugin, sql_get_column_num);
        preludedb_plugin_sql_set_get_operator_string_func(plugin, get_operator_string);
        preludedb_plugin_sql_set_fetch_row_func(plugin, sql_fetch_row);
        preludedb_plugin_sql_set_build_constraint_string_func(plugin, sql_build_constraint_string);
        preludedb_plugin_sql_set_build_time_extract_string_func(plugin, sql_build_time_extract_string);
        preludedb_plugin_sql_set_build_time_constraint_string_func(plugin, sql_build_time_constraint_string);
        preludedb_plugin_sql_set_build_time_interval_string_func(plugin, sql_build_time_interval_string);
        preludedb_plugin_sql_set_build_limit_offset_string_func(plugin, sql_build_limit_offset_string);

        return 0;
}



int sqlite3_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}


