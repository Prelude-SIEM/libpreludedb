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
#include <libprelude/idmef.h>

#include "db-type.h"
#include "sql-connection-data.h"
#include "db-connection.h" 
#include "db-connection-data.h"
#include "db-interface.h"
#include "param-string.h"
#include "db-interface-string.h"

#define is_empty(x) (*(x) == '\0') 

static void *get_sql_connection_data(const char *config)
{
	prelude_sql_connection_data_t *data;
	char *val;
	int ret;
	
	data = prelude_sql_connection_data_new();
	if ( ! data )
		return NULL;
	
	val = parameter_value(config, "type");
	if ( val ) {
		ret = prelude_sql_connection_data_set_type(data, val);
		if ( ret < 0 ) {
			prelude_sql_connection_data_destroy(data);
			free(val);
			return NULL;
		}
		
		free(val);
	}

	val = parameter_value(config, "host");
	if ( val ) {
		ret = prelude_sql_connection_data_set_host(data, val);
		if ( ret < 0 ) {
			prelude_sql_connection_data_destroy(data);
			free(val);
			return NULL;
		}
		
		free(val);
	}
	
	val = parameter_value(config, "name");
	if ( val ) {
		ret = prelude_sql_connection_data_set_name(data, val);
		if ( ret < 0 ) {
			prelude_sql_connection_data_destroy(data);
			free(val);
			return NULL;
		}
		
		free(val);
	}
	
	val = parameter_value(config, "user");
	if ( val ) {
		ret = prelude_sql_connection_data_set_user(data, val);
		if ( ret < 0 ) {
			prelude_sql_connection_data_destroy(data);
			free(val);
			return NULL;
		}
		
		free(val);
	}
	
	
	val = parameter_value(config, "pass");
	if ( val ) {
		ret = prelude_sql_connection_data_set_pass(data, val);
		if ( ret < 0 ) {
			prelude_sql_connection_data_destroy(data);
			free(val);
			return NULL;
		}
		
		free(val);
	}

	val = parameter_value(config, "port");
	if ( val ) {
		ret = prelude_sql_connection_data_set_port(data, val);
		if ( ret < 0 ) {
			prelude_sql_connection_data_destroy(data);
			free(val);
			return NULL;
		}
		
		free(val);
	}

	
	return data;
}




prelude_db_interface_t *prelude_db_interface_new_string(const char *config)
{
	char *name = NULL;
	char *class = NULL;
	char *format = NULL;
	prelude_db_connection_data_t *data;
	void *specific = NULL;
	prelude_db_type_t type;
	prelude_db_interface_t *iface = NULL;
	
	name = parameter_value(config, "interface");
	format = parameter_value(config, "format");
	class = parameter_value(config, "class");
	
	if ( ! class || is_empty(class) ) {
		log(LOG_ERR, "Interface class not specified!\n");
		goto end;
	}
	
	if ( strcasecmp(class, "SQL") == 0 ) {
		specific = get_sql_connection_data(config);
		type = prelude_db_type_sql;
	} else {
		log(LOG_ERR, "Unknown interface class \"\"\n", class);
		goto end;
	}
	
	data = prelude_db_connection_data_new(type, specific);
	if ( ! data )
		goto end;
	
	iface = prelude_db_interface_new(name, format, data);

end:	
	if ( name )
		free(name);
		
	if ( format )
		free(format);
		
	if ( class )
		free(class);
	
	return iface;
}

