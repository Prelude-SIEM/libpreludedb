/*****
*
* Copyright (C) 2001-2004 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2003-2005 Nicolas Delon <nicolas@prelude-ids.org>
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


#include <libprelude/prelude-plugin.h>


typedef struct {
	PRELUDE_PLUGIN_GENERIC;

	int (*get_alert_idents)(preludedb_sql_t *sql,
				idmef_criteria_t *criteria,
				int limit, int offset, preludedb_result_idents_order_t order,
				void **res);

	int (*get_heartbeat_idents)(preludedb_sql_t *sql,
				    idmef_criteria_t *criteria, 
				    int limit, int offset, preludedb_result_idents_order_t order,
				    void **res);

	size_t (*get_message_ident_count)(void *res);

	int (*get_next_message_ident)(void *res, uint64_t *ident);

	void (*destroy_message_idents_resource)(void *res);

	int (*get_alert)(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message);

	int (*get_heartbeat)(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message);
        
	int (*delete_alert)(preludedb_sql_t *sql, uint64_t ident);

	int (*delete_heartbeat)(preludedb_sql_t *sql, uint64_t ident);

	int (*insert_message)(preludedb_sql_t *sql, idmef_message_t *message);

	int (*get_values)(preludedb_sql_t *sql, preludedb_path_selection_t *selection,
			  idmef_criteria_t *criteria, int distinct, int limit, int offset, void **res);

	int (*get_next_values)(void *res, preludedb_path_selection_t *selection, idmef_value_t ***values);

	void (*destroy_values_resource)(void *res);
} preludedb_plugin_format_t;


#define	preludedb_plugin_format_get_alert_idents_func(p) (p)->get_alert_idents
#define	preludedb_plugin_format_get_heartbeat_idents_func(p) (p)->get_heartbeat_idents
#define	preludedb_plugin_format_get_message_ident_count_func(p) (p)->get_message_ident_count
#define	preludedb_plugin_format_get_next_message_ident_func(p) (p)->get_next_message_ident
#define preludedb_plugin_format_destroy_message_idents_resource_func(p) (p)->destroy_message_idents_resource
#define preludedb_plugin_format_get_alert_func(p) (p)->get_alert
#define preludedb_plugin_format_get_heartbeat_func(p) (p)->get_heartbeat
#define preludedb_plugin_format_delete_alert_func(p) (p)->delete_alert
#define preludedb_plugin_format_delete_heartbeat_func(p) (p)->delete_heartbeat
#define preludedb_plugin_format_insert_message_func(p) (p)->insert_message
#define preludedb_plugin_format_get_values_func(p) (p)->get_values
#define preludedb_plugin_format_get_next_values_func(p) (p)->get_next_values
#define preludedb_plugin_format_destroy_values_resource_func(p) (p)->destroy_values_resource

#define	preludedb_plugin_format_set_get_alert_idents_func(p, f) preludedb_plugin_format_get_alert_idents_func(p) = (f)
#define	preludedb_plugin_format_set_get_heartbeat_idents_func(p, f) preludedb_plugin_format_get_heartbeat_idents_func(p) = (f)
#define	preludedb_plugin_format_set_get_message_ident_count_func(p, f) preludedb_plugin_format_get_message_ident_count_func(p) = (f)
#define	preludedb_plugin_format_set_get_next_message_ident_func(p, f) preludedb_plugin_format_get_next_message_ident_func(p) = (f)
#define	preludedb_plugin_format_set_destroy_message_idents_resource_func(p, f) preludedb_plugin_format_destroy_message_idents_resource_func(p) = (f)
#define preludedb_plugin_format_set_get_alert_func(p, f) preludedb_plugin_format_get_alert_func(p) = (f)
#define preludedb_plugin_format_set_get_heartbeat_func(p, f) preludedb_plugin_format_get_heartbeat_func(p) = (f)
#define preludedb_plugin_format_set_delete_alert_func(p, f) preludedb_plugin_format_delete_alert_func(p) = (f)
#define preludedb_plugin_format_set_delete_heartbeat_func(p, f) preludedb_plugin_format_delete_heartbeat_func(p) = (f)
#define preludedb_plugin_format_set_insert_message_func(p, f) preludedb_plugin_format_insert_message_func(p) = (f)
#define preludedb_plugin_format_set_get_values_func(p, f) preludedb_plugin_format_get_values_func(p) = (f)
#define preludedb_plugin_format_set_get_next_values_func(p, f) preludedb_plugin_format_get_next_values_func(p) = (f)
#define preludedb_plugin_format_set_destroy_values_resource_func(p, f) preludedb_plugin_format_destroy_values_resource_func(p) = (f)

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */
