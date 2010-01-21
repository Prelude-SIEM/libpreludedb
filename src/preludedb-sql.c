/*****
*
* Copyright (C) 2003-2005,2006,2007 PreludeIDS Technologies. All Rights Reserved.
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <libprelude/prelude-list.h>
#include <libprelude/prelude-linked-object.h>
#include <libprelude/common.h>
#include <libprelude/prelude-log.h>
#include <libprelude/prelude-error.h>
#include <libprelude/idmef.h>

#include "glthread/lock.h"
#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-plugin-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"


typedef enum {
        PRELUDEDB_SQL_STATUS_CONNECTED    = 0x01,
        PRELUDEDB_SQL_STATUS_TRANSACTION  = 0x02
} preludedb_sql_status_t;



#define assert_connected(sql)                                             \
        if ( ! (sql->status & PRELUDEDB_SQL_STATUS_CONNECTED) ) {         \
                int __ret;                                                \
                                                                          \
                __ret = preludedb_sql_connect(sql);                       \
                if ( __ret < 0 ) {                                        \
                        gl_recursive_lock_unlock(sql->mutex);             \
                        return __ret;                                     \
                }                                                         \
        }


struct preludedb_sql {
        char *type;
        preludedb_sql_settings_t *settings;
        preludedb_plugin_sql_t *plugin;
        preludedb_sql_status_t status;
        void *session;
        FILE *logfile;
        prelude_bool_t internal_transaction_disabled;

        gl_recursive_lock_t mutex;
};

struct preludedb_sql_table {
        preludedb_sql_t *sql;
        void *res;
        prelude_list_t row_list;
};

struct preludedb_sql_row {
        prelude_list_t list;
        preludedb_sql_table_t *table;
        void *res;
        prelude_list_t field_list;
};

struct preludedb_sql_field {
        prelude_list_t list;
        preludedb_sql_row_t *row;
        int num;
        const char *value;
        size_t len;
};


int _preludedb_sql_transaction_start(preludedb_sql_t *sql);
int _preludedb_sql_transaction_end(preludedb_sql_t *sql);
int _preludedb_sql_transaction_abort(preludedb_sql_t *sql);
void _preludedb_sql_enable_internal_transaction(preludedb_sql_t *sql);
void _preludedb_sql_disable_internal_transaction(preludedb_sql_t *sql);


extern prelude_list_t _sql_plugin_list;




static inline void update_sql_from_errno(preludedb_sql_t *sql, preludedb_error_t error)
{
        if ( preludedb_error_check(error, PRELUDEDB_ERROR_CONNECTION) ) {
                _preludedb_plugin_sql_close(sql->plugin, sql->session);
                sql->status &= ~PRELUDEDB_SQL_STATUS_CONNECTED;
        }
}



/**
 * preludedb_sql_new:
 * @new: Pointer to a sql object to initialize.
 * @type: Type of the database.
 * @settings: Settings for the choosen database.
 *
 * This function initialize the @new object, load and setup the plugin that
 * handle the database named @type with the configuration stored in @settings.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_new(preludedb_sql_t **new, const char *type, preludedb_sql_settings_t *settings)
{
        *new = calloc(1, sizeof(**new));
        if ( ! *new )
                return preludedb_error_from_errno(errno);

        gl_recursive_lock_init(((*new)->mutex));

        if ( ! type ) {
                type = preludedb_sql_settings_get_type(settings);
                if ( ! type )
                        return preludedb_error_verbose(PRELUDEDB_ERROR_INVALID_SETTINGS_STRING, "No database type specified");
        }

        (*new)->type = strdup(type);
        if ( ! (*new)->type ) {
                free(*new);
                return preludedb_error_from_errno(errno);
        }

        (*new)->settings = settings;

        (*new)->plugin = (preludedb_plugin_sql_t *) prelude_plugin_search_by_name(&_sql_plugin_list, type);
        if ( ! (*new)->plugin ) {
                free((*new)->type);
                free(*new);
                return preludedb_error_verbose(PRELUDEDB_ERROR_CANNOT_LOAD_SQL_PLUGIN, "Could not load sql plugin '%s'", type);
        }

        if ( preludedb_sql_settings_get_log(settings) )
                preludedb_sql_enable_query_logging(*new, preludedb_sql_settings_get_log(settings));

        return 0;
}



/**
 * preludedb_sql_destroy:
 * @sql: Pointer to a sql object.
 *
 * Destroy @sql and the underlying plugin.
 */
void preludedb_sql_destroy(preludedb_sql_t *sql)
{
        if ( sql->status & PRELUDEDB_SQL_STATUS_CONNECTED )
                _preludedb_plugin_sql_close(sql->plugin, sql->session);

        if ( sql->logfile )
                fclose(sql->logfile);

        gl_recursive_lock_destroy(sql->mutex);
        preludedb_sql_settings_destroy(sql->settings);

        free(sql->type);
        free(sql);
}



