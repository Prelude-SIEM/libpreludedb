/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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

#ifndef _LIBPRELUDEDB_PLUGIN_FORMAT_H
#define _LIBPRELUDEDB_PLUGIN_FORMAT_H

typedef struct {
	PLUGIN_GENERIC;

	int (*format_get_alert_uident_list)(prelude_db_connection_t *connection,
					    idmef_criterion_t *criterion,
					    prelude_db_alert_uident_t **alert_idents);

	int (*format_get_heartbeat_uident_list)(prelude_db_connection_t *connection,
						idmef_criterion_t *criterion,
						prelude_db_heartbeat_uident_t **heartbeat_idents);

	idmef_message_t *(*format_get_alert)(prelude_db_connection_t *connection,
					     prelude_db_alert_uident_t *uident,
					     idmef_selection_t *selection);

	idmef_message_t *(*format_get_heartbeat)(prelude_db_connection_t *connection,
						 prelude_db_heartbeat_uident_t *uident,
						 idmef_selection_t *selection);

	int (*format_delete_alert)(prelude_db_connection_t *connection,
				   prelude_db_alert_uident_t *uident);

	int (*format_delete_heartbeat)(prelude_db_connection_t *connection, 
				       prelude_db_heartbeat_uident_t *uident);

	int (*format_insert_idmef_message)(prelude_db_connection_t *connection,
					   const idmef_message_t *message);
	
	void *(*format_select_values)(prelude_db_connection_t *connection,
				      int distinct,
			              idmef_selection_t *selection, 
				      idmef_criterion_t *criteria);

	idmef_object_value_list_t *(*format_get_values)(prelude_db_connection_t *connection,
							void *data,
						        idmef_selection_t *selection);
} plugin_format_t;

#define	plugin_get_alert_uident_list_func(p) (p)->format_get_alert_uident_list
#define	plugin_get_heartbeat_uident_list_func(p) (p)->format_get_heartbeat_uident_list
#define plugin_get_alert_func(p) (p)->format_get_alert
#define plugin_get_heartbeat_func(p) (p)->format_get_heartbeat
#define plugin_delete_alert_func(p) (p)->format_delete_alert
#define plugin_delete_heartbeat_func(p) (p)->format_delete_heartbeat
#define plugin_insert_idmef_message_func(p) (p)->format_insert_idmef_message
#define plugin_select_values_func(p) (p)->format_select_values
#define plugin_get_values_func(p) (p)->format_get_values

#define	plugin_set_get_alert_uident_list_func(p, f) plugin_get_alert_uident_list_func(p) = (f)
#define	plugin_set_get_heartbeat_uident_list_func(p, f) plugin_get_heartbeat_uident_list_func(p) = (f)
#define plugin_set_get_alert_func(p, f) plugin_get_alert_func(p) = (f)
#define plugin_set_get_heartbeat_func(p, f) plugin_get_heartbeat_func(p) = (f)
#define plugin_set_delete_alert_func(p, f) plugin_delete_alert_func(p) = (f)
#define plugin_set_delete_heartbeat_func(p, f) plugin_delete_heartbeat_func(p) = (f)
#define plugin_set_insert_idmef_message_func(p, f) plugin_insert_idmef_message_func(p) = (f)
#define plugin_set_select_values_func(p, f) plugin_select_values_func(p) = (f)
#define plugin_set_get_values_func(p, f) plugin_get_values_func(p) = (f)

int format_plugins_init(const char *dirname);

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */
