/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
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
#include <stdarg.h>

#include <libprelude/prelude-strbuf.h>
#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-util.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object.h"
#include "db-object-selection.h"

#include "idmef-db-select.h"

/* maximum length of text representation of object value */
#define VALLEN 262144

typedef struct table_entry {
	struct list_head list;
	char *table;
	char *top_field;
	char *ident_field;
	char *condition;
	char *alias;
} table_entry_t;




typedef struct table_list {
	char *top_table;
	struct list_head tables;
} table_list_t;




static char *normalize_condition(char *condition, char *table, char *alias)
{
	char buf[1024];
	char *start;
	char *ret;

	/*
	 * In Perl, this would be:
	 * ( $buf = $condition ) =~ s/$table/$alias/ ;
	 *
	 * Hm, again, why are we using C here?
	 */
	start = strstr(condition, table);
	if ( ! start ) {
		/* This shouldn't happen, but we'll handle it cleanly */
		strncpy(buf, condition, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
	} else {
		if ( start - condition > sizeof(buf)-1 ) {
			log(LOG_ERR, "buffer too small?!\n");
			return NULL;
		}
		strncpy(buf, condition, start - condition);
		buf[start - condition] = '\0';
		strncat(buf, alias, sizeof(buf)-1);
		strncat(buf, start + strlen(table), sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
	}

	ret = strdup(buf);
	if ( ! ret ) {
		log(LOG_ERR, "out of memory\n");
		return NULL;
	}

	return ret;
}




static table_entry_t *table_entry_new(char *table,  char *top_field,
				      char *ident_field, char *alias,
				      char *condition)
{
	table_entry_t *entry;

	entry = calloc(1, sizeof(*entry));
	if ( ! entry ) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}

	entry->table = strdup(table);
	if ( ! entry->table ) {
		log(LOG_ERR, "out of memory!\n");
		goto error;
	}


	entry->top_field = strdup(top_field);
	if ( ! entry->top_field ) {
		log(LOG_ERR, "out of memory!\n");
		goto error;
	}

	entry->ident_field = strdup(ident_field);
	if ( ! entry->ident_field ) {
		log(LOG_ERR, "out of memory!\n");
		goto error;
	}

	entry->alias = strdup(alias);
	if ( ! entry->alias ) {
		log(LOG_ERR, "out of memory!\n");
		goto error;
	}

	if ( condition ) {
		entry->condition = normalize_condition(condition, table, alias);
		if ( ! entry->condition )
			goto error;
	}

	/* o! no errors! ;P */
	return entry;

error:
	if ( entry->table )
		free(entry->table);

	if ( entry->top_field )
		free(entry->top_field);

	if ( entry->ident_field )
		free(entry->ident_field);

	if ( entry->alias )
		free(entry->alias);

	if ( entry->condition )
		free(entry->condition);

	free(entry);

	return NULL;
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




static table_list_t *table_list_new(void)
{
	table_list_t *tlist;

	tlist = calloc(1, sizeof(*tlist));
	if ( ! tlist ) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}

	INIT_LIST_HEAD(&tlist->tables);

	return tlist;
}




static void table_list_destroy(table_list_t *tlist)
{
	struct list_head *tmp, *n;
	table_entry_t *entry;

	list_for_each_safe(tmp, n, &tlist->tables) {
		entry = list_entry(tmp, table_entry_t, list);
		list_del(&entry->list);
		table_entry_destroy(entry);
	}

	if ( tlist->top_table )
		free(tlist->top_table);

	free(tlist);
}




static prelude_strbuf_t *table_list_to_strbuf_for_alerts(table_list_t *tlist)
{
	int ret;
	struct list_head *tmp;
	table_entry_t *entry;
	prelude_strbuf_t *buf;
        
	buf = prelude_strbuf_new();
	if ( ! buf )
		return NULL;

	ret = prelude_strbuf_sprintf(buf, "%s", tlist->top_table);
	if ( ret < 0 )
		goto error;

	list_for_each(tmp, &tlist->tables) {
		entry = list_entry(tmp, table_entry_t, list);
		ret = prelude_strbuf_sprintf(buf, " LEFT JOIN %s AS %s ON %s.%s = %s.%s",
                                             entry->table, entry->alias, tlist->top_table,
                                             entry->top_field, entry->alias,
                                             entry->ident_field);
		if ( ret < 0 )
			goto error;

		if ( entry->condition ) {
			ret = prelude_strbuf_sprintf(buf, " AND (%s)", entry->condition);
			if ( ret < 0 )
				goto error;
		}
	}

	return buf;

error:
	prelude_strbuf_destroy(buf);
	return NULL;
}




static prelude_strbuf_t *table_list_to_strbuf_for_values(table_list_t *tlist, prelude_strbuf_t *where)
{
	int ret;
	struct list_head *tmp;
	table_entry_t *entry;
        prelude_strbuf_t *buf;
        
	buf = prelude_strbuf_new();
	if ( ! buf )
		return NULL;

	/* 
	 * We must always join with top table, otherwise e.g. a request for
	 * all source/target address pairs would give us all possible 
	 * source/target combinations, even the ones that never appeared
	 * in one alert. 
	 */
	ret = prelude_strbuf_sprintf(buf, "%s", tlist->top_table);
	if ( ret < 0 ) 
		goto error;

	list_for_each(tmp, &tlist->tables) {
		entry = list_entry(tmp, table_entry_t, list);
		ret = prelude_strbuf_sprintf(buf, ", %s AS %s",
                                             entry->table, entry->alias);
		if ( ret < 0 )
			goto error;
		
		ret = prelude_strbuf_sprintf(where, "%s%s.%s = %s.%s",
                                            prelude_strbuf_is_empty(where) ? "" : " AND ",
                                             tlist->top_table, entry->top_field, 
                                             entry->alias, entry->ident_field);
		if ( ret < 0 )
			goto error;

		if ( entry->condition ) {
			ret = prelude_strbuf_sprintf(where, " AND (%s)", entry->condition);
			if ( ret < 0 )
				goto error;
		}
	}

	return buf;

error:
	prelude_strbuf_destroy(buf);
	return NULL;
}




static int set_top_table(table_list_t *tlist, char *table)
{
	tlist->top_table = strdup(table);
	if ( ! tlist->top_table ) {
		log(LOG_ERR, "out of memory!\n");
		return -1;
	}

	return 0;
}




static char *add_table(table_list_t *tlist, char *table, char *top_table,
		       char *top_field, char *ident_field, char *condition)
{
	struct list_head *tmp;
	table_entry_t *entry;
	int id;
	int ret;
	char alias[64];
	char *ncond;

	if ( top_table ) {
		if ( tlist->top_table ) {
			if ( strcmp(tlist->top_table, top_table) != 0 ) {
				log(LOG_ERR, "different top-level tables!\n");
				return NULL;
			}
		} else {
			ret = set_top_table(tlist, top_table);
			if ( ret < 0 )
				return NULL;
		}
	} else {

		/* requesting a top-level table; this must be handled specially */
#ifdef DEBUG
		log(LOG_INFO, "special case in add_table()\n");
#endif

		if ( ! tlist->top_table ) {
			ret = set_top_table(tlist, table);
			return ( ret < 0 ) ? NULL : tlist->top_table;
		} else {
			ret = strcmp(table, tlist->top_table);
			if ( ret == 0 )
				return tlist->top_table;
			else {
				log(LOG_ERR, "attempt to set different top-level"
					     "table; different top-level objects"
					     "requested?\n");
				return NULL;
			}
		}

	}

	/* check if the table was already added */

	id = 0;

	list_for_each(tmp, &tlist->tables) {
		entry = list_entry(tmp, table_entry_t, list);

		if ( strcmp(table, entry->table) == 0 ) {
			if ( condition ) {
				ncond = normalize_condition(condition,
							    entry->table,
							    entry->alias);
				if ( strcmp(ncond, entry->condition) == 0 ) {
					free(ncond);
					return entry->alias;  /* already added */
				}
				free(ncond);
			} else
				return entry->alias;  /* already added */
		}
		id++;
	}

	/* not found, we have to add it */
	snprintf(alias, sizeof(alias)-1, "t%d", id);
	alias[sizeof(alias)-1] = '\0'; /* just in case */

	entry = table_entry_new(table,  top_field, ident_field, alias, condition);
	if ( ! entry )
		return NULL;

	list_add_tail(&entry->list, &tlist->tables);

	return entry->alias;

}




static int aggregate_function_to_sql(prelude_strbuf_t *buf, int flags, char *field)
{
	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN )
		return prelude_strbuf_sprintf(buf, "MIN(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX )
		return prelude_strbuf_sprintf(buf, "MAX(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG )
		return prelude_strbuf_sprintf(buf, "AVG(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD )
		return prelude_strbuf_sprintf(buf, "STD(%s)", field);

	if ( flags & PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT)
		return prelude_strbuf_sprintf(buf, "COUNT(%s)", field);

	return prelude_strbuf_sprintf(buf, "%s", field);
}




static int add_field(prelude_strbuf_t *fields, char *table,
                     char *field, char *alias, int flags)
{
	int ret;
	prelude_strbuf_t *tmp, *tmp2;
        
	tmp = prelude_strbuf_new();
	if ( ! tmp )
		return -1;
	
	tmp2 = prelude_strbuf_new();
	if ( ! tmp )
		return -1;

	if ( table ) {
		ret = prelude_strbuf_sprintf(tmp2, "%s.%s", table, field);
		if ( ret < 0 )
			goto error;
	}

	ret = aggregate_function_to_sql(tmp, flags, prelude_strbuf_get_string(tmp2));
	if ( ret < 0 )
		goto error;

	if ( alias ) {
		ret = prelude_strbuf_sprintf(tmp, " AS %s", alias);
		if ( ret < 0 )
			goto error;
	}

	/* Add the field to list  */
	if ( prelude_strbuf_is_empty(fields) ) {
		/* list empty */
		ret = prelude_strbuf_sprintf(fields, "%s", prelude_strbuf_get_string(tmp));
		if ( ret < 0 )
			goto error;
	} else {
		ret = prelude_strbuf_sprintf(fields, ", %s", prelude_strbuf_get_string(tmp));
		if ( ret < 0 )
			goto error;
	}


	ret = 0;

error:
	prelude_strbuf_destroy(tmp);
	prelude_strbuf_destroy(tmp2);

	return ret;
}


static int relation_substring_to_sql(prelude_strbuf_t *where, char *table, char *field,
				     idmef_relation_t rel, char *val)
{
	size_t len;

	if ( val[1] == '*' )
		val[1] = '%';

	len = strlen(val);

	if ( len >= 2 && val[len - 2] == '*' )
		val[len - 2] = '%';

	return (table ?
		prelude_strbuf_sprintf(where, "%s.%s LIKE %s", table, field, val) :
		prelude_strbuf_sprintf(where, "%s LIKE %s", field, val));
}



/* 
 * NOTE: This function assumes that val is already escaped
 */
static int relation_to_sql(prelude_strbuf_t *where, char *table, char *field, idmef_relation_t rel, char *val)
{
	switch (rel) {
	case relation_substring:
		return relation_substring_to_sql(where, table, field, rel, val);

	case relation_regexp:
		return -1; /* unsupported */

	case relation_greater:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s > %s", table, field, val) :
			prelude_strbuf_sprintf(where, "%s > %s", field, val));

	case relation_greater_or_equal:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s >= %s", table, field, val) :
			prelude_strbuf_sprintf(where, "%s >= %s", field, val));

	case relation_less:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s < %s", table, field, val) :
			prelude_strbuf_sprintf(where, "%s < %s", field, val));


	case relation_less_or_equal:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s <= %s", table, field, val) :
			prelude_strbuf_sprintf(where, "%s <= %s", field, val));

	case relation_equal:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s = %s", table, field, val) :
			prelude_strbuf_sprintf(where, "%s = %s", field, val));

	case relation_not_equal:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s <> %s", table, field, val) :
			prelude_strbuf_sprintf(where, "%s <> %s", field, val));

	case relation_is_null:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s IS NULL", table, field) :
			prelude_strbuf_sprintf(where, "%s IS NULL", field));

	case relation_is_not_null:
		return (table ?
			prelude_strbuf_sprintf(where, "%s.%s IS NOT NULL", table, field) :
			prelude_strbuf_sprintf(where, "%s IS NOT NULL", field));

	default:
		/* nop */;
	}

	return -1;
}


