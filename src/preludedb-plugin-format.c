/*****
*
* Copyright (C) 2005-2016 CS-SI. All Rights Reserved.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "preludedb.h"
#include "preludedb-plugin-format-prv.h"


void preludedb_plugin_format_set_check_schema_version_func(preludedb_plugin_format_t *plugin,
                                                           preludedb_plugin_format_check_schema_version_func_t func)
{
        plugin->check_schema_version = func;
}


void preludedb_plugin_format_set_get_alert_idents_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_get_alert_idents_func_t func)
{
        plugin->get_alert_idents = func;
}


void preludedb_plugin_format_set_get_heartbeat_idents_func(preludedb_plugin_format_t *plugin,
                                                           preludedb_plugin_format_get_heartbeat_idents_func_t func)
{
        plugin->get_heartbeat_idents = func;
}



void preludedb_plugin_format_set_get_message_ident_count_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_get_message_ident_count_func_t func)
{
        plugin->get_message_ident_count = func;
}


void preludedb_plugin_format_set_get_message_ident_func(preludedb_plugin_format_t *plugin,
                                                        preludedb_plugin_format_get_message_ident_func_t func)
{
        plugin->get_message_ident = func;
}


void preludedb_plugin_format_set_destroy_message_idents_resource_func(preludedb_plugin_format_t *plugin,
                                                                      preludedb_plugin_format_destroy_message_idents_resource_func_t func)
{
        plugin->destroy_message_idents_resource = func;
}


void preludedb_plugin_format_set_get_alert_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_get_alert_func_t func)
{
        plugin->get_alert = func;
}



void preludedb_plugin_format_set_get_heartbeat_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_get_heartbeat_func_t func)
{
        plugin->get_heartbeat = func;
}


void preludedb_plugin_format_set_delete_alert_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_delete_alert_func_t func)
{
        plugin->delete_alert = func;
}


void preludedb_plugin_format_set_delete_alert_from_list_func(preludedb_plugin_format_t *plugin,
                                                             preludedb_plugin_format_delete_alert_from_list_func_t func)
{
        plugin->delete_alert_from_list = func;
}


void preludedb_plugin_format_set_delete_alert_from_result_idents_func(preludedb_plugin_format_t *plugin,
                                                                      preludedb_plugin_format_delete_alert_from_result_idents_func_t func)
{
        plugin->delete_alert_from_result_idents = func;
}


void preludedb_plugin_format_set_delete_heartbeat_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_delete_heartbeat_func_t func)
{
        plugin->delete_heartbeat = func;
}


void preludedb_plugin_format_set_delete_heartbeat_from_list_func(preludedb_plugin_format_t *plugin,
                                                                 preludedb_plugin_format_delete_heartbeat_from_list_func_t func)
{
        plugin->delete_heartbeat_from_list = func;
}


void preludedb_plugin_format_set_delete_heartbeat_from_result_idents_func(preludedb_plugin_format_t *plugin,
                                                                          preludedb_plugin_format_delete_heartbeat_from_result_idents_func_t func)
{
        plugin->delete_heartbeat_from_result_idents = func;
}


void preludedb_plugin_format_set_insert_message_func(preludedb_plugin_format_t *plugin,
                                                     preludedb_plugin_format_insert_message_func_t func)
{
        plugin->insert_message = func;
}



void preludedb_plugin_format_set_get_values_func(preludedb_plugin_format_t *plugin,
                                                 preludedb_plugin_format_get_values_func_t func)
{
        plugin->get_values = func;
}


void preludedb_plugin_format_set_get_result_values_count_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_get_result_values_count_func_t func)
{
        plugin->get_result_values_count = func;
}


void preludedb_plugin_format_set_get_result_values_field_func(preludedb_plugin_format_t *plugin,
                                                         preludedb_plugin_format_get_result_values_field_func_t func)
{
        plugin->get_result_values_field = func;
}


void preludedb_plugin_format_set_get_result_values_row_func(preludedb_plugin_format_t *plugin,
                                                            preludedb_plugin_format_get_result_values_row_func_t func)
{
        plugin->get_result_values_row = func;
}


void preludedb_plugin_format_set_destroy_values_resource_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_destroy_values_resource_func_t func)
{
        plugin->destroy_values_resource = func;
}


void preludedb_plugin_format_set_update_from_result_idents_func(preludedb_plugin_format_t *plugin,
                                                                preludedb_plugin_format_update_from_result_idents_func_t func)
{
        plugin->update_from_result_idents = func;
}


void preludedb_plugin_format_set_update_from_list_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_update_from_list_func_t func)
{
        plugin->update_from_list = func;
}


void preludedb_plugin_format_set_update_func(preludedb_plugin_format_t *plugin,
                                             preludedb_plugin_format_update_func_t func)
{
        plugin->update = func;
}


void preludedb_plugin_format_set_get_path_column_count_func(preludedb_plugin_format_t *plugin,
                                                            preludedb_plugin_format_get_path_column_count_func_t func)
{
        plugin->get_path_column_count = func;
}



/**
 * preludedb_plugin_format_set_path_resolve_func
 * @plugin: Plugin object the @func function applies to
 * @func: Pointer to a path resolution function
 *
 * Setter for plugin supporting IDMEF path resolution
 */
