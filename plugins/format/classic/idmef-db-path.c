/*****
*
* Copyright (C) 2005 Nicolas Delon <nicolas@prelude-ids.org>
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
#include <string.h>
#include <stdarg.h>

#include <libprelude/prelude.h>

#include "preludedb-path-selection.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "idmef-db-path.h"

#define FIELD_CONTEXT_WHERE    1
#define FIELD_CONTEXT_SELECT   2
#define FIELD_CONTEXT_FUNCTION 3


typedef struct classic_path_table {
	PRELUDE_LINKED_OBJECT;
	const idmef_path_t *path;
	char *table_name;
	char aliased_table_name[16];
	char parent_type;
	prelude_string_t *index_constraints;
} classic_path_table_t;


struct classic_join {
	idmef_class_id_t top_class;
	prelude_list_t path_tables;
	unsigned int next_id;
};


struct classic_select {
	prelude_string_t *fields;
	unsigned int field_count;
	prelude_string_t *order_by;
	prelude_string_t *group_by;
};


typedef struct classic_idmef_class {
	idmef_class_id_t id;
	int (*resolve_table_name)(const idmef_path_t *path, char **table_name);
	int (*resolve_field_name)(const idmef_path_t *path, int field_context,
				  const char *table_name, prelude_string_t *output);
} classic_idmef_class_t;




static int classic_path_table_to_string(classic_path_table_t *path_table, prelude_string_t *output)
{
	int ret;

	ret = prelude_string_sprintf(output, " LEFT JOIN %s AS %s ON (", path_table->table_name, path_table->aliased_table_name);
	if ( ret < 0 )
		return ret;

	if ( path_table->parent_type ) {
		ret = prelude_string_sprintf(output, "%s._parent_type='%c' AND ",
					     path_table->aliased_table_name, path_table->parent_type);
		if ( ret < 0 )
			return ret;
	}

	ret = prelude_string_sprintf(output, "%s._message_ident=top_table._ident", path_table->aliased_table_name);
	if ( ret < 0 )
		return ret;

	if ( ! prelude_string_is_empty(path_table->index_constraints) ) {
		ret = prelude_string_sprintf(output, " AND %s", prelude_string_get_string(path_table->index_constraints));
		if ( ret < 0 )
			return ret;
	}

	return prelude_string_cat(output, ")");
}



static const char *classic_path_table_get_aliased_table_name(classic_path_table_t *path_table)
{
	return path_table->aliased_table_name;
}



int classic_join_new(classic_join_t **join)
{
	*join = calloc(1, sizeof (**join));
	if ( ! *join )
		return prelude_error_from_errno(errno);

	prelude_list_init(&(*join)->path_tables);

	return 0;
}



void classic_join_destroy(classic_join_t *join)
{
	prelude_list_t *tmp, *next;
	classic_path_table_t *path_table;

	prelude_list_for_each_safe(&join->path_tables, tmp, next) {
		path_table = prelude_list_entry(tmp, classic_path_table_t, list);
		free(path_table->table_name);
		prelude_string_destroy(path_table->index_constraints);
		prelude_linked_object_del((prelude_linked_object_t *) path_table);
		free(path_table);
	}

	free(join);
}


void classic_join_set_top_class(classic_join_t *join, idmef_class_id_t top_class)
{
	join->top_class = top_class;
}



static classic_path_table_t *classic_join_lookup_path_table(const classic_join_t *join, const idmef_path_t *path)
{
	prelude_list_t *tmp;
	classic_path_table_t *path_table;
	unsigned int depth;
	int last_element_is_listed;
	int ret;

	depth = idmef_path_get_depth(path);
	last_element_is_listed = ((ret = idmef_path_get_index(path, depth - 1)) > -1 ||
				  prelude_error_get_code(ret) != PRELUDE_ERROR_IDMEF_PATH_INDEX_FORBIDDEN);

	prelude_list_for_each(&join->path_tables, tmp) {
		path_table = prelude_list_entry(tmp, classic_path_table_t, list);
		if ( depth == idmef_path_get_depth(path_table->path) ) {
			if ( last_element_is_listed )
				ret = idmef_path_compare(path, path_table->path);
			else
				ret = idmef_path_ncompare(path, path_table->path, depth - 1);

			if ( ret == 0 )
				return path_table;
		}
	}

	return NULL;
}




static char resolve_file_parent_type(const idmef_path_t *path)
{
	if ( idmef_path_get_class(path, 3) == IDMEF_CLASS_ID_FILE_ACCESS &&
	     idmef_path_get_class(path, 4) == IDMEF_CLASS_ID_USER_ID )
		return 'F';

	return 0;
}



static char resolve_target_parent_type(const idmef_path_t *path)
{
	if ( idmef_path_get_depth(path) == 3 )
		return 0;

	if ( idmef_path_get_class(path, 2) == IDMEF_CLASS_ID_FILE )
		return resolve_file_parent_type(path);

	return 'T';
}



static char resolve_source_parent_type(const idmef_path_t *path)
{
	return (idmef_path_get_depth(path) > 3) ? 'S' : 0;
}



static char resolve_tool_alert_parent_type(const idmef_path_t *path)
{
	if ( idmef_path_get_class(path, 2) == IDMEF_CLASS_ID_ALERTIDENT )
		return 'T';

	return 0;
}



static char resolve_correlation_alert_parent_type(const idmef_path_t *path)
{
	if ( idmef_path_get_class(path, 2) == IDMEF_CLASS_ID_ALERTIDENT )
		return 'C';

	return 0;
}



static char resolve_alert_parent_type(const idmef_path_t *path)
{
	switch ( idmef_path_get_class(path, 1) ) {
	case IDMEF_CLASS_ID_CLASSIFICATION: case IDMEF_CLASS_ID_ASSESSMENT:
	case IDMEF_CLASS_ID_OVERFLOW_ALERT:
		return 0;
		
	case IDMEF_CLASS_ID_TOOL_ALERT:
		return resolve_tool_alert_parent_type(path);

	case IDMEF_CLASS_ID_CORRELATION_ALERT:
		return resolve_correlation_alert_parent_type(path);

	case IDMEF_CLASS_ID_SOURCE:
		return resolve_source_parent_type(path);

	case IDMEF_CLASS_ID_TARGET:
		return resolve_target_parent_type(path);

	default:
		/* nop */;
	}

	return 'A';
}



