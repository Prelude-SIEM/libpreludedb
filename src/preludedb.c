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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <pthread.h>

#include <libprelude/prelude-plugin.h>
#include <libprelude/idmef.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-path-selection.h"

#include "preludedb.h"
#include "preludedb-plugin-format.h"


#define PRELUDEDB_PLUGIN_SYMBOL "preludedb_plugin_init"


struct preludedb {
	char *format_name;
	preludedb_sql_t *sql;
	preludedb_plugin_format_t *plugin;
};

struct preludedb_result_idents {
	preludedb_t *db;
	void *res;
};

struct preludedb_result_values {
	preludedb_t *db;
	preludedb_path_selection_t *selection;
	void *res;
};



static int load_format_plugins_if_needed(void)
{
	static prelude_bool_t plugins_loaded = FALSE;
	static pthread_mutex_t plugins_loaded_mutex = PTHREAD_MUTEX_INITIALIZER;
	int ret = 0;
	
	pthread_mutex_lock(&plugins_loaded_mutex);

	if ( ! plugins_loaded ) {
		ret = access(FORMAT_PLUGIN_DIR, F_OK);
		if ( ret < 0 )
			goto error;

		prelude_plugin_load_from_dir(FORMAT_PLUGIN_DIR, PRELUDEDB_PLUGIN_SYMBOL, NULL, NULL, NULL);
		if ( ret < 0 )
			goto error;

		plugins_loaded = TRUE;
	}

 error: 
	pthread_mutex_unlock(&plugins_loaded_mutex);

	return ret;
}



static int preludedb_autodetect_format(preludedb_t *db)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	preludedb_sql_field_t *field;
	int ret;

	ret = preludedb_sql_query(db->sql, "SELECT name from _format", &table);
	if ( ret <= 0 )
		return (ret < 0) ? ret : -1;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_row_fetch_field(row, 0, &field);
	if ( ret < 0 )
		goto error;

	ret = preludedb_set_format(db, preludedb_sql_field_get_value(field));

 error:
	preludedb_sql_table_destroy(table);

	return ret;	
	
}



/**
 * preludedb_new:
 * @db: Pointer to a db object to initialize.
 * @sql: Pointer to a sql object.
 * @format_name: Format name of the underlying database, if NULL the format will be automatically detected
 * @errbuf: Buffer that will be set to an error message if an error occur.
 * @size: size of @errbuf.
 * 
 * This function initialize the @db object and detect the format of the underlying database if no format name
 * is given.
 * 
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_new(preludedb_t **db, preludedb_sql_t *sql, const char *format_name, char *errbuf, size_t size)
{
	int ret;

	load_format_plugins_if_needed();

	*db = calloc(1, sizeof (**db));
	if ( ! *db ) {
		ret = preludedb_error_from_errno(errno);
		snprintf(errbuf, size, "%s", preludedb_strerror(ret));
		return ret;
	}

	(*db)->sql = sql;

	if ( format_name )
		ret = preludedb_set_format(*db, format_name);
	else
		ret = preludedb_autodetect_format(*db);

	if ( ret < 0 ) {
		preludedb_get_error(*db, ret, errbuf, size);

		if ( (*db)->format_name )
			free((*db)->format_name);
		free(*db);
	}

	return ret;
}



/**
 * preludedb_destroy:
 * @db: Pointer to a db object.
 *
 * Destroy @db object and the underlying @sql object given as argument to preludedb_new.
 */
void preludedb_destroy(preludedb_t *db)
{
	preludedb_sql_destroy(db->sql);
	free(db->format_name);
	free(db);
}



/**
 * preludedb_get_format:
 * @db: Pointer to a db object.
 *
 * Returns: the format name currently used by the @db object.
 */
const char *preludedb_get_format(preludedb_t *db)
{
	return prelude_plugin_name(db->plugin);
}



