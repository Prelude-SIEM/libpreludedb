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
#include "db-type.h"
#include "db-connection.h"
#include "plugin-format.h"

#include "idmef-db-insert.h"
#include "idmef-db-select.h"
#include "db-object.h"

#define CONFIG_FILE FORMAT_CONFIG_DIR"/classic/schema.txt"

static plugin_format_t plugin;

struct	db_ident_list
{
	int		size;
	uint64_t *      idents;
	int		next;
};

struct db_ident_list * build_ident_list(prelude_sql_table_t *);
idmef_message_t *get_message(prelude_db_connection_t *, uint64_t, const char *, idmef_selection_t *);

struct db_ident_list * build_ident_list(prelude_sql_table_t *table)
{
	struct db_ident_list * ident_list;
	prelude_sql_row_t * row;
	prelude_sql_field_t * field;
	int cnt;
	int nrows;

	nrows = prelude_sql_rows_num(table);

	ident_list = calloc(1, sizeof (*ident_list));
	ident_list->size = nrows;
	ident_list->idents = calloc(nrows, sizeof (*ident_list->idents));
	ident_list->next = 0;

	for ( cnt = 0; cnt < nrows; cnt++ ) {
		row = prelude_sql_row_fetch(table);
		field = prelude_sql_field_fetch(row, 0);
		ident_list->idents[cnt] = prelude_sql_field_value_int64(field);
	}

	return ident_list;
}


static void * classic_get_ident_list(prelude_db_connection_t * connection,
				     idmef_criterion_t * criterion)
{
	idmef_selection_t *selection;
	idmef_object_t *first, *object;
	prelude_sql_table_t *table;
	const char *first_name;
	struct db_ident_list *ident_list;

	first = idmef_criterion_get_first_object(criterion);
	if ( ! first ) {
		log(LOG_ERR, "could not get first object from selection !\n");
		return NULL;
	}
	
	first_name = idmef_object_get_name(first);
	if ( ! first_name ) {
		log(LOG_ERR, "could not get object's name\n");
		return NULL;
	}

	if ( strncmp(first_name, "alert.", sizeof ("alert.") - 1) == 0 ) {
		object = idmef_object_new("alert.ident");
		if ( ! object ) {
			log(LOG_ERR, "could not create alert.ident object\n");
			return NULL;
		}

	} else {
		object = idmef_object_new("heartbeat.ident");
		if ( ! object ) {
			log(LOG_ERR, "could not create heartbeat.ident object\n");
			return NULL;
		}
	}

	selection = idmef_selection_new();
	if ( ! selection ) {
		log(LOG_ERR, "could not create IDMEF cache!\n");
		idmef_object_destroy(object);
		return NULL;
	}

	if ( idmef_selection_add_object(selection, object) < 0 ) {
		log(LOG_ERR, "could not add object '%s' in selection !\n");
		idmef_object_destroy(object);
		idmef_selection_destroy(selection);
		return NULL;
	}

	table = idmef_db_select(connection, selection, criterion);
	if ( ! table ) {
		idmef_selection_destroy(selection);
		return NULL;
	}

	ident_list = build_ident_list(table);

	idmef_selection_destroy(selection);
	prelude_sql_table_free(table);

	return ident_list;
}


static uint64_t classic_get_next_ident(prelude_db_connection_t *connection,
				       void *res)
{
	struct db_ident_list *ident_list = res;

	if ( ! ident_list )
		return 0;

	return ((ident_list->next < ident_list->size) ?
		ident_list->idents[ident_list->next++] :
		0);
}


idmef_message_t *get_message(prelude_db_connection_t *connection,
			     uint64_t ident,
			     const char *object_name,
			     idmef_selection_t *selection)
{
	idmef_message_t *message;
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
	int nfields, field_cnt;
 	idmef_object_t *object;
	idmef_value_t *value;
	idmef_criterion_t *criterion;
	const char *char_val;
	int cnt = 0;

	/* TODO: if selection is NULL, the whole message's content should be retrieved */
	if ( ! selection )
		return NULL;

	object = idmef_object_new(object_name);
	value = idmef_value_new_uint64(ident);
	criterion = idmef_criterion_new(object, relation_equal, value);

	table = idmef_db_select(connection, selection, criterion);

	idmef_criterion_destroy(criterion);

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
	
		idmef_selection_set_object_iterator(selection);
	
		for ( field_cnt = 0; field_cnt < nfields; field_cnt++ ) {

			field = prelude_sql_field_fetch(row, field_cnt);
			char_val = prelude_sql_field_value(field);

			object = idmef_object_ref(idmef_selection_get_next_object(selection));
			value = idmef_value_new_for_object(object, char_val);
			if ( idmef_message_set(message, object, value) < 0 ) {
				prelude_sql_table_free(table);
				idmef_message_destroy(message);
				return NULL;
			}

			cnt++;
		}
	}

	prelude_sql_table_free(table);

	return message;
}



static idmef_message_t *classic_get_alert(prelude_db_connection_t *connection,
					  uint64_t ident,
					  idmef_selection_t *selection)
{
	return get_message(connection, ident, "alert.ident", selection);
}



static idmef_message_t *classic_get_heartbeat(prelude_db_connection_t *connection,
					      uint64_t ident,
					      idmef_selection_t *selection)
{
	return get_message(connection, ident, "heartbeat.ident", selection);
}



int classic_insert_idmef_message(prelude_db_connection_t *connection, const idmef_message_t *message)
{
	return idmef_db_insert(connection, message);
}



void classic_free_ident_list(prelude_db_connection_t * connection,
			    void * res)
{
	struct db_ident_list * ident_list = res;

	free(ident_list->idents);
	free(ident_list);
}



plugin_generic_t * plugin_init(int argc, char **argv)
{
	/* System wide plugin options should go in here */
        
        plugin_set_name(&plugin, "Classic");
        plugin_set_desc(&plugin, "Prelude 0.8.0 database format");

	plugin_set_get_ident_list_func(&plugin, classic_get_ident_list);
	plugin_set_free_ident_list_func(&plugin, classic_free_ident_list);
	plugin_set_get_next_ident_func(&plugin, classic_get_next_ident);
	plugin_set_get_alert_func(&plugin, classic_get_alert);
	plugin_set_get_heartbeat_func(&plugin, classic_get_heartbeat);
	plugin_set_insert_idmef_message_func(&plugin, classic_insert_idmef_message);

	db_objects_init(CONFIG_FILE);

	return (plugin_generic_t *) &plugin;
}