static char resolve_parent_type(const idmef_path_t *path)
{
	if ( idmef_path_get_class(path, 0) == IDMEF_CLASS_ID_HEARTBEAT )
		return 'H';

	return resolve_alert_parent_type(path);
}



static int add_index_constraint(classic_path_table_t *path_table, int parent_level, unsigned int index)
{
	int ret;

	if ( ! prelude_string_is_empty(path_table->index_constraints) ) {
		ret = prelude_string_cat(path_table->index_constraints, " AND ");
		if ( ret < 0 )
			return ret;
	}

	if ( parent_level == -1 )
		ret = prelude_string_sprintf(path_table->index_constraints, "%s._index=%d",
					     path_table->aliased_table_name, index);
	else
		ret = prelude_string_sprintf(path_table->index_constraints, "%s._parent%d_index=%d",
					     path_table->aliased_table_name, parent_level, index);

	return ret;
}



static int resolve_indexes(classic_path_table_t *path_table)
{
	unsigned int max_depth;
	unsigned int depth;
	unsigned int parent_level;
	int index;
	int ret;

	max_depth = idmef_path_get_depth(path_table->path);
	parent_level = 0;

	for ( depth = 1; depth < max_depth - 2; depth++ ) {
		index = idmef_path_get_index(path_table->path, depth);
		if ( prelude_error_get_code(index) != PRELUDE_ERROR_IDMEF_PATH_INDEX_FORBIDDEN ) {
			if ( index >= -1 ) {
				ret = add_index_constraint(path_table, parent_level, index);
				if ( ret < 0 )
					return ret;
			}

			parent_level++;
		}
	}

	if ( (index = idmef_path_get_index(path_table->path, max_depth - 1)) > -1 ||
	     (index = idmef_path_get_index(path_table->path, max_depth - 2)) > -1 ) {
		ret = add_index_constraint(path_table, -1, index);
		if ( ret < 0 )
			return ret;
	}

	return 0;
}



static void classic_path_table_destroy(classic_path_table_t *path_table)
{
	
}



