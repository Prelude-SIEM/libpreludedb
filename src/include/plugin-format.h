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

	void *(*format_get_ident_list)(prelude_db_connection_t *connection,
				       idmef_criterion_t *criterion);

	void (*format_free_ident_list)(prelude_db_connection_t *connection,
				       void *message_list);

	uint64_t (*format_get_next_ident)(prelude_db_connection_t *connection,
					  void *message_list);

	idmef_message_t *(*format_get_alert)(prelude_db_connection_t *connection,
					     uint64_t ident,
					     idmef_selection_t *selection);

	idmef_message_t *(*format_get_heartbeat)(prelude_db_connection_t *connection,
						 uint64_t ident,
						 idmef_selection_t *selection);

	int (*format_delete_alert)(prelude_db_connection_t *connection, uint64_t ident);

	int (*format_delete_heartbeat)(prelude_db_connection_t *connection, uint64_t ident);

	int (*format_insert_idmef_message)(prelude_db_connection_t * connection,
					   const idmef_message_t * message);

} plugin_format_t;

#define	plugin_get_ident_list_func(p) (p)->format_get_ident_list
#define plugin_free_ident_list_func(p) (p)->format_free_ident_list
#define plugin_get_next_ident_func(p) (p)->format_get_next_ident
#define plugin_get_alert_func(p) (p)->format_get_alert
#define plugin_get_heartbeat_func(p) (p)->format_get_heartbeat
#define plugin_delete_alert_func(p) (p)->format_delete_alert
#define plugin_delete_heartbeat_func(p) (p)->format_delete_heartbeat
#define	plugin_insert_idmef_message_func(p) (p)->format_insert_idmef_message

#define	plugin_set_get_ident_list_func(p, f) plugin_get_ident_list_func(p) = (f)
#define plugin_set_free_ident_list_func(p, f) plugin_free_ident_list_func(p) = (f)
#define	plugin_set_get_next_ident_func(p, f) plugin_get_next_ident_func(p) = (f)
#define plugin_set_get_alert_func(p, f) plugin_get_alert_func(p) = (f)
#define plugin_set_get_heartbeat_func(p, f) plugin_get_heartbeat_func(p) = (f)
#define plugin_set_delete_alert_func(p, f) plugin_delete_alert_func(p) = (f)
#define plugin_set_delete_heartbeat_func(p, f) plugin_delete_heartbeat_func(p) = (f)
#define	plugin_set_insert_idmef_message_func(p, f) plugin_insert_idmef_message_func(p) = (f)

int format_plugins_init(const char *dirname);

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */
