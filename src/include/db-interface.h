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

char *prelude_db_interface_get_name(prelude_db_interface_t *db);

char *prelude_db_interface_get_format(prelude_db_interface_t *db);

int prelude_db_interface_connect(prelude_db_interface_t *interface);

prelude_db_alert_uident_list_t *prelude_db_interface_get_alert_uident_list(prelude_db_interface_t *interface,
									 idmef_criteria_t *criteria);

prelude_db_heartbeat_uident_list_t *prelude_db_interface_get_heartbeat_uident_list(prelude_db_interface_t *interface,
										 idmef_criteria_t *criteria);

void prelude_db_interface_free_alert_uident_list(prelude_db_alert_uident_list_t *uident_list);

void prelude_db_interface_free_heartbeat_uident_list(prelude_db_heartbeat_uident_list_t *uident_list);

prelude_db_alert_uident_t *prelude_db_interface_get_next_alert_uident(prelude_db_alert_uident_list_t *uident_list);

prelude_db_heartbeat_uident_t *prelude_db_interface_get_next_heartbeat_uident(prelude_db_heartbeat_uident_list_t *uident_list);

idmef_message_t *prelude_db_interface_get_alert(prelude_db_interface_t *interface, 
						prelude_db_alert_uident_t *uident,
						idmef_selection_t *selection);

idmef_message_t *prelude_db_interface_get_heartbeat(prelude_db_interface_t *interface,
						    prelude_db_heartbeat_uident_t *uident,
						    idmef_selection_t *selection);

int prelude_db_interface_delete_alert(prelude_db_interface_t *interface, prelude_db_alert_uident_t *uident);

int prelude_db_interface_delete_heartbeat(prelude_db_interface_t *interface, prelude_db_heartbeat_uident_t *uident);

int prelude_db_interface_insert_idmef_message(prelude_db_interface_t *interface, const idmef_message_t *msg);

void *prelude_db_interface_select_values(prelude_db_interface_t *interface,
					 idmef_selection_t *selection,
					 idmef_criteria_t *criteria,
					 int distinct,
					 int limit);

idmef_object_value_list_t *prelude_db_interface_get_values(prelude_db_interface_t *interface,
							   void *data,
							   idmef_selection_t *selection);

prelude_db_connection_t *prelude_db_interface_get_connection(prelude_db_interface_t *interface);

prelude_db_connection_data_t *prelude_db_interface_get_connection_data(prelude_db_interface_t *interface);

int prelude_db_interface_errno(prelude_db_interface_t *interface);

const char * prelude_db_interface_error(prelude_db_interface_t *interface);

int prelude_db_interface_disconnect(prelude_db_interface_t *interface);

int prelude_db_interface_destroy(prelude_db_interface_t *interface);

#endif /* _LIBPRELUDEDB_DB_INTERFACE_H */
