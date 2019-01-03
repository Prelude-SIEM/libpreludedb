/*****
*
* Copyright (C) 2005-2019 CS-SI. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#ifndef _LIBPRELUDEDB_PLUGIN_FORMAT_H
#define _LIBPRELUDEDB_PLUGIN_FORMAT_H

#include "preludedb.h"
#include <libprelude/prelude-plugin.h>

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct preludedb_plugin_format preludedb_plugin_format_t;


typedef int (*preludedb_plugin_format_check_schema_version_func_t)(const char *version);

typedef int (*preludedb_plugin_format_get_alert_idents_func_t)(preludedb_t *db, idmef_criteria_t *criteria,
                                                               int limit, int offset, const preludedb_path_selection_t *order,
                                                               void **res);

typedef int (*preludedb_plugin_format_get_heartbeat_idents_func_t)(preludedb_t *db, idmef_criteria_t *criteria,
                                                                   int limit, int offset, const preludedb_path_selection_t *order,
                                                                   void **res);

typedef size_t (*preludedb_plugin_format_get_message_ident_count_func_t)(void *res);
typedef int (*preludedb_plugin_format_get_message_ident_func_t)(void *res, unsigned int row_index, uint64_t *ident);
typedef void (*preludedb_plugin_format_destroy_message_idents_resource_func_t)(void *res);
typedef int (*preludedb_plugin_format_get_alert_func_t)(preludedb_t *db, uint64_t ident, idmef_message_t **message);
typedef int (*preludedb_plugin_format_get_heartbeat_func_t)(preludedb_t *db, uint64_t ident, idmef_message_t **message);
typedef int (*preludedb_plugin_format_delete_func_t)(preludedb_t *db, idmef_criteria_t *criteria);
typedef int (*preludedb_plugin_format_delete_alert_func_t)(preludedb_t *db, uint64_t ident);
typedef ssize_t (*preludedb_plugin_format_delete_alert_from_list_func_t)(preludedb_t *db, uint64_t *idents, size_t size);
typedef ssize_t (*preludedb_plugin_format_delete_alert_from_result_idents_func_t)(preludedb_t *db,
                                                                                  preludedb_result_idents_t *results);
typedef int (*preludedb_plugin_format_delete_heartbeat_func_t)(preludedb_t *db, uint64_t ident);
typedef ssize_t (*preludedb_plugin_format_delete_heartbeat_from_list_func_t)(preludedb_t *db, uint64_t *idents, size_t size);
typedef ssize_t (*preludedb_plugin_format_delete_heartbeat_from_result_idents_func_t)(preludedb_t *db,
                                                                                      preludedb_result_idents_t *results);
typedef int (*preludedb_plugin_format_insert_message_func_t)(preludedb_t *db, idmef_message_t *message);

typedef int (*preludedb_plugin_format_get_result_values_count_func_t)(preludedb_result_values_t *results);

typedef int (*preludedb_plugin_format_get_result_values_field_func_t)(preludedb_result_values_t *results, void *row, preludedb_selected_path_t *selected, preludedb_result_values_get_field_cb_func_t cb, void **out);

typedef int (*preludedb_plugin_format_get_result_values_row_func_t)(preludedb_result_values_t *results, unsigned int rownum, void **row);

typedef int (*preludedb_plugin_format_get_values_func_t)(preludedb_t *db, preludedb_path_selection_t *selection,
                                                         idmef_criteria_t *criteria, int distinct, int limit, int offset, void **res);

typedef void (*preludedb_plugin_format_destroy_values_resource_func_t)(void *res);

typedef int (*preludedb_plugin_format_update_func_t)(preludedb_t *db, const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                                     idmef_criteria_t *criteria, preludedb_path_selection_t *order, int limit, int offset);

typedef int (*preludedb_plugin_format_update_from_list_func_t)(preludedb_t *db, const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                                               uint64_t *idents, size_t size);

typedef int (*preludedb_plugin_format_update_from_result_idents_func_t)(preludedb_t *db, const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                                                        preludedb_result_idents_t *results);

typedef int (*preludedb_plugin_format_get_path_column_count_func_t)(preludedb_selected_path_t *selected);

typedef int (*preludedb_plugin_format_path_resolve_func_t)(preludedb_selected_path_t *selected, preludedb_selected_object_t *object, void *data, prelude_string_t *out);

typedef int (*preludedb_plugin_format_init_func_t)(preludedb_t *db);

typedef void (*preludedb_plugin_format_destroy_func_t)(preludedb_t *db);

typedef int (*preludedb_plugin_format_optimize_func_t)(preludedb_t *db);


void preludedb_plugin_format_set_check_schema_version_func(preludedb_plugin_format_t *plugin,
                                                           preludedb_plugin_format_check_schema_version_func_t func);

void preludedb_plugin_format_set_get_alert_idents_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_get_alert_idents_func_t func);

void preludedb_plugin_format_set_get_heartbeat_idents_func(preludedb_plugin_format_t *plugin,
                                                           preludedb_plugin_format_get_heartbeat_idents_func_t func);

void preludedb_plugin_format_set_get_message_ident_count_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_get_message_ident_count_func_t func);

void preludedb_plugin_format_set_get_message_ident_func(preludedb_plugin_format_t *plugin,
                                                        preludedb_plugin_format_get_message_ident_func_t func);

void preludedb_plugin_format_set_destroy_message_idents_resource_func(preludedb_plugin_format_t *plugin,
                                                                      preludedb_plugin_format_destroy_message_idents_resource_func_t func);

void preludedb_plugin_format_set_get_alert_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_get_alert_func_t func);

void preludedb_plugin_format_set_get_heartbeat_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_get_heartbeat_func_t func);

void preludedb_plugin_format_set_delete_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_delete_func_t func);

void preludedb_plugin_format_set_delete_alert_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_delete_alert_func_t func);

void preludedb_plugin_format_set_delete_alert_from_list_func(preludedb_plugin_format_t *plugin,
                                                             preludedb_plugin_format_delete_alert_from_list_func_t func);

void preludedb_plugin_format_set_delete_alert_from_result_idents_func(preludedb_plugin_format_t *plugin,
                                                                      preludedb_plugin_format_delete_alert_from_result_idents_func_t func);

ssize_t _preludedb_plugin_format_delete_alert_from_list(preludedb_plugin_format_t *plugin,
                                                        preludedb_t *db, uint64_t *idents, size_t size);

ssize_t _preludedb_plugin_format_delete_alert_from_result_idents(preludedb_plugin_format_t *plugin,
                                                                 preludedb_t *db, preludedb_result_idents_t *result);

void preludedb_plugin_format_set_delete_heartbeat_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_delete_heartbeat_func_t func);

void preludedb_plugin_format_set_delete_heartbeat_from_list_func(preludedb_plugin_format_t *plugin,
                                                                 preludedb_plugin_format_delete_heartbeat_from_list_func_t func);

void preludedb_plugin_format_set_delete_heartbeat_from_result_idents_func(preludedb_plugin_format_t *plugin,
                                                                          preludedb_plugin_format_delete_heartbeat_from_result_idents_func_t func);

ssize_t _preludedb_plugin_format_delete_heartbeat_from_list(preludedb_plugin_format_t *plugin,
                                                            preludedb_t *db, uint64_t *idents, size_t size);

ssize_t _preludedb_plugin_format_delete_heartbeat_from_result_idents(preludedb_plugin_format_t *plugin,
                                                                     preludedb_t *db, preludedb_result_idents_t *result);

void preludedb_plugin_format_set_insert_message_func(preludedb_plugin_format_t *plugin,
                                                     preludedb_plugin_format_insert_message_func_t func);

void preludedb_plugin_format_set_get_values_func(preludedb_plugin_format_t *plugin,
                                                 preludedb_plugin_format_get_values_func_t func);

void preludedb_plugin_format_set_get_result_values_count_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_get_result_values_count_func_t func);

void preludedb_plugin_format_set_get_result_values_row_func(preludedb_plugin_format_t *plugin,
                                                            preludedb_plugin_format_get_result_values_row_func_t func);

void preludedb_plugin_format_set_get_result_values_field_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_get_result_values_field_func_t func);

void preludedb_plugin_format_set_update_func(preludedb_plugin_format_t *plugin,
                                             preludedb_plugin_format_update_func_t func);

void preludedb_plugin_format_set_update_from_list_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_update_from_list_func_t func);

void preludedb_plugin_format_set_update_from_result_idents_func(preludedb_plugin_format_t *plugin,
                                                                preludedb_plugin_format_update_from_result_idents_func_t func);

void preludedb_plugin_format_set_get_path_column_count_func(preludedb_plugin_format_t *plugin,
                                                            preludedb_plugin_format_get_path_column_count_func_t func);

void preludedb_plugin_format_set_path_resolve_func(preludedb_plugin_format_t *plugin,
                                                   preludedb_plugin_format_path_resolve_func_t func);

void preludedb_plugin_format_set_destroy_values_resource_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_destroy_values_resource_func_t func);

void preludedb_plugin_format_set_init_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_init_func_t func);

void preludedb_plugin_format_set_destroy_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_destroy_func_t func);

void preludedb_plugin_format_set_optimize_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_optimize_func_t func);

int preludedb_plugin_format_new(preludedb_plugin_format_t **ret);

#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */
