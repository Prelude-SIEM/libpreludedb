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
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#include <libprelude/prelude-io.h>
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-getopt.h>
#include <libprelude/idmef.h>
#include <libprelude/plugin-common.h>
#include <libprelude/plugin-common-prv.h>

#include "idmef-object-list.h"
#include "idmef-db-values.h"
#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "plugin-format.h"

#include "idmef-db-output.h"
#include "idmef-db-read.h"

static int is_enabled = 0;
static plugin_format_t plugin;



static int format_write(prelude_db_connection_t *connection, idmef_message_t *message)
{
	if (is_enabled == 0)
		return -1;
				
	return idmef_db_output(connection, message);
}

static idmef_db_values_t *format_read(prelude_db_connection_t *connection, 
        	       		      idmef_object_list_t *objects,
        	                      idmef_criterion_t *criterion)
{
	if (is_enabled == 0)
		return NULL;
				
	return idmef_db_read(connection, objects, criterion);
}

static int set_state(prelude_option_t *opt, const char *arg) 
{       
	if (is_enabled == 0) {
		is_enabled = 1;
		plugin_subscribe((plugin_generic_t *) &plugin);
	} else {
		plugin_unsubscribe((plugin_generic_t *) &plugin);
		is_enabled = 0;
	}
        
        return prelude_option_success;
}



static int get_state(char *buf, size_t size) 
{
        snprintf(buf, size, "%s", (is_enabled == 1) ? "enabled" : "disabled");
        return prelude_option_success;
}




plugin_generic_t *plugin_init(int argc, char **argv)
{
	/* System wide plugin options should go in here */
        
        plugin_set_name(&plugin, "Classic");
        plugin_set_desc(&plugin, "Prelude 0.8.0 database format");
        plugin_set_write_func(&plugin, format_write);
        plugin_set_read_func(&plugin, format_read);
       
	set_state(NULL, NULL);
       
	return (plugin_generic_t *) &plugin;
}




