/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"

#include "db-path.h"

#include "idmef-db-select.h"


typedef struct table_entry {
	prelude_list_t list;
	char *table;
	char *top_field;
	char *ident_field;
	char *condition;
	char *alias;
} table_entry_t;




typedef struct table_list {
	char *top_table;
	prelude_list_t tables;
} table_list_t;


static inline const char *get_string(prelude_string_t *string)
{
        const char *s;

        if ( ! string )
                return NULL;

        s = prelude_string_get_string(string);

        return s ? s : "";
}


static int normalize_condition(const char *condition, const char *table, const char *alias, char **output)
{
	char *output_ptr;
	unsigned int table_len = strlen(table);
	unsigned int alias_len = strlen(alias);

	output_ptr = *output = calloc(1, strlen(condition) + 1);
	if ( ! output_ptr )
		return prelude_error_from_errno(errno);

	while ( *condition ) {
		if ( strncmp(condition, table, table_len) == 0 ) {
			memcpy(output_ptr, alias, alias_len);
			condition += table_len;
			output_ptr += alias_len;
		} else {
			*output_ptr++ = *condition++;
		}
	}

	return 0;
}




static int table_entry_new(table_entry_t **entry,
			   const char *table, const char *top_field,
			   const char *ident_field, const char *alias,
			   const char *condition)
{
	int ret;

	*entry = calloc(1, sizeof(**entry));
	if ( ! *entry )
		return preludedb_error_from_errno(errno);

	(*entry)->table = strdup(table);
	if ( ! (*entry)->table ) {
		ret = preludedb_error_from_errno(errno);
		goto error;
	}

	(*entry)->top_field = strdup(top_field);
	if ( ! (*entry)->top_field ) {
		ret = preludedb_error_from_errno(errno);
		goto error;
	}

	(*entry)->ident_field = strdup(ident_field);
	if ( ! (*entry)->ident_field ) {
		ret = preludedb_error_from_errno(errno);
		goto error;
	}

	(*entry)->alias = strdup(alias);
	if ( ! (*entry)->alias ) {
		ret = preludedb_error_from_errno(errno);
		goto error;
	}

	if ( condition ) {
		ret = normalize_condition(condition, table, alias, &(*entry)->condition);
		if ( ret < 0 )
			goto error;
	}

	return 0;

error:
	if ( (*entry)->table )
		free((*entry)->table);

	if ( (*entry)->top_field )
		free((*entry)->top_field);

	if ( (*entry)->ident_field )
		free((*entry)->ident_field);

	if ( (*entry)->alias )
		free((*entry)->alias);

	if ( (*entry)->condition )
		free((*entry)->condition);

	free((*entry));

	return ret;
}




static void table_entry_destroy(table_entry_t *entry)
{
	free(entry->table);
	free(entry->top_field);
	free(entry->ident_field);
	free(entry->alias);

	if ( entry->condition )
		free(entry->condition);

	free(entry);
}




static int table_list_new(table_list_t **tlist)
{
	*tlist = calloc(1, sizeof(**tlist));
	if ( ! *tlist )
		return preludedb_error_from_errno(errno);

	prelude_list_init(&(*tlist)->tables);

	return 0;
}




static void table_list_destroy(table_list_t *tlist)
{
        table_entry_t *entry;
	prelude_list_t *tmp, *n;

	prelude_list_for_each_safe(&tlist->tables, tmp, n) {
		entry = prelude_list_entry(tmp, table_entry_t, list);

                prelude_list_del(&entry->list);
		table_entry_destroy(entry);
	}

	if ( tlist->top_table )
		free(tlist->top_table);

	free(tlist);
}




static int table_list_to_string_for_alerts(table_list_t *tlist, prelude_string_t **string)
{
	int ret;
	prelude_list_t *tmp;
	table_entry_t *entry;
        
	ret = prelude_string_new(string);
	if ( ret < 0 )
		return ret;

	if ( ! tlist->top_table )
		return 0;

	ret = prelude_string_sprintf(*string, "%s", tlist->top_table);
	if ( ret < 0 )
		goto error;

	prelude_list_for_each(&tlist->tables, tmp) {
		entry = prelude_list_entry(tmp, table_entry_t, list);

                ret = prelude_string_sprintf(*string, " LEFT JOIN %s AS %s ON %s.%s = %s.%s",
                                             entry->table, entry->alias, tlist->top_table,
                                             entry->top_field, entry->alias,
                                             entry->ident_field);
		if ( ret < 0 )
			goto error;

		if ( entry->condition ) {
			ret = prelude_string_sprintf(*string, " AND (%s)", entry->condition);
			if ( ret < 0 )
				goto error;
		}
	}

	return 0;

error:
	prelude_string_destroy(*string);

	return ret;
}




static int table_list_to_string_for_values(table_list_t *tlist, prelude_string_t *where,
					   prelude_string_t **string)
{
	int ret;
	prelude_list_t *tmp;
	table_entry_t *entry;
        
	ret = prelude_string_new(string);
	if ( ret < 0 )
		return ret;

	/* 
	 * We must always join with top table, otherwise e.g. a request for
	 * all source/target address pairs would give us all possible 
	 * source/target combinations, even the ones that never appeared
	 * in one alert. 
	 */
	ret = prelude_string_sprintf(*string, "%s", tlist->top_table);
	if ( ret < 0 ) 
		goto error;

	prelude_list_for_each(&tlist->tables, tmp) {
		entry = prelude_list_entry(tmp, table_entry_t, list);

                ret = prelude_string_sprintf(*string, ", %s AS %s", entry->table, entry->alias);
		if ( ret < 0 )
			goto error;
		
		ret = prelude_string_sprintf(where, "%s%s.%s = %s.%s",
					     prelude_string_is_empty(where) ? "" : " AND ",
                                             tlist->top_table, entry->top_field, 
                                             entry->alias, entry->ident_field);
		if ( ret < 0 )
			goto error;

		if ( entry->condition ) {
			ret = prelude_string_sprintf(where, " AND (%s)", entry->condition);
			if ( ret < 0 )
				goto error;
		}
	}

	return 0;

error:
	prelude_string_destroy(*string);

	return ret;
}




static int set_top_table(table_list_t *tlist, const char *table)
{
	tlist->top_table = strdup(table);
	if ( ! tlist->top_table )
		return preludedb_error_from_errno(errno);

	return 0;
}




static int add_table(table_list_t *tlist, const char *table, const char *top_table,
		     const char *top_field, const char *ident_field, const char *condition,
		     const char **alias)
{
	prelude_list_t *tmp;
	table_entry_t *entry;
	int id;
	int ret;
	char buf[64];
	char *ncond;

	if ( top_table ) {
		if ( tlist->top_table ) {
			if ( strcmp(tlist->top_table, top_table) != 0 )
				return preludedb_error(PRELUDEDB_ERROR_INVALID_OBJECT_SELECTION);
		} else {
			ret = set_top_table(tlist, top_table);
			if ( ret < 0 )
				return ret;
		}
	} else {

		/* requesting a top-level table; this must be handled specially */

		if ( ! tlist->top_table ) {
			ret = set_top_table(tlist, table);
			if ( ret < 0 )
				return ret;

			*alias = tlist->top_table;

			return 0;

		} else {
			ret = strcmp(table, tlist->top_table);
			if ( ret != 0 )
				return preludedb_error(PRELUDEDB_ERROR_INVALID_OBJECT_SELECTION);

			*alias = tlist->top_table;
			return 0;
		}

	}

	/* check if the table was already added */

	id = 0;

	prelude_list_for_each(&tlist->tables, tmp) {
		entry = prelude_list_entry(tmp, table_entry_t, list);

		if ( strcmp(table, entry->table) == 0 ) {
			if ( condition ) {
				ret = normalize_condition(condition, entry->table, entry->alias, &ncond);
				if ( ret < 0 )
					return ret;
				
				if ( strcmp(ncond, entry->condition) == 0 ) {
					free(ncond);
					*alias = entry->alias;  /* already added */
					return 0;
				}
				free(ncond);
			} else {
				*alias = entry->alias;  /* already added */
				return 0;
			}
		}
		id++;
	}

	/* not found, we have to add it */
	snprintf(buf, sizeof(buf) - 1, "t%d", id);

	ret = table_entry_new(&entry, table, top_field, ident_field, buf, condition);
	if ( ret < 0 )
		return ret;

	prelude_list_add_tail(&tlist->tables, &entry->list);

	*alias = entry->alias;

	return 0;
}




