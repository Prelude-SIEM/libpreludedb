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
#include <libprelude/plugin-common.h>
#include <libprelude/plugin-common-prv.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-uident.h"
#include "db-connection.h" 
#include "db-connection-data.h"
#include "db-interface.h"
#include "plugin-format.h"


struct prelude_db_interface {
	char *name;
	prelude_db_connection_t *db_connection;
	prelude_db_connection_data_t *connection_data;
	plugin_format_t *format;
	struct list_head filter_list;
};



struct prelude_db_alert_uident_list {
	prelude_db_interface_t *interface;
	prelude_db_alert_uident_t *uidents;
	size_t size;
	int next_uident;
};



struct prelude_db_heartbeat_uident_list {
	prelude_db_interface_t *interface;
	prelude_db_heartbeat_uident_t *uidents;
	size_t size;
	int next_uident;
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
        
        interface->format = (plugin_format_t *) plugin_search_by_name(format);
        if ( ! interface->format ) {
                log(LOG_ERR, "couldn't find format plugin '%s'.\n", format);
                free(interface->name);
                free(interface);
                return NULL;
        }

        return interface;
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



prelude_db_alert_uident_list_t *prelude_db_interface_get_alert_uident_list(prelude_db_interface_t *interface,
                                                                           idmef_criteria_t *criteria)
{
        int size;
        prelude_db_alert_uident_t *alert_uidents;
        prelude_db_alert_uident_list_t *uident_list;
        
	if ( ! interface )
		return NULL;
		
	if ( ! interface->db_connection )
		return NULL;
		
	if ( ! interface->format )
		return NULL;
	
	if ( ! interface->format->format_get_alert_uident_list )
		return NULL;

	size = interface->format->format_get_alert_uident_list(interface->db_connection, criteria, &alert_uidents);
	if ( size <= 0 )
		return NULL;

	uident_list = malloc(sizeof(*uident_list));
        if ( ! uident_list ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
         
	uident_list->interface = interface;
	uident_list->uidents = alert_uidents;
	uident_list->size = size;
	uident_list->next_uident = 0;

	return uident_list;
}



prelude_db_heartbeat_uident_list_t *prelude_db_interface_get_heartbeat_uident_list(prelude_db_interface_t *interface,
										   idmef_criteria_t *criteria)
{
        int size;
	prelude_db_heartbeat_uident_list_t *uident_list;
	prelude_db_heartbeat_uident_t *heartbeat_uidents;
	
	if ( ! interface )
		return NULL;
		
	if ( ! interface->db_connection )
		return NULL;
		
	if ( ! interface->format )
		return NULL;
	
	if ( ! interface->format->format_get_heartbeat_uident_list )
		return NULL;
        
	size = interface->format->format_get_heartbeat_uident_list(interface->db_connection, criteria, &heartbeat_uidents);
	if ( size <= 0 )
		return NULL;

	uident_list = malloc(sizeof (*uident_list));
        if ( ! uident_list ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
        
	uident_list->interface = interface;
	uident_list->uidents = heartbeat_uidents;
	uident_list->size = size;
	uident_list->next_uident = 0;

	return uident_list;
}



void prelude_db_interface_free_alert_uident_list(prelude_db_alert_uident_list_t *uident_list)
{
	if ( ! uident_list )
		return;

	free(uident_list->uidents);
	free(uident_list);
}



void prelude_db_interface_free_heartbeat_uident_list(prelude_db_heartbeat_uident_list_t *uident_list)
{
	if ( ! uident_list )
		return;

	free(uident_list->uidents);
	free(uident_list);
}



prelude_db_alert_uident_t *prelude_db_interface_get_next_alert_uident(prelude_db_alert_uident_list_t *uident_list)
{
	prelude_db_alert_uident_t *uident;
	
	if ( ! uident_list )
		return NULL;

	return ((uident_list->next_uident < uident_list->size) ?
		&uident_list->uidents[uident_list->next_uident++] :
		NULL);
}



prelude_db_heartbeat_uident_t *prelude_db_interface_get_next_heartbeat_uident(prelude_db_heartbeat_uident_list_t *uident_list)
{
	if ( ! uident_list )
		return NULL;

	return ((uident_list->next_uident < uident_list->size) ? 
		&uident_list->uidents[uident_list->next_uident++] :
		NULL);
}



idmef_message_t *prelude_db_interface_get_alert(prelude_db_interface_t *interface,
						prelude_db_alert_uident_t *uident,
						idmef_selection_t *selection)
{
	if ( ! interface )
		return NULL;

	if ( ! interface->format->format_get_alert )
		return NULL;

	return interface->format->format_get_alert(interface->db_connection, uident, selection);
}



idmef_message_t *prelude_db_interface_get_heartbeat(prelude_db_interface_t *interface,
						    prelude_db_heartbeat_uident_t *uident,
						    idmef_selection_t *selection)
{
	if ( ! interface )
		return NULL;

	if ( ! interface->format->format_get_heartbeat )
		return NULL;

	return interface->format->format_get_heartbeat(interface->db_connection, uident, selection);
}



int prelude_db_interface_delete_alert(prelude_db_interface_t *interface, prelude_db_alert_uident_t *uident)
{
	if ( ! interface )
		return -1;

	if ( ! interface->format->format_delete_alert )
		return -2;

	return interface->format->format_delete_alert(interface->db_connection, uident);
}



int prelude_db_interface_delete_heartbeat(prelude_db_interface_t *interface, prelude_db_heartbeat_uident_t *uident)
{
	if ( ! interface )
		return -1;

	if ( ! interface->format->format_delete_heartbeat )
		return -2;

	return interface->format->format_delete_heartbeat(interface->db_connection, uident);
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
	prelude_sql_connection_t * sql;
	
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
	prelude_sql_connection_t * sql;
	
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
					 idmef_selection_t *selection,
					 idmef_criteria_t *criteria,
					 int distinct,
					 int limit)
{
	if ( ! interface || 
	     ! interface->format || 
	     ! interface->format->format_select_values)
		return NULL;
	
	return interface->format->format_select_values
                (interface->db_connection, selection, criteria, distinct, limit);
}




idmef_object_value_list_t *prelude_db_interface_get_values(prelude_db_interface_t *interface,
							   void *data,
							   idmef_selection_t *selection)
{
	if ( ! interface || 
	     ! interface->format || 
	     ! interface->format->format_get_values)
		return NULL;
	
	return interface->format->format_get_values(interface->db_connection,
						    data, selection);
}




int prelude_db_interface_destroy(prelude_db_interface_t *interface)
{
	if ( ! interface )
		return -1;
	
	if ( interface->db_connection )
		prelude_db_interface_disconnect(interface);
	
	prelude_db_connection_data_destroy(interface->connection_data);
	
	/* FIXME: when filters are implemented, destroy interface->filter_list here */
	
	if ( interface->name ) 
		free(interface->name);
		
	free(interface);
	
	return 0;
}