static char *value_to_sql(prelude_sql_connection_t *conn, idmef_value_t *value, char *buf, size_t size)
{
	if ( idmef_value_get_type(value) == type_time ) {
		idmef_time_t *time;

		time = idmef_value_get_time(value);
		if ( ! time )
			return NULL;

		if ( idmef_time_get_db_timestamp(time, buf, size) < 0 )
			return NULL;
	} else {
		if ( idmef_value_to_string(value, buf, size) < 0 )
			return NULL;
	}

	return prelude_sql_escape(conn, buf);
}



static int criterion_to_sql(prelude_sql_connection_t *conn,
			   prelude_strbuf_t *where,
			    table_list_t *tables,
			    idmef_criterion_t *criterion)
{
	idmef_object_t *object;
	db_object_t *db;
	char buf[VALLEN];
	char *table, *field, *function, *top_table, *top_field, *ident_field;
	char *condition, *table_alias;
	idmef_relation_t relation;
	char *value = NULL;
	int retval;

	object = idmef_criterion_get_object(criterion);
	db = db_object_find(object);
	if ( ! db ) {
		const char *objname = idmef_object_get_name(object);
		char *objnum = idmef_object_get_numeric(object);

		log(LOG_ERR, "object %s [%s] not handled by database specification!\n",
		    objname, objnum);
		free(objnum);
		return -1;
	}

	table = db_object_get_table(db);
	field = db_object_get_field(db);
	function = db_object_get_function(db);
	top_table = db_object_get_top_table(db);
	top_field = db_object_get_top_field(db);
	condition = db_object_get_condition(db);
	ident_field = db_object_get_ident_field(db);

	relation = idmef_criterion_get_relation(criterion);

	/* Add table to JOIN list */
	table_alias = add_table(tables, table, top_table, top_field,
				ident_field, condition);
	if ( ! table_alias )
		return -1;

	if ( idmef_criterion_get_value(criterion) ) {
		value = value_to_sql(conn, idmef_criterion_get_value(criterion), buf, VALLEN);
		if ( ! value )
			return -2;
	}

	retval = relation_to_sql(where,
				 field ? table_alias : NULL,
				 field ? field : function,
				 relation, value);

	free(value);

	return retval;
}