/**
 * preludedb_sql_enable_query_logging:
 * @sql: Pointer to a sql object.
 * @filename: Where the logs will be written.
 *
 * Log all queries in the specified file.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_enable_query_logging(preludedb_sql_t *sql, const char *filename)
{
        int fd, ret;

        if ( ! filename ) {
                sql->logfile = stdout;
                return 0;
        }

        sql->logfile = fopen(filename, "a");
        if ( ! sql->logfile )
                return preludedb_error_verbose(prelude_error_code_from_errno(errno),
                                               "Could not open '%s' for writing: %s", filename, strerror(errno));

        fd = fileno(sql->logfile);

        ret = fcntl(fd, F_GETFD);
        if ( ret < 0 )
                return 0;

        fcntl(fd, F_SETFD, ret | FD_CLOEXEC);

        return 0;
}



/**
 * preludedb_sql_disable_query_logging:
 * @sql: Pointer to a sql object.
 *
 * Disable query logging.
 */
void preludedb_sql_disable_query_logging(preludedb_sql_t *sql)
{
        if ( sql->logfile && sql->logfile != stdout )
                fclose(sql->logfile);

        sql->logfile = NULL;
}



static int preludedb_sql_connect(preludedb_sql_t *sql)
{
        int ret;

        ret = _preludedb_plugin_sql_open(sql->plugin, sql->settings, &sql->session);
        if ( ret < 0 )
                return ret;

        sql->status = PRELUDEDB_SQL_STATUS_CONNECTED;

        return 0;
}



static int preludedb_sql_table_new(preludedb_sql_table_t **new, preludedb_sql_t *sql, void *res)
{
        *new = malloc(sizeof(**new));
        if ( ! *new )
                return preludedb_error_from_errno(errno);

        (*new)->sql = sql;
        (*new)->res = res;
        prelude_list_init(&(*new)->row_list);

        return 0;
}



/**
 * preludedb_sql_query:
 * @sql: Pointer to a sql object.
 * @query: The SQL query to execute.
 * @table: Pointer to a table where the query result will be stored if the type of query return
 * results (i.e a SELECT can results, but an INSERT never results) and if the query is sucessfull.
 *
 * Execute a SQL query.
 *
 * Returns: the number of result or a negative value if an error occured.
 */
int preludedb_sql_query(preludedb_sql_t *sql, const char *query, preludedb_sql_table_t **table)
{
        int ret;
        void *res;
        struct timeval start, end;

        gl_recursive_lock_lock(sql->mutex);
        assert_connected(sql);

        gettimeofday(&start, NULL);

        ret = _preludedb_plugin_sql_query(sql->plugin, sql->session, query, &res);

        if ( ret < 0 )
                update_sql_from_errno(sql, ret);

        gettimeofday(&end, NULL);
        gl_recursive_lock_unlock(sql->mutex);

        if ( sql->logfile ) {
                fprintf(sql->logfile, "%fs %s\n",
                        (end.tv_sec + (double) end.tv_usec / 1000000) -
                        (start.tv_sec + (double) start.tv_usec / 1000000), query);

                fflush(sql->logfile);
        }

        if ( ret <= 0 )
                return ret;

        ret = preludedb_sql_table_new(table, sql, res);
        if ( ret < 0 ) {
                _preludedb_plugin_sql_resource_destroy(sql->plugin, sql->session, res);
                return ret;
        }

        return preludedb_sql_table_get_row_count(*table);
}



/**
 * preludedb_sql_query_sprintf:
 * @sql: Pointer to a sql object.
 * @table: Pointer to a table where the query result will be stored if the type of query return
 * results (i.e a SELECT can results, but an INSERT never results) and if the query is sucessfull.
 * @format: The SQL query to execute in a printf format string.
 * @...: Arguments referenced in @format.
 *
 * Execute a SQL query.
 *
 * Returns: 1 if the query returns results, 0 if it does not, or negative value if an error occur.
 */
int preludedb_sql_query_sprintf(preludedb_sql_t *sql, preludedb_sql_table_t **table,
                                const char *format, ...)
{
        int ret;
        va_list ap;
        prelude_string_t *query;

        ret = prelude_string_new(&query);
        if ( ret < 0 )
                return ret;

        va_start(ap, format);
        ret = prelude_string_vprintf(query, format, ap);
        va_end(ap);

        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_query(sql, prelude_string_get_string(query), table);

 error:
        prelude_string_destroy(query);

        return ret;
}