static int classic_join_new_path_table(classic_join_t *join, classic_path_table_t **path_table,
				       const idmef_path_t *path, char *table_name)
{
	int ret;
	idmef_class_id_t top_class;

	top_class = idmef_path_get_class(path, 0);

	if ( join->top_class ) {
		if ( top_class != join->top_class )
			return -1; /* conflicting top tables */

	} else {
		join->top_class = top_class;
	}

	*path_table = calloc(1, sizeof (**path_table));
	if ( ! *path_table )
		return prelude_error_from_errno(errno);

	ret = prelude_string_new(&(*path_table)->index_constraints);
	if ( ret < 0 ) {
		free(*path_table);
		return ret;
	}

	(*path_table)->path = path;
	(*path_table)->table_name = table_name;
	sprintf((*path_table)->aliased_table_name, "t%d", join->next_id++);

	(*path_table)->parent_type = resolve_parent_type((*path_table)->path);

	ret = resolve_indexes(*path_table);
	if ( ret < 0 ) {
		classic_path_table_destroy(*path_table);
		return ret;
	}

	prelude_linked_object_add_tail(&join->path_tables, (prelude_linked_object_t *) *path_table);

	return 0;
}



int classic_join_to_string(classic_join_t *join, prelude_string_t *output)
{
	prelude_list_t *tmp;
	classic_path_table_t *path_table;
	int ret;

	ret = prelude_string_sprintf(output, "%s AS top_table",
				     (join->top_class == IDMEF_CLASS_ID_ALERT) ? "Prelude_Alert" : "Prelude_Heartbeat");
	if ( ret < 0 )
		return ret;

	prelude_list_for_each(&join->path_tables, tmp) {
		path_table = prelude_list_entry(tmp, classic_path_table_t, list);
		ret = classic_path_table_to_string(path_table, output);
		if ( ret < 0 )
			return ret;
	}

	return ret;
}



int classic_select_new(classic_select_t **select)
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



void classic_select_destroy(classic_select_t *select)
{
	prelude_string_destroy(select->fields);
	prelude_string_destroy(select->order_by);
	prelude_string_destroy(select->group_by);
	free(select);
}



int classic_select_add_field(classic_select_t *select, const char *field_name, preludedb_selected_path_flags_t flags)
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
	int i;
	const char *function_name = NULL;
	int ret;

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

	return 0;	
}



int classic_select_fields_to_string(classic_select_t *select, prelude_string_t *output)
{
	return prelude_string_ncat(output,
				   prelude_string_get_string(select->fields), prelude_string_get_len(select->fields));
}



int classic_select_modifiers_to_string(classic_select_t *select, prelude_string_t *output)
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