/**
 * preludedb_set_format:
 * @db: Pointer to a db object.
 * @format_name: New format to use.
 *
 * Change the current format plugin.
 *
 * Returns: 0 on success or negative value if an error occur.
 */
int preludedb_set_format(preludedb_t *db, const char *format_name)
{
	db->plugin = (preludedb_plugin_format_t *) prelude_plugin_search_by_name(format_name);
	if ( ! db->plugin )
		return -1;

	db->format_name = strdup(format_name);
	
	return db->format_name ? 0 : preludedb_error_from_errno(errno);
}



/**
 * preludedb_get_sql:
 * @db: Pointer to a db object.
 * 
 * Returns: a pointer to the underlying sql object.
 */
preludedb_sql_t *preludedb_get_sql(preludedb_t *db)
{
	return db->sql;
}



/**
 * preludedb_get_error:
 * @db: Pointer to a db object.
 * @error: Error code to build the error string from.
 * @errbuf: Buffer where the error message will be stored,
 * @size: size of this buffer must be PRELUDEDB_ERRBUF_SIZE.
 *
 * Build an error message from the error code given as argument and from
 * the sql plugin error string (if any) if the error code is db related.
 *
 * Returns: a pointer to the error string or NULL if an error occured.
 */
char *preludedb_get_error(preludedb_t *db, preludedb_error_t error, char *errbuf, size_t size)
{
	int ret;
	size_t offset;

	ret = snprintf(errbuf, size, "%s", preludedb_strerror(error));
	if ( ret < 0 || ret >= size )
		return NULL;

	offset = ret;

	if ( prelude_error_get_source(error) == PRELUDE_ERROR_SOURCE_PRELUDEDB ) {
		const char *plugin_error;

		plugin_error = preludedb_sql_get_plugin_error(db->sql);

		if ( plugin_error ) {
			ret = snprintf(errbuf + offset, size - offset, ": %s", plugin_error);
			if ( ret < 0 || ret >= size - offset )
				return NULL;
		}
	}

	return errbuf;
}



/**
 * preludedb_insert_message:
 * @db: Pointer to a db object.
 * @message: Pointer to an IDMEF message.
 *
 * Insert an IDMEF message into the database.
 * 
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_insert_message(preludedb_t *db, idmef_message_t *message)
{
	return db->plugin->insert_message(db->sql, message);
}



/**
 * preludedb_result_idents_destroy:
 * @result: Pointer to an idents result object.
 *
 * Destroy the @result object.
 */
void preludedb_result_idents_destroy(preludedb_result_idents_t *result)
{
	result->db->plugin->destroy_message_idents_resource(result->res);
	free(result);
}


/**
 * preludedb_result_idents_get_next:
 * @result: Pointer to an idents result object.
 * @ident: Pointer to an ident where the next ident will be stored.
 * 
 * Retrieve the next ident from the idents result object.
 *
 * Returns: 1 if an ident is available, 0 if there is no more idents available or
 * a negative value if an error occur.
 */
int preludedb_result_idents_get_next(preludedb_result_idents_t *result, uint64_t *ident)
{
	return result->db->plugin->get_next_message_ident(result->res, ident);
}


/**
 * preludedb_result_values_destroy:
 * @result: Pointer to a result values object.
 *
 * Destroy the @result object.
 */
void preludedb_result_values_destroy(preludedb_result_values_t *result)
{
	result->db->plugin->destroy_values_resource(result->res);
	free(result);
}



/**
 * preludedb_result_values_get_next:
 * @result: Pointer to a values result object.
 * @values: Pointer to a values array where the next row of values will be stored.
 * 
 * Retrieve the next values row from the values result object.
 *
 * Returns: the number of returned values, 0 if there are no values or a negative value if an
 * error occur.
 */
int preludedb_result_values_get_next(preludedb_result_values_t *result, idmef_value_t ***values)
{
	return result->db->plugin->get_next_values(result->res, result->selection, values);
}



