/*****
*
* Copyright (C) 2005-2016 CS-SI. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoannv@gmail.com>
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
#include <string.h>
#include <stdarg.h>

#include <libprelude/prelude.h>

#include "preludedb-path-selection.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "preludedb-sql-select.h"
#include "preludedb-plugin-format.h"
#include "preludedb-plugin-format-prv.h"


preludedb_plugin_format_t *_preludedb_get_plugin_format(preludedb_t *db);


struct preludedb_sql_select {
        preludedb_t *db;
        prelude_string_t *fields;
        prelude_string_t *order_by;
        prelude_string_t *group_by;
        unsigned int field_count;
        unsigned int index;
        preludedb_sql_select_flags_t flags;
};



static const char *func_to_string(preludedb_selected_object_type_t type)
{
        switch (type) {
            case PRELUDEDB_SELECTED_OBJECT_TYPE_AVG:
                return "AVG";

            case PRELUDEDB_SELECTED_OBJECT_TYPE_COUNT:
                return "COUNT";

            case PRELUDEDB_SELECTED_OBJECT_TYPE_MAX:
                return "MAX";

            case PRELUDEDB_SELECTED_OBJECT_TYPE_MIN:
                return "MIN";

            case PRELUDEDB_SELECTED_OBJECT_TYPE_INTERVAL:
                return "INTERVAL";

            default:
                return NULL;
    }
}


static int preludedb_selected_object_to_string(preludedb_sql_select_t *select, preludedb_selected_path_t *selected,
                                               preludedb_selected_object_t *object, prelude_string_t *out, void *data, int depth)
{
        int ret = 0;
        preludedb_selected_object_type_t type;
        prelude_string_t *tmp1 = NULL, *tmp2 = NULL;
        preludedb_plugin_format_t *format = _preludedb_get_plugin_format(select->db);

        type = preludedb_selected_object_get_type(object);
        if ( type == PRELUDEDB_SELECTED_OBJECT_TYPE_STRING )
                return prelude_string_cat(out, preludedb_selected_object_get_data(object));

        else if ( type == PRELUDEDB_SELECTED_OBJECT_TYPE_INT )
                return prelude_string_sprintf(out, "%d", *(const int *) preludedb_selected_object_get_data(object));

        else if ( type == PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH )
                return format->path_resolve(selected, object, data, out);

        else if ( type == PRELUDEDB_SELECTED_OBJECT_TYPE_EXTRACT ) {
                ret = prelude_string_new(&tmp1);
                if ( ret < 0 )
                        goto error;

                ret = preludedb_selected_object_to_string(select, selected, preludedb_selected_object_get_arg(object, 0), tmp1, data, depth + 1);
                if ( ret < 0 )
                        goto error;

                ret = preludedb_sql_build_time_extract_string(preludedb_get_sql(select->db), out, prelude_string_get_string(tmp1),
                                                              *(const int *) preludedb_selected_object_get_data(preludedb_selected_object_get_arg(object, 1)), 0);
        }

        else if ( type == PRELUDEDB_SELECTED_OBJECT_TYPE_INTERVAL ) {
                ret = prelude_string_new(&tmp1);
                if ( ret < 0 )
                        goto error;

                ret = prelude_string_new(&tmp2); /* field, value, unit */
                if ( ret < 0 )
                        goto error;

                ret = preludedb_selected_object_to_string(select, selected, preludedb_selected_object_get_arg(object, 0), tmp1, data, depth + 1);
                if ( ret < 0 )
                        goto error;

                ret = preludedb_selected_object_to_string(select, selected, preludedb_selected_object_get_arg(object, 1), tmp2, data, depth + 1);
                if ( ret < 0 )
                        goto error;

                ret = preludedb_sql_build_time_interval_string(preludedb_get_sql(select->db), out, prelude_string_get_string(tmp1), prelude_string_get_string(tmp2),
                                                               *(const int *) preludedb_selected_object_get_data(preludedb_selected_object_get_arg(object, 2)));
        }

       else if ( type == PRELUDEDB_SELECTED_OBJECT_TYPE_TIMEZONE ) {
                ret = prelude_string_new(&tmp1);
                if ( ret < 0 )
                        goto error;

                ret = preludedb_selected_object_to_string(select, selected, preludedb_selected_object_get_arg(object, 0), tmp1, data, depth + 1);
                if ( ret < 0 )
                        goto error;

                ret = preludedb_sql_build_time_timezone_string(preludedb_get_sql(select->db), out, prelude_string_get_string(tmp1),
                                                               preludedb_selected_object_get_data(preludedb_selected_object_get_arg(object, 1)));
        }

        else if ( preludedb_selected_object_is_function(object) ) {
                const char *func = func_to_string(type);

                if ( ! func ) {
                        ret = preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "Unknown function enumeration '%d'", type);
                        goto error;
                }

                prelude_string_sprintf(out, "%s(", func);
                ret = preludedb_selected_object_to_string(select, selected, preludedb_selected_object_get_arg(object, 0), out, data, depth + 1);
                prelude_string_cat(out, ")");
        }

        if ( select->flags & PRELUDEDB_SQL_SELECT_FLAGS_ALIAS_FUNCTION && depth == 0 )
                prelude_string_sprintf(out, " AS FUNC%u", select->index++);

    error:
        if ( tmp1 )
                prelude_string_destroy(tmp1);

        if ( tmp2 )
                prelude_string_destroy(tmp2);

        return ret;
}



static int sql_select_add_options(preludedb_sql_select_t *select, unsigned int num_field, preludedb_selected_path_flags_t flags)
{
        int ret;

        do {
                if ( flags & PRELUDEDB_SELECTED_PATH_FLAGS_GROUP_BY ) {
                        if ( ! prelude_string_is_empty(select->group_by) ) {
                                ret = prelude_string_cat(select->group_by, ", ");
                                if ( ret < 0 )
                                        return ret;
                        }

                        ret = prelude_string_sprintf(select->group_by, "%d", select->field_count + 1);
                        if ( ret < 0 )
                                return ret;
                }

                if ( flags & (PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_ASC|PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_DESC) ) {
                        if ( ! prelude_string_is_empty(select->order_by) ) {
                                ret = prelude_string_cat(select->order_by, ", ");
                                if ( ret < 0 )
                                        return ret;
                        }

                        ret = prelude_string_sprintf(select->order_by, "%d %s", select->field_count + 1,
                                                     (flags & PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_ASC) ? "ASC" : "DESC");
                        if ( ret < 0 )
                                return ret;
                }

                select->field_count++;

        } while ( --num_field );

        return 0;
}



void preludedb_sql_select_set_flags(preludedb_sql_select_t *select, preludedb_sql_select_flags_t flags)
{
        select->flags = flags;
}



/**
 * preludedb_sql_select_new
 * @db: Pointer to a #preludedb_t object
 * @select: Pointer where to store the created object
 *
 * Create new #preludedb_sql_select_t object, and store it into @select.
 */
int preludedb_sql_select_new(preludedb_t *db, preludedb_sql_select_t **select)
{
        int ret;

        *select = calloc(1, sizeof (**select));
        if ( ! *select )
                return prelude_error_from_errno(errno);

        ret = prelude_string_new(&(*select)->fields);
        if ( ret < 0 ) {
                free(*select);
                return ret;
        }

        ret = prelude_string_new(&(*select)->order_by);
        if ( ret < 0 ) {
                prelude_string_destroy((*select)->fields);
                free(*select);
                return ret;
        }

        ret = prelude_string_new(&(*select)->group_by);
        if ( ret < 0 ) {
                prelude_string_destroy((*select)->fields);
                prelude_string_destroy((*select)->order_by);
                free(*select);
                return ret;
        }

        (*select)->flags = 0;
        (*select)->db = preludedb_ref(db);
        return 0;
}



/**
 * preludedb_sql_select_destroy
 * @select: Pointer to a #preludedb_sql_select_t object
 *
 * Destroy the @select object
 */
void preludedb_sql_select_destroy(preludedb_sql_select_t *select)
{
        preludedb_destroy(select->db);
        prelude_string_destroy(select->fields);
        prelude_string_destroy(select->order_by);
        prelude_string_destroy(select->group_by);
        free(select);
}