static int default_table_name_resolver(const idmef_path_t *path, char **table_name)
{
	const char *class_name;
	prelude_string_t *string;
	int ret;

	class_name = idmef_class_get_name(idmef_path_get_class(path, idmef_path_get_depth(path) - 2));

	ret = prelude_string_new(&string);
	if ( ret < 0 )
		return ret;

	ret = prelude_string_cat(string, "Prelude_");
	if ( ret < 0 )
		goto error;

	while ( *class_name ) {
		ret = prelude_string_sprintf(string, "%c", *class_name++ + 'A' - 'a');
		if ( ret < 0 )
			goto error;

		while ( *class_name ) {
			if ( *class_name == '_' ) {
				class_name++;
				break;
			} else {
				ret = prelude_string_sprintf(string, "%c", *class_name++);
				if ( ret < 0 )
					return ret;
			}
		}
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
		return prelude_string_sprintf(output, "%s.time, %s.usec, %s.gmtoff", table_name, table_name, table_name);

	return prelude_string_sprintf(output, "%s.time", table_name);
}



static int time_without_usec_field_name_resolver(const idmef_path_t *path, int field_context,
						 const char *table_name, prelude_string_t *output)
{
	const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

	if ( field_context == FIELD_CONTEXT_SELECT )
		return prelude_string_sprintf(output, "%s.%s, %s.%s_gmtoff, 0",
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
	else if ( strcmp(child_name, "analyzer_time") )
		*table_name = strdup("Prelude_AnalyzerTime");
	else
		return default_table_name_resolver(path, table_name);

	return *table_name ? 0 : prelude_error_from_errno(errno);
}



static int message_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name, prelude_string_t *output)
{
	const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

	if ( strcmp(child_name, "create_time") ||
	     strcmp(child_name, "detect_time") ||
	     strcmp(child_name, "analyzer_time") )
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



static int snmp_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name, prelude_string_t *output)
{
	const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

	if ( strcmp(child_name, "oid") == 0 )
		child_name = "snmp_oid";

	return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



static int checksum_field_name_resolver(const idmef_path_t *path, int field_context, const char *table_name, prelude_string_t *output)
{
	const char *child_name = idmef_path_get_name(path, idmef_path_get_depth(path) - 1);

	if ( strcmp(child_name, "key") == 0 )
		child_name = "checksum_key";

	return prelude_string_sprintf(output, "%s.%s", table_name, child_name);
}



static const classic_idmef_class_t classes[] = {
	{ IDMEF_CLASS_ID_ALERT, message_table_name_resolver, message_field_name_resolver },
	{ IDMEF_CLASS_ID_HEARTBEAT, message_table_name_resolver, message_field_name_resolver },
	{ IDMEF_CLASS_ID_PROCESS, process_table_name_resolver, default_field_name_resolver },
	{ IDMEF_CLASS_ID_SNMP_SERVICE, default_table_name_resolver, snmp_field_name_resolver },
	{ IDMEF_CLASS_ID_CHECKSUM, default_table_name_resolver, checksum_field_name_resolver }
};



static const classic_idmef_class_t default_class = {
	0,
	default_table_name_resolver,
	default_field_name_resolver
};



static const classic_idmef_class_t *search_path(const idmef_path_t *path)
{
	idmef_class_id_t class_id;
	int i;

	class_id = idmef_path_get_class(path, idmef_path_get_depth(path) - 2);

	for ( i = 0; i < sizeof (classes) / sizeof (classes[0]); i++ )
		if ( class_id == classes[i].id )
			return &classes[i];

	return &default_class;
}



static int classic_resolve_path(const idmef_path_t *path, int field_context, classic_join_t *join, prelude_string_t *output)
{
	const classic_idmef_class_t *class;
	classic_path_table_t *path_table;
	char *table_name;
	int ret;

	if ( idmef_path_get_depth(path) == 2 && idmef_path_get_value_type(path, 1) != IDMEF_VALUE_TYPE_TIME )
		return default_field_name_resolver(path, field_context, "top_table", output);

	class = search_path(path);

	path_table = classic_join_lookup_path_table(join, path);

	if ( ! path_table ) {
		ret = class->resolve_table_name(path, &table_name);
		if ( ret < 0 )
			return ret;

		ret = classic_join_new_path_table(join, &path_table, path, table_name);
		if ( ret < 0 )
			return ret;
	}

	return class->resolve_field_name(path, field_context,
					 classic_path_table_get_aliased_table_name(path_table), output);
}



int classic_resolve_selected(preludedb_sql_t *sql,
			     const preludedb_selected_path_t *selected, classic_join_t *join, classic_select_t *select)
{
	idmef_path_t *path;
	preludedb_selected_path_flags_t flags;
	prelude_string_t *field_name;
	int ret;
	int field_context;

	ret = prelude_string_new(&field_name);
	if ( ret < 0 )
		return ret;

	path = preludedb_selected_path_get_path(selected);
	flags = preludedb_selected_path_get_flags(selected);

	if ( flags & (PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN|
		      PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX|
		      PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG|
		      PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD|
		      PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT) )
		field_context = FIELD_CONTEXT_FUNCTION;
	else
		field_context = FIELD_CONTEXT_SELECT;

	ret = classic_resolve_path(path, field_context, join, field_name);
	if ( ret < 0 )
		goto error;

	ret = classic_select_add_field(select, prelude_string_get_string(field_name), flags);

 error:
	prelude_string_destroy(field_name);

	return ret;	
}



int classic_resolve_selection(preludedb_sql_t *sql,
			      const preludedb_path_selection_t *selection, classic_join_t *join, classic_select_t *select)
{
	const preludedb_selected_path_t *selected = NULL;
	int ret;

	while ( (selected = preludedb_path_selection_get_next(selection, selected)) ) {
		ret = classic_resolve_selected(sql, selected, join, select);
		if ( ret < 0 )
			return ret;
	}

	return 0;
}



static int classic_resolve_criterion(preludedb_sql_t *sql,
				     const idmef_criterion_t *criterion, classic_join_t *join, prelude_string_t *output)
{
	prelude_string_t *field_name;
	int ret;

	ret = prelude_string_new(&field_name);
	if ( ret < 0 )
		return ret;

	ret = classic_resolve_path(idmef_criterion_get_path(criterion), FIELD_CONTEXT_WHERE, join, field_name);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_build_criterion_string(sql, output,
						   prelude_string_get_string(field_name),
						   idmef_criterion_get_operator(criterion),
						   idmef_criterion_get_value(criterion));

 error:
	prelude_string_destroy(field_name);

	return ret;
}



int classic_resolve_criteria(preludedb_sql_t *sql,
			     const idmef_criteria_t *criteria, classic_join_t *join, prelude_string_t *output)
{
        idmef_criteria_t *or, *and;
	int ret;

        or = idmef_criteria_get_or(criteria);
        and = idmef_criteria_get_and(criteria);

        if ( or ) {
                ret = prelude_string_cat(output, "((");
                if ( ret < 0 )
                        return ret;
        }

        ret = classic_resolve_criterion(sql, idmef_criteria_get_criterion(criteria), join, output);
        if ( ret < 0 )
                return ret;

        if ( and ) {
                ret = prelude_string_cat(output, " AND ");
                if ( ret < 0 )
                        return ret;

                ret = classic_resolve_criteria(sql, and, join, output);
                if ( ret < 0 )
                        return ret;
        }

        if ( or ) {
                ret = prelude_string_cat(output, ") OR (");
                if ( ret < 0 )
                        return ret;

                ret = classic_resolve_criteria(sql, or, join, output);
                if ( ret < 0 )
                        return ret;

                ret = prelude_string_cat(output, "))");
                if ( ret < 0 )
                        return ret;
        }

        return 0;

}



/* int main() */
/* { */
/* 	classic_join_t *join; */
/* 	classic_select_t *select; */
/* 	prelude_string_t *query; */
/* 	idmef_path_t *path; */
/* 	preludedb_selected_path_t *selected; */
/* 	preludedb_path_selection_t *selection; */
/* 	int ret; */

/* 	ret = classic_join_new(&join); */
/* 	if ( ret < 0 ) */
/* 		goto error; */

/* 	ret = classic_select_new(&select); */
/* 	if ( ret < 0 ) */
/* 		goto error; */

/* 	preludedb_path_selection_new(&selection); */

/* 	idmef_path_new_fast(&path, "alert.messageid"); */
/* 	preludedb_selected_path_new(&selected, path, */
/* 				    PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX|PRELUDEDB_SELECTED_OBJECT_ORDER_ASC); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.create_time"); */
/* 	preludedb_selected_path_new(&selected, path, 0); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.source(0).node.address(3).address"); */
/* 	preludedb_selected_path_new(&selected, path, PRELUDEDB_SELECTED_OBJECT_GROUP_BY); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.source(1).node.address.netmask"); */
/* 	preludedb_selected_path_new(&selected, path, 0); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.target(0).process.arg(5)"); */
/* 	preludedb_selected_path_new(&selected, path, 0); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.analyzer(0).name"); */
/* 	preludedb_selected_path_new(&selected, path, 0); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.classification.reference.meaning"); */
/* 	preludedb_selected_path_new(&selected, path, 0); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	idmef_path_new_fast(&path, "alert.target.file(0).file_access(2).user_id.name"); */
/* 	preludedb_selected_path_new(&selected, path, 0); */
/* 	preludedb_path_selection_add(selection, selected); */

/* 	ret = classic_resolve_selection(selection, join, select); */
/* 	if ( ret < 0 ) */
/* 		goto error; */

/* 	prelude_string_new(&query); */

/* 	prelude_string_cat(query, "SELECT "); */

/* 	ret = classic_select_fields_to_string(select, query); */
/* 	if ( ret < 0 ) { */
/* 		fprintf(stderr, "classic_select_fields_to_string failed\n"); */
/* 		goto error; */
/* 	} */

/* 	ret = prelude_string_cat(query, " FROM "); */

/* 	ret = classic_join_to_string(join, query); */
/* 	if ( ret < 0 ) */
/* 		goto error; */

/* 	ret = classic_select_modifiers_to_string(select, query); */
/* 	if ( ret < 0 ) */
/* 		goto error; */

/* 	printf("%s\n", prelude_string_get_string(query)); */

/* 	return 0; */

/*  error: */
/* 	prelude_perror(ret, "error"); */

/* 	return 1; */
/* } */
