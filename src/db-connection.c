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
/* #include "plugin-filter.h" */


/* FIXME: add interface deletion */

/* FIXME: add filters */

/* FIXME: better error codes management? */

struct list_head db_interfaces;

prelude_db_interface_t *prelude_db_connect_sql(const char *name, const char *dbformat, 
                                const char *dbtype, const char *dbhost, 
				const char *dbport, const char *dbname, 
				const char *dbuser, const char *dbpass)
{
	sql_connection_t *sql_conn;
	prelude_db_interface_t *interface;
	plugin_format_t *format;
	
	sql_conn = sql_connect(dbtype, dbhost, dbport, dbname, dbuser, dbpass);
	if (!sql_conn) {
		log(LOG_ERR, "%s on %s: database connection failed\n", dbname, dbhost);
		return NULL;
	}
	
	format = (plugin_format_t *) plugin_search_by_name(dbformat);
	if (!format) {
		log(LOG_ERR, "%s: format plugin not found\n");
		return NULL;
	}
	
	interface = (prelude_db_interface_t *) calloc(1, sizeof(prelude_db_interface_t));
	if (!interface) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}
	
	interface->name = strdup(name);
	interface->db_connection.type = sql;
	interface->db_connection.connection.sql = sql_conn;
	interface->format = format;
	interface->active = 1;
	interface->connected = 1;
	INIT_LIST_HEAD(&interface->filter_list);
	
	list_add(&interface->list, &db_interfaces);

	log(LOG_INFO, "- DB interface %s connected to database %s on %s format %s\n", 
		name, dbname, dbhost, dbformat);
	
	return interface;
}

int prelude_db_interface_activate(prelude_db_interface_t *interface)
{
	if (!interface)
		return -1; 
		
	if (interface->active)
		return -2;
		
	interface->active = 1;
	
	return 0;
}


int prelude_db_interface_deactivate(prelude_db_interface_t *interface)
{
	if (!interface)
		return -1; 
		
	if (!interface->active)
		return -2;
		
	interface->active = 0;
	
	return 0;
}


/*
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
*/

int prelude_db_interface_write_idmef_message(prelude_db_interface_t *interface, idmef_message_t *msg)
{
	printf("db_interface_write_idmef_message()\n");

	if (!interface)
		return -1;
		
	if (!msg)
		return -2;
		
	if (!interface->active)
		return -3;
		
	if (!interface->connected)
		return -4;
		
	return interface->format->format_write(&interface->db_connection, msg);
}

int prelude_db_interface_disconnect(prelude_db_interface_t *interface)
{
	int ret;

	if (!interface)
		return -1;
	
	if (!interface->connected)
		return -2;

	ret = prelude_db_interface_deactivate(interface);
	if (ret < 0)
		return -3;
	
	switch (interface->db_connection.type) {
		case sql: sql_close(interface->db_connection.connection.sql);
			  return 0;
			  
		default:  log(LOG_ERR, "not supported connection type %d\n", interface->db_connection.type);
			  return -4;
	}
		
	return 0;
}

int prelude_db_interface_destroy(prelude_db_interface_t *interface)
{
	if (!interface)
		return -1;
	
	if (interface->active)
		db_interface_deactivate(interface);

	if (interface->connected)
		db_interface_disconnect(interface);
	
	/* FIXME: when filters are implemented, destroy interface->filter_list here */
	
	if (interface->name) 
		free(interface->name);
		
	free(interface);
	
	return 0;
}

int prelude_db_init_interfaces(void)
{
	prelude_db_interface_t *interface;
	int ret;
	
	INIT_LIST_HEAD(&db_interfaces);

	/* FIXME: read this from config file */
	interface = prelude_db_connect_sql("iface1", "classic", "pgsql", "localhost", NULL, "prelude", "prelude", "prelude");
	if (!interface) {
		log(LOG_ERR, "db connection failed\n");
		return -1;
	}
	
	return 0;
}

int prelude_db_shutdown_interfaces(void)
{
	int ret, ret2;
	struct list_head *tmp, *n;
	prelude_db_interface_t *interface;

	ret2 = 0;
	list_for_each_safe(tmp, n, &db_interfaces) {
		interface = list_entry(tmp, prelude_db_interface_t, list);
		ret = prelude_db_interface_disconnect(interface);
		if (ret < 0) {
			log(LOG_ERR, "could not disconnect interface %s\n", 
				interface->name);
			ret2--;
		}
		
		ret = prelude_db_interface_destroy(interface);
		if (ret < 0) {
			log(LOG_ERR, "could not destroy interface %s\n", 
				interface->name);
			ret2--;
		}
	}
	
	return ret2;
}

int prelude_db_write_idmef_message(idmef_message_t *msg)
{
	struct list_head *tmp;
	prelude_db_interface_t *interface;
	
	list_for_each(tmp, &db_interfaces) {
		interface = list_entry(tmp, prelude_db_interface_t, list);
		printf("writing message on DB interface %s\n", interface->name);
		prelude_db_interface_write_idmef_message(interface, msg);
	}
	
	return 0;
	
}

