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
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-message-ident.h"
#include "db-connection.h" 
#include "db-connection-data.h"
#include "db-object-selection.h"
#include "db-message-cache.h"
#include "plugin-format.h"

#include "db-interface.h"



struct prelude_db_interface {
	char *name;
	prelude_db_connection_t *db_connection;
	prelude_db_connection_data_t *connection_data;
	db_message_cache_t *cache;
	plugin_format_t *format;
	struct list_head filter_list;
};



struct prelude_db_message_ident_list {
	prelude_db_interface_t *interface;
	void *res;
};



prelude_db_interface_t *prelude_db_interface_new(const char *name,
						 const char *format,
						 prelude_db_connection_data_t *data) 
{
        prelude_db_interface_t *interface;

	if ( ! data )
		return NULL;
		
        interface = calloc(1, sizeof(*interface));
        if ( ! interface ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }

        interface->connection_data = data;
	INIT_LIST_HEAD(&interface->filter_list);

        interface->name = strdup(name ? name : "(unnamed)");
        if ( ! interface->name ) {
                log(LOG_ERR, "memory exhausted.\n");
                free(interface);
                return NULL;
        }
        
        if ( ! format )
                return interface;
        
        interface->format = (plugin_format_t *) prelude_plugin_search_by_name(format);
        if ( ! interface->format ) {
                log(LOG_ERR, "couldn't find format plugin '%s'.\n", format);
                free(interface->name);
                free(interface);
                return NULL;
        }

        return interface;
}



void prelude_db_interface_destroy(prelude_db_interface_t *interface)
{
	if ( interface->db_connection )
		prelude_db_interface_disconnect(interface);
	
	prelude_db_connection_data_destroy(interface->connection_data);
	
	/* FIXME: when filters are implemented, destroy interface->filter_list here */
	
	if ( interface->name ) 
		free(interface->name);

	if ( interface->cache )
		prelude_db_interface_disable_message_cache(interface);

	free(interface);
}



int prelude_db_interface_enable_message_cache(prelude_db_interface_t *interface, const char *cache_directory)
{
	interface->cache = db_message_cache_new(cache_directory);

	return interface->cache ? 0 : -1;
}



void prelude_db_interface_disable_message_cache(prelude_db_interface_t *interface)
{
	db_message_cache_destroy(interface->cache);
	interface->cache = NULL;
}



char *prelude_db_interface_get_name(prelude_db_interface_t *db) 
{
        return db ? db->name : NULL;
}




char *prelude_db_interface_get_format(prelude_db_interface_t *db) 
{
        return db ? plugin_name(db->format) : NULL;
}




int prelude_db_interface_connect(prelude_db_interface_t *db) 
{
        int type;
        prelude_sql_connection_t *cnx;

        /*
         * YV: if more type, divide the handling of theses in individual function.
         */
        type = prelude_db_connection_data_get_type(db->connection_data);
        if ( type != prelude_db_type_sql ) {
                log(LOG_ERR, "%s: unknown database type %d\n", db->name, type);
                return -1;
        }
        
        cnx = prelude_sql_connect(prelude_db_connection_data_get(db->connection_data));
        if ( ! cnx ) {
                log(LOG_ERR, "%s: SQL connection failed.\n", db->name);
                return -1;
        }
                	
        db->db_connection = prelude_db_connection_new(prelude_db_type_sql, cnx);
        if ( ! db->db_connection ) {
                log(LOG_ERR, "%s: error creating db_connection_t object\n", db->name);
                return -1;
        }
                
        return 0;
}



#if 0
static int db_connection_add_filter(prelude_db_interface_t *conn, const char *filter)
{
	filter_plugin_t *filter;
	
	if (!conn)
		return -1;
		
	filter = (plugin_filter_t *) search_plugin_by_name(filter);
	if (!filter)
		return -2;
	
	list_add_tail(...);
}

static int db_connection_delete_filter(prelude_db_interface_t *conn, const char *filter);
#endif



int prelude_db_interface_insert_idmef_message(prelude_db_interface_t *interface, const idmef_message_t *msg)
{
	if ( ! interface )
		return -1;

	if ( ! msg )
		return -2;

	if ( ! interface->db_connection )
		return -3;

	if ( ! interface->format )
		return -4;

	if ( ! interface->format->format_insert_idmef_message )
		return -5;

	return interface->format->format_insert_idmef_message(interface->db_connection, msg);
}