void preludedb_plugin_format_set_path_resolve_func(preludedb_plugin_format_t *plugin,
                                                    preludedb_plugin_format_path_resolve_func_t func)
{
        plugin->path_resolve = func;
}


/**
 * preludedb_plugin_format_set_init_func
 * @plugin: Plugin object the @func function applies to
 * @func: Pointer to an initialization function
 *
 * Setter for plugin supporting initialization
 */
void preludedb_plugin_format_set_init_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_init_func_t func)
{
        plugin->init = func;
}



/**
 * preludedb_plugin_format_set_optimize_func
 * @plugin: Plugin object the @func function applies to
 * @func: Pointer to an optimization function
 *
 * Setter for plugin supporting optimization
 */
void preludedb_plugin_format_set_optimize_func(preludedb_plugin_format_t *plugin, preludedb_plugin_format_optimize_func_t func)
{
        plugin->optimize = func;
}



int preludedb_plugin_format_new(preludedb_plugin_format_t **ret)
{
        *ret = calloc(1, sizeof(**ret));
        if ( ! *ret )
                return preludedb_error_from_errno(errno);

        return 0;
}



ssize_t _preludedb_plugin_format_delete_alert_from_list(preludedb_plugin_format_t *plugin,
                                                        preludedb_t *db, uint64_t *idents, size_t size)
{
        size_t i;
        int ret = 0;

        if ( plugin->delete_alert_from_list )
                return plugin->delete_alert_from_list(db, idents, size);

        for ( i = 0; i < size; i++ ) {
                ret = plugin->delete_alert(db, idents[i]);
                if ( ret < 0 )
                        return ret;
        }

        return i;
}



ssize_t _preludedb_plugin_format_delete_alert_from_result_idents(preludedb_plugin_format_t *plugin,
                                                                 preludedb_t *db, preludedb_result_idents_t *result)
{
        int ret;
        uint64_t ident;
        unsigned int count = 0;

        if ( plugin->delete_alert_from_result_idents )
                return plugin->delete_alert_from_result_idents(db, result);

        while ( (ret = preludedb_result_idents_get(result, count, &ident)) ) {

                ret = plugin->delete_alert(db, ident);
                if ( ret < 0 )
                        return ret;

                count++;
        }

        return count;
}



ssize_t _preludedb_plugin_format_delete_heartbeat_from_list(preludedb_plugin_format_t *plugin,
                                                            preludedb_t *db, uint64_t *idents, size_t size)
{
        size_t i;
        int ret = 0;

        if ( plugin->delete_heartbeat_from_list )
                return plugin->delete_heartbeat_from_list(db, idents, size);

        for ( i = 0; i < size; i++ ) {
                ret = plugin->delete_heartbeat(db, idents[i]);
                if ( ret < 0 )
                        return ret;
        }

        return i;
}



ssize_t _preludedb_plugin_format_delete_heartbeat_from_result_idents(preludedb_plugin_format_t *plugin,
                                                                     preludedb_t *db, preludedb_result_idents_t *result)
{
        int ret;
        uint64_t ident;
        unsigned int count = 0;

        if ( plugin->delete_heartbeat_from_result_idents )
                return plugin->delete_heartbeat_from_result_idents(db, result);

        while ( (ret = preludedb_result_idents_get(result, count, &ident)) ) {

                ret = plugin->delete_heartbeat(db, ident);
                if ( ret < 0 )
                        return ret;

                count++;
        }

        return count;
}
