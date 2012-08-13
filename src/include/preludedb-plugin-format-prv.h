/*****
*
* Copyright (C) 2005-2012 CS-SI. All Rights Reserved.
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
        preludedb_plugin_format_get_next_message_ident_func_t get_next_message_ident;
        preludedb_plugin_format_destroy_message_idents_resource_func_t destroy_message_idents_resource;
        preludedb_plugin_format_get_alert_func_t get_alert;
        preludedb_plugin_format_get_heartbeat_func_t get_heartbeat;
        preludedb_plugin_format_delete_alert_func_t delete_alert;
        preludedb_plugin_format_delete_alert_from_list_func_t delete_alert_from_list;
        preludedb_plugin_format_delete_alert_from_result_idents_func_t delete_alert_from_result_idents;
        preludedb_plugin_format_delete_heartbeat_func_t delete_heartbeat;
        preludedb_plugin_format_delete_heartbeat_from_list_func_t delete_heartbeat_from_list;
        preludedb_plugin_format_delete_heartbeat_from_result_idents_func_t delete_heartbeat_from_result_idents;
        preludedb_plugin_format_insert_message_func_t insert_message;
        preludedb_plugin_format_get_values_func_t get_values;
        preludedb_plugin_format_get_next_values_func_t get_next_values;
        preludedb_plugin_format_destroy_values_resource_func_t destroy_values_resource;
};

#endif
