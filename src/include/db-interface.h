/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
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

#ifndef _LIBPRELUDEDB_DB_INTERFACE_H
#define _LIBPRELUDEDB_DB_INTERFACE_H

typedef struct prelude_db_interface prelude_db_interface_t;

typedef struct prelude_db_alert_uident_list prelude_db_alert_uident_list_t;
typedef struct prelude_db_heartbeat_uident_list prelude_db_heartbeat_uident_list_t;


prelude_db_interface_t *prelude_db_interface_new(const char *name,
						 const char *format,
						 prelude_db_connection_data_t *data);


void prelude_db_interface_destroy(prelude_db_interface_t *interface);

int prelude_db_interface_enable_message_cache(prelude_db_interface_t *interface, const char *cache_directory);

void prelude_db_interface_disable_message_cache(prelude_db_interface_t *interface);

char *prelude_db_interface_get_name(prelude_db_interface_t *db);

char *prelude_db_interface_get_format(prelude_db_interface_t *db);

int prelude_db_interface_connect(prelude_db_interface_t *interface);

prelude_db_message_ident_list_t *prelude_db_interface_get_alert_ident_list(prelude_db_interface_t *interface,
									   idmef_criteria_t *criteria,
									   int limit, int offset);

prelude_db_message_ident_list_t *prelude_db_interface_get_heartbeat_ident_list(prelude_db_interface_t *interface,
									       idmef_criteria_t *criteria,
									       int limit, int offset);

void prelude_db_interface_alert_ident_list_destroy(prelude_db_message_ident_list_t *ident_list);

void prelude_db_interface_heartbeat_ident_list_destroy(prelude_db_message_ident_list_t *ident_list);

prelude_db_message_ident_t *prelude_db_interface_get_next_alert_ident(prelude_db_message_ident_list_t *ident_list);

prelude_db_message_ident_t *prelude_db_interface_get_next_heartbeat_ident(prelude_db_message_ident_list_t *ident_list);

idmef_message_t *prelude_db_interface_get_alert(prelude_db_interface_t *interface, 
						prelude_db_message_ident_t *ident,
						idmef_object_list_t *object_list);

idmef_message_t *prelude_db_interface_get_heartbeat(prelude_db_interface_t *interface,
						    prelude_db_message_ident_t *ident,
						    idmef_object_list_t *object_list);

int prelude_db_interface_delete_alert(prelude_db_interface_t *interface, prelude_db_message_ident_t *ident);

int prelude_db_interface_delete_heartbeat(prelude_db_interface_t *interface, prelude_db_message_ident_t *ident);

int prelude_db_interface_insert_idmef_message(prelude_db_interface_t *interface, const idmef_message_t *msg);

void *prelude_db_interface_select_values(prelude_db_interface_t *interface,
					 prelude_db_object_selection_t *object_selection,
					 idmef_criteria_t *criteria,
					 int distinct,
					 int limit, int offset);

idmef_object_value_list_t *prelude_db_interface_get_values(prelude_db_interface_t *interface,
							   void *data,
							   prelude_db_object_selection_t *object_selection);

prelude_db_connection_t *prelude_db_interface_get_connection(prelude_db_interface_t *interface);

prelude_db_connection_data_t *prelude_db_interface_get_connection_data(prelude_db_interface_t *interface);

int prelude_db_interface_errno(prelude_db_interface_t *interface);

const char * prelude_db_interface_error(prelude_db_interface_t *interface);

int prelude_db_interface_disconnect(prelude_db_interface_t *interface);

#endif /* _LIBPRELUDEDB_DB_INTERFACE_H */