/**
 * preludedb_sql_insert:
 * @sql: Pointer to a sql object.
 * @table: the name of the table where to insert values.
 * @fields: a list of comma separated field names where the values will be inserted.
 * @format: The values to insert in a printf format string.
 * @...: Argument referenced throught @format.
 *
 * Insert values in a table.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_insert(preludedb_sql_t *sql, const char *table, const char *fields,
                         const char *format, ...)
{
        int ret;
        va_list ap;
        prelude_string_t *query;

        ret = prelude_string_new(&query);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_sprintf(query, "INSERT INTO %s (%s) VALUES(", table, fields);
        if ( ret < 0 )
                goto error;

        va_start(ap, format);
        ret = prelude_string_vprintf(query, format, ap);
        va_end(ap);
        if ( ret < 0 )
                goto error;

        ret = prelude_string_cat(query, ")");
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_query(sql, prelude_string_get_string(query), NULL);

 error:
        prelude_string_destroy(query);

        return ret;
}



/**
 * preludedb_sql_build_limit_offset_string:
 * @sql: Pointer to a sql object.
 * @limit: The limit value, a value inferior to zero will disable the limit.
 * @offset: The offset value, a value inferior to zero will disable the offset.
 * @output: Where the limit/offset built string will be stored.
 *
 * Build a limit/offset string for a SQL query, depending on the underlying type of database.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_build_limit_offset_string(preludedb_sql_t *sql, int limit, int offset, prelude_string_t *output)
{
        return _preludedb_plugin_sql_build_limit_offset_string(sql->plugin, sql->session, limit, offset, output);
}



int _preludedb_sql_transaction_start(preludedb_sql_t *sql)
{
        int ret;

        gl_recursive_lock_lock(sql->mutex);

        if ( sql->status & PRELUDEDB_SQL_STATUS_TRANSACTION ) {
                gl_recursive_lock_unlock(sql->mutex);
                return preludedb_error(PRELUDEDB_ERROR_ALREADY_IN_TRANSACTION);
        }

        ret = preludedb_sql_query(sql, "BEGIN", NULL);
        if ( ret < 0 )
                gl_recursive_lock_unlock(sql->mutex);
        else
                sql->status |= PRELUDEDB_SQL_STATUS_TRANSACTION;

        return ret;
}



/**
 * preludedb_sql_transaction_start:
 * @sql: Pointer to a sql object.
 *
 * Begin a sql transaction.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_transaction_start(preludedb_sql_t *sql)
{
        if ( sql->internal_transaction_disabled )
                return 0;

        return _preludedb_sql_transaction_start(sql);
}



int _preludedb_sql_transaction_end(preludedb_sql_t *sql)
{
        int ret;

        if ( ! (sql->status & PRELUDEDB_SQL_STATUS_TRANSACTION) )
                return preludedb_error(PRELUDEDB_ERROR_NOT_IN_TRANSACTION);

        ret = preludedb_sql_query(sql, "COMMIT", NULL);
        sql->status &= ~PRELUDEDB_SQL_STATUS_TRANSACTION;

        gl_recursive_lock_unlock(sql->mutex);

        return ret;
}




/**
 * preludedb_sql_transaction_end:
 * @sql: Pointer to a sql object.
 *
 * Finish a sql transaction (SQL COMMIT command).
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_transaction_end(preludedb_sql_t *sql)
{
        if ( sql->internal_transaction_disabled )
                return 0;

        return _preludedb_sql_transaction_end(sql);
}



int _preludedb_sql_transaction_abort(preludedb_sql_t *sql)
{
        int ret;
        char *original_error = NULL;

        if ( ! (sql->status & PRELUDEDB_SQL_STATUS_TRANSACTION) )
                return preludedb_error(PRELUDEDB_ERROR_NOT_IN_TRANSACTION);

        if ( _prelude_thread_get_error() )
                original_error = strdup(_prelude_thread_get_error());

        sql->status &= ~PRELUDEDB_SQL_STATUS_TRANSACTION;

        if ( original_error && ! (sql->status & PRELUDEDB_SQL_STATUS_CONNECTED) ) {
                ret = preludedb_error_verbose(PRELUDEDB_ERROR_QUERY, "%s. No ROLLBACK possible due to connection closure",
                                              original_error);
                goto error;
        }

        ret = preludedb_sql_query(sql, "ROLLBACK", NULL);
        if ( ret < 0 ) {
                if ( original_error )
                        ret = preludedb_error_verbose(PRELUDEDB_ERROR_QUERY, "%s.\nROLLBACK failed: %s", original_error, preludedb_strerror(ret));
                else
                        ret = preludedb_error_verbose(PRELUDEDB_ERROR_QUERY, "ROLLBACK failed: %s", preludedb_strerror(ret));
        }

    error:
        if ( original_error )
                free(original_error);

        gl_recursive_lock_unlock(sql->mutex);

        return ret;
}




/**
 * preludedb_sql_transaction_abort:
 * @sql: Pointer to a sql object.
 *
 * Abort a sql transaction (SQL ROLLBACK command).
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_transaction_abort(preludedb_sql_t *sql)
{
        if ( sql->internal_transaction_disabled )
                return 0;

        return _preludedb_sql_transaction_abort(sql);
}



/**
 * preludedb_sql_escape_fast:
 * @sql: Pointer to a sql object.
 * @input: Buffer to escape
 * @input_size: Buffer size.
 * @output: Where the new escaped buffer will be stored.
 *
 * Escape a string buffer.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_escape_fast(preludedb_sql_t *sql, const char *input, size_t input_size, char **output)
{
        int ret;

        gl_recursive_lock_lock(sql->mutex);

        assert_connected(sql);
        ret = _preludedb_plugin_sql_escape(sql->plugin, sql->session, input, input_size, output);

        gl_recursive_lock_unlock(sql->mutex);

        return ret;
}



/**
 * preludedb_sql_escape:
 * @sql: Pointer to a sql object.
 * @input: Buffer to escape.
 * @output: Where the new escaped buffer will be stored.
 *
 * Escape a string buffer.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_escape(preludedb_sql_t *sql, const char *input, char **output)
{
        if ( ! input ) {
                *output = strdup("NULL");
                return *output ? 0 : preludedb_error_from_errno(errno);
        }

        return preludedb_sql_escape_fast(sql, input, strlen(input), output);
}



/**
 * preludedb_sql_escape_binary:
 * @sql: Pointer to a sql object.
 * @input: Buffer to escape.
 * @input_size: Buffer size.
 * @output: Where the new escaped buffer will be stored.
 *
 * Escape a binary buffer.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_escape_binary(preludedb_sql_t *sql, const unsigned char *input, size_t input_size,
                                char **output)
{
        int ret;

        gl_recursive_lock_lock(sql->mutex);

        assert_connected(sql);
        ret = _preludedb_plugin_sql_escape_binary(sql->plugin, sql->session, input, input_size, output);

        gl_recursive_lock_unlock(sql->mutex);

        return ret;
}



/**
 * preludedb_sql_unescape_binary:
 * @sql: Pointer to a sql object.
 * @input: Buffer to unescape.
 * @input_size: Buffer size.
 * @output: Where the new unescaped buffer will be stored.
 * @output_size: Size of the new unescape buffer.
 *
 * Unescape to a binary buffer.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_unescape_binary(preludedb_sql_t *sql, const char *input, size_t input_size,
                                  unsigned char **output, size_t *output_size)
{
        int ret;

        gl_recursive_lock_lock(sql->mutex);

        assert_connected(sql);
        ret = _preludedb_plugin_sql_unescape_binary(sql->plugin, sql->session, input, input_size, output, output_size);

        gl_recursive_lock_unlock(sql->mutex);

        return ret;
}



/**
 * preludedb_sql_table_destroy:
 * @table: Pointer to a table object.
 *
 * Destroy the @table object.
 */
void preludedb_sql_table_destroy(preludedb_sql_table_t *table)
{
        preludedb_sql_t *sql = table->sql;
        preludedb_sql_row_t *row;
        preludedb_sql_field_t *field;
        prelude_list_t *tmp, *next_row, *next_field;

        prelude_list_for_each_safe(&table->row_list, tmp, next_row) {
                row = prelude_list_entry(tmp, preludedb_sql_row_t, list);

                prelude_list_for_each_safe(&row->field_list, tmp, next_field) {
                        field = prelude_list_entry(tmp, preludedb_sql_field_t, list);
                        free(field);
                }

                free(row);
        }

        _preludedb_plugin_sql_resource_destroy(sql->plugin, sql->session, table->res);
        free(table);
}



static int preludedb_sql_row_new(preludedb_sql_row_t **row, preludedb_sql_table_t *table, void *res)
{
        *row = malloc(sizeof(**row));
        if ( ! *row )
                return preludedb_error_from_errno(errno);

        (*row)->table = table;
        (*row)->res = res;

        prelude_list_init(&(*row)->list);
        prelude_list_init(&(*row)->field_list);

        prelude_list_add_tail(&table->row_list, &(*row)->list);

        return 0;
}



static int preludedb_sql_field_new(preludedb_sql_field_t **field,
                                   preludedb_sql_row_t *row, int num, const char *value, size_t len)
{
        *field = malloc(sizeof(**field));
        if ( ! field )
                return preludedb_error_from_errno(errno);

        (*field)->row = row;
        (*field)->num = num;
        (*field)->value = value;
        (*field)->len = len;

        prelude_list_init(&(*field)->list);

        prelude_list_add_tail(&row->field_list, &(*field)->list);

        return 0;
}



/**
 * preludedb_sql_table_get_column_name:
 * @table: Pointer to a table object.
 * @column_num: Column number.
 *
 * Get the name of the column number @column_num.
 *
 * Returns: the name of the column or NULL if the column @column_num could not be found.
 */
const char *preludedb_sql_table_get_column_name(preludedb_sql_table_t *table, unsigned int column_num)
{
        return _preludedb_plugin_sql_get_column_name(table->sql->plugin, table->sql->session, table->res, column_num);
}



/**
 * preludedb_sql_table_get_column_num:
 * @table: Pointer to a table object.
 * @column_name: Column name.
 *
 * Get the number of the column named @column_name.
 *
 * Returns: the number of the column or -1 if the column @column_name could not be found.
 */
