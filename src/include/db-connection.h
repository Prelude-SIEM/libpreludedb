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


#ifndef _LIBPRELUDEDB_DB_CONNECTION_H
#define _LIBPRELUDEDB_DB_CONNECTION_H

typedef enum {
	sql = 1
} prelude_db_type_t;

typedef struct {
	prelude_db_type_t type;
	union {
		sql_connection_t *sql;
	} connection;
} prelude_db_connection_t;

#include "plugin-format.h" /* for plugin_format_t */

typedef struct {
	prelude_db_connection_t db_connection;
	plugin_format_t *format;
	struct list_head filter_list;
	int active;
	int connected;
	char *name;
} prelude_db_interface_t;

/* Interface-specific functions */
prelude_db_interface_t *prelude_db_connect(const char *config);
prelude_db_interface_t *prelude_db_connect_sql_config(const char *config);
prelude_db_interface_t *prelude_db_connect_sql(const char *name, const char *dbformat, 
                                               const char *dbtype, const char *dbhost, 
                                               const char *dbport, const char *dbname, 
                                               const char *dbuser, const char *dbpass);
int prelude_db_interface_activate(prelude_db_interface_t *interface);
int prelude_db_interface_deactivate(prelude_db_interface_t *interface);
int prelude_db_interface_write_idmef_message(prelude_db_interface_t *interface, const idmef_message_t *msg);
int prelude_db_interface_disconnect(prelude_db_interface_t *interface);
int prelude_db_interface_destroy(prelude_db_interface_t *interface);

#endif /* _LIBPRELUDEDB_DB_CONNECTION_H */

