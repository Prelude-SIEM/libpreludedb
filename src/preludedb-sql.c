/*****
*
* Copyright (C) 2001-2004 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2002, 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <assert.h>
#include <pthread.h>

#include <libprelude/prelude-list.h>
#include <libprelude/common.h>
#include <libprelude/prelude-log.h>
#include <libprelude/prelude-error.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-util.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-plugin-sql.h"



struct preludedb_sql {
	char *type;
	preludedb_sql_settings_t *settings;
	preludedb_plugin_sql_t *plugin;
	preludedb_sql_status_t status;
        void *session;
	FILE *logfile;
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



#define assert_connected(sql)					\
	if ( sql->status < PRELUDEDB_SQL_STATUS_CONNECTED )	\
		return -1;



static void update_sql_from_errno(preludedb_sql_t *sql, int error)
{
	/* FIXME: to be implemented  */
}



static int load_sql_plugins_if_needed(void)
{
	static prelude_bool_t plugins_loaded = FALSE;
	static pthread_mutex_t plugins_loaded_mutex = PTHREAD_MUTEX_INITIALIZER;
	int ret = 0;

	pthread_mutex_lock(&plugins_loaded_mutex);

	if ( ! plugins_loaded ) {
		ret = access(SQL_PLUGIN_DIR, F_OK);
		if ( ret < 0 )
			goto error;

		prelude_plugin_load_from_dir(SQL_PLUGIN_DIR, NULL, NULL);
		if ( ret < 0 )
			goto error;

		plugins_loaded = TRUE;
	}

 error: 
	pthread_mutex_unlock(&plugins_loaded_mutex);

	return ret;
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
	int ret;

	ret = load_sql_plugins_if_needed();
	if ( ret < 0 )
		return ret;

	*new = calloc(1, sizeof (**new));
	if ( ! *new )
		return preludedb_error_from_errno(errno);

	(*new)->type = strdup(type);
	if ( ! (*new)->type ) {
		free(*new);
		return preludedb_error_from_errno(errno);
	}

	(*new)->settings = settings;

	(*new)->plugin = (preludedb_plugin_sql_t *) prelude_plugin_search_by_name(type);
	if ( ! (*new)->plugin ) {
		free((*new)->type);
		free(*new);
		return -1;
	}

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
	if ( sql->status >= PRELUDEDB_SQL_STATUS_CONNECTED )
		sql->plugin->close(sql->session);

	if ( sql->logfile )
		fclose(sql->logfile);

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
	sql->logfile = fopen(filename, "a");

	return sql->logfile ? 0 : preludedb_error_from_errno(errno);
}



/**
 * preludedb_sql_disable_query_logging:
 * @sql: Pointer to a sql object.
 *
 * Disable query logging.
 */
void preludedb_sql_disable_query_logging(preludedb_sql_t *sql)
{
	fclose(sql->logfile);
	sql->logfile = NULL;
}



/**
 * preludedb_sql_connect:
 * @sql: Pointer to a sql object.
 *
 * Connect to the database.
 *
 * Nota: This function must be called even on non client/server underlying database
 * to get the @sql object ready to be used.
 *
 * Returns: 0 if the database was already connected, 1 if the the database has been
 * sucessfully connected, or a negative value if an error occur.
 */
int preludedb_sql_connect(preludedb_sql_t *sql)
{
	int ret;

	if ( sql->status >= PRELUDEDB_SQL_STATUS_CONNECTED )
		return 0;

	ret = sql->plugin->open(sql->settings, &sql->session);
	if ( ret < 0 )
		return ret;

	sql->status = PRELUDEDB_SQL_STATUS_CONNECTED;

	return 1;
}



/**
 * preludedb_sql_disconnect:
 * @sql: Pointer to a sql object.
 * 
 * Disconnect from the database.
 */
void preludedb_sql_disconnect(preludedb_sql_t *sql)
{
	if ( sql->status == PRELUDEDB_SQL_STATUS_DISCONNECTED )
		return;

	sql->plugin->close(sql->session);

	sql->status = PRELUDEDB_SQL_STATUS_DISCONNECTED;
	sql->session = NULL;
}



/**
 * preludedb_sql_get_plugin_error:
 * @sql: Pointer to a sql object.
 *
 * Get sql plugin specific error message.
 *
 * Returns: a non NULL pointer or a NULL pointer if no error is available.
 */
