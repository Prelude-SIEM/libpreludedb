/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
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

#include "idmef-db-insert.h"
#include "idmef-db-select.h"
#include "idmef-db-get.h"
#include "idmef-db-delete.h"
#include "db-path.h"


prelude_plugin_generic_t *classic_LTX_prelude_plugin_init(void);


#define CONFIG_FILE FORMAT_CONFIG_DIR"/classic/schema.txt"

struct db_value_info {
	idmef_value_type_id_t type;
	preludedb_selected_path_flags_t flags;
};

struct db_result {
	preludedb_sql_table_t *table;
	struct db_value_info *values_info;
	unsigned int value_count;
};



static int classic_get_alert_idents(preludedb_sql_t *sql, idmef_criteria_t *criteria,
				    int limit, int offset, preludedb_result_idents_order_t order,
				    void **res)
{
	return idmef_db_select_idents(sql, 'A', criteria, limit, offset, order,
				      (preludedb_sql_table_t **) res);
}



static int classic_get_heartbeat_idents(preludedb_sql_t *sql, idmef_criteria_t *criteria,
					int limit, int offset, preludedb_result_idents_order_t order,
					void **res)
{
	return idmef_db_select_idents(sql, 'H', criteria, limit, offset, order,
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

	ret = preludedb_sql_row_fetch_field(row, 0, &field);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_field_to_uint64(field, ident);

	return (ret < 0) ? ret : 1;
}



static void classic_destroy_message_idents_resource(void *res)
{
	preludedb_sql_table_destroy(res);
}



static int classic_get_alert(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message)
{
	return get_alert(sql, ident, message);
}



static int classic_get_heartbeat(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message)
{
	return get_heartbeat(sql, ident, message);
}



static int classic_delete_alert(preludedb_sql_t *sql, uint64_t ident)
{
	return delete_alert(sql, ident);
}



static int classic_delete_heartbeat(preludedb_sql_t *sql, uint64_t ident)
{
	return delete_heartbeat(sql, ident);
}



static int classic_insert_idmef_message(preludedb_sql_t *sql, idmef_message_t *message)
{
	return idmef_db_insert(sql, message);
}



static int classic_get_values(preludedb_sql_t *sql, preludedb_path_selection_t *selection, 
			      idmef_criteria_t *criteria, int distinct, int limit, int offset, void **res)
{

	return idmef_db_select(sql, selection, criteria, distinct, limit, offset, AS_VALUES, (preludedb_sql_table_t **) res);
}



static int get_value(preludedb_sql_row_t *row, int cnt, preludedb_selected_path_t *selected, idmef_value_t **value)
{
	preludedb_selected_path_flags_t flags;
	idmef_path_t *path;
	idmef_value_type_id_t type;
	preludedb_sql_field_t *field;
	const char *char_val;
	int ret;

	flags = preludedb_selected_path_get_flags(selected);
	path = preludedb_selected_path_get_path(selected);
	type = idmef_path_get_value_type(path);

	ret = preludedb_sql_row_fetch_field(row, cnt, &field);
	if ( ret < 0 )
		return ret;

	if ( ret == 0 ) {
		*value = NULL;
		return 1;
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
		uint32_t gmtoff = 0, usec = 0;
		int retrieved = 1;
		idmef_time_t *time;

		if ( ! (flags & (PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN|PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX|
				 PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG|PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD)) ) {
			preludedb_sql_field_t *gmtoff_field, *usec_field;

			ret = preludedb_sql_row_fetch_field(row, cnt + 1, &gmtoff_field);
			if ( ret < 0 )
				return ret;

			if ( ret > 0 ) {
				ret = preludedb_sql_field_to_uint32(gmtoff_field, &gmtoff);
				if ( ret < 0 )
					return ret;
			}

			ret = preludedb_sql_row_fetch_field(row, cnt + 2, &usec_field);
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
		if ( ret < 0 )
			idmef_time_destroy(time);
	}
	default:
		ret = idmef_value_new_from_path(value, path, char_val);
	}

	return ret;
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

		ret = get_value(row, value_cnt, selected, *values + value_cnt);
		if ( ret < 0 )
			break;

		sql_cnt += ret;
	}

	if ( ret < 0 ) {
		unsigned int cnt;

		for ( cnt = 0; cnt < value_cnt; cnt++ )
			idmef_value_destroy((*values)[cnt]);
	}

	return value_cnt;
}



static void classic_destroy_values_resource(void *res)
{
	preludedb_sql_table_destroy(res);
}



prelude_plugin_generic_t *classic_LTX_prelude_plugin_init(void)
{
	static preludedb_plugin_format_t plugin;

	memset(&plugin, 0, sizeof (plugin));
        
        prelude_plugin_set_name(&plugin, "Classic");
        prelude_plugin_set_desc(&plugin, "Prelude 0.8.0 database format");

	preludedb_plugin_format_set_get_alert_idents_func(&plugin, classic_get_alert_idents);
	preludedb_plugin_format_set_get_heartbeat_idents_func(&plugin, classic_get_heartbeat_idents);
	preludedb_plugin_format_set_get_message_ident_count_func(&plugin, classic_get_message_ident_count);
	preludedb_plugin_format_set_get_next_message_ident_func(&plugin, classic_get_next_message_ident);
	preludedb_plugin_format_set_destroy_message_idents_resource_func(&plugin, classic_destroy_message_idents_resource);
	preludedb_plugin_format_set_get_alert_func(&plugin, classic_get_alert);
	preludedb_plugin_format_set_get_heartbeat_func(&plugin, classic_get_heartbeat);
	preludedb_plugin_format_set_delete_alert_func(&plugin, classic_delete_alert);
	preludedb_plugin_format_set_delete_heartbeat_func(&plugin, classic_delete_heartbeat);
	preludedb_plugin_format_set_insert_message_func(&plugin, classic_insert_idmef_message);
	preludedb_plugin_format_set_get_values_func(&plugin, classic_get_values);
	preludedb_plugin_format_set_get_next_values_func(&plugin, classic_get_next_values);
	preludedb_plugin_format_set_destroy_values_resource_func(&plugin, classic_destroy_values_resource);

	db_paths_init(CONFIG_FILE);

	return (void *) &plugin;
}