static int criteria_to_sql(prelude_sql_connection_t *conn,
			   prelude_strbuf_t *where,
			   table_list_t *tables,
			   idmef_criteria_t *criteria)
{
	idmef_criteria_t *criteria_ptr, *criteria_prev;
	char *operator;

	criteria_ptr = NULL;
	criteria_prev = NULL;
	while ( (criteria_ptr = idmef_criteria_get_next(criteria, criteria_ptr)) ) {

		if ( criteria_prev ) {
			switch ( idmef_criteria_get_operator(criteria_prev) ) {
			case operator_and:
				operator = "AND";
				break;

			case operator_or:
				operator = "OR";
				break;

			default:
				log(LOG_ERR, "unknown operator %d\n", idmef_criteria_get_operator(criteria_prev));
				return -1;
			}

			if ( prelude_strbuf_sprintf(where, " %s ", operator) < 0 )
				return -1;

		}

		if ( idmef_criteria_is_criterion(criteria_ptr) ) {
			if ( criterion_to_sql(conn, where, tables, idmef_criteria_get_criterion(criteria_ptr)) < 0 )
				return -1;

		} else {
			if ( prelude_strbuf_sprintf(where, " ( ") < 0 )
				return -1;

			if ( criteria_to_sql(conn, where, tables, criteria_ptr) < 0 )
				return -1;

			if ( prelude_strbuf_sprintf(where, " ) ") < 0 )
				return -1;
		}

		criteria_prev = criteria_ptr;
	}

	return 0;
}



