/*****
*
* Copyright (C) 2005-2019 CS-SI. All Rights Reserved.
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
#include <libprelude/idmef-criteria.h>

#include "preludedb-path-selection.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "classic-sql-join.h"
#include "classic-path-resolve.h"

#define FIELD_CONTEXT_WHERE    1
#define FIELD_CONTEXT_SELECT   2
#define FIELD_CONTEXT_FUNCTION 3

int classic_get_path_column_count(preludedb_selected_path_t *selected);

static int default_table_name_resolver(const idmef_path_t *path, char **table_name)
{
        char c;
        int ret;
        const char *class_name;
        prelude_string_t *string;
        prelude_bool_t next_is_maj = TRUE;

        class_name = idmef_class_get_name(idmef_path_get_class(path, idmef_path_get_depth(path) - 2));

        ret = prelude_string_new(&string);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_cat(string, "Prelude_");
        if ( ret < 0 )
                goto error;

        while ( *class_name ) {
                c = *class_name++;
                if ( c == '_' ) {
                        next_is_maj = TRUE;
                        continue;
                }

                if ( next_is_maj ) {
                        c += 'A' - 'a';
                        next_is_maj = FALSE;
                }

                ret = prelude_string_ncat(string, &c, 1);
                if ( ret < 0 )
                        goto error;
        }

        ret = prelude_string_get_string_released(string, table_name);

 error:
        prelude_string_destroy(string);

        return ret;
}



static int default_field_name_resolver(const idmef_path_t *path, int field_context,
                                const char *table_name, prelude_string_t *output)
{
        return prelude_string_sprintf(output, "%s.%s", table_name,
                                      idmef_path_get_name(path, idmef_path_get_depth(path) - 1));
}


static int time_with_usec_field_name_resolver(const idmef_path_t *path, int field_context,
                                              const char *table_name, prelude_string_t *output)
{
        if ( field_context == FIELD_CONTEXT_SELECT )
                return prelude_string_sprintf(output, "%s.time, %s.gmtoff, %s.usec", table_name, table_name, table_name);

        return prelude_string_sprintf(output, "%s.time", table_name);
}



static int time_without_usec_field_name_resolver(const idmef_path_t *path, int field_context,
                                                 const char *table_name, prelude_string_t *output)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( field_context == FIELD_CONTEXT_SELECT )
                return prelude_string_sprintf(output, "%s.%s, %s.%s_gmtoff",
                                              table_name, child_name, table_name, child_name);

        return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



static int message_table_name_resolver(const idmef_path_t *path, char **table_name)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "create_time") == 0 )
                *table_name = strdup("Prelude_CreateTime");
        else if ( strcmp(child_name, "detect_time") == 0 )
                *table_name = strdup("Prelude_DetectTime");
        else if ( strcmp(child_name, "analyzer_time") == 0 )
                *table_name = strdup("Prelude_AnalyzerTime");
        else
                return default_table_name_resolver(path, table_name);

        return *table_name ? 0 : prelude_error_from_errno(errno);
}



static int message_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name,
                                       prelude_string_t *output)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "create_time") == 0 ||
             strcmp(child_name, "detect_time") == 0 ||
             strcmp(child_name, "analyzer_time") == 0 )
                return time_with_usec_field_name_resolver(path, field_context, table_name, output);

        return default_field_name_resolver(path, field_context, table_name, output);
}



static int process_table_name_resolver(const idmef_path_t *path, char **table_name)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "arg") == 0 )
                *table_name = strdup("Prelude_ProcessArg");
        else if ( strcmp(child_name, "env") == 0 )
                *table_name = strdup("Prelude_ProcessEnv");
        else
                *table_name = strdup("Prelude_Process");

        return *table_name ? 0 : prelude_error_from_errno(errno);
}



static int file_access_table_name_resolver(const idmef_path_t *path, char **table_name)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "permission") == 0 )
                *table_name = strdup("Prelude_FileAccess_Permission");
        else
                *table_name = strdup("Prelude_FileAccess");

        return *table_name ? 0 : prelude_error_from_errno(errno);
}



static int web_service_table_name_resolver(const idmef_path_t *path, char **table_name)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "arg") == 0 )
                *table_name = strdup("Prelude_WebServiceArg");
        else
                *table_name = strdup("Prelude_WebService");

        return *table_name ? 0 : prelude_error_from_errno(errno);
}



static int snmp_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name,
                                    prelude_string_t *output)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "oid") == 0 )
                child_name = "snmp_oid";

        return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



static int checksum_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name,
                                        prelude_string_t *output)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "key") == 0 )
                child_name = "checksum_key";

        return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



static int file_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name,
                                    prelude_string_t *output)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if ( strcmp(child_name, "create_time") == 0 ||
             strcmp(child_name, "modify_time") == 0 ||
             strcmp(child_name, "access_time") == 0 )
                return time_without_usec_field_name_resolver(path, field_context, table_name, output);

        return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



static int addata_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name,
                                    prelude_string_t *output)
{
        const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

        if (  field_context == FIELD_CONTEXT_SELECT && strcmp(child_name, "data") == 0 )
                return prelude_string_sprintf(output, "%s.%s, %s.type",
                                              table_name, child_name, table_name);

        return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



typedef struct classic_idmef_class {
        idmef_class_id_t id;
        int (*resolve_table_name)(const idmef_path_t *path, char **table_name);
        int (*resolve_field_name)(const idmef_path_t *path, int field_context,
                                  const char *table_name, prelude_string_t *output);
} classic_idmef_class_t;


static const classic_idmef_class_t classes[] = {
        { IDMEF_CLASS_ID_ALERT, message_table_name_resolver, message_field_name_resolver },
        { IDMEF_CLASS_ID_HEARTBEAT, message_table_name_resolver, message_field_name_resolver },
        { IDMEF_CLASS_ID_PROCESS, process_table_name_resolver, default_field_name_resolver },
        { IDMEF_CLASS_ID_SNMP_SERVICE, default_table_name_resolver, snmp_field_name_resolver },
        { IDMEF_CLASS_ID_WEB_SERVICE, web_service_table_name_resolver, default_field_name_resolver },
        { IDMEF_CLASS_ID_CHECKSUM, default_table_name_resolver, checksum_field_name_resolver },
        { IDMEF_CLASS_ID_FILE, default_table_name_resolver, file_field_name_resolver },
        { IDMEF_CLASS_ID_FILE_ACCESS, file_access_table_name_resolver, default_field_name_resolver },
        { IDMEF_CLASS_ID_ADDITIONAL_DATA, default_table_name_resolver, addata_field_name_resolver },
};



static const classic_idmef_class_t default_class = {
        0,
        default_table_name_resolver,
        default_field_name_resolver
};



static const classic_idmef_class_t *search_path(const idmef_path_t *path)
{
        idmef_class_id_t class_id;
        unsigned int i;

        class_id = idmef_path_get_class(path, idmef_path_get_depth(path) - 2);

        for ( i = 0; i < sizeof (classes) / sizeof (classes[0]); i++ )
                if ( class_id == classes[i].id )
                        return &classes[i];

        return &default_class;
}



static int get_field_context(preludedb_selected_path_t *selected)
{
        preludedb_selected_object_t *object;

        object = preludedb_selected_path_get_object(selected);
        if ( preludedb_selected_object_is_function(object) || preludedb_selected_path_get_flags(selected) & PRELUDEDB_SELECTED_PATH_FLAGS_GROUP_BY )
                return FIELD_CONTEXT_FUNCTION;
        else
                return FIELD_CONTEXT_SELECT;
}



static int _classic_path_resolve(const idmef_path_t *path, int field_context, void *data, prelude_string_t *output)
{
        classic_sql_join_t *join = data;
        const classic_idmef_class_t *class;
        classic_sql_joined_table_t *table;
        char *table_name;
        int ret;

        if ( idmef_path_get_depth(path) == 2 && idmef_path_get_value_type(path, 1) != IDMEF_VALUE_TYPE_TIME ) {
                classic_sql_join_set_top_class(join, idmef_path_get_class(path, 0));
                return default_field_name_resolver(path, field_context, "top_table", output);
        }

        class = search_path(path);

        table = classic_sql_join_lookup_table(join, path);

        if ( ! table ) {
                ret = class->resolve_table_name(path, &table_name);
                if ( ret < 0 )
                        return ret;

                ret = classic_sql_join_new_table(join, &table, path, table_name);
                if ( ret < 0 )
                        return ret;
        }

        return class->resolve_field_name(path, field_context,
                                         classic_sql_joined_table_get_name(table), output);
}


int classic_path_resolve(preludedb_selected_path_t *selpath, preludedb_selected_object_t *object, void *data, prelude_string_t *output)
{
        const idmef_path_t *path = preludedb_selected_object_get_data(object);
        return _classic_path_resolve(path, get_field_context(selpath), data, output);
}



static int classic_path_resolve_criterion(preludedb_sql_t *sql,
                                          idmef_criteria_t *criterion,
                                          classic_sql_join_t *join, prelude_string_t *output)
{
        prelude_string_t *field_name;
        int ret;

        ret = prelude_string_new(&field_name);
        if ( ret < 0 )
                return ret;

        ret = _classic_path_resolve(idmef_criteria_get_path(criterion), FIELD_CONTEXT_WHERE, join, field_name);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_build_criterion_string(sql, output,
                                                   prelude_string_get_string(field_name),
                                                   idmef_criteria_get_operator(criterion),
                                                   idmef_criteria_get_value(criterion));

 error:
        prelude_string_destroy(field_name);

        return ret;
}



int classic_path_resolve_criteria(preludedb_sql_t *sql,
                                  idmef_criteria_t *criteria,
                                  classic_sql_join_t *join, prelude_string_t *output)
{
        int ret;
        const char *operator;
        idmef_criteria_t *left, *right;

        if ( idmef_criteria_is_criterion(criteria) )
                return classic_path_resolve_criterion(sql, criteria, join, output);

        left = idmef_criteria_get_left(criteria);
        right = idmef_criteria_get_right(criteria);

        ret = prelude_string_sprintf(output, "%s(", (idmef_criteria_get_operator(criteria) == IDMEF_CRITERION_OPERATOR_NOT) ? "NOT" : "");
        if ( ret < 0 )
                return ret;

        if ( left ) {
                ret = classic_path_resolve_criteria(sql, left, join, output);
                if ( ret < 0 )
                        return ret;

                operator = preludedb_sql_criteria_operator_to_string(idmef_criteria_get_operator(criteria) & (~IDMEF_CRITERION_OPERATOR_NOT));
                if ( ! operator )
                        return -1;

                ret = prelude_string_sprintf(output, " %s ", operator);
                if ( ret < 0 )
                        return ret;
        }

        ret = classic_path_resolve_criteria(sql, right, join, output);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_cat(output, ")");
        if ( ret < 0 )
                return ret;

        return 0;

}