static prelude_db_message_ident_list_t *
prelude_db_interface_get_message_ident_list(prelude_db_interface_t *interface,
					    idmef_criteria_t *criteria,
					    void *(*get_list_func)(prelude_db_connection_t *,
								   idmef_criteria_t *,
								   int limit, int offset),
					    int limit, int offset)
{
	void *res;
	prelude_db_message_ident_list_t *ident_list;

	if ( ! interface->db_connection )
		return NULL;

	if ( ! get_list_func )
		return NULL;
        
	ident_list = malloc(sizeof (*ident_list));
	if ( ! ident_list ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	res = get_list_func(interface->db_connection, criteria, limit, offset);
	if ( ! res ) {
		free(ident_list);
		return NULL;
	}

	ident_list->interface = interface;
	ident_list->res = res;

	return ident_list;
}



prelude_db_message_ident_list_t *prelude_db_interface_get_alert_ident_list(prelude_db_interface_t *interface,
									   idmef_criteria_t *criteria,
									   int limit, int offset)
{
	if ( ! interface->format )
		return NULL;

	return prelude_db_interface_get_message_ident_list(interface, criteria,
							   interface->format->format_get_alert_ident_list,
							   limit, offset);
}



prelude_db_message_ident_list_t *prelude_db_interface_get_heartbeat_ident_list(prelude_db_interface_t *interface,
									       idmef_criteria_t *criteria,
									       int limit, int offset)
{
	if ( ! interface->format )
		return NULL;

	return prelude_db_interface_get_message_ident_list(interface, criteria,
							   interface->format->format_get_heartbeat_ident_list,
							   limit, offset);
}



unsigned int prelude_db_interface_get_alert_ident_list_len(prelude_db_message_ident_list_t *ident_list)
{
	return ident_list->interface->format->format_get_alert_ident_list_len(ident_list->interface->db_connection,
									      ident_list->res);
}



unsigned int prelude_db_interface_get_heartbeat_ident_list_len(prelude_db_message_ident_list_t *ident_list)
{
	return ident_list->interface->format->format_get_heartbeat_ident_list_len(ident_list->interface->db_connection,
										  ident_list->res);
}



static void prelude_db_interface_message_ident_list_destroy(prelude_db_message_ident_list_t *ident_list,
							    void (*destroy_ident_list_func)(prelude_db_connection_t *connection,
											    void *res))
{
	if ( ! destroy_ident_list_func )
		return;

	destroy_ident_list_func(ident_list->interface->db_connection, ident_list->res);

	free(ident_list);
}



void prelude_db_interface_alert_ident_list_destroy(prelude_db_message_ident_list_t *ident_list)
{
	prelude_db_interface_message_ident_list_destroy(ident_list,
							ident_list->interface->format->format_alert_ident_list_destroy);
}



void prelude_db_interface_heartbeat_ident_list_destroy(prelude_db_message_ident_list_t *ident_list)
{
	prelude_db_interface_message_ident_list_destroy(ident_list,
							ident_list->interface->format->format_heartbeat_ident_list_destroy);
}



prelude_db_message_ident_t *prelude_db_interface_get_next_alert_ident(prelude_db_message_ident_list_t *ident_list)
{
	return ident_list->interface->format->format_get_next_alert_ident(ident_list->interface->db_connection,
									  ident_list->res);
}



prelude_db_message_ident_t *prelude_db_interface_get_next_heartbeat_ident(prelude_db_message_ident_list_t *ident_list)
{
	return ident_list->interface->format->format_get_next_heartbeat_ident(ident_list->interface->db_connection,
									      ident_list->res);
}



idmef_message_t *prelude_db_interface_get_alert(prelude_db_interface_t *interface,
						prelude_db_message_ident_t *ident,
						idmef_object_list_t *object_list)
{
	idmef_message_t *message;

	if ( interface->cache ) {
		message = db_message_cache_read(interface->cache, "alert", prelude_db_message_ident_get_ident(ident));
		if ( message )
			return message;			
	}

	if ( ! interface->format->format_get_alert )
		return NULL;

	message = interface->format->format_get_alert(interface->db_connection, ident, object_list);
	if ( ! message )
		return NULL;

	if ( interface->cache ) {
		if ( db_message_cache_write(interface->cache, message) < 0 )
			log(LOG_ERR, "could not write message into cache\n");
	}

	return message;	
}



idmef_message_t *prelude_db_interface_get_heartbeat(prelude_db_interface_t *interface,
						    prelude_db_message_ident_t *ident,
						    idmef_object_list_t *object_list)
{
	idmef_message_t *message;

	if ( interface->cache ) {
		message = db_message_cache_read(interface->cache, "heartbeat", prelude_db_message_ident_get_ident(ident));
		if ( message )
			return message;			
	}

	if ( ! interface->format->format_get_heartbeat )
		return NULL;

	message = interface->format->format_get_heartbeat(interface->db_connection, ident, object_list);
	if ( ! message )
		return NULL;

	if ( interface->cache ) {
		if ( db_message_cache_write(interface->cache, message) < 0 )
			log(LOG_ERR, "could not write message into cache\n");
	}

	return message;
}



int prelude_db_interface_delete_alert(prelude_db_interface_t *interface, prelude_db_message_ident_t *ident)
{
	if ( ! interface->format->format_delete_alert )
		return -1;

	return interface->format->format_delete_alert(interface->db_connection, ident);
}



int prelude_db_interface_delete_heartbeat(prelude_db_interface_t *interface, prelude_db_message_ident_t *ident)
{
	if ( ! interface->format->format_delete_heartbeat )
		return -1;

	return interface->format->format_delete_heartbeat(interface->db_connection, ident);
}



prelude_db_connection_t *prelude_db_interface_get_connection(prelude_db_interface_t *interface)
{
	return interface ? interface->db_connection : NULL;
}



prelude_db_connection_data_t *prelude_db_interface_get_connection_data(prelude_db_interface_t *interface)
{
	return interface ? interface->connection_data : NULL;
}



int prelude_db_interface_errno(prelude_db_interface_t *interface)
{
        int type;
	prelude_sql_connection_t *sql;
	
	if ( ! interface )
		return -1;

	if ( ! interface->db_connection )
		return -2;

        type = prelude_db_connection_get_type(interface->db_connection);
	if ( type != prelude_db_type_sql )
		return -3;

	sql = prelude_db_connection_get(interface->db_connection);

	return prelude_sql_errno(sql);
}



const char *prelude_db_interface_error(prelude_db_interface_t *interface)
{
	prelude_sql_connection_t *sql;
	
	if ( ! interface )
		return NULL;

	if ( ! interface->db_connection )
		return NULL;

	if ( prelude_db_connection_get_type(interface->db_connection) != prelude_db_type_sql )
		return NULL;

	sql = prelude_db_connection_get(interface->db_connection);

	return prelude_sql_error(sql);
}



int prelude_db_interface_disconnect(prelude_db_interface_t *interface)
{
        int type;
        
	if ( ! interface )
		return -1;
	
	if ( ! interface->db_connection )
		return -2;
        
        /*
         * YV: if more type, divide the handling of theses in individual function.
         */
        
        type = prelude_db_connection_get_type(interface->db_connection);
        if ( type == prelude_db_type_sql )
                prelude_sql_close(prelude_db_connection_get(interface->db_connection));
        else {
                log(LOG_ERR, "%s: not supported connection type %d\n",
                    interface->name, prelude_db_connection_get_type(interface->db_connection));
                return -4;
        }
        
	prelude_db_connection_destroy(interface->db_connection);
	interface->db_connection = NULL;
	
	return 0;
}



void *prelude_db_interface_select_values(prelude_db_interface_t *interface,
					 prelude_db_object_selection_t *object_selection,
					 idmef_criteria_t *criteria,
					 int distinct,
					 int limit, int offset)
{
	if ( ! interface || 
	     ! interface->format || 
	     ! interface->format->format_select_values)
		return NULL;
	
	return interface->format->format_select_values
                (interface->db_connection, object_selection, criteria, distinct, limit, offset);
}



idmef_object_value_list_t *prelude_db_interface_get_values(prelude_db_interface_t *interface,
							   void *data,
							   prelude_db_object_selection_t *object_selection)
{
	if ( ! interface || 
	     ! interface->format || 
	     ! interface->format->format_get_values)
		return NULL;
	
	return interface->format->format_get_values(interface->db_connection,
						    data, object_selection);
}