int preludedb_sql_table_get_column_num(preludedb_sql_table_t *table, const char *column_name)
{
        return _preludedb_plugin_sql_get_column_num(table->sql->plugin, table->sql->session, table->res, column_name);
}



/**
 * preludedb_sql_table_get_column_count:
 * @table: Pointer to a table object.
 *
 * Get the the number of columns.
 *
 * Returns: the number of columns.
 */
unsigned int preludedb_sql_table_get_column_count(preludedb_sql_table_t *table)
{
        return _preludedb_plugin_sql_get_column_count(table->sql->plugin, table->sql->session, table->res);
}



/**
 * preludedb_sql_table_get_column_count:
 * @table: Pointer to a table object.
 *
 * Get the the number of columns.
 *
 * Returns: the number of columns.
 */
unsigned int preludedb_sql_table_get_row_count(preludedb_sql_table_t *table)
{
        return _preludedb_plugin_sql_get_row_count(table->sql->plugin, table->sql->session, table->res);
}



/**
 * preludedb_sql_table_fetch_row:
 * @table: Pointer to a table object.
 * @row: Pointer to the row object where the result will be stored.
 *
 * Fetch the next table's row.
 *
 * Returns: 1 if the table returns a new row, 0 if there is no more rows to fetch or
 * a negative value if an error occur.
 */
int preludedb_sql_table_fetch_row(preludedb_sql_table_t *table, preludedb_sql_row_t **row)
{
        int ret;
        void *res;

        ret = _preludedb_plugin_sql_fetch_row(table->sql->plugin, table->sql->session, table->res, &res);
        if ( ret < 0 ) {
                update_sql_from_errno(table->sql, ret);
                return ret;
        }

        if ( ret == 0 )
                return 0;

        ret = preludedb_sql_row_new(row, table, res);
        if ( ret < 0 )
                return ret;

        return 1;
}



/**
 * preludedb_sql_row_fetch_field:
 * @row: Pointer to a row object.
 * @column_num: The column number of the field to be fetched.
 * @field: Pointer to the field object where the result will be stored.
 *
 * Fetch the field of column @column_num
 *
 * Returns: 1 if the row returns a non-empty field, 0 if it returns an empty field, or
 * a negative value if an error occur.
 */
int preludedb_sql_row_fetch_field(preludedb_sql_row_t *row, unsigned int column_num,
                                  preludedb_sql_field_t **field)
{
        const char *value;
        size_t len;
        int ret;

        ret = _preludedb_plugin_sql_fetch_field(row->table->sql->plugin,
                                                row->table->sql->session, row->table->res, row->res, column_num, &value, &len);
        if ( ret < 0 ) {
                update_sql_from_errno(row->table->sql, ret);
                return ret;
        }

        if ( ret == 0 )
                return 0;

        ret = preludedb_sql_field_new(field, row, column_num, value, len);
        if ( ret < 0 )
                return ret;

        return 1;
}



/**
 * preludedb_sql_row_fetch_field_by_name:
 * @row: Pointer to a row object.
 * @column_name: The column name of the field to be fetched.
 * @field: Pointer to the field object where the result will be stored.
 *
 * Fetch the field of column @column_name
 *
 * Returns: 1 if the row returns a non-empty field, 0 if it returns an empty field, or
 * a negative value if an error occur.
 */
int preludedb_sql_row_fetch_field_by_name(preludedb_sql_row_t *row, const char *column_name,
                                          preludedb_sql_field_t **field)
{
        int column_num;

        column_num = preludedb_sql_table_get_column_num(row->table, column_name);
        if ( column_num < 0 )
                return column_num;

        return preludedb_sql_row_fetch_field(row, column_num, field);
}



/**
 * preludedb_sql_field_get_value:
 * @field: Pointer to a field object.
 *
 * Get the raw value of the field.
 *
 * Returns: field value.
 */
const char *preludedb_sql_field_get_value(preludedb_sql_field_t *field)
{
        return field->value;
}



/**
 * preludedb_sql_field_get_len:
 * @field: Pointer to a field object.
 *
 * Get the field value length.
 *
 * Returns: field value length.
 */
size_t preludedb_sql_field_get_len(preludedb_sql_field_t *field)
{
        return field->len;
}



/**
 * preludedb_sql_field_to_{int8,uint8,int16,uint16,int32,uint32,int64,uint64,float,double}:
 * @field: Pointer to a field object.
 * @value: Pointer to the output value.
 *
 * Get the typed value of @field.
 *
 * Returns: 0 on success, -1 if the wanted type does not match the field type.
 */
#define preludedb_sql_field_to(name, type_name, format)                                         \
int preludedb_sql_field_to_ ## name(preludedb_sql_field_t *field, type_name *value)             \
{                                                                                               \
        int ret;                                                                                \
                                                                                                \
        ret = sscanf(preludedb_sql_field_get_value(field), format, value);                      \
        if ( ret < 0 )                                                                          \
                return preludedb_error(PRELUDEDB_ERROR_INVALID_VALUE);                          \
                                                                                                \
        return 0;                                                                               \
}

