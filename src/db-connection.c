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

typedef struct {
	struct list_head list;
	db_connection_t db_connection;
	plugin_format_t *format;
	struct list_head filter_list;
	int active;
} db_interface_t;

struct list_head db_interfaces;

db_interface_t *db_connect_sql(const char *dbformat, const char *dbtype, const char *dbhost, 
				const char *dbport, const char *dbname, const char *dbuser, 
				const char *dbpass)
{
	sql_connection_t *sql_conn;
	db_interface_t *interface;
	plugin_format_t *format;
	
	sql_conn = sql_connect(dbtype, dbhost, dbport, dbname, dbuser, dbpass);
	if (!sql_conn) {
		log(LOG_ERR, "%s@%s: database connection failed\n", dbname, dbhost);
		return NULL;
	}
	
	format = (plugin_format_t *) plugin_search_by_name(dbformat);
	if (!format) {
		log(LOG_ERR, "%s: format plugin not found\n");
		return NULL;
	}
	
	interface = (db_interface_t *) calloc(1, sizeof(db_interface_t));
	if (!interface) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}
	
	interface->db_connection.type = sql;
	interface->db_connection.connection.sql = sql_conn;
	interface->format = format;
	interface->active = 0;
	INIT_LIST_HEAD(&interface->filter_list);
	
	list_add(&interface->list, &db_interfaces);

	printf("connected to %s@%s\n", dbname, dbhost);
	
	return interface;
}

static int db_interface_activate(db_interface_t *interface)
{
	if (!interface)
		return -1; 
		
	if (interface->active)
		return -2;
		
	interface->active = 1;
	
	return 0;
}

/*
static int db_connection_add_filter(db_interface_t *conn, const char *filter)
{
	filter_plugin_t *filter;
	
	if (!conn)
		return -1;
		
	filter = (plugin_filter_t *) search_plugin_by_name(filter);
	if (!filter)
		return -2;
	
	list_add_tail(...);
}

static int db_connection_delete_filter(db_interface_t *conn, const char *filter);
*/

static int db_interface_write_idmef_message(db_interface_t *interface, idmef_message_t *msg)
{
	printf("db_interface_write_idmef_message()\n");

	if (!interface)
		return -1;
		
	if (!msg)
		return -2;
		
	if (interface->db_connection.type != sql)
		return -3;
		
	return interface->format->format_write(&interface->db_connection, msg);
}

static int db_disconnect(db_interface_t *interface)
{
	if (!interface || !(interface->active))
		return -1;

	interface->active = 0;
	
	switch (interface->db_connection.type) {
		case sql: sql_close(interface->db_connection.connection.sql);
			  return 0;
			  
		default:  log(LOG_ERR, "not supported connection type %d\n", interface->db_connection.type);
			  return -2;
	}
	
	list_del(&interface->list);
	
	return 0;
}

int db_interfaces_init(void)
{
	db_interface_t *interface;
	int ret;
	
	INIT_LIST_HEAD(&db_interfaces);

	/* FIXME: read this from config file */
	interface = db_connect_sql("classic", "pgsql", "localhost", NULL, "prelude", "prelude", "prelude");
	if (!interface) {
		log(LOG_ERR, "db connection failed\n");
		return -1;
	}
	
	ret = db_interface_activate(interface);
	
	return ret;
}

int db_write_idmef_message(idmef_message_t *msg)
{
	struct list_head *tmp;
	db_interface_t *interface;

	printf("db_write_idmef_message\n");
	
	list_for_each(tmp, &db_interfaces) {
		interface = list_entry(tmp, db_interface_t, list);
		db_interface_write_idmef_message(interface, msg);
	}
	
	return 0;
	
}

