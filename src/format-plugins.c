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
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <inttypes.h>
#include <assert.h>

#include <libprelude/prelude-plugin.h>
#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"
#include "db-message-ident.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object-selection.h"

#include "plugin-format.h"



/**
 * format_plugins_init:
 * @dirname: Pointer to a directory string.
 *
 * Tell the DB plugins subsystem to load database format plugins from @dirname.
 *
 * Returns: 0 on success, -1 if an error occured.
 */
int format_plugins_init(const char *dirname)
{
        int ret;
	
	ret = access(dirname, F_OK);
	if ( ret < 0 ) {
		if ( errno == ENOENT )
			return 0;

                log(LOG_ERR, "can't access %s.\n", dirname);

                return -1;
	}

        ret = prelude_plugin_load_from_dir(dirname, NULL, NULL);
        if ( ret < 0 ) {
                log(LOG_ERR, "couldn't load plugin subsystem.\n");
                return -1;
        }

        return ret;
}
