/*****
*
* Copyright (C) 2005-2020 CS GROUP - France. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_PLUGIN_FORMAT_PRV_H
#define _LIBPRELUDEDB_PLUGIN_FORMAT_PRV_H

#include "preludedb-plugin-format.h"

struct preludedb_plugin_format {
        PRELUDE_PLUGIN_GENERIC;

        preludedb_plugin_format_check_schema_version_func_t check_schema_version;
        preludedb_plugin_format_get_alert_idents_func_t get_alert_idents;
        preludedb_plugin_format_get_heartbeat_idents_func_t get_heartbeat_idents;
        preludedb_plugin_format_get_message_ident_count_func_t get_message_ident_count;
        preludedb_plugin_format_get_message_ident_func_t get_message_ident;
        preludedb_plugin_format_destroy_message_idents_resource_func_t destroy_message_idents_resource;
        preludedb_plugin_format_get_alert_func_t get_alert;
        preludedb_plugin_format_get_heartbeat_func_t get_heartbeat;
        preludedb_plugin_format_delete_func_t delete;
        preludedb_plugin_format_delete_alert_func_t delete_alert;
        preludedb_plugin_format_delete_alert_from_list_func_t delete_alert_from_list;
        preludedb_plugin_format_delete_alert_from_result_idents_func_t delete_alert_from_result_idents;
        preludedb_plugin_format_delete_heartbeat_func_t delete_heartbeat;
        preludedb_plugin_format_delete_heartbeat_from_list_func_t delete_heartbeat_from_list;
        preludedb_plugin_format_delete_heartbeat_from_result_idents_func_t delete_heartbeat_from_result_idents;
        preludedb_plugin_format_insert_message_func_t insert_message;
        preludedb_plugin_format_get_values_func_t get_values;
        preludedb_plugin_format_get_result_values_count_func_t get_result_values_count;
        preludedb_plugin_format_get_result_values_row_func_t get_result_values_row;
        preludedb_plugin_format_get_result_values_field_func_t get_result_values_field;
        preludedb_plugin_format_destroy_values_resource_func_t destroy_values_resource;
        preludedb_plugin_format_update_func_t update;
        preludedb_plugin_format_update_from_list_func_t update_from_list;
        preludedb_plugin_format_update_from_result_idents_func_t update_from_result_idents;
        preludedb_plugin_format_get_path_column_count_func_t get_path_column_count;
        preludedb_plugin_format_path_resolve_func_t path_resolve;
        preludedb_plugin_format_init_func_t init;
        preludedb_plugin_format_init_func_t optimize;
        preludedb_plugin_format_destroy_func_t destroy_func;
};

#endif
