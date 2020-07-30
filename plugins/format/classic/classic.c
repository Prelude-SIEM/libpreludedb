/*****
*
* Copyright (C) 2003-2020 CS GROUP - France. All Rights Reserved.
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
#include "classic-path-resolve.h"


#define CLASSIC_SCHEMA_VERSION "14.8"


int classic_LTX_prelude_plugin_version(void);
int classic_get_path_column_count(preludedb_selected_path_t *selected);
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

int classic_unescape_binary_safe(preludedb_sql_t *sql, preludedb_sql_field_t *field,
                                 idmef_additional_data_type_t type, unsigned char **output, size_t *outsize);

int classic_unescape_binary_safe(preludedb_sql_t *sql, preludedb_sql_field_t *field,
                                 idmef_additional_data_type_t type, unsigned char **output, size_t *outsize)
{
        int ret;
        size_t size;
        unsigned char *value;

        ret = preludedb_sql_unescape_binary(sql,
                                            preludedb_sql_field_get_value(field),
                                            preludedb_sql_field_get_len(field),
                                            (unsigned char **) &value, &size);
        if ( ret < 0 )
                return ret;


        if ( type == IDMEF_ADDITIONAL_DATA_TYPE_CHARACTER || type == IDMEF_ADDITIONAL_DATA_TYPE_BYTE_STRING ) {
                /*
                 * These are the only two case where we don't need to append a terminating 0
                 */
                *outsize = size;
                *output = value;
        }
        else {
                if ( (size + 1) < size )
                        return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "Value is too big");

                *output = malloc(size + 1);
                if ( ! *output )
                        return preludedb_error_from_errno(errno);

                memcpy(*output, value, size);
                (*output)[size] = 0;
                *outsize = size;

                free(value);
        }

        return 0;
}