#define preludedb_sql_field_to_int8(name, type_name, format, min, max)                          \
int preludedb_sql_field_to_ ## name(preludedb_sql_field_t *field, type_name *value)             \
{                                                                                               \
        int tmp, ret;                                                                           \
                                                                                                \
        ret = sscanf(preludedb_sql_field_get_value(field), format, &tmp);                       \
        if ( ret <= 0 || tmp < min || tmp > max )                                               \
                return preludedb_error(PRELUDEDB_ERROR_INVALID_VALUE);                          \
                                                                                                \
        *value = (type_name) tmp;                                                               \
                                                                                                \
        return 0;                                                                               \
}

/*
 * %hh is not a portable convertion specifier
 */
preludedb_sql_field_to_int8(int8, int8_t, "%d", PRELUDE_INT8_MIN, PRELUDE_INT8_MAX)
preludedb_sql_field_to_int8(uint8, uint8_t, "%u", 0, PRELUDE_UINT8_MAX)
preludedb_sql_field_to(int16, int16_t, "%hd")
preludedb_sql_field_to(uint16, uint16_t, "%hu")
preludedb_sql_field_to(int32, int32_t, "%d")
preludedb_sql_field_to(uint32, uint32_t, "%u")
preludedb_sql_field_to(int64, int64_t, "%" PRELUDE_PRId64)
preludedb_sql_field_to(uint64, uint64_t, "%" PRELUDE_PRIu64)
preludedb_sql_field_to(float, float, "%f")
preludedb_sql_field_to(double, double, "%lf")



/**
 * preludedb_sql_field_to_string:
 * @field: Pointer to a field type.
 * @output: Pointer to a string object where the field value will be added.
 *
 * Get the string value of @field.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_field_to_string(preludedb_sql_field_t *field, prelude_string_t *output)
{
        return prelude_string_ncat(output, field->value, field->len);
}


static int build_criterion_fixed_sql_time_value(preludedb_sql_t *sql,
                                                const idmef_value_t *value, char *buf, size_t size)
{
        const idmef_time_t *time;

        time = idmef_value_get_time(value);
        if ( ! time )
                return -1;

        return preludedb_sql_time_to_timestamp(sql, time, buf, size, NULL, 0, NULL, 0);
}



static int build_criterion_fixed_sql_like_value(const idmef_value_t *value, char **output)
{
        int ret;
        size_t i, len;
        const char *input;
        idmef_data_t *data;
        prelude_string_t *outbuf;
        const prelude_string_t *string;
        prelude_bool_t escape_next = FALSE;

        if ( idmef_value_get_type(value) == IDMEF_VALUE_TYPE_DATA ) {
                data = idmef_value_get_data(value);
                if ( ! data )
                        return -1;

                input = idmef_data_get_data(data);
                if ( ! input )
                        return -1;

                len = idmef_data_get_len(data);
        }

        else {
                string = idmef_value_get_string(value);
                if ( ! string )
                        return -1;

                input = prelude_string_get_string(string);
                if ( ! input )
                        return -1;

                len = prelude_string_get_len(string);
        }

        ret = prelude_string_new(&outbuf);
        if ( ret < 0 )
                return ret;

        for ( i = 0; i < len; i++ ) {

                /*
                 * Always escape %, since these are SQL specific.
                 */
                if ( *input == '%' )
                        prelude_string_cat(outbuf, "\\%");

                /*
                 * Convert unescaped * to % character.
                 */
                else if ( *input == '*' && ! escape_next )
                        prelude_string_cat(outbuf, "%");

                else prelude_string_ncat(outbuf, input, 1);

                escape_next = (! escape_next && *input == '\\') ? TRUE : FALSE;
                input++;
        }

        ret = prelude_string_get_string_released(outbuf, output);
        prelude_string_destroy(outbuf);

        return ret;
}



static int build_criterion_fixed_sql_value(preludedb_sql_t *sql,
                                           prelude_string_t *output,
                                           const idmef_value_t *value,
                                           idmef_criterion_operator_t operator)
{
        int ret;
        prelude_string_t *string;
        char *escaped;

        if ( idmef_value_get_type(value) == IDMEF_VALUE_TYPE_TIME ) {
                char buf[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE];

                ret = build_criterion_fixed_sql_time_value(sql, value, buf, sizeof(buf));
                if ( ret < 0 )
                        return ret;

                return prelude_string_cat(output, buf);
        }

        if ( operator & IDMEF_CRITERION_OPERATOR_SUBSTR ) {
                char *tmp;

                ret = build_criterion_fixed_sql_like_value(value, &tmp);
                if ( ret < 0 )
                        return ret;

                ret = preludedb_sql_escape(sql, tmp, &escaped);
                if ( ret < 0 ) {
                        free(tmp);
                        return ret;
                }

                ret = prelude_string_cat(output, escaped);

                free(tmp);
                free(escaped);

                return ret;
        }

        ret = prelude_string_new(&string);
        if ( ret < 0 )
                return ret;

        ret = idmef_value_to_string(value, string);
        if ( ret < 0 ) {
                prelude_string_destroy(string);
                return ret;
        }

        ret = preludedb_sql_escape(sql, prelude_string_get_string(string), &escaped);
        prelude_string_destroy(string);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_cat(output, escaped);
        free(escaped);

        return ret;
}



