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

#include <libprelude/prelude-log.h>

#include "db.h"

static int initialized = 0;

int prelude_db_init(void)
{
	int ret;

	/* FIXME: we should rather return error here */
	if (initialized++) {
		log(LOG_ERR, "attempt to re-initialize db subsystem! pretending to be OK\n");
		return 0;
	}
	
	log(LOG_INFO, "- Starting DB subsystem\n");

	/* When asynchronous subsystem is in, initialize it here */
/*	
	ret = db_dispatch_init();
	if (ret < 0)
		return -1;
*/	

	ret = sql_plugins_init(SQL_PLUGIN_DIR, 0, NULL);	
	if (ret < 0)
		return -2;
		
	ret = format_plugins_init(FORMAT_PLUGIN_DIR, 0, NULL);	
	if (ret < 0)
		return -3;

	/* When filters are in, initialize them here */
/*	
	ret = filter_plugins_init(FILTER_PLUGIN_DIR, 0, NULL);
	if (ret < 0)
		return -4;
*/	
	ret = prelude_db_init_interfaces();
	if (ret < 0)
		return -5;
	
	return 0; /* success */
}

int prelude_db_output_idmef_message(idmef_message_t *idmef)
{
	/* when async queuing is introduced, add message to queue here */

	return prelude_db_write_idmef_message(idmef);
}

int prelude_db_shutdown(void) 
{
	return prelude_db_shutdown_interfaces();
}
