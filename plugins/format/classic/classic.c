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
#include <libprelude/plugin-common.h>
#include <libprelude/plugin-common-prv.h>

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

#define CONFIG_FILE FORMAT_CONFIG_DIR"/classic/schema.txt"

static plugin_format_t plugin;


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



static prelude_db_object_selection_t *object_list_to_selection(idmef_object_list_t *object_list)
{
	prelude_db_object_selection_t *selection;
	prelude_db_selected_object_t *selected;
	idmef_object_t *object;

	selection = prelude_db_object_selection_new();
	if ( ! selection )
		return NULL;

	object = NULL;
	while ( (object = idmef_object_list_get_next(object_list, object)) ) {

		selected = prelude_db_selected_object_new(idmef_object_ref(object), 0);
		if ( ! selected ) {
			idmef_object_destroy(object); /* destroy the new reference to the object */
			prelude_db_object_selection_destroy(selection);
			return NULL;
		}

		prelude_db_object_selection_add(selection, selected);
	}

	return selection;
}


/*
 * FIXME: cleanup
 */


static idmef_message_t *get_message(prelude_db_connection_t *connection,
				    uint64_t ident,
				    const char *object_name,
				    idmef_object_list_t *object_list)
{
	idmef_message_t *message;
	prelude_db_object_selection_t *selection;
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
	int nfields, field_cnt;
	idmef_object_t *object;
	idmef_value_t *value;
	idmef_criterion_t *criterion;
	idmef_criteria_t *criteria;
	const char *char_val;
	int cnt = 0;

	object = idmef_object_new_fast(object_name);
	if ( ! object )
		return NULL;
	
	value = idmef_value_new_uint64(ident);
	if ( ! value ) {
		idmef_object_destroy(object);
		return NULL;
	}

	criterion = idmef_criterion_new(object, relation_equal, value);
	if ( ! criterion ) {
		idmef_object_destroy(object);
		idmef_value_destroy(value);
		return NULL;
	}

	criteria = idmef_criteria_new();
	if ( ! criteria ) {
		idmef_criterion_destroy(criterion);
		return NULL;
	}

	if ( idmef_criteria_add_criterion(criteria, criterion, operator_and) < 0 ) {
		idmef_criterion_destroy(criterion);
		idmef_criteria_destroy(criteria);
		return NULL;
	}

	selection = object_list_to_selection(object_list);
	if ( ! selection ) {
		idmef_criteria_destroy(criteria);
		return NULL;
	}

	table = idmef_db_select(connection, selection, criteria, 
	                        NO_DISTINCT, NO_LIMIT, NO_OFFSET, AS_MESSAGES);

	prelude_db_object_selection_destroy(selection);

	idmef_criteria_destroy(criteria);

	if ( ! table )
		return NULL;

	message = idmef_message_new();
	if ( ! message ) {
		log(LOG_ERR, "could not create new message !\n");
		return NULL;
	}

	idmef_message_enable_cache(message);

	nfields = prelude_sql_fields_num(table);

	while ( (row = prelude_sql_row_fetch(table)) ) {

#ifdef DEBUG
		log(LOG_INFO, "+ row %d\n", cnt);

#endif

		object = NULL;

		for ( field_cnt = 0; field_cnt < nfields; field_cnt++ ) {

			/* FIXME: handle enumeration The Right Way(tm) */
			object = idmef_object_list_get_next(object_list, object);
			(void) idmef_object_ref(object); /* increment refcount */

#ifdef DEBUG
			log(LOG_INFO, " * object: %s\n", idmef_object_get_name(object));
#endif

			/*
			 * don't set unambigous objects more than once,
			 * the value is always the same anyhow
			 * (plus this is not allowed by idmef_object_set()).
			 * Otherwise we'd end up having more then one value
			 * for object that can only have single incarnation,
			 * what would be clearly incorrect.
			 */
			if ( cnt && ! idmef_object_is_ambiguous(object) )
				continue;

			field = prelude_sql_field_fetch(row, field_cnt);
			if ( ! field )
				continue;

			char_val = prelude_sql_field_value(field);

#ifdef DEBUG
			log(LOG_INFO, " * read value: %s\n", char_val);
#endif

			value = idmef_value_new_for_object(object, char_val);
			if ( ! value )
				log(LOG_ERR, "could not create container!\n");

			if ( idmef_message_set(message, object, value) < 0 ) {
				log(LOG_INFO, "idmef_message_set() failed\n");

				prelude_sql_table_free(table);
				idmef_message_destroy(message);
				return NULL;
			}

		}

		cnt++;

	}

	prelude_sql_table_free(table);

	return message;
}



static idmef_message_t *classic_get_message(prelude_db_connection_t *connection,
					    prelude_db_message_ident_t *message_ident,
					    idmef_object_list_t *object_list,
					    idmef_message_t *(*get_alert_func)(prelude_db_connection_t *connection,
									       uint64_t ident),
					    const char *object_name)
{
	idmef_object_t *object;
	int lists, n;
	uint64_t ident;

	ident = prelude_db_message_ident_get_ident(message_ident);

	if ( ! object_list )
		return get_alert(connection, ident);

	n = 0;

	object = NULL;
	while ( (object = idmef_object_list_get_next(object_list, object)) ) {
		lists = idmef_object_has_lists(object);
		if ( lists > 1 )
			return get_alert_func(connection, ident);

		if ( lists == 1 && ++n == 2 )
			return get_alert_func(connection, ident);
	}

	return get_message(connection, ident, object_name, object_list);
}



static idmef_message_t *classic_get_alert(prelude_db_connection_t *connection,
					  prelude_db_message_ident_t *ident,
					  idmef_object_list_t *object_list)
{
	return classic_get_message(connection, ident, object_list, get_alert, "alert.ident");
}



static idmef_message_t *classic_get_heartbeat(prelude_db_connection_t *connection,
					      prelude_db_message_ident_t *ident,
					      idmef_object_list_t *object_list)
{
	return classic_get_message(connection, ident, object_list, get_heartbeat, "heartbeat.ident");
}



static int classic_insert_idmef_message(prelude_db_connection_t *connection, const idmef_message_t *message)
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
			const char *char_val;

			char_val = prelude_sql_field_value(field);
			if ( ! char_val )
				goto error;

			value = idmef_value_new_for_object(object, char_val);
			if ( ! value ) {
				log(LOG_ERR, "could not build idmef value from object\n");
				goto error;
			}
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



plugin_generic_t *plugin_init(int argc, char **argv)
{
	/* System wide plugin options should go in here */
        
        plugin_set_name(&plugin, "Classic");
        plugin_set_desc(&plugin, "Prelude 0.8.0 database format");

	plugin_set_get_alert_ident_list_func(&plugin, classic_get_alert_ident_list);
	plugin_set_get_heartbeat_ident_list_func(&plugin, classic_get_heartbeat_ident_list);
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

	return (plugin_generic_t *) &plugin;
}