static int aggregate_function_to_sql(prelude_string_t *buf, int flags, const char *field)
{
        if ( ! field )
                return 0;
        
	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN )
		return prelude_string_sprintf(buf, "MIN(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX )
		return prelude_string_sprintf(buf, "MAX(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG )
		return prelude_string_sprintf(buf, "AVG(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD )
		return prelude_string_sprintf(buf, "STD(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT)
		return prelude_string_sprintf(buf, "COUNT(%s)", field);

	return prelude_string_sprintf(buf, "%s", field);
}




static int add_field(prelude_string_t *fields, const char *table,
                     const char *field, const char *alias, int flags)
{
	int ret;
	prelude_string_t *tmp, *tmp2;
        
	ret = prelude_string_new(&tmp);
	if ( ret < 0 )
		return ret;
	
	ret = prelude_string_new(&tmp2);
	if ( ret < 0 )
		return ret;

	if ( table ) {
		ret = prelude_string_sprintf(tmp2, "%s.%s", table, field);
		if ( ret < 0 )
			goto error;
	}

	ret = aggregate_function_to_sql(tmp, flags, prelude_string_get_string(tmp2));
	if ( ret < 0 )
		goto error;

	if ( alias ) {
		ret = prelude_string_sprintf(tmp, " AS %s", alias);
		if ( ret < 0 )
			goto error;
	}

	/* Add the field to list  */
	if ( prelude_string_is_empty(fields) ) {
		/* list empty */
		ret = prelude_string_sprintf(fields, "%s", get_string(tmp));
		if ( ret < 0 )
			goto error;
	} else {
		ret = prelude_string_sprintf(fields, ", %s", get_string(tmp));
		if ( ret < 0 )
			goto error;
	}


	ret = 0;

error:
	prelude_string_destroy(tmp);
	prelude_string_destroy(tmp2);

	return ret;
}



static int criterion_to_sql(preludedb_sql_t *sql, prelude_string_t *where, table_list_t *tables, idmef_criterion_t *criterion)
{
	idmef_path_t *path;
	db_path_t *db;
	char field_name[128];
	char *table, *field, *function, *top_table, *top_field, *ident_field;
	char *condition;
	const char *table_alias;
	int ret;

	path = idmef_criterion_get_path(criterion);
	db = db_path_find(path);
	if ( ! db ) {
		prelude_log(PRELUDE_LOG_ERR,
			    "path '%s' not handled by database specification.\n", idmef_path_get_name(path));
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);
	}

	table = db_path_get_table(db);
	field = db_path_get_field(db);
	function = db_path_get_function(db);
	top_table = db_path_get_top_table(db);
	top_field = db_path_get_top_field(db);
	condition = db_path_get_condition(db);
	ident_field = db_path_get_ident_field(db);

	/* Add table to JOIN list */
	ret = add_table(tables, table, top_table, top_field, ident_field, condition, &table_alias);
	if ( ret < 0 )
		return ret;

	if ( table_alias )
		ret = snprintf(field_name, sizeof (field_name), "%s.%s", table_alias, field);
	else
		ret = snprintf(field_name, sizeof (field_name), "%s", field);

	if ( ret < -1 || ret >= sizeof (field_name) )
		return -1;

	return preludedb_sql_build_criterion_string(sql, where, field_name,
						    idmef_criterion_get_relation(criterion),
						    idmef_criterion_get_value(criterion));
}




static int criteria_to_sql(preludedb_sql_t *sql, prelude_string_t *where, table_list_t *tables, idmef_criteria_t *criteria)
{
        int ret;
        idmef_criteria_t *or, *and;

        or = idmef_criteria_get_or(criteria);
        and = idmef_criteria_get_and(criteria);
        
        if ( or ) {
                ret = prelude_string_sprintf(where, "((");
		if ( ret < 0 )
			return ret;
	}

        ret = criterion_to_sql(sql, where, tables, idmef_criteria_get_criterion(criteria));
        if ( ret < 0 )
                return ret;

        if ( and ) {
                ret = prelude_string_sprintf(where, " AND ");
		if ( ret < 0 )
			return ret;

		ret = criteria_to_sql(sql, where, tables, and);
		if ( ret < 0 )
			return ret;
        }

        if ( or ) {
                ret = prelude_string_sprintf(where, ") OR (");
		if ( ret < 0 )
			return ret;

                ret = criteria_to_sql(sql, where, tables, or);
		if ( ret < 0 )
			return ret;

                ret = prelude_string_sprintf(where, "))");
		if ( ret < 0 )
			return ret;
        }

        return 0;
}



static int path_to_table(table_list_t *tables, db_path_t *db_path, const char **table_alias)
{
	/* Add path's table to LEFT JOIN list */

	return add_table(tables,
			 db_path_get_table(db_path),
			 db_path_get_top_table(db_path),
			 db_path_get_top_field(db_path),
			 db_path_get_ident_field(db_path),
			 db_path_get_condition(db_path),
			 table_alias);
}



static int path_to_group(prelude_string_t *group, int flags, int index)
{
	int ret;

	if ( !(flags & PRELUDEDB_SELECTED_OBJECT_GROUP_BY) )
		return 0;

	ret = prelude_string_sprintf(group, "%s%d", prelude_string_is_empty(group) ? "" : ",", index);

	return (ret < 0) ? ret : 1;
}



static int path_to_order(prelude_string_t *order_buf, int flags, int index)
{
	int ret;
	char *order;

	if ( flags & PRELUDEDB_SELECTED_OBJECT_ORDER_ASC )
		order = "ASC";
	else if ( flags & PRELUDEDB_SELECTED_OBJECT_ORDER_DESC )
		order = "DESC";
	else
		return 0;

	ret = prelude_string_sprintf(order_buf, "%s%d %s",
				     prelude_string_is_empty(order_buf) ? "" : ",",
				     index, order);

	return (ret < 0) ? ret : 1;
}



static int strisdigit(const char *str)
{
	while ( *str++ )
		if ( ! isdigit(*str) )
			return 0;

	return 1;
}



static int path_to_field(prelude_string_t *fields,
			   prelude_string_t *group,
			   prelude_string_t *order,
			   idmef_path_t *path, int flags,
			   db_path_t *db_path, 
			   table_list_t *tables,
			   int *index)
{
	char *field = db_path_get_field(db_path);
	char *usec_field = db_path_get_usec_field(db_path);
	char *gmtoff_field = db_path_get_gmtoff_field(db_path);
	char *function = db_path_get_function(db_path);
	const char *table_alias;
	int ret;

	ret = path_to_table(tables, db_path, &table_alias);
	if ( ret < 0 )
		return ret;

	ret = add_field(fields, field ? table_alias : NULL, field ? field : function, NULL, flags);
	if ( ret < 0 )
		return ret;

	ret = path_to_group(group, flags, *index);
	if ( ret < 0 )
		return ret;

	ret = path_to_order(order, flags, *index);
	if ( ret < 0 )
		return ret;

	if ( ! (flags & (PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN|PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX|
			 PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG|PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD)) ) {

		if ( gmtoff_field ) {
			ret = add_field(fields, table_alias, gmtoff_field, NULL, 0);
			if ( ret < 0 )
				return ret;
		}

		if ( usec_field ) {
			ret = add_field(fields, strisdigit(usec_field) ? NULL : table_alias, usec_field, NULL, 0);
			if ( ret < 0 )
				return ret;
		}
	}

	return 0;
}



/*
 * This function does not modify WHERE clause as for now, but we pass it the
 * relevant buffer just in case
 */
static int paths_to_sql(preludedb_sql_t *sql,
			  prelude_string_t *fields,
			  prelude_string_t *where,
			  prelude_string_t *group,
			  prelude_string_t *order,
			  table_list_t *tables,
			  preludedb_path_selection_t *selection)
{
	preludedb_selected_path_t *selected;
	db_path_t *db_path;
	idmef_path_t *path;
	int flags;
	int index;
	int ret;

	index = 1;
	selected = NULL;
	while ( (selected = preludedb_path_selection_get_next(selection, selected)) ) {

		path = preludedb_selected_path_get_path(selected);
		flags = preludedb_selected_path_get_flags(selected);

		db_path = db_path_find(path);
		if ( ! db_path ) {
			prelude_log(PRELUDE_LOG_ERR,
				    "path '%s' not handled by database specification.\n",
				    idmef_path_get_name(path));
			return preludedb_error(PRELUDEDB_ERROR_GENERIC);
		}

		ret = path_to_field(fields, group, order, path, flags, db_path, tables, &index);
		if ( ret < 0 )
			return ret;

		index++;
	}

	return 0;
}




static int join_wheres(prelude_string_t *out, prelude_string_t *in)
{
	int ret = 0;

	if ( ! prelude_string_is_empty(in) ) {
		ret = prelude_string_sprintf(out, "%s(%s) ",
				    prelude_string_is_empty(out) ? "" : "AND ", get_string(in));
	}
	
	return ret;
}



static int build_request(preludedb_sql_t *sql, preludedb_path_selection_t *selection, idmef_criteria_t *criteria,
			 int distinct, int limit, int offset, int as_values, prelude_string_t *query)
{
	prelude_string_t 
		*str_tables = NULL,
		*where1 = NULL,
		*where2 = NULL,
		*where3 = NULL,
		*where = NULL,
		*fields = NULL,
		*group = NULL,
		*order = NULL,
		*limit_offset = NULL;
	table_list_t *tables = NULL;
	int ret = -1;

	ret = prelude_string_new(&fields);
	if ( ret < 0 )
		goto error;

	ret = table_list_new(&tables);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&where1);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&where2);
	if ( ret < 0 )
		goto error;
	
	ret = prelude_string_new(&where3);
	if ( ret < 0 )
		goto error;
	
	ret = prelude_string_new(&where);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&group);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&order);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&limit_offset);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_build_limit_offset_string(sql, limit, offset, limit_offset);
	if ( ret < 0 )
		goto error;

	ret = paths_to_sql(sql, fields, where1, group, order, tables, selection);
	if ( ret < 0 )
		goto error;

	/* criteria is optional */
	if ( criteria ) {                
                ret = criteria_to_sql(sql, where, tables, criteria);
                if ( ret < 0 )
                        goto error;
        }

	if ( as_values )
		ret = table_list_to_string_for_values(tables, where3, &str_tables);
	else
		ret = table_list_to_string_for_alerts(tables, &str_tables);

	if ( ret < 0 )
		goto error;

	/* build a complete WHERE statement */
	ret = join_wheres(where, where1);
	if ( ret < 0 )
		goto error;

	ret = join_wheres(where, where2);
	if ( ret < 0 )
		goto error;

	ret = join_wheres(where, where3);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_sprintf
		(query, "SELECT%s %s FROM %s %s %s %s %s %s %s %s;",
		 distinct ? " DISTINCT" : "",
		 get_string(fields), get_string(str_tables),
		 prelude_string_is_empty(where) ? "" : "WHERE", get_string(where),
		 prelude_string_is_empty(group) ? "" : "GROUP BY", get_string(group),
		 prelude_string_is_empty(order) ? "" : "ORDER BY", get_string(order),
		 get_string(limit_offset));


