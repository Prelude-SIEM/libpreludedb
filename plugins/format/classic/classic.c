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

#include "idmef-db-output.h"
#include "idmef-db-read.h"
#include "db-object.h"

#define CONFIG_FILE FORMAT_CONFIG_DIR"/classic/schema.txt"

static plugin_format_t plugin;



static int format_write(prelude_db_connection_t *connection, idmef_message_t *message)
{
	return idmef_db_output(connection, message);
}


struct my_data {
	idmef_criterion_t *crit;
	idmef_object_t *obj;
};


static int add_criterion(idmef_value_t *val, void *extra)
{
	struct my_data *data = extra;
	
	return idmef_criterion_add(data->crit, 
				   idmef_criterion_new(data->obj, relation_equal, 
				   		       idmef_value_clone(val)));
}


static void* format_prepare(prelude_db_connection_t *connection, idmef_cache_t *cache,
        	            idmef_criterion_t *criterion)
{
	/*return idmef_db_prepare(connection, cache, criterion);*/
	
	idmef_cache_t *ident_cache;
	idmef_cached_object_t *ident_cached;	
	idmef_object_t *first;
	char *first_name, *end;
	struct my_data ident;
	char buf[128];
	void *handle;
	int ret;

	
	/* 
	 * STAGE 1: find alert.ident or heartbeat.ident of all objects
	 *          matching criteria
	 */

	
	ret = idmef_cache_index(cache);
	if ( ret < 0 ) {
		log(LOG_ERR, "could not index cache!\n");
		idmef_object_destroy(ident.obj);
		idmef_cache_destroy(ident_cache);
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
	
	buf[sizeof(buf)-1]='\0'; /* ensure that result will be null-terminated */
	strncpy(buf, first_name, sizeof(buf)-1);
	strncat(buf, ".ident", sizeof(buf)-1);
		
	free(first_name);
	
	ident.obj = idmef_object_new(buf);
	if ( ! ident.obj ) {
		log(LOG_ERR, "could not create %s object!\n");
		return NULL;
	}

	ident_cache = idmef_cache_new();
	if ( ! ident_cache ) {
		log(LOG_ERR, "could not create IDMEF cache!\n");
		idmef_object_destroy(ident.obj);
		return NULL;
	}
	
	ident_cached = idmef_cache_register_object(ident_cache, ident.obj);
	if ( ! ident_cached ) {
		log(LOG_ERR, "could not register %s object!\n");
		idmef_object_destroy(ident.obj);
		idmef_cache_destroy(ident_cache);
		return NULL;
	}	

	ret = idmef_cache_reindex(cache);
	if ( ret < 0 ) {
		log(LOG_ERR, "could not reindex cache!\n");
		idmef_object_destroy(ident.obj);
		idmef_cache_destroy(ident_cache);
		return NULL;		
	}

	handle = idmef_db_prepare(connection, ident_cache, criterion);	
	if ( ! handle ) {
		idmef_object_destroy(ident.obj);
		idmef_cache_destroy(ident_cache);		
		return NULL;
	}

	/* Build the new criteria set */
	
	ident.crit = idmef_criterion_new_chain(operator_or);
	if ( ! ident.crit ) {
		log(LOG_ERR, "could not create criteria chain\n");
		idmef_object_destroy(ident.obj);
		idmef_cache_destroy(ident_cache);		
		return NULL;		
	}
	
	while ( ( idmef_db_read(handle) > 0 ) ) {
		idmef_value_iterate(idmef_cached_object_get(ident_cached), &ident, add_criterion);
	}
	
	
	/* STAGE 2: execute built query to get actual results */
	
	handle = idmef_db_prepare(connection, cache, ident.crit);	
	
	idmef_object_destroy(ident.obj);
	idmef_criterion_destroy(ident.crit);
	idmef_cache_destroy(ident_cache);
	
	return handle;
	
}




static int format_read(void *handle)
{
	return idmef_db_read(handle);
}




plugin_generic_t *plugin_init(int argc, char **argv)
{
	/* System wide plugin options should go in here */
        
        plugin_set_name(&plugin, "Classic");
        plugin_set_desc(&plugin, "Prelude 0.8.0 database format");
        plugin_set_write_func(&plugin, format_write);
        plugin_set_prepare_func(&plugin, format_prepare);
        plugin_set_read_func(&plugin, format_read);

	db_objects_init(CONFIG_FILE);
       
	return (plugin_generic_t *) &plugin;
}