const char *preludedb_sql_get_plugin_error(preludedb_sql_t *sql)
{
	if ( sql->status < PRELUDEDB_SQL_STATUS_CONNECTED )
		return NULL;

	if ( ! sql->plugin->get_error )
		return NULL;

	return sql->plugin->get_error(sql->session);
}



static int preludedb_sql_table_new(preludedb_sql_table_t **new, preludedb_sql_t *sql, void *res)
{
	*new = malloc(sizeof (**new));
	if ( ! *new )
		return preludedb_error_from_errno(errno);

	(*new)->sql = sql;
	(*new)->res = res;
	PRELUDE_INIT_LIST_HEAD(&(*new)->row_list);

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
 * Returns: 1 if the query returns results, 0 if it does not, or negative value if an error occur.
 */
int preludedb_sql_query(preludedb_sql_t *sql, const char *query, preludedb_sql_table_t **table)
{
	int ret;
	void *res;

	assert_connected(sql);

	if ( sql->logfile ) {
		size_t ret;

		ret = fprintf(sql->logfile, "%s\n", query);
		if ( ret < strlen(query) + 1 || fflush(sql->logfile) == EOF )
			log(LOG_ERR, "could not log query: %s.\n", strerror(errno));

		/* Show must go on: don't stop trying executing the query even if we cannot log it */
	}

	ret = sql->plugin->query(sql->session, query, &res);
	if ( ret < 0 ) {
		update_sql_from_errno(sql, ret);
		return ret;
	}

	if ( ret == 0 )
		return 0;

	ret = preludedb_sql_table_new(table, sql, res);
	if ( ret < 0 ) {
		sql->plugin->resource_destroy(sql->session, res);
		return ret;
	}

	return 1;
}



/**
 * preludedb_sql_query_sprintf:
 * @sql: Pointer to a sql object.
 * @table: Pointer to a table where the query result will be stored if the type of query return 
 * results (i.e a SELECT can results, but an INSERT never results) and if the query is sucessfull.
 * @format: The SQL query to execute in a printf format string.
 *
 * Execute a SQL query.
 *
 * Returns: 1 if the query returns results, 0 if it does not, or negative value if an error occur.
 */
int preludedb_sql_query_sprintf(preludedb_sql_t *sql, preludedb_sql_table_t **table,
				const char *format, ...)
{
        va_list ap;
	prelude_string_t *query;
	int ret;

	assert_connected(sql);

	query = prelude_string_new();
	if ( ! query )
		return -1;
        
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
 *
 * Insert values in a table.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_insert(preludedb_sql_t *sql, const char *table, const char *fields,
			 const char *format, ...)
{
	va_list ap;
	prelude_string_t *query;
	int ret;

	assert_connected(sql);

	query = prelude_string_new();
	if ( ! query )
		return -1;

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
	if ( ret < 0 )
		update_sql_from_errno(sql, ret);

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
int preludedb_sql_build_limit_offset_string(preludedb_sql_t *sql, int limit, int offset,
					    prelude_string_t *output)
{
	return sql->plugin->build_limit_offset_string(sql->session, limit, offset, output);
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
	int ret;

	if ( sql->status == PRELUDEDB_SQL_STATUS_DISCONNECTED ||
	     sql->status == PRELUDEDB_SQL_STATUS_TRANSACTION )
		return -1;

	ret = preludedb_sql_query(sql, "BEGIN", NULL);
	if ( ret < 0 ) {
		update_sql_from_errno(sql, ret);
		return ret;
	}

	sql->status = PRELUDEDB_SQL_STATUS_TRANSACTION;

	return 0;
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
	int ret;

	if ( sql->status != PRELUDEDB_SQL_STATUS_TRANSACTION )
		return -1;
	
	ret = preludedb_sql_query(sql, "COMMIT", NULL);
	if ( ret < 0 )
		update_sql_from_errno(sql, ret);

	sql->status = PRELUDEDB_SQL_STATUS_CONNECTED;

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
	int ret;

	if ( sql->status != PRELUDEDB_SQL_STATUS_TRANSACTION )
		return -1;

	ret = preludedb_sql_query(sql, "ROLLBACK", NULL);
	if ( ret < 0 )
		update_sql_from_errno(sql, ret);

	sql->status = PRELUDEDB_SQL_STATUS_CONNECTED;

	return ret;
}



/**
 * preludedb_sql_escape:
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
	assert_connected(sql);

	if ( sql->plugin->escape )
		return sql->plugin->escape(sql->session, input, input_size, output);

	return sql->plugin->escape_binary(sql->session, (const unsigned char *) input, input_size, output);
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
	assert_connected(sql);

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
int preludedb_sql_escape_binary(preludedb_sql_t *sql, const unsigned char *input, size_t input_size, char **output)
{
	assert_connected(sql);

	return sql->plugin->escape_binary(sql->session, input, input_size, output);
}



/**
 * preludedb_sql_unescape_binary:
 * @sql: Pointer to a sql object.
 * @input: Buffer to unescape.
 * @input_size: Buffer size.
 * @output: Where the new unescaped buffer will be stored.
 * @output: Size of the new unescape buffer.
 *
 * Unescape to a binary buffer.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_sql_unescape_binary(preludedb_sql_t *sql, const char *input, size_t input_size,
				  unsigned char **output, size_t *output_size)
{
	assert_connected(sql);

	if ( sql->plugin->unescape_binary) 
		return sql->plugin->unescape_binary(sql->session, input, output, output_size);

	*output = malloc(input_size);
	if ( ! *output )
		return preludedb_error_from_errno(errno);

	memcpy(*output, input, input_size);
	*output_size = input_size;

	return 0;
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

	prelude_list_for_each_safe(tmp, next_row, &table->row_list) {
		row = prelude_list_entry(tmp, preludedb_sql_row_t, list);

                prelude_list_for_each_safe(tmp, next_field, &row->field_list) {
			field = prelude_list_entry(tmp, preludedb_sql_field_t, list);
			free(field);
		}

                free(row);
	}
        
	sql->plugin->resource_destroy(sql->session, table->res);
	free(table);
}



static int preludedb_sql_row_new(preludedb_sql_row_t **row, preludedb_sql_table_t *table, void *res)
{
	*row = malloc(sizeof (**row));
	if ( ! *row )
		return preludedb_error_from_errno(errno);

	(*row)->table = table;
	(*row)->res = res;

        PRELUDE_INIT_LIST_HEAD(&(*row)->list);
	PRELUDE_INIT_LIST_HEAD(&(*row)->field_list);

        prelude_list_add_tail(&(*row)->list, &table->row_list);
	
	return 0;
}



static int preludedb_sql_field_new(preludedb_sql_field_t **field,
				   preludedb_sql_row_t *row, int num, const char *value, size_t len)
{
	*field = malloc(sizeof (**field));
	if ( ! field )
		preludedb_error_from_errno(errno);
	
	(*field)->row = row;
	(*field)->num = num;
	(*field)->value = value;
	(*field)->len = len;

        PRELUDE_INIT_LIST_HEAD(&(*field)->list);

        prelude_list_add_tail(&(*field)->list, &row->field_list);
	
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
	return table->sql->plugin->get_column_name(table->sql->session, table->res, column_num);
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
	return table->sql->plugin->get_column_num(table->sql->session, table->res, column_name);
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
	return table->sql->plugin->get_column_count(table->sql->session, table->res);
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
	return table->sql->plugin->get_row_count(table->sql->session, table->res);
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
	void *res;
	int ret;

	ret = table->sql->plugin->fetch_row(table->sql->session, table->res, &res);
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

	ret = row->table->sql->plugin->fetch_field(row->table->sql->session, row->table->res, row->res,
						   column_num, &value, &len);
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
#define preludedb_sql_field_to(name, type_name, format)						\
int preludedb_sql_field_to_ ## name(preludedb_sql_field_t *field, type_name *value)		\
{												\
	return (sscanf(preludedb_sql_field_get_value(field), format, value) <= 0) ? -1 : 0;	\
}

preludedb_sql_field_to(int8, int8_t, "%hhd")
preludedb_sql_field_to(uint8, uint8_t, "%hhu")
preludedb_sql_field_to(int16, int16_t, "%hd")
preludedb_sql_field_to(uint16, uint16_t, "%hu")
preludedb_sql_field_to(int32, int32_t, "%d")
preludedb_sql_field_to(uint32, uint32_t, "%u")
preludedb_sql_field_to(int64, int64_t, "%" PRId64)
preludedb_sql_field_to(uint64, uint64_t, "%" PRIu64)
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



/**
 * preludedb_sql_get_relation_string:
 * @relation: An idmef value relation.
 *
 * Get the sql representation of the given idmef value relation.
 *
 * Returns: sql representation of the relation, or NULL if there is no sql representation
 * for the given relation.
 */
const char *preludedb_sql_get_relation_string(idmef_value_relation_t relation)
{
        int i;
        struct tbl {
                idmef_value_relation_t relation;
                const char *name;
        } tbl[] = {
                { IDMEF_VALUE_RELATION_SUBSTR,                            "LIKE" },
                { IDMEF_VALUE_RELATION_REGEX,                               NULL },
                { IDMEF_VALUE_RELATION_GREATER,                              ">" },
                { IDMEF_VALUE_RELATION_GREATER|IDMEF_VALUE_RELATION_EQUAL,  ">=" },
                { IDMEF_VALUE_RELATION_LESSER,                               "<" },
                { IDMEF_VALUE_RELATION_LESSER|IDMEF_VALUE_RELATION_EQUAL,   "<=" },
                { IDMEF_VALUE_RELATION_EQUAL,                                "=" },
                { IDMEF_VALUE_RELATION_NOT_EQUAL,                           "!=" },
                { IDMEF_VALUE_RELATION_IS_NULL,                        "IS NULL" },
                { IDMEF_VALUE_RELATION_IS_NOT_NULL,                "IS NOT NULL" },
                { 0, NULL },
        };

        for ( i = 0; tbl[i].relation != 0; i++ )
                if ( relation == tbl[i].relation )
                        return tbl[i].name;
        
	return NULL;
}



static int build_time_constraint(preludedb_sql_t *sql,
				 prelude_string_t *output, const char *field,
				 preludedb_sql_time_constraint_type_t type,
				 idmef_value_relation_t relation, int value)
{
	uint32_t gmt_offset;

	if ( prelude_get_gmt_offset(&gmt_offset) < 0 )
		return -1;

	return sql->plugin->build_time_constraint_string(output, field, type, relation, value, gmt_offset);
}



static int build_time_interval(preludedb_sql_t *sql,
			       preludedb_sql_time_constraint_type_t type, int value,
			       char *buf, size_t size)

{
	return sql->plugin->build_time_interval_string(type, value, buf, size);
}



static const struct {
	preludedb_sql_time_constraint_type_t type;
	int (*get_value)(idmef_criterion_value_non_linear_time_t *);
} non_linear_time_values[] = {
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR,	idmef_criterion_value_non_linear_time_get_year	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH,	idmef_criterion_value_non_linear_time_get_month	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY,	idmef_criterion_value_non_linear_time_get_yday	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY,	idmef_criterion_value_non_linear_time_get_mday	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY,	idmef_criterion_value_non_linear_time_get_wday	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR,	idmef_criterion_value_non_linear_time_get_hour	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_MIN,	idmef_criterion_value_non_linear_time_get_min	},
	{ PRELUDEDB_SQL_TIME_CONSTRAINT_SEC,	idmef_criterion_value_non_linear_time_get_sec	}
};



static int build_criterion_non_linear_time_value_relation_equal(preludedb_sql_t *sql,
								prelude_string_t *output,
								const char *field,
								idmef_value_relation_t relation,
								idmef_criterion_value_non_linear_time_t *time)
{
	int i;
	int value;
	int prev = 0;
	int ret;

	for ( i = 0; i < sizeof (non_linear_time_values) / sizeof (non_linear_time_values[0]); i++ ) {
		value = non_linear_time_values[i].get_value(time);
		if ( value == -1 )
			continue;

		if ( prev++ ) {
			ret = prelude_string_cat(output, " AND ");
			if ( ret < 0 )
				return ret;
		}
		

		ret = build_time_constraint(sql, output, field, non_linear_time_values[i].type, relation, value);
		if ( ret < 0 )
			return ret;
	}

	return 0;
}


static int build_criterion_non_linear_time_value_relation_not_equal(preludedb_sql_t *sql,
								    prelude_string_t *output,
								    const char *field,
								    idmef_criterion_value_non_linear_time_t *time)
{
	int ret;

	ret = prelude_string_cat(output, "NOT(");
	if ( ret < 0 )
		return ret;

	ret = build_criterion_non_linear_time_value_relation_equal(sql, output,
								   field, IDMEF_VALUE_RELATION_EQUAL, time);
	if ( ret < 0 )
		return ret;

	return prelude_string_cat(output, ")");
}



static int build_criterion_timestamp(preludedb_sql_t *sql,
				     prelude_string_t *output,
				     const char *field,
				     idmef_value_relation_t relation,
				     idmef_criterion_value_non_linear_time_t *time)
{
	struct tm tm;
	int month, day, hour, min, sec;
	idmef_time_t t;
	char buf[IDMEF_TIME_MAX_STRING_SIZE];
	int offset = 0;
	int ret;

	if ( relation == IDMEF_VALUE_RELATION_GREATER ||
	     relation == (IDMEF_VALUE_RELATION_LESSER|IDMEF_VALUE_RELATION_EQUAL) )
		offset = 1;

	month = idmef_criterion_value_non_linear_time_get_month(time);
	day = idmef_criterion_value_non_linear_time_get_mday(time);
	hour = idmef_criterion_value_non_linear_time_get_hour(time);
	min = idmef_criterion_value_non_linear_time_get_min(time);
	sec = idmef_criterion_value_non_linear_time_get_sec(time);

	memset(&tm, 0, sizeof (tm));
	tm.tm_mday = 1;
	tm.tm_isdst = -1;

	tm.tm_year = idmef_criterion_value_non_linear_time_get_year(time) - 1900;

	/*
	 * This is not ascii art ;)
	 */

	if ( month != -1 ) {
		tm.tm_mon = month - 1;

		if ( day != -1 ) {
			tm.tm_mday = day;

			if ( hour != -1 ) {
				tm.tm_hour = hour;

				if ( min != -1 ) {
					tm.tm_min = min;

					if ( sec != -1 ) {
						tm.tm_sec = sec + offset;

					} else {
						tm.tm_min += offset;
					}
				} else {
					tm.tm_hour += offset;
				}
			} else {
				tm.tm_mday += offset;
			}
		} else {
			tm.tm_mon += offset;
		}
	} else {
		tm.tm_year += offset;
	}

	if ( relation & IDMEF_VALUE_RELATION_LESSER )
		tm.tm_sec -= 1;

	t.sec = mktime(&tm);
	t.usec = 0;

	ret = preludedb_sql_time_to_timestamp(&t, buf, sizeof (buf), NULL, 0, NULL, 0);
	if ( ret < 0 )
		return ret;

	return prelude_string_sprintf(output, "%s %s %s",
				      field,
				      preludedb_sql_get_relation_string(relation|IDMEF_VALUE_RELATION_EQUAL),
				      buf);
}



static int build_criterion_hour(preludedb_sql_t *sql,
				prelude_string_t *output,
				const char *field,
				idmef_value_relation_t relation,
				idmef_criterion_value_non_linear_time_t *timeval)
{
	int hour, min, sec;
	unsigned int total_seconds;
	int relation_offset;
	uint32_t gmt_offset;
	char interval[128];
	int ret;

	hour = idmef_criterion_value_non_linear_time_get_hour(timeval);
	min = idmef_criterion_value_non_linear_time_get_min(timeval);
	sec = idmef_criterion_value_non_linear_time_get_sec(timeval);

	if ( relation & IDMEF_VALUE_RELATION_EQUAL )
		relation_offset = 0;
	else
		relation_offset = 1;

	total_seconds = hour * 3600;
	if ( min != -1 ) {
		total_seconds += min * 60;
		if ( sec != -1 ) {
			total_seconds += sec;
		} else {
			total_seconds += relation_offset * 60;
		}
	} else {
		total_seconds += relation_offset * 3600;
	}

	ret = prelude_get_gmt_offset(&gmt_offset);
	if ( ret < 0 )
		return ret;

	ret = build_time_interval(sql, PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR, gmt_offset / 3600, interval, sizeof (interval));
	if ( ret < 0 )
		return ret;

	return prelude_string_sprintf(output,
				      "EXTRACT(HOUR FROM %s + %s) * 3600 + "
				      "EXTRACT(MINUTE FROM %s + %s) * 60 + "
				      "EXTRACT(SECOND FROM %s + %s) %s %d",
				      field, interval,
				      field, interval,
				      field, interval,
				      preludedb_sql_get_relation_string(relation | IDMEF_VALUE_RELATION_EQUAL),
				      total_seconds);
}



static int build_criterion_non_linear_time_value_relation_lesser_or_greater(preludedb_sql_t *sql,
									    prelude_string_t *output,
									    const char *field,
									    idmef_value_relation_t relation,
									    idmef_criterion_value_non_linear_time_t *time)
{
	int tmp;

	if ( idmef_criterion_value_non_linear_time_get_year(time) != -1 )
		return build_criterion_timestamp(sql, output, field, relation, time);

	if ( idmef_criterion_value_non_linear_time_get_hour(time) != -1 )
		return build_criterion_hour(sql, output, field, relation, time);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_yday(time)) != -1 )
		return build_time_constraint(sql, output, field, PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY, relation, tmp);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_wday(time)) != -1 )
		return build_time_constraint(sql, output, field, PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY, relation, tmp);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_month(time)) != -1 )
		return build_time_constraint(sql, output, field, PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH, relation, tmp);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_mday(time)) != -1 )
		return build_time_constraint(sql, output, field, PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY, relation, tmp);

	return -1;
}



