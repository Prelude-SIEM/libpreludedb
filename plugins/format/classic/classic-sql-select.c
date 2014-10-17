/*****
*
* Copyright (C) 2005-2012 CS-SI. All Rights Reserved.
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
#include <string.h>
#include <stdarg.h>

#include <libprelude/prelude.h>

#include "preludedb-path-selection.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "classic-sql-select.h"


struct classic_sql_select {
        prelude_string_t *fields;
        unsigned int field_count;
        prelude_string_t *order_by;
        prelude_string_t *group_by;
};



int classic_sql_select_new(classic_sql_select_t **select)
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

        return 0;
}



void classic_sql_select_destroy(classic_sql_select_t *select)
{
        prelude_string_destroy(select->fields);
        prelude_string_destroy(select->order_by);
        prelude_string_destroy(select->group_by);
        free(select);
}



int classic_sql_select_add_field(classic_sql_select_t *select, const char *field_name,
                                 preludedb_selected_path_flags_t flags, unsigned int num_field)
{
        static const struct {
                int flag;
                const char *function_name;
        } sql_functions_table[] = {
                { PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN, "MIN" },
                { PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX, "MAX" },
                { PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG, "AVG" },
                { PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD, "STD" },
                { PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT, "COUNT" }
        };
        int i, ret;
        const char *function_name = NULL;

        if ( ! prelude_string_is_empty(select->fields) ) {
                ret = prelude_string_cat(select->fields, ", ");
                if ( ret < 0 )
                        return ret;
        }

        for ( i = 0; i < sizeof (sql_functions_table) / sizeof (sql_functions_table[0]); i++ ) {
                if ( flags & sql_functions_table[i].flag ) {
                        function_name = sql_functions_table[i].function_name;
                        break;
                }
        }

        if ( function_name )
                ret = prelude_string_sprintf(select->fields, "%s(%s)", function_name, field_name);
        else
                ret = prelude_string_cat(select->fields, field_name);

        if ( ret < 0 )
                return ret;


        do {
                select->field_count++;

                if ( flags & PRELUDEDB_SELECTED_OBJECT_GROUP_BY ) {
                        if ( ! prelude_string_is_empty(select->group_by) ) {
                                ret = prelude_string_cat(select->group_by, ", ");
                                if ( ret < 0 )
                                        return ret;
                        }

                        ret = prelude_string_sprintf(select->group_by, "%d", select->field_count);
                        if ( ret < 0 )
                                return ret;
                }

                if ( flags & (PRELUDEDB_SELECTED_OBJECT_ORDER_ASC|PRELUDEDB_SELECTED_OBJECT_ORDER_DESC) ) {
                        if ( ! prelude_string_is_empty(select->order_by) ) {
                                ret = prelude_string_cat(select->order_by, ", ");
                                if ( ret < 0 )
                                        return ret;
                        }

                        ret = prelude_string_sprintf(select->order_by, "%d %s", select->field_count,
                                                     (flags & PRELUDEDB_SELECTED_OBJECT_ORDER_ASC) ? "ASC" : "DESC");
                        if ( ret < 0 )
                                return ret;
                }
        } while ( --num_field );

        return 0;
}



int classic_sql_select_fields_to_string(classic_sql_select_t *select, prelude_string_t *output)
{
        return prelude_string_ncat(output,
                                   prelude_string_get_string(select->fields), prelude_string_get_len(select->fields));
}



int classic_sql_select_modifiers_to_string(classic_sql_select_t *select, prelude_string_t *output)
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