error:
	if ( tables )
		table_list_destroy(tables);

	if ( str_tables )
		prelude_string_destroy(str_tables);

	if ( fields )
		prelude_string_destroy(fields);

	if ( where1 )
		prelude_string_destroy(where1);

	if ( where2 )
		prelude_string_destroy(where2);

	if ( where3 )
		prelude_string_destroy(where3);

	if ( where )
		prelude_string_destroy(where);

	if ( group )
		prelude_string_destroy(group);

	if ( order )
		prelude_string_destroy(order);

	if ( limit_offset )
		prelude_string_destroy(limit_offset);

	return ret;
}



int idmef_db_select_idents(preludedb_sql_t *sql, char type, idmef_criteria_t *criteria,
			   int limit, int offset, preludedb_result_idents_order_t order,
			   preludedb_sql_table_t **table)
{
	prelude_string_t 
		*fields = NULL,
		*str_tables = NULL,
		*where = NULL, *where1 = NULL,
		*str_order = NULL,
		*limit_offset = NULL,
		*query = NULL;
	table_list_t *tables = NULL;
	int ret = -1;
	const char *table_name = (type == 'A') ? "Prelude_Alert" : "Prelude_Heartbeat";

	ret = prelude_string_new(&fields);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&where1);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&where);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&str_order);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&limit_offset);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_new(&query);
	if ( ret < 0 )
		goto error;

	ret = table_list_new(&tables);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_build_limit_offset_string(sql, limit, offset, limit_offset);
	if ( ret < 0 )
		goto error;

	if ( criteria ) {
		ret = criteria_to_sql(sql, where1, tables, criteria);
		if ( ret < 0 )
			goto error;
	}

	ret = prelude_string_sprintf(fields, "DISTINCT(%s._ident)",
				     tables->top_table ? tables->top_table : table_name);
	if ( ret < 0 )
		goto error;

	if ( order ) {
		char condition[64];
		const char *alias;
		
		snprintf(condition, sizeof(condition), "Prelude_CreateTime._parent_type='%c'", type);
		
		ret = add_table(tables,
				"Prelude_CreateTime", table_name, "_ident", "_message_ident", condition, &alias);
		if ( ret < 0 )
			goto error;

		ret = prelude_string_sprintf(fields, ", %s.time", alias);
		if ( ret < 0 )
			goto error;
		
		ret = prelude_string_sprintf(str_order, "ORDER BY 2 %s",
					     (order == PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_DESC)
					     ? "DESC" : "ASC");
		if ( ret < 0 )
			goto error;
	}

	ret = join_wheres(where, where1);
	if ( ret < 0 )
		goto error;

	ret = table_list_to_string_for_alerts(tables, &str_tables);
	if ( ret < 0 )
		goto error;

	ret = prelude_string_sprintf(query, "SELECT %s FROM %s %s %s %s %s;",
				     prelude_string_get_string(fields),
				     prelude_string_get_string_or_default(str_tables, table_name),
				     prelude_string_is_empty(where) ? "" : "WHERE",
				     prelude_string_get_string_or_default(where, ""),
				     prelude_string_get_string_or_default(str_order, ""),
				     prelude_string_get_string_or_default(limit_offset, ""));
	if ( ret < 0 )
		goto error;
			
	ret = preludedb_sql_query(sql, prelude_string_get_string(query), table);

 error:
	if ( fields )
		prelude_string_destroy(fields);

	if ( str_tables )
		prelude_string_destroy(str_tables);

	if ( where1 )
		prelude_string_destroy(where1);

	if ( where )
		prelude_string_destroy(where);

	if ( str_order )
		prelude_string_destroy(str_order);

	if ( limit_offset )
		prelude_string_destroy(limit_offset);

	if ( tables )
		table_list_destroy(tables);

	return ret;

}



int idmef_db_select(preludedb_sql_t *sql,
		    preludedb_path_selection_t *selection,
		    idmef_criteria_t *criteria,
		    int distinct,
		    int limit,
		    int offset,
		    int as_values,
		    preludedb_sql_table_t **table)
{
	prelude_string_t *query;
	int ret;

	ret = prelude_string_new(&query);
	if ( ret < 0 )
		return ret;

	ret = build_request(sql, selection, criteria, distinct, limit, offset, as_values, query);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_query(sql, prelude_string_get_string(query), table);

 error:
	prelude_string_destroy(query);

	return ret;
}