static char *object_to_table(table_list_t *tables, db_object_t *db_object)
{
	/* Add object's table to LEFT JOIN list */

	return add_table(tables,
			 db_object_get_table(db_object),
			 db_object_get_top_table(db_object),
			 db_object_get_top_field(db_object),
			 db_object_get_ident_field(db_object),
			 db_object_get_condition(db_object));
}



static int object_to_field(prelude_strbuf_t *fields,
			   idmef_object_t *object, int flags,
			   db_object_t *db_object,
			   char *table_alias)
{
	idmef_object_t *alert = NULL, *heartbeat = NULL;
	char *table = db_object_get_table(db_object);
	char *field = db_object_get_field(db_object);
	char *function = db_object_get_function(db_object);
	int retval;

	alert = idmef_object_new_fast("alert.ident");
	if ( ! alert ) {
		log(LOG_ERR, "could not create alert.ident object!\n");
		return -1;
	}

	heartbeat = idmef_object_new_fast("heartbeat.ident");
	if ( ! heartbeat ) {
		log(LOG_ERR, "could not create heartbeat.ident object!\n");
		idmef_object_destroy(alert);
		return -2;
	}

	/*
	 * Add field to SELECT list, with alias if we're dealing with
	 * alert.ident or heartbeat.ident
	 */
	if ( ( idmef_object_compare(object, alert) == 0 ) ||
	     ( idmef_object_compare(object, heartbeat) == 0 ) )
		retval = add_field(fields, table, field, "ident", flags);
	else
		retval = add_field(fields,
				   field ? table_alias : NULL,
				   field ? field : function,
				   NULL,
				   flags);

	idmef_object_destroy(alert);
	idmef_object_destroy(heartbeat);

	return (retval < 0) ? -3 : retval;
}