static int get_message_idents_set_order(idmef_class_id_t message_type, const preludedb_path_selection_t *order,
                                        classic_sql_join_t *join, preludedb_sql_select_t *select)
{
        preludedb_selected_path_t *selected_path = NULL;
        int ret;

        while ( (selected_path = preludedb_path_selection_get_next(order, selected_path)) ) {
                ret = preludedb_sql_select_add_selected(select, selected_path, join);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}



static int get_message_idents(preludedb_t *db, idmef_class_id_t message_type,
                              idmef_criteria_t *criteria, int limit, int offset,
                              const preludedb_path_selection_t *order,
                              preludedb_sql_table_t **table)
{
        prelude_string_t *query;
        prelude_string_t *where = NULL;
        classic_sql_join_t *join;
        preludedb_sql_t *sql = preludedb_get_sql(db);
        preludedb_sql_select_t *select;
        int ret;

        ret = prelude_string_new(&query);
        if ( ret < 0 )
                return ret;

        ret = classic_sql_join_new(&join);
        if ( ret < 0 ) {
                prelude_string_destroy(query);
                return ret;
        }

        ret = preludedb_sql_select_new(db, &select);
        if ( ret < 0 ) {
                prelude_string_destroy(query);
                classic_sql_join_destroy(join);
                return ret;
        }

        classic_sql_join_set_top_class(join, message_type);

        ret = preludedb_sql_select_add_field(select, "DISTINCT(top_table._ident)");
        if ( ret < 0 )
                goto error;

        if ( order ) {
                ret = get_message_idents_set_order(message_type, order, join, select);
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

        ret = preludedb_sql_select_fields_to_string(select, query);
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

        ret = preludedb_sql_select_modifiers_to_string(select, query);
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
        preludedb_sql_select_destroy(select);

        return ret;
}



static int classic_get_alert_idents(preludedb_t *db, idmef_criteria_t *criteria,
                                    int limit, int offset, const preludedb_path_selection_t *order,
                                    void **res)
{
        return get_message_idents(db, IDMEF_CLASS_ID_ALERT, criteria, limit, offset, order,
                                  (preludedb_sql_table_t **) res);
}



static int classic_get_heartbeat_idents(preludedb_t *db, idmef_criteria_t *criteria,
                                        int limit, int offset, const preludedb_path_selection_t *order,
                                        void **res)
{
        return get_message_idents(db, IDMEF_CLASS_ID_HEARTBEAT, criteria, limit, offset, order,
                                  (preludedb_sql_table_t **) res);
}



static size_t classic_get_message_ident_count(void *res)
{
        return preludedb_sql_table_get_row_count(res);
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



static int classic_get_values(preludedb_t *db, preludedb_path_selection_t *selection,
                              idmef_criteria_t *criteria, int distinct, int limit, int offset, void **res)
{
        prelude_string_t *where = NULL;
        prelude_string_t *query;
        classic_sql_join_t *join;
        preludedb_sql_select_t *select;
        int ret;

        ret = classic_sql_join_new(&join);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_select_new(db, &select);
        if ( ret < 0 ) {
                classic_sql_join_destroy(join);
                return ret;
        }

        ret = prelude_string_new(&query);
        if ( ret < 0 ) {
                classic_sql_join_destroy(join);
                preludedb_sql_select_destroy(select);
                return ret;
        }

        ret = preludedb_sql_select_add_selection(select, selection, join);
        if ( ret < 0 )
                goto error;

        if ( criteria ) {
                ret = prelude_string_new(&where);
                if ( ret < 0 )
                        goto error;

                ret = classic_path_resolve_criteria(preludedb_get_sql(db), criteria, join, where);
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

        ret = preludedb_sql_select_fields_to_string(select, query);
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

        ret = preludedb_sql_select_modifiers_to_string(select, query);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_build_limit_offset_string(preludedb_get_sql(db), limit, offset, query);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_query(preludedb_get_sql(db), prelude_string_get_string(query), (preludedb_sql_table_t **) res);

 error:
        prelude_string_destroy(query);
        if ( where )
                prelude_string_destroy(where);
        classic_sql_join_destroy(join);
        preludedb_sql_select_destroy(select);

        return ret;
}


static int get_value_time(preludedb_selected_path_t *selected,
                          preludedb_sql_row_t *row, preludedb_sql_field_t *field, int cnt, idmef_time_t **time)
{
        int ret;
        uint32_t usec = 0;
        int32_t gmtoff = 0;
        int retrieved = 1;
        unsigned int num_field;

        num_field = classic_get_path_column_count(selected);

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

                retrieved++;
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

                retrieved ++;
        }

        ret = idmef_time_new(time);
        if ( ret < 0 )
                return ret;

        preludedb_sql_time_from_timestamp(*time, preludedb_sql_field_get_value(field), gmtoff, usec);
        return retrieved;
}


static int get_data(preludedb_sql_t *sql, preludedb_sql_row_t *row, preludedb_sql_field_t *field, int cnt,
                    preludedb_selected_path_t *selected, idmef_value_type_id_t *type, unsigned char **unescaped, size_t *len)
{
        int ret;
        preludedb_sql_field_t *typefield;
        idmef_additional_data_type_t dtype;

        if ( classic_get_path_column_count(selected) != 2 )
                return 0;

        ret = preludedb_sql_row_get_field(row, cnt + 1, &typefield);
        if ( ret <= 0 )
                return ret;

        dtype = idmef_class_enum_to_numeric(IDMEF_CLASS_ID_ADDITIONAL_DATA_TYPE, preludedb_sql_field_get_value(typefield));
        if ( dtype < 0 )
                return dtype;

        switch(dtype) {
                case IDMEF_ADDITIONAL_DATA_TYPE_STRING:
                case IDMEF_ADDITIONAL_DATA_TYPE_CHARACTER:
                case IDMEF_ADDITIONAL_DATA_TYPE_PORTLIST:
                case IDMEF_ADDITIONAL_DATA_TYPE_XML:
                        *type = IDMEF_VALUE_TYPE_STRING;
                        break;

                case IDMEF_ADDITIONAL_DATA_TYPE_REAL:
                        *type = IDMEF_VALUE_TYPE_FLOAT;
                        break;

                case IDMEF_ADDITIONAL_DATA_TYPE_INTEGER:
                case IDMEF_ADDITIONAL_DATA_TYPE_BOOLEAN:
                case IDMEF_ADDITIONAL_DATA_TYPE_NTPSTAMP:
                        *type = IDMEF_VALUE_TYPE_INT64;
                        break;

                case IDMEF_ADDITIONAL_DATA_TYPE_BYTE:
                case IDMEF_ADDITIONAL_DATA_TYPE_BYTE_STRING:
                        *type = IDMEF_VALUE_TYPE_DATA;
                        break;

                case IDMEF_ADDITIONAL_DATA_TYPE_DATE_TIME:
                        *type = IDMEF_VALUE_TYPE_TIME;
                        break;

                case IDMEF_ADDITIONAL_DATA_TYPE_ERROR:
                        return -1;
        }

        ret = classic_unescape_binary_safe(sql, field, dtype, unescaped, len);
        return (ret < 0) ? ret : 1;
}


static int get_value(preludedb_sql_t *sql, preludedb_sql_row_t *row, int cnt, preludedb_selected_path_t *selected, preludedb_result_values_get_field_cb_func_t cb, void **out)
{
        char *char_val;
        unsigned char *unescaped = NULL;
        size_t len;
        preludedb_sql_field_t *field;
        idmef_value_type_id_t type, orig_type;
        unsigned int retrieved = 1;
        int ret;
        const void *data;
        preludedb_selected_object_type_t datatype;

        ret = preludedb_sql_row_get_field(row, cnt, &field);
        if ( ret < 0 )
                return ret;

        if ( ret == 0 )
                return cb(out, NULL, 0, 0);

        orig_type = type = preludedb_selected_object_get_value_type(preludedb_selected_path_get_object(selected), &data, &datatype);
        char_val = preludedb_sql_field_get_value(field);
        len = preludedb_sql_field_get_len(field);

        if ( type == IDMEF_VALUE_TYPE_DATA ) {
                ret = get_data(sql, row, field, cnt, selected, &type, &unescaped, &len);
                if ( ret < 0 )
                        return ret;

                retrieved += ret;
                if ( ret )
                        char_val = (char *) unescaped;
        }

        else if ( type == IDMEF_VALUE_TYPE_ENUM )
                type = IDMEF_VALUE_TYPE_STRING;

        switch ( type ) {
        case IDMEF_VALUE_TYPE_TIME: {
                idmef_time_t *time;

                if ( orig_type == IDMEF_VALUE_TYPE_DATA )
                        ret = idmef_time_new_from_string(&time, char_val);
                else
                        ret = get_value_time(selected, row, field, cnt, &time);

                if ( ret < 0 )
                        return ret;

                retrieved += ret;
                ret = cb(out, time, 0, type);
                idmef_time_destroy(time);
                break;
        }

        default:
                ret = cb(out, char_val, len, type);
                break;
        }

        if ( unescaped )
                free(unescaped);

        return (ret < 0) ? ret : retrieved;
}


static int classic_get_result_values_field(preludedb_result_values_t *results, void *row, preludedb_selected_path_t *selected, preludedb_result_values_get_field_cb_func_t cb, void **out)
{
        unsigned int cnum;

        cnum = preludedb_selected_path_get_column_index(selected);
        if ( cnum < 0 )
                return cnum;

        return get_value(preludedb_get_sql(preludedb_result_values_get_db(results)), row, cnum, selected, cb, out);
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
        const void *data;
        idmef_value_type_id_t vtype;
        preludedb_selected_object_t *object;
        preludedb_selected_object_type_t datatype;

        object = preludedb_selected_path_get_object(selected);

        if ( preludedb_selected_object_get_type(object) != PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH )
                return 1; /* anything that is not an IDMEF path is one field */

        if ( preludedb_selected_path_get_flags(selected) & PRELUDEDB_SELECTED_PATH_FLAGS_GROUP_BY )
                return 1; /* if it's an IDMEF path, but we use group by, then it's one field too */

        vtype = preludedb_selected_object_get_value_type(object, &data, &datatype);
        prelude_return_val_if_fail(datatype == PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH, -1);

        if ( idmef_path_get_class(data, idmef_path_get_depth(data) - 2) == IDMEF_CLASS_ID_ADDITIONAL_DATA && vtype == IDMEF_VALUE_TYPE_DATA )
                return 2;

        if ( vtype == IDMEF_VALUE_TYPE_TIME )
                return idmef_path_get_depth(data) == 2 ? 3 : 2;

        return 1;
}



static int classic_check_schema_version(const char *version)
{
        int ret;
        unsigned int schema_version, code_version;

        if ( ! version )
                return preludedb_error(PRELUDEDB_ERROR_SCHEMA_VERSION_INVALID);

        ret = prelude_parse_version(version, &schema_version);
        if ( ret < 0 )
                return ret;

        ret = prelude_parse_version(CLASSIC_SCHEMA_VERSION, &code_version);
        if ( ret < 0 )
                return ret;

        if ( schema_version > code_version )
                return preludedb_error_verbose(PRELUDEDB_ERROR_SCHEMA_VERSION_TOO_RECENT,
                                               "Database schema version %s is too recent (%s required)",
                                               version, CLASSIC_SCHEMA_VERSION);

        if ( schema_version < code_version )
                return preludedb_error_verbose(PRELUDEDB_ERROR_SCHEMA_VERSION_TOO_OLD,
                                               "Database schema version %s is too old (%s required)",
                                               version, CLASSIC_SCHEMA_VERSION);

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
        preludedb_plugin_format_set_get_result_values_row_func(plugin, classic_get_result_values_row);
        preludedb_plugin_format_set_get_result_values_field_func(plugin, classic_get_result_values_field);
        preludedb_plugin_format_set_get_result_values_count_func(plugin, classic_get_result_values_count);

        preludedb_plugin_format_set_destroy_values_resource_func(plugin, classic_destroy_values_resource);
        preludedb_plugin_format_set_get_path_column_count_func(plugin, classic_get_path_column_count);
        preludedb_plugin_format_set_path_resolve_func(plugin, classic_path_resolve);

        return 0;
}



int classic_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}