static int build_criterion_non_linear_time_value(preludedb_sql_t *sql,
						 prelude_string_t *output,
						 const char *field,
						 idmef_value_relation_t relation,
						 idmef_criterion_value_non_linear_time_t *time)
{
	if ( relation == IDMEF_VALUE_RELATION_EQUAL )
		return build_criterion_non_linear_time_value_relation_equal
			(sql, output, field, relation, time);

	if ( relation == IDMEF_VALUE_RELATION_NOT_EQUAL )
		return build_criterion_non_linear_time_value_relation_not_equal
			(sql, output, field, time);

	if ( relation & (IDMEF_VALUE_RELATION_LESSER | IDMEF_VALUE_RELATION_GREATER) )
		return build_criterion_non_linear_time_value_relation_lesser_or_greater
			(sql, output, field, relation, time);

	return -1;
}



static int build_criterion_fixed_sql_time_value(idmef_value_t *value, char *buf, size_t size)
{
	idmef_time_t *time;

	time = idmef_value_get_time(value);
	if ( ! time )
		return -1;

	return preludedb_sql_time_to_timestamp(time, buf, size, NULL, 0, NULL, 0);
}



static int build_criterion_fixed_sql_like_value(idmef_value_t *value, char *buf, size_t size)
{
        size_t i = 0;
        const char *input;
        prelude_string_t *string;

	string = idmef_value_get_string(value);
	if ( ! string )
		return -1;

        input = prelude_string_get_string(string);
        if ( ! input )
                return -1;
        
        while ( *input ) {

                if ( i == (size - 1) )
                        return -1;
                
                if ( *input == '*' )
                        buf[i++] = '%';
                
                else if ( *input == '\\' && *(input + 1) == '*' ) {
                        input++;
                        buf[i++] = '*';
                }
                
                else buf[i++] = *input;

                input++;
        }

        buf[i] = 0;
        
	return 0;
}



