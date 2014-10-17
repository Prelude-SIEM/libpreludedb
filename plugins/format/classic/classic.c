/*****
*
* Copyright (C) 2003-2012 CS-SI. All Rights Reserved.
* Author: Nicolas Delon <nicolas.delon@prelude-ids.com>
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
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include <libprelude/idmef.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"
#include "preludedb-plugin-format.h"

#include "classic-insert.h"
#include "classic-get.h"
#include "classic-delete.h"
#include "classic-sql-join.h"
#include "classic-sql-select.h"
#include "classic-path-resolve.h"


#define CLASSIC_SCHEMA_VERSION 14.7


int classic_LTX_prelude_plugin_version(void);
int classic_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data);
int classic_get_path_column_count(preludedb_selected_path_t *selected);


struct db_value_info {
        idmef_value_type_id_t type;
        preludedb_selected_path_flags_t flags;
};

struct db_result {
        preludedb_sql_table_t *table;
        struct db_value_info *values_info;
        unsigned int value_count;
};



static int get_message_idents_set_order(preludedb_sql_t *sql,
                                        idmef_class_id_t message_type, preludedb_result_idents_order_t order,
                                        classic_sql_join_t *join, classic_sql_select_t *select)
{
        preludedb_selected_path_t *selected_path;
        idmef_path_t *path;
        int ret;

        if ( message_type == IDMEF_CLASS_ID_ALERT )
                ret = idmef_path_new_fast(&path, "alert.create_time");
        else
                ret = idmef_path_new_fast(&path, "heartbeat.create_time");

        if ( ret < 0 )
                return ret;

        ret = preludedb_selected_path_new(&selected_path, path,
                                          (order == PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_DESC ?
                                           PRELUDEDB_SELECTED_OBJECT_ORDER_DESC :
                                           PRELUDEDB_SELECTED_OBJECT_ORDER_ASC));
        if ( ret < 0 ) {
                idmef_path_destroy(path);
                return ret;
        }

        ret = classic_path_resolve_selected(sql, selected_path, join, select);

        preludedb_selected_path_destroy(selected_path);

        return ret;
}



static int get_message_idents(preludedb_sql_t *sql, idmef_class_id_t message_type,
                              idmef_criteria_t *criteria, int limit, int offset,
                              preludedb_result_idents_order_t order,
                              preludedb_sql_table_t **table)
{
        prelude_string_t *query;
        prelude_string_t *where = NULL;
        classic_sql_join_t *join;
        classic_sql_select_t *select;
        int ret;

        ret = prelude_string_new(&query);
        if ( ret < 0 )
                return ret;

        ret = classic_sql_join_new(&join);
        if ( ret < 0 ) {
                prelude_string_destroy(query);
                return ret;
        }

        ret = classic_sql_select_new(&select);
        if ( ret < 0 ) {
                prelude_string_destroy(query);
                classic_sql_join_destroy(join);
                return ret;
        }

        classic_sql_join_set_top_class(join, message_type);

        ret = classic_sql_select_add_field(select, "DISTINCT(top_table._ident)", 0);
        if ( ret < 0 )
                goto error;

        if ( order ) {
                ret = get_message_idents_set_order(sql, message_type, order, join, select);
                if ( ret < 0 )
                        return ret;
        }

        if ( criteria ) {
                ret = prelude_string_new(&where);
                if ( ret < 0 )
                        goto error;

                ret = classic_path_resolve_criteria(sql, criteria, join, where);
                if ( ret < 0 ) {
                        prelude_string_destroy(where);
                        goto error;
                }
        }

        ret = prelude_string_sprintf(query, "SELECT ");
        if ( ret < 0 )
                goto error;

        ret = classic_sql_select_fields_to_string(select, query);
        if ( ret < 0 )
                goto error;

        ret = prelude_string_cat(query, " FROM ");
        if ( ret < 0 )
                goto error;

        ret = classic_sql_join_to_string(join, query);
        if ( ret < 0 )
                goto error;

        if ( where ) {
                ret = prelude_string_cat(query, " WHERE ");
                if ( ret < 0 )
                        goto error;

                ret = prelude_string_cat(query, prelude_string_get_string(where));
                if ( ret < 0 )
                        goto error;
        }

        ret = classic_sql_select_modifiers_to_string(select, query);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_build_limit_offset_string(sql, limit, offset, query);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_query(sql, prelude_string_get_string(query), table);

 error:
        prelude_string_destroy(query);
        if ( where )
                prelude_string_destroy(where);
        classic_sql_join_destroy(join);
        classic_sql_select_destroy(select);

        return ret;
}



static int classic_get_alert_idents(preludedb_sql_t *sql, idmef_criteria_t *criteria,
                                    int limit, int offset, preludedb_result_idents_order_t order,
                                    void **res)
{
        return get_message_idents(sql, IDMEF_CLASS_ID_ALERT, criteria, limit, offset, order,
                                  (preludedb_sql_table_t **) res);
}



static int classic_get_heartbeat_idents(preludedb_sql_t *sql, idmef_criteria_t *criteria,
                                        int limit, int offset, preludedb_result_idents_order_t order,
                                        void **res)
{
        return get_message_idents(sql, IDMEF_CLASS_ID_HEARTBEAT, criteria, limit, offset, order,
                                  (preludedb_sql_table_t **) res);
}



static size_t classic_get_message_ident_count(void *res)
{
        return preludedb_sql_table_get_row_count(res);
}



static int classic_get_next_message_ident(void *res, uint64_t *ident)
{
        preludedb_sql_row_t *row;
        preludedb_sql_field_t *field;
        int ret;

        ret = preludedb_sql_table_fetch_row(res, &row);
        if ( ret <= 0 )
                return ret;

        ret = preludedb_sql_row_get_field(row, 0, &field);
        if ( ret <= 0 )
                return ret;

        ret = preludedb_sql_field_to_uint64(field, ident);

        return (ret < 0) ? ret : 1;
}



static int classic_get_message_ident(void *res, unsigned int row_index, uint64_t *ident)
{
        preludedb_sql_row_t *row;
        preludedb_sql_field_t *field;
        int ret;

        ret = preludedb_sql_table_get_row(res, row_index, &row);
        if ( ret <= 0 )
                return ret;

        ret = preludedb_sql_row_get_field(row, 0, &field);
        if ( ret <= 0 )
                return ret;

        ret = preludedb_sql_field_to_uint64(field, ident);

        return (ret < 0) ? ret : 1;
}


static void classic_destroy_message_idents_resource(void *res)
{
        preludedb_sql_table_destroy(res);
}



static int classic_get_values(preludedb_sql_t *sql, preludedb_path_selection_t *selection,
                              idmef_criteria_t *criteria, int distinct, int limit, int offset, void **res)
{
        prelude_string_t *where = NULL;
        prelude_string_t *query;
        classic_sql_join_t *join;
        classic_sql_select_t *select;
        int ret;

        ret = classic_sql_join_new(&join);
        if ( ret < 0 )
                return ret;

        ret = classic_sql_select_new(&select);
        if ( ret < 0 ) {
                classic_sql_join_destroy(join);
                return ret;
        }

        ret = prelude_string_new(&query);
        if ( ret < 0 ) {
                classic_sql_join_destroy(join);
                classic_sql_select_destroy(select);
                return ret;
        }

        ret = classic_path_resolve_selection(sql, selection, join, select);
        if ( ret < 0 )
                goto error;

        if ( criteria ) {
                ret = prelude_string_new(&where);
                if ( ret < 0 )
                        goto error;

                ret = classic_path_resolve_criteria(sql, criteria, join, where);
                if ( ret < 0 )
                        goto error;
        }

        ret = prelude_string_cat(query, "SELECT ");
        if ( ret < 0 )
                goto error;

        if ( distinct ) {
                ret = prelude_string_cat(query, "DISTINCT ");
                if ( ret < 0 )
                        goto error;
        }

        ret = classic_sql_select_fields_to_string(select, query);
        if ( ret < 0 )
                goto error;

        ret = prelude_string_cat(query, " FROM ");
        if ( ret < 0 )
                goto error;

        ret = classic_sql_join_to_string(join, query);
        if ( ret < 0 )
                goto error;

        if ( where ) {
                ret = prelude_string_sprintf(query, " WHERE %s", prelude_string_get_string(where));
                if ( ret < 0 )
                        goto error;
        }

        ret = classic_sql_select_modifiers_to_string(select, query);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_build_limit_offset_string(sql, limit, offset, query);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_query(sql, prelude_string_get_string(query), (preludedb_sql_table_t **) res);

 error:
        prelude_string_destroy(query);
        if ( where )
                prelude_string_destroy(where);
        classic_sql_join_destroy(join);
        classic_sql_select_destroy(select);

        return ret;
}



static int get_value(preludedb_sql_row_t *row, int cnt, preludedb_selected_path_t *selected, idmef_value_t **value)
{
        preludedb_selected_path_flags_t flags;
        idmef_path_t *path;
        idmef_value_type_id_t type;
        preludedb_sql_field_t *field;
        const char *char_val;
        unsigned int retrieved = 1;
        int ret, num_field;

        flags = preludedb_selected_path_get_flags(selected);
        path = preludedb_selected_path_get_path(selected);
        type = idmef_path_get_value_type(path, idmef_path_get_depth(path) - 1);

        ret = preludedb_sql_row_get_field(row, cnt, &field);
        if ( ret < 0 )
                return ret;

        num_field = classic_get_path_column_count(selected);
        if ( ret == 0 ) {
                *value = NULL;
                return num_field;
        }

        char_val = preludedb_sql_field_get_value(field);

        if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT ) {
               uint32_t count;

               ret = preludedb_sql_field_to_uint32(field, &count);
               if ( ret < 0 )
                       return ret;

               ret = idmef_value_new_uint32(value, count);
               if ( ret < 0 )
                       return ret;

               return 1;
        }


        switch ( type ) {
        case IDMEF_VALUE_TYPE_TIME: {
                uint32_t usec = 0;
                int32_t gmtoff = 0;
                idmef_time_t *time;

                if ( num_field > 1 ) {
                        preludedb_sql_field_t *gmtoff_field;

                        ret = preludedb_sql_row_get_field(row, cnt + 1, &gmtoff_field);
                        if ( ret < 0 )
                                return ret;

                        if ( ret > 0 ) {
                                ret = preludedb_sql_field_to_int32(gmtoff_field, &gmtoff);
                                if ( ret < 0 )
                                        return ret;
                        }
                }

                if ( num_field > 2 ) {
                        preludedb_sql_field_t *usec_field;

                        ret = preludedb_sql_row_get_field(row, cnt + 2, &usec_field);
                        if ( ret < 0 )
                                return ret;

                        if ( ret > 0 ) {
                                if ( preludedb_sql_field_to_uint32(usec_field, &usec) < 0 )
                                        return ret;
                        }

                        retrieved += 2;
                }

                ret = idmef_time_new(&time);
                if ( ret < 0 )
                        return ret;

                preludedb_sql_time_from_timestamp(time, char_val, gmtoff, usec);

                ret = idmef_value_new_time(value, time);
                if ( ret < 0 ) {
                        idmef_time_destroy(time);
                        return ret;
                }

                break;
        }
        default:
                ret = idmef_value_new_from_path(value, path, char_val);
                if ( ret < 0 )
                        return ret;
        }

        return retrieved;
}



static int classic_get_next_values(void *res, preludedb_path_selection_t *selection, idmef_value_t ***values)
{
        preludedb_sql_row_t *row;
        preludedb_selected_path_t *selected;
        unsigned int sql_cnt, value_cnt;
        unsigned int column_count;
        int ret;

        ret = preludedb_sql_table_fetch_row(res, &row);
        if ( ret <= 0 )
                return ret;

        column_count = preludedb_path_selection_get_count(selection);

        *values = malloc(column_count * sizeof (**values));
        if ( ! *values )
                return preludedb_error_from_errno(errno);

        selected = NULL;
        sql_cnt = 0;
        for ( value_cnt = 0; value_cnt < column_count; value_cnt++ ) {
                selected = preludedb_path_selection_get_next(selection, selected);

                ret = get_value(row, sql_cnt, selected, *values + value_cnt);
                if ( ret < 0 )
                        break;

                sql_cnt += ret;
        }

        if ( ret < 0 ) {
                unsigned int cnt;

                for ( cnt = 0; cnt < value_cnt; cnt++ )
                        idmef_value_destroy((*values)[cnt]);

                free(*values);
                return ret;
        }

        return value_cnt;
}


static int classic_get_result_values_field(preludedb_result_values_t *results, void *row, preludedb_selected_path_t *selected, idmef_value_t **values)
{
        unsigned int cnum;

        cnum = preludedb_selected_path_get_column_index(selected);
        if ( cnum < 0 )
                return cnum;

        return get_value(row, cnum, selected, values);
}


static int classic_get_result_values_row(preludedb_result_values_t *results, unsigned int rnum, void **row)
{
        return preludedb_sql_table_get_row(preludedb_result_values_get_data(results), rnum, (preludedb_sql_row_t **) row);
}


static int classic_get_result_values_count(preludedb_result_values_t *results)
{
        return preludedb_sql_table_get_row_count(preludedb_result_values_get_data(results));
}


static void classic_destroy_values_resource(void *res)
{
        preludedb_sql_table_destroy(res);
}


int classic_get_path_column_count(preludedb_selected_path_t *selected)
{
        idmef_path_t *path;
        idmef_value_type_id_t type;
        preludedb_sql_time_constraint_type_t tc;
        preludedb_selected_path_flags_t flags;

        path = preludedb_selected_path_get_path(selected);
        tc = preludedb_selected_path_get_time_constraint(selected);
        flags = preludedb_selected_path_get_flags(selected);
        type = idmef_path_get_value_type(path, -1);

        if ( type == IDMEF_VALUE_TYPE_TIME && ! tc &&
             ! (flags & (PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN|PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX|
                         PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG|PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD|
                         PRELUDEDB_SELECTED_OBJECT_GROUP_BY|PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT)) )
                return idmef_path_get_depth(path) == 2 ? 3 : 2;

        return 1;
}



static int classic_check_schema_version(const char *version)
{
        double d;
        char *eptr;

        if ( ! version )
                return preludedb_error(PRELUDEDB_ERROR_SCHEMA_VERSION_INVALID);

        d = prelude_simple_strtod(version, &eptr);
        if ( *eptr != 0 )
                return preludedb_error(PRELUDEDB_ERROR_SCHEMA_VERSION_INVALID);

        if ( d > CLASSIC_SCHEMA_VERSION )
                return preludedb_error_verbose(PRELUDEDB_ERROR_SCHEMA_VERSION_TOO_RECENT,
                                               "Database schema version %g is too recent (%g required)",
                                               d, CLASSIC_SCHEMA_VERSION);

        if ( d < CLASSIC_SCHEMA_VERSION )
                return preludedb_error_verbose(PRELUDEDB_ERROR_SCHEMA_VERSION_TOO_OLD,
                                               "Database schema version %g is too old (%g required)",
                                               d, CLASSIC_SCHEMA_VERSION);

        return 0;
}



int classic_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data)
{
        int ret;
        preludedb_plugin_format_t *plugin;

        ret = preludedb_plugin_format_new(&plugin);
        if ( ret < 0 )
                return ret;

        prelude_plugin_set_name((prelude_plugin_generic_t *) plugin, "Classic");
        prelude_plugin_entry_set_plugin(pe, (void *) plugin);

        preludedb_plugin_format_set_check_schema_version_func(plugin, classic_check_schema_version);
        preludedb_plugin_format_set_get_alert_idents_func(plugin, classic_get_alert_idents);
        preludedb_plugin_format_set_get_heartbeat_idents_func(plugin, classic_get_heartbeat_idents);
        preludedb_plugin_format_set_get_message_ident_count_func(plugin, classic_get_message_ident_count);
        preludedb_plugin_format_set_get_message_ident_func(plugin, classic_get_message_ident);
        preludedb_plugin_format_set_get_next_message_ident_func(plugin, classic_get_next_message_ident);
        preludedb_plugin_format_set_destroy_message_idents_resource_func(plugin,
                                                                         classic_destroy_message_idents_resource);
        preludedb_plugin_format_set_get_alert_func(plugin, classic_get_alert);
        preludedb_plugin_format_set_get_heartbeat_func(plugin, classic_get_heartbeat);
        preludedb_plugin_format_set_delete_alert_func(plugin, classic_delete_alert);
        preludedb_plugin_format_set_delete_alert_from_list_func(plugin, classic_delete_alert_from_list);
        preludedb_plugin_format_set_delete_alert_from_result_idents_func(plugin, classic_delete_alert_from_result_idents);
        preludedb_plugin_format_set_delete_heartbeat_func(plugin, classic_delete_heartbeat);
        preludedb_plugin_format_set_delete_heartbeat_from_list_func(plugin, classic_delete_heartbeat_from_list);
        preludedb_plugin_format_set_delete_heartbeat_from_result_idents_func(plugin, classic_delete_heartbeat_from_result_idents);

        preludedb_plugin_format_set_insert_message_func(plugin, classic_insert);
        preludedb_plugin_format_set_get_values_func(plugin, classic_get_values);
        preludedb_plugin_format_set_get_next_values_func(plugin, classic_get_next_values);

        preludedb_plugin_format_set_get_result_values_row_func(plugin, classic_get_result_values_row);
        preludedb_plugin_format_set_get_result_values_field_func(plugin, classic_get_result_values_field);
        preludedb_plugin_format_set_get_result_values_count_func(plugin, classic_get_result_values_count);

        preludedb_plugin_format_set_destroy_values_resource_func(plugin, classic_destroy_values_resource);
        preludedb_plugin_format_set_get_path_column_count_func(plugin, classic_get_path_column_count);

        return 0;
}



int classic_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}
