
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
#include "sql-connection-data.h"
#include "plugin-sql.h"
#include "db-connection.h" 
#include "db-connection-data.h"
#include "db-interface.h"
#include "plugin-format.h"
#include "param-string.h"

struct prelude_db_interface {
	char *name;
	prelude_db_connection_t *db_connection;
	prelude_db_connection_data_t *connection_data;
	char *format_name;
	plugin_format_t *format;
	struct list_head filter_list;
	int active;
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
        interface->format = (plugin_format_t *) plugin_search_by_name(format);
        if ( ! interface->format ) {
                log(LOG_ERR, "couldn't find format plugin '%s'.\n", format);
                free(interface);
                return NULL;
        }
        
        interface->name = strdup(name);
        interface->format_name = strdup(format);
        
	INIT_LIST_HEAD(&interface->filter_list);
        
        return interface;
}




char *prelude_db_interface_get_name(prelude_db_interface_t *db) 
{
        return db ? db->name : NULL;
}




char *prelude_db_interface_get_format(prelude_db_interface_t *db) 
{
        return db ? db->format_name : NULL;
}




int prelude_db_interface_connect(prelude_db_interface_t *db) 
{
        sql_connection_t *cnx;

        if ( ! db->format ) {
                log(LOG_ERR, "database format not specified.\n");
                return -1;
        }

	switch ( prelude_db_connection_data_get_type(db->connection_data) ) {

		case prelude_db_type_sql:
        		cnx = sql_connect(prelude_db_connection_data_get(db->connection_data));
        		if ( ! cnx ) {
                		log(LOG_ERR, "%s: SQL connection failed.\n", db->name);
                		return -1;
                	}
                	
                	db->db_connection = prelude_db_connection_new(
                		prelude_db_type_sql, cnx);
                	if ( ! db->db_connection ) {
                		log(LOG_ERR, "%s: error creating db_connection_t object\n", 
                			db->name);
                		return -1;
                	}
                	
                default:
                	log(LOG_ERR, "%s: unknown database type %d\n", db->name, 
                		prelude_db_connection_data_get_type(db->connection_data));
                	return -1;
        }

        db->active = 1;
        
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
	
	if ( ! interface->db_connection )
		return -3;
	
	if ( ! interface->active )
		return -4;
	
	if ( ! interface->format->format_write )
		return -5;
		
	return interface->format->format_write(interface->db_connection, msg);
}



int prelude_db_interface_disconnect(prelude_db_interface_t *interface)
{
	int ret;

	if ( ! interface )
		return -1;
	
	if ( ! interface->db_connection )
		return -2;

	ret = prelude_db_interface_deactivate(interface);
	if ( ret < 0 )
		return -3;
	
	switch ( prelude_db_connection_get_type(interface->db_connection) ) {
            
        	case prelude_db_type_sql:
         	       sql_close(prelude_db_connection_get(interface->db_connection));
                	return 0;
                
        	default:
                	log(LOG_ERR, "%s: not supported connection type %d\n", interface->name, 
                		prelude_db_connection_get_type(interface->db_connection));
                	return -4;
	}

	/* not reached */		
	return 0;
}



int prelude_db_interface_destroy(prelude_db_interface_t *interface)
{
	if ( ! interface )
		return -1;
	
	if ( interface->active )
		prelude_db_interface_deactivate(interface);

	if ( interface->db_connection )
		prelude_db_interface_disconnect(interface);
	
	/* FIXME: when filters are implemented, destroy interface->filter_list here */
	
	if ( interface->name ) 
		free(interface->name);
		
	if ( interface->format_name )
		free(interface->format_name);
		
	free(interface);
	
	return 0;
}