static int build_criterion_fixed_sql_value(preludedb_sql_t *sql,
					   prelude_string_t *output,
					   idmef_value_t *value,
					   idmef_value_relation_t relation)
{
	int ret;
	char buf[IDMEF_TIME_MAX_STRING_SIZE];
	prelude_string_t *string;
	char *tmp;

	if ( idmef_value_get_type(value) == IDMEF_VALUE_TYPE_TIME ) {
		ret = build_criterion_fixed_sql_time_value(value, buf, sizeof (buf));
		if ( ret < 0 )
			return ret;

		return prelude_string_cat(output, buf);
	}

	if ( relation == IDMEF_VALUE_RELATION_SUBSTR ) {
		ret = build_criterion_fixed_sql_like_value(value, buf, sizeof (buf));
		if ( ret < 0 )
			return ret;

		ret = preludedb_sql_escape(sql, buf, &tmp);
		if ( ret < 0 )
			return ret;

		ret = prelude_string_cat(output, tmp);

		free(tmp);

		return ret;		
	}

	string = prelude_string_new();
	if ( ! string )
		return -1;

	ret = idmef_value_to_string(value, string);
	if ( ret < 0 ) {
		prelude_string_destroy(string);
		return ret;
	}

	ret = preludedb_sql_escape(sql, prelude_string_get_string(string), &tmp);
	prelude_string_destroy(string);
	if ( ret < 0 )
		return ret;

	ret = prelude_string_cat(output, tmp);
	free(tmp);

	return ret;
}



