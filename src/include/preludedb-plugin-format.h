/*****
*
* Copyright (C) 2005 PreludeIDS Technologies. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoann.v@prelude-ids.com>
*
* This file is part of the PreludeDB library.
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

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct preludedb_plugin_format preludedb_plugin_format_t;


typedef int (*preludedb_plugin_format_check_schema_version_func_t)(const char *version);

typedef int (*preludedb_plugin_format_get_alert_idents_func_t)(preludedb_sql_t *sql, idmef_criteria_t *criteria,
                                                               int limit, int offset, preludedb_result_idents_order_t order,
                                                               void **res);

typedef int (*preludedb_plugin_format_get_heartbeat_idents_func_t)(preludedb_sql_t *sql, idmef_criteria_t *criteria,
                                                                   int limit, int offset, preludedb_result_idents_order_t order,
                                                                   void **res);

typedef size_t (*preludedb_plugin_format_get_message_ident_count_func_t)(void *res);
typedef int (*preludedb_plugin_format_get_next_message_ident_func_t)(void *res, uint64_t *ident);
typedef void (*preludedb_plugin_format_destroy_message_idents_resource_func_t)(void *res);
typedef int (*preludedb_plugin_format_get_alert_func_t)(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message);
typedef int (*preludedb_plugin_format_get_heartbeat_func_t)(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message);
typedef int (*preludedb_plugin_format_delete_alert_func_t)(preludedb_sql_t *sql, uint64_t ident);
typedef int (*preludedb_plugin_format_delete_heartbeat_func_t)(preludedb_sql_t *sql, uint64_t ident);
typedef int (*preludedb_plugin_format_insert_message_func_t)(preludedb_sql_t *sql, idmef_message_t *message);
typedef int (*preludedb_plugin_format_get_values_func_t)(preludedb_sql_t *sql, preludedb_path_selection_t *selection,
                                                         idmef_criteria_t *criteria, int distinct, int limit, int offset, void **res);

typedef int (*preludedb_plugin_format_get_next_values_func_t)(void *res, preludedb_path_selection_t *selection, idmef_value_t ***values);
typedef void (*preludedb_plugin_format_destroy_values_resource_func_t)(void *res);



void preludedb_plugin_format_set_check_schema_version_func(preludedb_plugin_format_t *plugin,
                                                           preludedb_plugin_format_check_schema_version_func_t func);

void preludedb_plugin_format_set_get_alert_idents_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_get_alert_idents_func_t func);

void preludedb_plugin_format_set_get_heartbeat_idents_func(preludedb_plugin_format_t *plugin,
                                                           preludedb_plugin_format_get_heartbeat_idents_func_t func);

void preludedb_plugin_format_set_get_message_ident_count_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_get_message_ident_count_func_t func);

void preludedb_plugin_format_set_get_next_message_ident_func(preludedb_plugin_format_t *plugin,
                                                             preludedb_plugin_format_get_next_message_ident_func_t func);

void preludedb_plugin_format_set_destroy_message_idents_resource_func(preludedb_plugin_format_t *plugin,
                                                                      preludedb_plugin_format_destroy_message_idents_resource_func_t func);

void preludedb_plugin_format_set_get_alert_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_get_alert_func_t func);

void preludedb_plugin_format_set_get_heartbeat_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_get_heartbeat_func_t func);

void preludedb_plugin_format_set_delete_alert_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_delete_alert_func_t func);

void preludedb_plugin_format_set_delete_heartbeat_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_delete_heartbeat_func_t func);

void preludedb_plugin_format_set_insert_message_func(preludedb_plugin_format_t *plugin,
                                                     preludedb_plugin_format_insert_message_func_t func);

void preludedb_plugin_format_set_get_values_func(preludedb_plugin_format_t *plugin,
                                                 preludedb_plugin_format_get_values_func_t func);

void preludedb_plugin_format_set_get_next_values_func(preludedb_plugin_format_t *plugin,
                                                      preludedb_plugin_format_get_next_values_func_t func);


void preludedb_plugin_format_set_destroy_values_resource_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_destroy_values_resource_func_t func);

int preludedb_plugin_format_new(preludedb_plugin_format_t **ret);

#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */
