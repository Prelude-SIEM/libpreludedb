/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
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
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include <libprelude/prelude-log.h>
#include <libprelude/prelude-io.h>
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-getopt.h>
#include <libprelude/idmef.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-message-ident.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object-selection.h"
#include "plugin-format.h"

#include "idmef-db-insert.h"
#include "idmef-db-select.h"
#include "idmef-db-get.h"
#include "idmef-db-delete.h"
#include "db-object.h"


prelude_plugin_generic_t *classic_LTX_prelude_plugin_init(void);


#define CONFIG_FILE FORMAT_CONFIG_DIR"/classic/schema.txt"

static plugin_format_t plugin;


static int get_ident(prelude_db_connection_t *connection, prelude_db_message_ident_t *ident,
		     const char *table_name, const char *field_name, uint64_t *result)
{
	prelude_sql_connection_t *sql;
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;

	sql = prelude_db_connection_get_sql(connection);
	if ( ! sql )
		return -1;

	table = prelude_sql_query(sql, "SELECT ident FROM %s WHERE analyzerid = %llu AND %s = %llu;",
				  table_name,
				  prelude_db_message_ident_get_analyzerid(ident),
				  field_name,
				  prelude_db_message_ident_get_ident(ident));
	if ( ! table )
		return prelude_sql_errno(sql) ? -1 : 0;

	row = prelude_sql_row_fetch(table);
	if ( ! row )
		goto error;

	field = prelude_sql_field_fetch(row, 0);
	if ( ! field )
		goto error;

	*result = prelude_sql_field_value_uint64(field);

	return 1;
	
 error:
	prelude_sql_table_free(table);
	return -1;
}



static int get_alert_ident(prelude_db_connection_t *connection, prelude_db_message_ident_t *ident, uint64_t *result)
{
	return get_ident(connection, ident, "Prelude_Alert", "alert_ident", result);
}



static int get_heartbeat_ident(prelude_db_connection_t *connection, prelude_db_message_ident_t *ident, uint64_t *result)
{
	return get_ident(connection, ident, "Prelude_Heartbeat", "heartbeat_ident", result);
}



static prelude_sql_table_t *get_ident_list(prelude_db_connection_t *connection,
					   idmef_criteria_t *criteria,
					   int limit, int offset,
					   const char *object_prefix)
{
	prelude_db_object_selection_t *selection;
	prelude_db_selected_object_t *selected;
	idmef_object_t *alert_ident; /* alert or heartbeat ident actually */
	idmef_object_t *analyzerid;
	prelude_sql_table_t *table;

	selection = prelude_db_object_selection_new();
	if ( ! selection ) {
		log(LOG_ERR, "could not create object selection.\n");
		return NULL;
	}

	alert_ident = idmef_object_new("%s.ident", object_prefix);
	if ( ! alert_ident ) {
		log(LOG_ERR, "could not create %s.ident object.\n", object_prefix);
		prelude_db_object_selection_destroy(selection);
		return NULL;
	}

	selected = prelude_db_selected_object_new(alert_ident, 0);
	if ( ! selected ) {
		prelude_db_object_selection_destroy(selection);
		idmef_object_destroy(alert_ident);
		return NULL;
	}

	prelude_db_object_selection_add(selection, selected);

	analyzerid = idmef_object_new("%s.analyzer.analyzerid", object_prefix);
	if ( ! analyzerid ) {
		log(LOG_ERR, "could not create %s.analyzer.analyzerid object\n", object_prefix);
		prelude_db_object_selection_destroy(selection);
		return NULL;
	}

	selected = prelude_db_selected_object_new(analyzerid, 0);
	if ( ! selected ) {
		prelude_db_object_selection_destroy(selection);
		idmef_object_destroy(analyzerid);
		return NULL;
	}

	prelude_db_object_selection_add(selection, selected);

	table = idmef_db_select(connection, selection, criteria, 
	                        NO_DISTINCT, limit, offset, AS_MESSAGES);

	prelude_db_object_selection_destroy(selection);

	return table;
}



static void *classic_get_alert_ident_list(prelude_db_connection_t *connection,
					  idmef_criteria_t *criteria,
					  int limit, int offset)
{
	return get_ident_list(connection, criteria, limit, offset, "alert");
}



static void *classic_get_heartbeat_ident_list(prelude_db_connection_t *connection,
					      idmef_criteria_t *criteria,
					      int limit, int offset)
{
	return get_ident_list(connection, criteria, limit, offset, "heartbeat");
}



static unsigned int get_message_ident_list_len(prelude_db_connection_t *connection,
					       void *res)
{
	prelude_sql_table_t *table = res;

	return prelude_sql_rows_num(table);
}


static unsigned int classic_get_alert_ident_list_len(prelude_db_connection_t *connection,
						     void *res)
{
	return get_message_ident_list_len(connection, res);
}



static unsigned int classic_get_heartbeat_ident_list_len(prelude_db_connection_t *connection,
							 void *res)
{
	return get_message_ident_list_len(connection, res);
}



static prelude_db_message_ident_t *classic_get_next_message_ident(prelude_db_connection_t *connection,
								  void *res)
{
	prelude_sql_table_t *table = res;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
	uint64_t ident;
	uint64_t analyzerid;

	row = prelude_sql_row_fetch(table);
	if ( ! row )
		return NULL;

	field = prelude_sql_field_fetch(row, 0);
	if ( ! field ) {
		log(LOG_ERR, "could not retrieve ident field from row.\n");
		return NULL;
	}

	ident = prelude_sql_field_value_int64(field);

	field = prelude_sql_field_fetch(row, 1);
	if ( ! field ) {
		log(LOG_ERR, "could not retrieve analyzerid field from row.\n");
		return NULL;
	}

	analyzerid = prelude_sql_field_value_int64(field);

	return prelude_db_message_ident_new(analyzerid, ident);
}



static void classic_message_ident_list_destroy(prelude_db_connection_t *connection, void *res)
{
	prelude_sql_table_free(res);
}


static idmef_message_t *classic_get_alert(prelude_db_connection_t *connection,
					  prelude_db_message_ident_t *message_ident)
{
	uint64_t ident;

	if ( get_alert_ident(connection, message_ident, &ident) <= 0 )
		return NULL;

	return get_alert(connection, message_ident, ident);
}



static idmef_message_t *classic_get_heartbeat(prelude_db_connection_t *connection,
					      prelude_db_message_ident_t *message_ident)
{
	uint64_t ident;

	if ( get_heartbeat_ident(connection, message_ident, &ident) <= 0 )
		return NULL;

	return get_heartbeat(connection, message_ident, ident);
}



static int classic_delete_alert(prelude_db_connection_t *connection,
				prelude_db_message_ident_t *message_ident)
{
	uint64_t ident;
	int ret;

	ret = get_alert_ident(connection, message_ident, &ident);
	if ( ret <= 0 )
		return ret;

	return delete_alert(connection, ident);
}



static int classic_delete_heartbeat(prelude_db_connection_t *connection,
				    prelude_db_message_ident_t *message_ident)
{
	uint64_t ident;
	int ret;

	ret = get_heartbeat_ident(connection, message_ident, &ident);
	if ( ret <= 0 )
		return ret;

	return delete_heartbeat(connection, ident);
}



static int classic_insert_idmef_message(prelude_db_connection_t *connection, idmef_message_t *message)
{
	return idmef_db_insert(connection, message);
}



static void *classic_select_values(prelude_db_connection_t *connection,
			           prelude_db_object_selection_t *selection, 
				   idmef_criteria_t *criteria,
				   int distinct,
				   int limit, int offset)
{
	return idmef_db_select(connection, selection, criteria, 
			       distinct, limit, offset, AS_VALUES);
}



static idmef_value_t *sql_field_to_idmef_value(idmef_object_t *object,
					       prelude_sql_field_t *field)
{
	const char *char_val;
	idmef_value_t *value;

	char_val = prelude_sql_field_value(field);

#ifdef DEBUG
	log(LOG_INFO, " * read value: %s\n", char_val);
#endif

	switch ( idmef_object_get_value_type(object) ) {
	case IDMEF_VALUE_TYPE_TIME: {
		idmef_time_t *time;

		time = idmef_time_new_db_timestamp(char_val);
		if ( ! time )
			return NULL;

		value = idmef_value_new_time(time);
		if ( ! value )
			idmef_time_destroy(time);

		break;
	}
	default:
		value = idmef_value_new_for_object(object, char_val);
	}

	if ( ! value )
		log(LOG_ERR, "could not create container!\n");

	return value;	
}



/*
 * FIXME: cleanup
 */


static idmef_object_value_list_t *classic_get_values(prelude_db_connection_t *connection,
						     void *data,
						     prelude_db_object_selection_t *selection)
{
	prelude_sql_table_t *table = data;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
	int field_cnt, nfields;
	idmef_object_value_list_t *objval_list = NULL;
	idmef_object_value_t *objval;
	prelude_db_selected_object_t *selected;
	idmef_object_t *object;
	idmef_value_t *value;
	int flags;
	int cnt;

	if ( ! table )
		return NULL;

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		prelude_sql_table_free(table);
		return NULL;
	}

	nfields = prelude_sql_fields_num(table);	

	selected = NULL;

	cnt = 0;
	for ( field_cnt = 0; field_cnt < nfields; field_cnt++ ) {

		selected = prelude_db_object_selection_get_next(selection, selected);
		object = prelude_db_selected_object_get_object(selected);
		flags = prelude_db_selected_object_get_flags(selected);

		field = prelude_sql_field_fetch(row, field_cnt);
		if ( ! field )
			continue;

		if ( cnt == 0 ) {
			objval_list = idmef_object_value_list_new();
			if ( ! objval_list )
				return NULL;
		}

		if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT ) {
			value = prelude_sql_field_value_idmef(field);
			if ( ! value ) {
				log(LOG_ERR, "could not get idmef value from sql field\n");
				goto error;
			}

		} else {
			value = sql_field_to_idmef_value(object, field);
			if ( ! value )
				goto error;
		}

		objval = idmef_object_value_new(idmef_object_ref(object), value);
		if ( ! objval ) {
			idmef_object_destroy(object); /* destroy the new created reference */
			idmef_value_destroy(value);
			goto error;
		}

		if ( idmef_object_value_list_add(objval_list, objval) < 0 ) {
			idmef_object_value_destroy(objval);
			log(LOG_ERR, "could not add to value list\n");
			goto error;
		}

		cnt++;
	}

	if ( cnt == 0 )
		return classic_get_values(connection, table, selection);

	return objval_list;

error:
	idmef_object_value_list_destroy(objval_list);
	prelude_sql_table_free(table);

	return NULL;

}



prelude_plugin_generic_t *classic_LTX_prelude_plugin_init(void)
{
	/* System wide plugin options should go in here */
        
        prelude_plugin_set_name(&plugin, "Classic");
        prelude_plugin_set_desc(&plugin, "Prelude 0.8.0 database format");

	plugin_set_get_alert_ident_list_func(&plugin, classic_get_alert_ident_list);
	plugin_set_get_heartbeat_ident_list_func(&plugin, classic_get_heartbeat_ident_list);
	plugin_set_get_alert_ident_list_len_func(&plugin, classic_get_alert_ident_list_len);
	plugin_set_get_heartbeat_ident_list_len_func(&plugin, classic_get_heartbeat_ident_list_len);
	plugin_set_get_next_alert_ident_func(&plugin, classic_get_next_message_ident);
	plugin_set_get_next_heartbeat_ident_func(&plugin, classic_get_next_message_ident);
	plugin_set_alert_ident_list_destroy_func(&plugin, classic_message_ident_list_destroy);
	plugin_set_heartbeat_ident_list_destroy_func(&plugin, classic_message_ident_list_destroy);
	plugin_set_get_alert_func(&plugin, classic_get_alert);
	plugin_set_get_heartbeat_func(&plugin, classic_get_heartbeat);
	plugin_set_delete_alert_func(&plugin, classic_delete_alert);
	plugin_set_delete_heartbeat_func(&plugin, classic_delete_heartbeat);
	plugin_set_insert_idmef_message_func(&plugin, classic_insert_idmef_message);
	plugin_set_select_values_func(&plugin, classic_select_values);
	plugin_set_get_values_func(&plugin, classic_get_values);

	db_objects_init(CONFIG_FILE);

	return (void *) &plugin;
}