/**
 * preludedb_sql_select_add_selected
 * @select: Pointer to a #preludedb_sql_select_t object
 * @selpath: Pointer to a #preludedb_selected_path_t to add to @select
 * @data: Data to be used in the format plugin path_resolution function.
 *
 * Add the given @selpath object string to the fields of @select.
 *
 * Returns: 0 in case of success, a negative value if an error occured.
 */
int preludedb_sql_select_add_selected(preludedb_sql_select_t *select, preludedb_selected_path_t *selpath, void *data)
{
        int ret;

        if ( ! prelude_string_is_empty(select->fields) ) {
                ret = prelude_string_cat(select->fields, ", ");
                if ( ret < 0 )
                        return ret;
        }

        ret = preludedb_selected_object_to_string(select, selpath, preludedb_selected_path_get_object(selpath), select->fields, data, 0);
        if ( ret < 0 )
                return ret;

        return sql_select_add_options(select, preludedb_selected_path_get_column_count(selpath), preludedb_selected_path_get_flags(selpath));
}




/**
 * preludedb_sql_select_add_selection
 * @select: Pointer to a #preludedb_sql_select_t object
 * @selection: Pointer to a #preludedb_path_selection_t to add to @select
 * @data: Data to be used in the format plugin path_resolution function.
 *
 * Add the given @selection string to the fields of @select.
 *
 * Returns: 0 in case of success, a negative value if an error occured.
 */
int preludedb_sql_select_add_selection(preludedb_sql_select_t *select, preludedb_path_selection_t *selection, void *data)
{
        preludedb_selected_path_t *selected = NULL;
        int ret;

        while ( (selected = preludedb_path_selection_get_next(selection, selected)) ) {
                ret = preludedb_sql_select_add_selected(select, selected, data);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}



/**
 * preludedb_sql_select_add_field
 * @select: Pointer to a #preludedb_sql_select_t object
 * @selection: A selection string
 *
 * Add the given @selection string to the fields of @select.
 *
 * Returns: 0 in case of success, a negative value if an error occured.
 */
int preludedb_sql_select_add_field(preludedb_sql_select_t *select, const char *selection)
{
        int ret;

        if ( ! prelude_string_is_empty(select->fields) ) {
                ret = prelude_string_cat(select->fields, ", ");
                if ( ret < 0 )
                        return ret;
        }

        ret = prelude_string_cat(select->fields, selection);
        if ( ret < 0 )
                return ret;

        select->field_count++;
        return 0;
}



/**
 * preludedb_sql_select_fields_to_string
 * @select: Pointer to a #preludedb_sql_select_t object
 * @output: #prelude_string_t where to store the converted output
 *
 * Convert the modifier part of the SQL selection object to string, that
 * is the GROUP BY / ORDER BY section.
 *
 * Returns: 0 in case of success, a negative value if an error occured.
 */
int preludedb_sql_select_fields_to_string(preludedb_sql_select_t *select, prelude_string_t *output)
{
        return prelude_string_ncat(output, prelude_string_get_string(select->fields), prelude_string_get_len(select->fields));
}



/**
 * preludedb_sql_select_modifiers_to_string
 * @select: Pointer to a #preludedb_sql_select_t object
 * @output: #prelude_string_t where to store the converted output
 *
 * Convert the modifier part of the SQL selection object to string, that
 * is the GROUP BY / ORDER BY section.
 *
 * Returns: 0 in case of success, a negative value if an error occured.
 */
int preludedb_sql_select_modifiers_to_string(preludedb_sql_select_t *select, prelude_string_t *output)
{
        int ret;

        if ( ! prelude_string_is_empty(select->group_by) ) {
                ret = prelude_string_sprintf(output, " GROUP BY %s", prelude_string_get_string(select->group_by));
                if ( ret < 0 )
                        return ret;
        }

        if ( ! prelude_string_is_empty(select->order_by) ) {
                ret = prelude_string_sprintf(output, " ORDER BY %s", prelude_string_get_string(select->order_by));
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}