static int
preludedb_get_message_idents(preludedb_t *db,
			     idmef_criteria_t *criteria,
			     int (*get_idents)(preludedb_sql_t *sql, idmef_criteria_t *criteria,
					       int limit, int offset,
					       preludedb_result_idents_order_t order,
					       void **res),
			     int limit, int offset,
			     preludedb_result_idents_order_t order,
			     preludedb_result_idents_t **result)
{
	int ret;

	*result = calloc(1, sizeof (**result));
	if ( ! *result )
		return preludedb_error_from_errno(errno);

	(*result)->db = db;

	ret = get_idents(db->sql, criteria, limit, offset, order, &(*result)->res);
	if ( ret <= 0 )
		free(*result);

	return ret;
}



/**
 * preludedb_get_alert_idents:
 * @db: Pointer to a db object.
 * @criteria: Pointer to an idmef criteria.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @order: Result order.
 * @result: Idents result.
 *
 * Returns: 1 if there are any results, 0 if there is no result, or negative value if an error occur.
 */
int preludedb_get_alert_idents(preludedb_t *db,
			       idmef_criteria_t *criteria, int limit, int offset,
			       preludedb_result_idents_order_t order,
			       preludedb_result_idents_t **result)
{
	return preludedb_get_message_idents(db, criteria, db->plugin->get_alert_idents, limit, offset, order, result);
}



/**
 * preludedb_get_heartbeat_idents:
 * @db: Pointer to a db object.
 * @criteria: Pointer to an idmef criteria.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @order: Result order.
 * @result: Idents result.
 *
 * Returns: 1 if there are any results, 0 if there is no result , or negative value if an error occur.
 */
int preludedb_get_heartbeat_idents(preludedb_t *db,
				   idmef_criteria_t *criteria, int limit, int offset,
				   preludedb_result_idents_order_t order,
				   preludedb_result_idents_t **result)
{
	return preludedb_get_message_idents(db, criteria, db->plugin->get_heartbeat_idents, limit, offset, order, result);
}



/**
 * preludedb_get_alert:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the alert.
 * @message: Pointer to an idmef message object where the retrieved message will be stored.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_get_alert(preludedb_t *db, uint64_t ident, idmef_message_t **message)
{
	return db->plugin->get_alert(db->sql, ident, message);
}



/**
 * preludedb_get_heartbeat:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the heartbeat.
 * @message: Pointer to an idmef message object where the retrieved message will be stored.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_get_heartbeat(preludedb_t *db, uint64_t ident, idmef_message_t **message)
{
	return db->plugin->get_heartbeat(db->sql, ident, message);
}


/**
 * preludedb_delete_alert:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the alert.
 *
 * Delete an alert.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_delete_alert(preludedb_t *db, uint64_t ident)
{
	return db->plugin->delete_alert(db->sql, ident);
}



/**
 * preludedb_delete_heartbeat:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the heartbeat.
 *
 * Delete an heartbeat.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_delete_heartbeat(preludedb_t *db, uint64_t ident)
{
	return db->plugin->delete_heartbeat(db->sql, ident);
}



/**
 * preludedb_get_values:
 * @db: Pointer to a db object.
 * @path_selection: Pointer to a path selection.
 * @criteria: Pointer to a criteria object.
 * @distinct: Get distinct or not distinct result rows.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @result: Values result.
 *
 * Returns: the number of result or a negative value if an error occured.
 */
int preludedb_get_values(preludedb_t *db,
			 preludedb_path_selection_t *path_selection,
			 idmef_criteria_t *criteria,
			 prelude_bool_t distinct,
			 int limit, int offset,
			 preludedb_result_values_t **result)
{
	int ret;

	*result = calloc(1, sizeof (**result));
	if ( ! *result )
		return preludedb_error_from_errno(errno);

	(*result)->db = db;
	(*result)->selection = path_selection;

	ret = db->plugin->get_values(db->sql, path_selection, criteria, distinct, limit, offset, &(*result)->res);
	if ( ret <= 0 )
		free(*result);

	return ret;
}