static int build_criterion_fixed_value(preludedb_sql_t *sql,
                                       prelude_string_t *output,
                                       const char *field,
                                       idmef_criterion_operator_t operator,
                                       const idmef_value_t *value)
{
        int ret;
        prelude_string_t *value_str;

        ret = prelude_string_new(&value_str);
        if ( ret < 0 )
                return ret;

        ret = build_criterion_fixed_sql_value(sql, value_str, value, operator);
        if ( ret < 0 )
                goto err;

        ret = _preludedb_plugin_sql_build_constraint_string(sql->plugin, output, field, operator, prelude_string_get_string(value_str));

 err:
        prelude_string_destroy(value_str);
        return ret;
}




static int build_criterion_broken_down_time_equal(preludedb_sql_t *sql, prelude_string_t *output,
                                                  const char *field, idmef_criterion_operator_t op, const struct tm *lt)
{
        unsigned int i;
        int ret, prev = 0, year, month;
        const struct {
                preludedb_sql_time_constraint_type_t type;
                const int *field_ptr;
        } tbl[] = {
                { PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR, &year        },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH, &month      },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY, &lt->tm_yday },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY, &lt->tm_mday },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY, &lt->tm_wday },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR, &lt->tm_hour },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_MIN,  &lt->tm_min  },
                { PRELUDEDB_SQL_TIME_CONSTRAINT_SEC,  &lt->tm_sec  }
        };

        year = lt->tm_year;
        if ( year != -1 )
                year += 1900;

        month = lt->tm_mon;
        if ( month != -1 )
                month += 1;

        for ( i = 0; i < sizeof(tbl) / sizeof(*tbl); i++ ) {

                if ( *(tbl[i].field_ptr) == -1 )
                        continue;

                if ( prev++ ) {
                        ret = prelude_string_cat(output, " AND ");
                        if ( ret < 0 )
                                return ret;
                }

                ret = _preludedb_plugin_sql_build_time_constraint_string(sql->plugin, output, field, tbl[i].type,
                                                                         op, *tbl[i].field_ptr, 0);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}



static int build_criterion_broken_down_time_not_equal(preludedb_sql_t *sql, prelude_string_t *output,
                                                      const char *field, const struct tm *lt)
{
        int ret;

        ret = prelude_string_cat(output, "NOT(");
        if ( ret < 0 )
                return ret;

        ret = build_criterion_broken_down_time_equal(sql, output, field, IDMEF_CRITERION_OPERATOR_EQUAL, lt);
        if ( ret < 0 )
                return ret;

        return prelude_string_cat(output, ")");
}



static int build_criterion_broken_down_time(preludedb_sql_t *sql, prelude_string_t *output,
                                            const char *field, idmef_criterion_operator_t op, const struct tm *time)
{
        if ( op == IDMEF_CRITERION_OPERATOR_EQUAL )
                return build_criterion_broken_down_time_equal(sql, output, field, op, time);

        else if ( op == IDMEF_CRITERION_OPERATOR_NOT_EQUAL )
                return build_criterion_broken_down_time_not_equal(sql, output, field, time);

        return preludedb_error(PRELUDEDB_ERROR_QUERY);
}




static int build_criterion_regex(preludedb_sql_t *sql, prelude_string_t *output,
                                 const char *field, idmef_criterion_operator_t op, const char *regex)
{
        int ret;
        char *escaped;

        ret = preludedb_sql_escape(sql, regex, &escaped);
        if ( ret < 0 )
                return ret;

        ret = _preludedb_plugin_sql_build_constraint_string(sql->plugin, output, field, op, escaped);
        free(escaped);

        return ret;
}