static int build_criterion_relation(prelude_string_t *output, idmef_value_relation_t relation)
{
	const char *tmp;

	tmp = preludedb_sql_get_relation_string(relation);
	if ( ! tmp )
		return -1;

	return prelude_string_sprintf(output, "%s ", tmp);
}



static int build_criterion_fixed_value(preludedb_sql_t *sql,
				       prelude_string_t *output,
				       const char *field,
				       idmef_value_relation_t relation,
				       idmef_value_t *value)
{
	int ret;

	ret = prelude_string_sprintf(output, "%s ", field);
	if ( ret < 0 )
		return ret;

	ret = build_criterion_relation(output, relation);
	if ( ret < 0 )
		return ret;

	return build_criterion_fixed_sql_value(sql, output, value, relation);
}



/**
 * preludedb_sql_build_criterion_string:
 * @sql: Pointer to a sql object.
 * @output: Pointer to a string object, where the result content will be stored.
 * @field: The sql field name.
 * @relation: The criterion operator.
 * @value: The criterion value.
 *
 * Build a sql "field operator value" string.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_build_criterion_string(preludedb_sql_t *sql,
					 prelude_string_t *output,
					 const char *field,
					 idmef_value_relation_t relation, idmef_criterion_value_t *value)
{
	if ( relation == IDMEF_VALUE_RELATION_IS_NULL )
		return prelude_string_sprintf(output, "%s = NULL", field);

	if ( relation == IDMEF_VALUE_RELATION_IS_NOT_NULL )
		return prelude_string_sprintf(output, "%s != NULL", field);

	switch ( idmef_criterion_value_get_type(value) ) {
	case idmef_criterion_value_type_fixed:
		return build_criterion_fixed_value(sql, output, field, relation,
						   idmef_criterion_value_get_fixed(value));

	case idmef_criterion_value_type_non_linear_time:
		return build_criterion_non_linear_time_value(sql, output, field, relation,
							     idmef_criterion_value_get_non_linear_time(value));
	}

	return -1;
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
int preludedb_sql_time_from_timestamp(idmef_time_t *time, const char *time_buf, uint32_t gmtoff, uint32_t usec)
{
	int ret;
        struct tm tm;
        
        memset(&tm, 0, sizeof (tm));
        
        ret = sscanf(time_buf, "%d-%d-%d %d:%d:%d",
                     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                     &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        if ( ret < 6 )
                return -1;

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
 * @time_buf_len: SQL timestamp buffer size.
 * @gmtoff_buf: GMT offset buffer.
 * @gmtoff_buf_len: GMT offset buffer size.
 * @usec_buf: Microseconds buffer.
 * @usec_buf_size: Microseconds buffer size.
 *
 * Set timestamp, GMT offset, and microseconds buffers with the idmef time object
 * given in input.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_sql_time_to_timestamp(const idmef_time_t *time,
				    char *time_buf, size_t time_buf_size,
				    char *gmtoff_buf, size_t gmtoff_buf_size,
				    char *usec_buf, size_t usec_buf_size)
{
        struct tm utc;
	time_t t;
        
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
                return -1;
        
        snprintf(time_buf, time_buf_size, "'%d-%.2d-%.2d %.2d:%.2d:%.2d'",
		 utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
		 utc.tm_hour, utc.tm_min, utc.tm_sec);

	if ( gmtoff_buf )
		snprintf(gmtoff_buf, gmtoff_buf_size, "%d", idmef_time_get_gmt_offset(time));

	if ( usec_buf )
		snprintf(usec_buf, usec_buf_size, "%d", idmef_time_get_usec(time));
        
        return 0;
}
