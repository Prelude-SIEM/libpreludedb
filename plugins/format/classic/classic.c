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

struct	db_message_list
{
	char *		type_name;
	int		size;
	uint64_t *      idents;
	int		next;
};

struct db_message_list * build_message_list(prelude_sql_table_t * table,
					    char * type_name)
{
	struct db_message_list * message_list;
	prelude_sql_row_t * row;
	prelude_sql_field_t * field;
	int cnt;
	int nrows;

	nrows = prelude_sql_rows_num(table);

	message_list = malloc(sizeof (*message_list));
	message_list->type_name = type_name;
	message_list->size = nrows;
	message_list->idents = calloc(nrows, sizeof (*message_list->idents));
	message_list->next = 0;

	for ( cnt = 0; cnt < nrows; cnt++ ) {
		row = prelude_sql_row_fetch(table);
		field = prelude_sql_field_fetch(row, 0);
		message_list->idents[cnt] = prelude_sql_field_value_int64(field);
	}

	return message_list;
}


static void * format_get_message_list(prelude_db_connection_t * connection,
				      idmef_cache_t * cache,
				      idmef_criterion_t * criterion)
{
	idmef_cache_t *ident_cache;
	idmef_cached_object_t *ident_cached;	
	idmef_object_t *first, *object;
	prelude_sql_table_t * table;
	char *first_name, *end;
	char buf[128];
	struct db_message_list * message_list;
	int ret;
	
	/* 
	 * STAGE 1: find alert.ident or heartbeat.ident of all objects
	 *          matching criteria
	 */

	
	ret = idmef_cache_index(cache);
	if ( ret < 0 ) {
		log(LOG_ERR, "could not index cache!\n");
		return NULL;		
	}
	
	/* 
	 * check if the first object is a subclass of alert or heartbeat 
	 * and add 'alert.ident' or 'heartbeat.ident' to cache
	 */
	first = idmef_cached_object_get_object(idmef_cache_get_object(cache, 0));
	if ( ! first ) {
		log(LOG_ERR, "could not get first object from cache!\n");
		return NULL;
	}
	
	first_name = idmef_object_get_name(first);
	end = strchr(first_name, '.');
	if ( end )
		*end = '\0';
	
	buf[sizeof(buf)-1] = '\0'; /* ensure that result will be null-terminated */
	strncpy(buf, first_name, sizeof(buf)-1);
	strncat(buf, ".ident", sizeof(buf)-1);
		
	object = idmef_object_new(buf);
	if ( ! object ) {
		log(LOG_ERR, "could not create %s object!\n");
		free(first_name);
		return NULL;
	}

	ident_cache = idmef_cache_new();
	if ( ! ident_cache ) {
		log(LOG_ERR, "could not create IDMEF cache!\n");
		free(first_name);
		idmef_object_destroy(object);
		return NULL;
	}

	ident_cached = idmef_cache_register_object(ident_cache, object);
	if ( ! ident_cached ) {
		log(LOG_ERR, "could not register %s object!\n");
		free(first_name);
		idmef_object_destroy(object);
		idmef_cache_destroy(ident_cache);
		return NULL;
	}	

	ret = idmef_cache_reindex(cache);
	if ( ret < 0 ) {
		log(LOG_ERR, "could not reindex cache!\n");
		free(first_name);
		idmef_object_destroy(object);
		idmef_cache_destroy(ident_cache);
		return NULL;		
	}

	table = idmef_db_select(connection, ident_cache, criterion);
	if ( ! table ) {
		free(first_name);
		idmef_object_destroy(object);
		idmef_cache_destroy(ident_cache);
		return NULL;
	}

	message_list = build_message_list(table, first_name);

	idmef_object_destroy(object);
	idmef_cache_destroy(ident_cache);
	prelude_sql_table_free(table);

	return message_list;
}


void *	format_get_message(prelude_db_connection_t * connection,
			     idmef_cache_t * cache,
			     void * res)
{
	struct db_message_list * message_list = res;
	prelude_sql_table_t * table;
	prelude_sql_row_t * row;
	prelude_sql_field_t * field;
	int nfields, field_cnt;
 	idmef_object_t * object;
	idmef_value_t * value;
	idmef_criterion_t * criterion;
	idmef_cached_object_t * cached_object;
	uint64_t ident;
	char buf[128];
	int cnt = 0;
	int * i;

	/* FIXME: free previous stored values in cache ? */

	if ( message_list->next >= message_list->size )
		return NULL;

	idmef_cache_purge(cache);

	ident = message_list->idents[message_list->next++];

	buf[sizeof(buf)-1] = '\0'; /* ensure that result will be null-terminated */
	strncpy(buf, message_list->type_name, sizeof(buf)-1);
	strncat(buf, ".ident", sizeof(buf)-1);

	object = idmef_object_new(buf);
	value = idmef_value_new_uint64(ident);
	criterion = idmef_criterion_new(object, relation_equal, value);

	table = idmef_db_select(connection, cache, criterion);

	idmef_object_destroy(object);
	idmef_value_destroy(value);
	idmef_criterion_destroy(criterion);

	if ( ! table )
		return NULL;

	nfields = prelude_sql_fields_num(table);
		
	while ( (row = prelude_sql_row_fetch(table)) ) {
		for ( field_cnt = 0; field_cnt < nfields; field_cnt++ ) {

			field = prelude_sql_field_fetch(row, field_cnt);

			value = prelude_sql_field_value_idmef(field);
			cached_object = idmef_cache_get_object(cache, cnt);

			if ( idmef_cached_object_set(cached_object, value) < 0 ) {
				prelude_sql_table_free(table);
				return NULL;
			}

			cnt++;
		}
	}

	prelude_sql_table_free(table);

	i = malloc(sizeof (*i));
	*i = 0;

	return i;
}



idmef_value_t * format_get_message_field_value(prelude_db_connection_t * connection,
					       idmef_cache_t * cache,
					       void * message_list,
					       void * res)
{
	int * i = res;
	idmef_cached_object_t * cached_object;
	idmef_value_t * value;

	if ( *i >= idmef_cache_get_object_count(cache) )
		return NULL;

	cached_object = idmef_cache_get_object(cache, (*i)++);

	value = idmef_cached_object_get(cached_object);

	return value;
}



int format_insert_idmef_message(prelude_db_connection_t * connection, const idmef_message_t * message)
{
	return idmef_db_insert(connection, message);
}



void format_free_message_list(prelude_db_connection_t * connection,
			      idmef_cache_t * cache,
			      void * res)
{
	struct db_message_list * message_list = res;

	free(message_list->type_name);
	free(message_list->idents);
	free(message_list);
}


void format_free_message(prelude_db_connection_t * connection,
			 idmef_cache_t * cache,
			 void * message_list,
			 void * res)
{
	int * i = res;

	free(i);
}


plugin_generic_t * plugin_init(int argc, char **argv)
{
	/* System wide plugin options should go in here */
        
        plugin_set_name(&plugin, "Classic");
        plugin_set_desc(&plugin, "Prelude 0.8.0 database format");

	plugin_set_get_message_list_func(&plugin, format_get_message_list);
	plugin_set_free_message_list_func(&plugin, format_free_message_list);
	plugin_set_get_message_func(&plugin, format_get_message);
	plugin_set_free_message_func(&plugin, format_free_message);
	plugin_set_get_message_field_value_func(&plugin, format_get_message_field_value);
	plugin_set_insert_idmef_message_func(&plugin, format_insert_idmef_message);

	db_objects_init(CONFIG_FILE);

	return (plugin_generic_t *) &plugin;
}
