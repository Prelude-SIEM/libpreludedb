/*****
*
* Copyright (C) 2002, 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#include "config.h"

#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/prelude-io.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"
#include "db.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-message-ident.h"
#include "db-object-selection.h"
#include "plugin-format.h"



static int initialized = 0;



int prelude_db_init(void)
{
	int ret;

	/* 
	 * Because we can be called many times (i.e. by different plugins
	 * in application), we do nothing and return success when we find
	 * we've been already initialized. 
	 */
	if ( initialized++ )
		return 0;

	ret = sql_plugins_init(SQL_PLUGIN_DIR);	
	if ( ret < 0 )
		return -2;
        
	ret = format_plugins_init(FORMAT_PLUGIN_DIR);	
	if ( ret < 0 )
		return -3;
        	
	return 0;
}



void prelude_db_shutdown(void) 
{
	/* nop */
}



const char *prelude_db_check_version(const char *req_version)
{
        int ret;
        int major, minor, micro;
        int rq_major, rq_minor, rq_micro;

        if ( ! req_version )
                return VERSION;
        
        ret = sscanf(VERSION, "%d.%d.%d", &major, &minor, &micro);
        if ( ret != 3 )
                return NULL;
        
        ret = sscanf(req_version, "%d.%d.%d", &rq_major, &rq_minor, &rq_micro);
        if ( ret != 3 )
                return NULL;
        
        if ( major > rq_major
             || (major == rq_major && minor > rq_minor)
             || (major == rq_major && minor == rq_minor && micro > rq_micro)
             || (major == rq_major && minor == rq_minor && micro == rq_micro) ) {
                return VERSION;
        }

        return NULL;
}
