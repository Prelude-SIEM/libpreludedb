/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>
#include <libprelude/idmef-tree.h>
#include <libprelude/plugin-common.h>
#include <libprelude/plugin-common-prv.h>

#include "sql-table.h"
#include "plugin-sql.h"
#include "db-connection.h"
#include "plugin-format.h"
#include "param-string.h"



prelude_db_interface_t *prelude_db_interface_new(void) 
{
        prelude_db_interface_t *interface;

        interface = calloc(1, sizeof(*interface));
        if ( ! interface ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
        
	INIT_LIST_HEAD(&interface->filter_list);
        
        return interface;
}



int prelude_db_interface_set_name(prelude_db_interface_t *db, const char *name) 
{
        if ( ! name )
                return -1;
        
        return (db->name = strdup(name)) ? 0 : -1;
}



int prelude_db_interface_set_type(prelude_db_interface_t *db, const char *type) 
{
        if ( ! type )
                return -1;
        
        return (db->type = strdup(type)) ? 0 : -1;
}



int prelude_db_interface_set_format(prelude_db_interface_t *db, const char *format) 
{
        if ( ! format )
                return -1;
        
        db->format = (plugin_format_t *) plugin_search_by_name(format);
        if ( ! db->format ) {
                log(LOG_ERR, "couldn't find format plugin '%s'.\n", format);
                return -1;
        }

        return 0;
}



int prelude_db_interfave_set_host(prelude_db_interface_t *db, const char *host) 
{
        if ( ! host )
                return -1;
        
        return (db->host = strdup(host)) ? 0 : -1;
}



int prelude_db_interface_set_port(prelude_db_interface_t *db, const char *port) 
{
        if ( ! port )
                return -1;
        
        return (db->port = strdup(port)) ? 0 : -1;
}



int prelude_db_interface_set_user(prelude_db_interface_t *db, const char *user) 
{
        if ( ! user )
                return -1;
        
        return (db->user = strdup(user)) ? 0 : -1;
}



int prelude_db_interface_set_pass(prelude_db_interface_t *db, const char *pass)
{
        if ( ! pass )
                return -1;
        
        return (db->pass = strdup(pass)) ? 0 : -1;
}



int prelude_db_interface_connect(prelude_db_interface_t *db) 
{
        sql_connection_t *cnx;
        
        if ( ! db->type ) {
                log(LOG_ERR, "database type not specified.\n");
                return -1;
        }

        if ( ! db->format ) {
                log(LOG_ERR, "database format not specified.\n");
                return -1;
        }

        cnx = sql_connect(db->type, db->host, db->port, db->name, db->user, db->pass);
        if ( ! cnx ) {
                log(LOG_ERR, "%s on %s: database connection failed.\n", db->name, db->host);
                return -1;
        }

        db->active = 1;
	db->connected = 1;
        db->db_connection.type = sql;
	db->db_connection.connection.sql = cnx;
        
	log(LOG_INFO, "- DB interface %p connected to database %s on %s format %s\n", 
            db, db->name, db->host, db->format->name);
        
        return 0;
}



int prelude_db_interface_activate(prelude_db_interface_t *interface)
{
	if ( ! interface )
		return -1; 
		
	if ( interface->active )
		return -2;
		
	interface->active = 1;
	
	return 0;
}


int prelude_db_interface_deactivate(prelude_db_interface_t *interface)
{
	if ( ! interface )
		return -1; 
		
	if ( ! interface->active )
		return -2;
		
	interface->active = 0;
	
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



int prelude_db_interface_write_idmef_message(prelude_db_interface_t *interface, const idmef_message_t *msg)
{
	if ( ! interface )
		return -1;
		
	if ( ! msg )
		return -2;
		
	if ( ! interface->active )
		return -3;
		
	if ( ! interface->connected )
		return -4;
		
	return interface->format->format_write(&interface->db_connection, msg);
}



int prelude_db_interface_disconnect(prelude_db_interface_t *interface)
{
	int ret;

	if ( ! interface )
		return -1;
	
	if ( ! interface->connected )
		return -2;

	ret = prelude_db_interface_deactivate(interface);
	if ( ret < 0 )
		return -3;
	
	switch (interface->db_connection.type) {
            
        case sql:
                sql_close(interface->db_connection.connection.sql);
                return 0;
                
        default:
                log(LOG_ERR, "not supported connection type %d\n", interface->db_connection.type);
                return -4;
	}
		
	return 0;
}



int prelude_db_interface_destroy(prelude_db_interface_t *interface)
{
	if ( ! interface )
		return -1;
	
	if ( interface->active )
		prelude_db_interface_deactivate(interface);

	if ( interface->connected )
		prelude_db_interface_disconnect(interface);
	
	/* FIXME: when filters are implemented, destroy interface->filter_list here */
	
	if ( interface->name ) 
		free(interface->name);
		
	free(interface);
	
	return 0;
}