static int object_to_group(prelude_strbuf_t *group, int flags, int index)
{
	int retval;

	if ( !(flags & PRELUDEDB_SELECTED_OBJECT_GROUP_BY) )
		return 0;

	retval = prelude_strbuf_sprintf(group, "%s%d", prelude_strbuf_is_empty(group) ? "" : ",", index);

	return (retval < 0) ? -1 : 1;
}



static int object_to_order(prelude_strbuf_t *order_buf, int flags, int index)
{
	int retval;
	char *order;

	if ( flags & PRELUDEDB_SELECTED_OBJECT_ORDER_ASC )
		order = "ASC";
	else if ( flags & PRELUDEDB_SELECTED_OBJECT_ORDER_DESC )
		order = "DESC";
	else
		return 0;

	retval = prelude_strbuf_sprintf(order_buf, "%s%d %s",
			prelude_strbuf_is_empty(order_buf) ? "" : ",",
			index, order);

	return (retval < 0) ? -1 : 1;
}




/*
 * This function does not modify WHERE clause as for now, but we pass it the
 * relevant buffer just in case
 */
static int objects_to_sql(prelude_sql_connection_t *conn,
			  prelude_strbuf_t *fields,
			  prelude_strbuf_t *where,
			  prelude_strbuf_t *group,
			  prelude_strbuf_t *order,
			  table_list_t *tables,
			  prelude_db_object_selection_t *selection)
{
	prelude_db_selected_object_t *selected;
	db_object_t *db_object;
	idmef_object_t *object;
	char *table_alias;
	int flags;
	int cnt;

	cnt = 0;
	selected = NULL;
	while ( (selected = prelude_db_object_selection_get_next(selection, selected)) ) {

		object = prelude_db_selected_object_get_object(selected);
		flags = prelude_db_selected_object_get_flags(selected);

		db_object = db_object_find(object);
		if ( ! db_object ) {
			const char *objname = idmef_object_get_name(object);
			char *objnum = idmef_object_get_numeric(object);

			log(LOG_ERR, "object %s [%s] not handled by database specification!\n",
				     objname, objnum);
			free(objnum);
			return -1;
		}

		table_alias = object_to_table(tables, db_object);
		if ( ! table_alias )
			return -2;

		if ( object_to_field(fields, object, flags, db_object, table_alias) < 0 )
			return -3;

		if ( object_to_group(group, flags, cnt + 1) < 0 )
			return -4;

		if ( object_to_order(order, flags, cnt + 1) < 0 )
			return -5;

		cnt++;
	}

	return 0;
}




static int join_wheres(prelude_strbuf_t *out,prelude_strbuf_t *in)
{
	int ret = 0;

	if ( ! prelude_strbuf_is_empty(in) ) {
		ret = prelude_strbuf_sprintf(out, "%s(%s) ",
				    prelude_strbuf_is_empty(out) ? "" : "AND ",
				    prelude_strbuf_get_string(in));
	}
	
	return ret;
}


static int limit_offset_to_sql(prelude_sql_connection_t *conn,
			       prelude_strbuf_t *strbuf,
			       int limit, int offset)
{
	const char *tmp;

	tmp = prelude_sql_limit_offset(conn, limit, offset);
	if ( ! tmp )
		return -1;

	prelude_strbuf_cat(strbuf, tmp);

	return 0;
}



