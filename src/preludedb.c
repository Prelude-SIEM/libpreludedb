/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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
#include "preludedb-object-selection.h"

#include "preludedb.h"

#include "preludedb-plugin-format.h"


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
	preludedb_object_selection_t *selection;
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

		prelude_plugin_load_from_dir(FORMAT_PLUGIN_DIR, NULL, NULL);
		if ( ret < 0 )
			goto error;

		plugins_loaded = TRUE;
	}

 error: 
	pthread_mutex_unlock(&plugins_loaded_mutex);

	return ret;
}



preludedb_t *preludedb_new(preludedb_sql_t *sql, const char *format_name)
{
	preludedb_t *db;
	int ret;

	load_format_plugins_if_needed();

	db = calloc(1, sizeof (*db));
	if ( ! db )
		return NULL;

	db->sql = sql;

	ret = preludedb_sql_connect(db->sql);
	if ( ret < 0 ) {
		free(db);
		return NULL;
	}

	if ( format_name )
		ret = preludedb_format_set(db, format_name);
	else
		ret = preludedb_format_autodetect(db);

	if ( ret < 0 ) {
		if ( db->format_name )
			free(db->format_name);
		free(db);
		return NULL;
	}

	return db;
}



void preludedb_destroy(preludedb_t *db)
{
	preludedb_sql_destroy(db->sql);
	free(db->format_name);
	free(db);
}



const char *preludedb_get_format_name(preludedb_t *db)
{
	return db->format_name;
}



int preludedb_connect(preludedb_t *db)
{
	return preludedb_sql_connect(db->sql);
}



void preludedb_disconnect(preludedb_t *db)
{
	return preludedb_sql_disconnect(db->sql);
}



int preludedb_format_set(preludedb_t *db, const char *format_name)
{
	db->plugin = (preludedb_plugin_format_t *) prelude_plugin_search_by_name(format_name);
	if ( ! db->plugin )
		return -1;

	db->format_name = strdup(format_name);
	
	return db->format_name ? 0 : preludedb_error_from_errno(errno);
}



int preludedb_format_autodetect(preludedb_t *db)
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

	ret = preludedb_format_set(db, preludedb_sql_field_get_value(field));

 error:
	preludedb_sql_table_destroy(table);

	return ret;	
	
}



int preludedb_reconnect(preludedb_t *db) 
{
	return preludedb_sql_connect(db->sql);
}



preludedb_sql_t *preludedb_get_sql(preludedb_t *db)
{
	return db->sql;
}



int preludedb_get_error(preludedb_t *db, preludedb_error_t error, char **output)
{
	prelude_string_t *string;
	int ret;

	string = prelude_string_new();
	if ( ! string )
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);

	ret = prelude_string_sprintf(string, "%s", preludedb_strerror(error));
	if ( ret < 0 )
		goto error;

	if ( prelude_error_get_source(error) == PRELUDE_ERROR_SOURCE_PRELUDEDB ) {
		const char *plugin_error;

		plugin_error = preludedb_sql_get_plugin_error(db->sql);

		if ( plugin_error ) {
			ret = prelude_string_sprintf(string, ": %s", plugin_error);
			if ( ret < 0 )
				goto error;
		}
	}

	*output = prelude_string_get_string_released(string);
	prelude_string_destroy(string);

	return 0;

 error:
	prelude_string_destroy(string);

	return ret;
}



int preludedb_insert_message(preludedb_t *db, idmef_message_t *message)
{
	return db->plugin->insert_message(db->sql, message);
}



void preludedb_result_idents_destroy(preludedb_result_idents_t *result)
{
	result->db->plugin->destroy_message_idents_resource(result->res);
	free(result);
}



int preludedb_result_idents_get_next(preludedb_result_idents_t *result, uint64_t *ident)
{
	return result->db->plugin->get_next_message_ident(result->res, ident);
}



void preludedb_result_values_destroy(preludedb_result_values_t *result)
{
	result->db->plugin->destroy_values_resource(result->res);
	free(result);
}



int preludedb_result_values_get_next(preludedb_result_values_t *result, idmef_value_t ***values)
{
	return result->db->plugin->get_next_values(result->res, result->selection, values);
}



static preludedb_result_idents_t *
preludedb_get_message_idents(preludedb_t *db,
			     idmef_criteria_t *criteria,
			     int (*get_idents)(preludedb_sql_t *sql, idmef_criteria_t *criteria,
					       int limit, int offset,
					       preludedb_result_idents_order_t order,
					       void **res),
			     int limit, int offset,
			     preludedb_result_idents_order_t order)
{
	preludedb_result_idents_t *result;
	int ret;

	result = calloc(1, sizeof (*result));
	if ( ! result )
		return NULL;

	result->db = db;

	ret = get_idents(db->sql, criteria, limit, offset, order, &result->res);
	if ( ret <= 0 ) {
		free(result);
		return NULL;
	}

	return result;
}



preludedb_result_idents_t *preludedb_get_alert_idents(preludedb_t *db,
						      idmef_criteria_t *criteria, int limit, int offset,
						      preludedb_result_idents_order_t order)
{
	return preludedb_get_message_idents(db, criteria, db->plugin->get_alert_idents, limit, offset, order);
}



preludedb_result_idents_t *preludedb_get_heartbeat_idents(preludedb_t *db,
							  idmef_criteria_t *criteria, int limit, int offset,
							  preludedb_result_idents_order_t order)
{
	return preludedb_get_message_idents(db, criteria, db->plugin->get_heartbeat_idents, limit, offset, order);
}



idmef_message_t *preludedb_get_alert(preludedb_t *db, uint64_t ident)
{
	idmef_message_t *message;
	int ret;

	ret = db->plugin->get_alert(db->sql, ident, &message);

	return (ret < 0) ? NULL : message;
}



idmef_message_t *preludedb_get_heartbeat(preludedb_t *db, uint64_t ident)
{
	idmef_message_t *message;
	int ret;

	ret = db->plugin->get_heartbeat(db->sql, ident, &message);

	return (ret < 0) ? NULL : message;
}



int preludedb_delete_alert(preludedb_t *db, uint64_t ident)
{
	return db->plugin->delete_alert(db->sql, ident);
}



int preludedb_delete_heartbeat(preludedb_t *db, uint64_t ident)
{
	return db->plugin->delete_heartbeat(db->sql, ident);
}



preludedb_result_values_t *preludedb_get_values(preludedb_t *db,
						preludedb_object_selection_t *object_selection,
						idmef_criteria_t *criteria,
						int distinct,
						int limit, int offset)
{
	preludedb_result_values_t *result;
	int ret;

	result = calloc(1, sizeof (*result));
	if ( ! result )
		return NULL;

	result->db = db;
	result->selection = object_selection;

	ret = db->plugin->get_values(db->sql, object_selection, criteria, distinct, limit, offset, &result->res);
	if ( ret < 0 ) {
		free(result);
		return NULL;
	}

	return result;
}