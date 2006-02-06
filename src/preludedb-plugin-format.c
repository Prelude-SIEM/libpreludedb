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


void preludedb_plugin_format_set_get_next_message_ident_func(preludedb_plugin_format_t *plugin,
                                                             preludedb_plugin_format_get_next_message_ident_func_t func)
{
        plugin->get_next_message_ident = func;
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



void preludedb_plugin_format_set_delete_heartbeat_func(preludedb_plugin_format_t *plugin,
                                                       preludedb_plugin_format_delete_heartbeat_func_t func)
{
        plugin->delete_heartbeat = func;
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


void preludedb_plugin_format_set_get_next_values_func(preludedb_plugin_format_t *plugin,
                                                      preludedb_plugin_format_get_next_values_func_t func)
{
        plugin->get_next_values = func;
}


void preludedb_plugin_format_set_destroy_values_resource_func(preludedb_plugin_format_t *plugin,
                                                              preludedb_plugin_format_destroy_values_resource_func_t func)
{
        plugin->destroy_values_resource = func;
}



int preludedb_plugin_format_new(preludedb_plugin_format_t **ret)
{
        *ret = calloc(1, sizeof(**ret));
        if ( ! *ret )
                return preludedb_error_from_errno(errno);

        return 0;
}