static prelude_strbuf_t *build_request(prelude_sql_connection_t *conn,
				       prelude_db_object_selection_t *selection,
				       idmef_criteria_t *criteria,
				       int distinct,
				       int limit,
				       int offset,
				       int as_values)
{
	prelude_strbuf_t 
		*request = NULL,
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

	request = prelude_strbuf_new();
	if ( ! request )
		goto error;

	fields = prelude_strbuf_new();
	if ( ! fields )
		goto error;

	tables = table_list_new();
	if ( ! tables )
		goto error;

	where1 = prelude_strbuf_new();
	if ( ! where1 )
		goto error;

	where2 = prelude_strbuf_new();
	if ( ! where2 )
		goto error;
	
	where3 = prelude_strbuf_new();
	if ( ! where3 )
		goto error;
	
	where = prelude_strbuf_new();
	if ( ! where )
		goto error;

	group = prelude_strbuf_new();
	if ( ! group )
		goto error;

	order = prelude_strbuf_new();
	if ( ! order )
		goto error;

	limit_offset = prelude_strbuf_new();
	if ( ! limit_offset )
		goto error;

	ret = limit_offset_to_sql(conn, limit_offset, limit, offset);
	if ( ret < 0 )
		goto error;

	ret = objects_to_sql(conn, fields, where1, group, order, tables, selection);
	if ( ret < 0 )
		goto error;

	/* criterion is optional */
	if ( criteria ) {
		ret = criteria_to_sql(conn, where2, tables, criteria);
		if ( ret < 0 )
			goto error;
	}

	if ( as_values )
		str_tables = table_list_to_strbuf_for_values(tables, where3);
	else
		str_tables = table_list_to_strbuf_for_alerts(tables);

	if ( ! str_tables )
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

	/* build the query */
	ret = prelude_strbuf_sprintf
		(request, "SELECT%s %s FROM %s %s %s %s %s %s %s %s;",
		 distinct ? " DISTINCT" : "",
		 prelude_strbuf_get_string(fields),
		 prelude_strbuf_get_string(str_tables),
		 prelude_strbuf_is_empty(where) ? "" : "WHERE",
		 prelude_strbuf_get_string(where),
		 prelude_strbuf_is_empty(group) ? "" : "GROUP BY", prelude_strbuf_get_string(group),
		 prelude_strbuf_is_empty(order) ? "" : "ORDER BY", prelude_strbuf_get_string(order),
		 prelude_strbuf_get_string(limit_offset));
	if ( ret < 0 )
		goto error;

	/* done, finally :-) */

error:

	if ( tables )
		table_list_destroy(tables);

	if ( str_tables )
		prelude_strbuf_destroy(str_tables);

	if ( fields )
		prelude_strbuf_destroy(fields);

	if ( where1 )
		prelude_strbuf_destroy(where1);

	if ( where2 )
		prelude_strbuf_destroy(where2);

	if ( where3 )
		prelude_strbuf_destroy(where3);
	
	if ( where )
		prelude_strbuf_destroy(where);

	if ( group )
		prelude_strbuf_destroy(group);

	if ( order )
		prelude_strbuf_destroy(order);

	if ( limit_offset )
		prelude_strbuf_destroy(limit_offset);

	if ( ret >= 0 )
		return request;

	if ( request )
		prelude_strbuf_destroy(request);

	return NULL;
}




prelude_sql_table_t *idmef_db_select(prelude_db_connection_t *conn,
				     prelude_db_object_selection_t *selection,
				     idmef_criteria_t *criteria,
				     int distinct,
				     int limit,
				     int offset,
				     int as_values)
{
	prelude_sql_connection_t *sql;
	prelude_strbuf_t *request;
	prelude_sql_table_t *table;

	if ( prelude_db_connection_get_type(conn) != prelude_db_type_sql ) {
		log(LOG_ERR, "SQL database required for classic format!\n");
		return NULL;
	}

	sql = prelude_db_connection_get(conn);

	request = build_request(sql, selection, criteria, distinct, limit, offset, as_values);
	if ( ! request )
		return NULL;

	table = prelude_sql_query(sql, prelude_strbuf_get_string(request));
	if ( ! table && prelude_sql_errno(sql) ) {
		log(LOG_ERR, "Query %s failed: %s\n",
		    prelude_strbuf_get_string(request),
		    prelude_sql_errno(sql) ? prelude_sql_error(sql) : "unknown error");
		prelude_strbuf_destroy(request);
		return NULL;
	}

#ifdef DEBUG
	log(LOG_INFO, "query returned %d rows\n", table ? prelude_sql_rows_num(table) : 0);
#endif

	prelude_strbuf_destroy(request);

	return table;
}
