#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/prelude-log.h>

#include "gdb.h"
#include "db.h"

static int initialized = 0;

int db_init(int argc, char **argv)
{
	int ret;

	if (initialized) {
		log(LOG_ERR, "attempt to re-initialize db subsystem! pretending to be OK\n");
		return 0;
	}
	
	initialized++;
	
	log(LOG_INFO, "starting db subsystem (%d)\n", initialized);

	gdb_setup(argc, argv);
/*	
	ret = db_dispatch_init();
	if (ret < 0)
		return -1;
*/	

	log(LOG_INFO, "loading plugins from %s\n", SQL_PLUGIN_DIR);

	ret = sql_plugins_init(SQL_PLUGIN_DIR, argc, argv);	
	if (ret < 0)
		return -2;

	log(LOG_INFO, "loading plugins from %s\n", FORMAT_PLUGIN_DIR);
		
	ret = format_plugins_init(FORMAT_PLUGIN_DIR, argc, argv);	
	if (ret < 0)
		return -3;

/*	
	ret = filter_plugins_init(FILTER_PLUGIN_DIR, argc, argv);
	if (ret < 0)
		return -4;
*/	
	ret = db_interfaces_init(argc, argv);
	if (ret < 0)
		return -5;
	
	return 0; /* success */
}

int db_available(void) 
{
	return sql_plugins_available();
}

void db_startup(void)
{
	db_interfaces_init();
}

int db_write_alert(idmef_message_t *idmef)
{
//	db_plugins_run(idmef);
//	
//	return 0; /* always return success. For now. */
//	return db_message_dispatch(idmef); /* _queue() ? */
	return db_write_idmef_message(idmef);
}