/**
 * preludedb_sql_build_criterion_string:
 * @sql: Pointer to a sql object.
 * @output: Pointer to a string object, where the result content will be stored.
 * @field: The sql field name.
 * @operator: The criterion operator.
 * @value: The criterion value.
 *
 * Build a sql "field operator value" string.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_build_criterion_string(preludedb_sql_t *sql,
                                         prelude_string_t *output,
                                         const char *field,
                                         idmef_criterion_operator_t operator, idmef_criterion_value_t *value)
{
        int ret = -1;
        const void *vptr;
        idmef_criterion_value_type_t type;

        if ( operator == IDMEF_CRITERION_OPERATOR_NULL )
                return prelude_string_sprintf(output, "%s %s", field,
                                              _preludedb_plugin_sql_get_operator_string(sql->plugin, operator));

        else if ( operator == IDMEF_CRITERION_OPERATOR_NOT_NULL )
                return prelude_string_sprintf(output, "%s %s", field,
                                              _preludedb_plugin_sql_get_operator_string(sql->plugin, operator));

        else if ( operator & IDMEF_CRITERION_OPERATOR_NOT ) {
                ret = prelude_string_sprintf(output, "(%s %s OR ", field,
                             _preludedb_plugin_sql_get_operator_string(sql->plugin, IDMEF_CRITERION_OPERATOR_NULL));
                if ( ret < 0 )
                        return ret;
        }

        type = idmef_criterion_value_get_type(value);
        vptr = idmef_criterion_value_get_value(value);

        if ( type == IDMEF_CRITERION_VALUE_TYPE_VALUE )
                ret = build_criterion_fixed_value(sql, output, field, operator, idmef_criterion_value_get_value(value));

        else if ( type == IDMEF_CRITERION_VALUE_TYPE_REGEX )
                ret = build_criterion_regex(sql, output, field, operator, idmef_criterion_value_get_regex(value));

        else if ( type == IDMEF_CRITERION_VALUE_TYPE_BROKEN_DOWN_TIME )
                ret = build_criterion_broken_down_time(sql, output, field, operator, idmef_criterion_value_get_broken_down_time(value));

        if ( ret >= 0 && operator & IDMEF_CRITERION_OPERATOR_NOT )
                ret = prelude_string_sprintf(output, ")");

        return ret;
}



/**
 * preludedb_sql_time_from_timestamp:
 * @time: Pointer to a time object.
 * @time_buf: SQL timestamp.
 * @gmtoff: GMT offset.
 * @usec: Microseconds.
 *
 * Set an idmef time using the timestamp, GMT offset and microseconds given in input.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_time_from_timestamp(idmef_time_t *time, const char *time_buf, int32_t gmtoff, uint32_t usec)
{
        int ret;
        struct tm tm;

        memset(&tm, 0, sizeof(tm));

        ret = sscanf(time_buf, "%d-%d-%d %d:%d:%d",
                     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                     &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        if ( ret < 6 )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "Database returned an unknown time format: '%s'", time_buf);

        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        tm.tm_isdst = -1;

        idmef_time_set_sec(time, prelude_timegm(&tm));
        idmef_time_set_usec(time, usec);
        idmef_time_set_gmt_offset(time, gmtoff);

        return 0;
}



/**
 * preludedb_sql_time_to_timestamp:
 * @time: Pointer to a time object.
 * @time_buf: SQL timestamp.
 * @time_buf_size: SQL timestamp buffer size.
 * @gmtoff_buf: GMT offset buffer.
 * @gmtoff_buf_size: GMT offset buffer size.
 * @usec_buf: Microseconds buffer.
 * @usec_buf_size: Microseconds buffer size.
 *
 * Set timestamp, GMT offset, and microseconds buffers with the idmef time object
 * given in input.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_time_to_timestamp(preludedb_sql_t *sql,
                                    const idmef_time_t *time,
                                    char *time_buf, size_t time_buf_size,
                                    char *gmtoff_buf, size_t gmtoff_buf_size,
                                    char *usec_buf, size_t usec_buf_size)
{
        int ret;
        time_t t;
        struct tm utc;

        if ( ! time ) {
                snprintf(time_buf, time_buf_size,  "NULL");

                if ( gmtoff_buf )
                        snprintf(gmtoff_buf, gmtoff_buf_size, "NULL");

                if ( usec_buf )
                        snprintf(usec_buf, usec_buf_size, "NULL");

                return 0;
        }

        t = idmef_time_get_sec(time);

        if ( ! gmtime_r(&t, &utc) )
                return prelude_error_from_errno(errno);

        ret = _preludedb_plugin_sql_build_timestamp_string(sql->plugin, &utc, time_buf, time_buf_size);
        if ( ret < 0 )
                return ret;

        if ( gmtoff_buf )
                snprintf(gmtoff_buf, gmtoff_buf_size, "%d", idmef_time_get_gmt_offset(time));

        if ( usec_buf )
                snprintf(usec_buf, usec_buf_size, "%d", idmef_time_get_usec(time));

        return 0;
}



/**
 * preludedb_sql_get_plugin_error:
 * @sql: Pointer to a sql object.
 *
 * Get sql plugin specific error message.
 * Deprecated: Use preludedb_strerror().
 *
 * Returns: a non NULL pointer or a NULL pointer if no error is available.
 */
const char *preludedb_sql_get_plugin_error(preludedb_sql_t *sql)
{
        /*
         * FIXME: deprecated.
         */
        return NULL;
}



void _preludedb_sql_enable_internal_transaction(preludedb_sql_t *sql)
{
        sql->internal_transaction_disabled = FALSE;
}



void _preludedb_sql_disable_internal_transaction(preludedb_sql_t *sql)
{
        sql->internal_transaction_disabled = TRUE;
}
