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

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "db-type.h"
#include "db-message-ident.h"
#include "sql-connection-data.h"
#include "sql.h"
#include "db-object-selection.h"
#include "db-connection.h" 
#include "db-connection-data.h"
#include "db-interface.h"
#include "param-string.h"

#include "db-interface-string.h"



static prelude_sql_connection_data_t *get_sql_connection_data(const char *config)
{
        char *val;
        int ret, i;
        prelude_sql_connection_data_t *data;
        struct {
                const char *entry;
                int (*func)(prelude_sql_connection_data_t *data, const char *value);
        } tbl[] = {
                { "type", prelude_sql_connection_data_set_type },
                { "host", prelude_sql_connection_data_set_host },
                { "name", prelude_sql_connection_data_set_name },
                { "user", prelude_sql_connection_data_set_user },
                { "pass", prelude_sql_connection_data_set_pass },
                { "port", prelude_sql_connection_data_set_port },
                { NULL  , NULL                                 },
        };

        data = prelude_sql_connection_data_new();
        if ( ! data )
                return NULL;
        
        for ( i = 0; tbl[i].entry != NULL; i++ ) {

                val = parameter_value(config, tbl[i].entry);
                if ( ! val )
                        continue;

                ret = tbl[i].func(data, val);

                free(val);
                
                if ( ret < 0 ) {
                        prelude_sql_connection_data_destroy(data);
                        return NULL;
                }
        }
        
	return data;
}




prelude_db_interface_t *prelude_db_interface_new_string(const char *config)
{
	char *name = NULL;
	char *class = NULL;
	char *format = NULL;
	void *specific = NULL;
	prelude_db_type_t type;
        prelude_db_connection_data_t *data;
	prelude_db_interface_t *iface = NULL;
        
	class = parameter_value(config, "class");
        format = parameter_value(config, "format");
	name = parameter_value(config, "interface");
        
	if ( ! class || ! *class ) {
		log(LOG_ERR, "Interface class not specified!\n");
		goto end;
	}
	
	if ( strcasecmp(class, "SQL") == 0 ) {
		specific = get_sql_connection_data(config);
		type = prelude_db_type_sql;
	} else {
		log(LOG_ERR, "Unknown interface class \"%s\"\n", class);
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

