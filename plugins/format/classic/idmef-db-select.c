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

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object.h"
#include "strbuf.h"

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




static strbuf_t *table_list_to_strbuf(table_list_t *tlist)
{
	strbuf_t *buf;
	int ret;
	struct list_head *tmp;
	table_entry_t *entry;

	buf = strbuf_new();
	if ( ! buf )
		return NULL;

	ret = strbuf_sprintf(buf, "%s", tlist->top_table);
	if ( ret < 0 )
		goto error;

	list_for_each(tmp, &tlist->tables) {
		entry = list_entry(tmp, table_entry_t, list);
		ret = strbuf_sprintf(buf, " LEFT JOIN %s AS %s ON %s.%s = %s.%s",
					  entry->table, entry->alias, tlist->top_table,
					  entry->top_field, entry->alias,
					  entry->ident_field);
		if ( ret < 0 )
			goto error;

		if ( entry->condition ) {
			ret = strbuf_sprintf(buf, " AND (%s)", entry->condition);
			if ( ret < 0 )
				goto error;
		}
	}

	return buf;

error:
	strbuf_destroy(buf);
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




static int aggregate_function_to_sql(strbuf_t *buf, 
				     idmef_aggregate_function_t func, 
				     char *field)
{
	switch (func) {
	case function_none:
			return strbuf_sprintf(buf, "%s", field);

	case function_min:
			return strbuf_sprintf(buf, "MIN(%s)", field);

	case function_max:
			return strbuf_sprintf(buf, "MAX(%s)", field);

	case function_avg:
			return strbuf_sprintf(buf, "AVG(%s)", field);

	case function_std:
			return strbuf_sprintf(buf, "STD(%s)", field);

	case function_count:
			return strbuf_sprintf(buf, "COUNT(%s)", field);


	default:
			return -1;
	}
}




static int add_field(strbuf_t *fields, char *table, char *field, char *alias,
		     idmef_aggregate_function_t func)
{
	strbuf_t *tmp, *tmp2;
	int ret;

	tmp = strbuf_new();
	if ( ! tmp )
		return -1;
	
	tmp2 = strbuf_new();
	if ( ! tmp )
		return -1;
	

	if ( table ) {
		ret = strbuf_sprintf(tmp2, "%s.%s", table, field);
		if ( ret < 0 )
			goto error;
	}

	ret = aggregate_function_to_sql(tmp, func, strbuf_string(tmp2));
	if ( ret < 0 )
		goto error;

	if ( alias ) {
		ret = strbuf_sprintf(tmp, " AS %s", alias);
		if ( ret < 0 )
			goto error;
	}

	/* Add the field to list  */
	if ( strbuf_empty(fields) ) {
		/* list empty */
		ret = strbuf_sprintf(fields, "%s", strbuf_string(tmp));
		if ( ret < 0 )
			goto error;
	} else {
		ret = strbuf_sprintf(fields, ", %s", strbuf_string(tmp));
		if ( ret < 0 )
			goto error;
	}


	ret = 0;

error:
	strbuf_destroy(tmp);
	strbuf_destroy(tmp2);

	return ret;
}




/* NOTE: This function assumes that val is already escaped! */
static int relation_to_sql(strbuf_t *where, char *table, char *field, idmef_relation_t rel, char *val)
{
	switch (rel) {
	case relation_substring:
		if (table)
			return strbuf_sprintf(where, "%s.%s LIKE '%%%s%'", table, field, val);
		else
			return strbuf_sprintf(where, "%s LIKE '%%%s%'", field, val);

	case relation_regexp:
		return -1; /* unsupported */

	case relation_greater:
		if (table)
			return strbuf_sprintf(where, "%s.%s > '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s > '%s'", field, val);

	case relation_greater_or_equal:
		if (table)
			return strbuf_sprintf(where, "%s.%s >= '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s >= '%s'", field, val);

	case relation_less:
		if (table)
			return strbuf_sprintf(where, "%s.%s < '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s < '%s'", field, val);


	case relation_less_or_equal:
		if (table)
			return strbuf_sprintf(where, "%s.%s <= '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s <= '%s'", field, val);

	case relation_equal:
		if (table)
			return strbuf_sprintf(where, "%s.%s = '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s = '%s'", field, val);

	case relation_not_equal:
		if (table)
			return strbuf_sprintf(where, "%s.%s <> '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s <> '%s'", field, val);

	case relation_is_null:
		if (table)
			return strbuf_sprintf(where, "%s.%s IS NULL", table, field);
		else
			return strbuf_sprintf(where, "%s IS NULL", field);

	case relation_is_not_null:
		if (table)
			return strbuf_sprintf(where, "%s.%s IS NOT NULL", table, field);
		else
			return strbuf_sprintf(where, "%s IS NOT NULL", field);

	default:
		return -1;
	}
}




static int criterion_to_sql(prelude_sql_connection_t *conn,
		     strbuf_t *where,
		     table_list_t *tables,
		     idmef_criterion_t *criterion)
{
	idmef_object_t *object;
	db_object_t *db;
	char buf[VALLEN];
	char *table, *field, *function, *top_table, *top_field, *ident_field;
	char *condition, *table_alias;
	char *operator;
	idmef_relation_t relation;
	idmef_criterion_t *entry;
	char *value;
	int ret, add_operator;

	if ( idmef_criterion_is_chain(criterion) ) {

		entry = NULL;
		add_operator = 0;
		while ( (entry = idmef_criterion_get_next(criterion, entry)) ) {

			if ( add_operator ) {
				switch ( idmef_criterion_get_operator(criterion) ) {
					case operator_or:
						operator = "OR";
						break;

					case operator_and:
						operator = "AND";
						break;

					default:
						log(LOG_ERR, "unknown operator!\n");
						return -1;
				}

				ret = strbuf_sprintf(where, " %s ", operator);
				if ( ret < 0 )
					return ret;
			} else {
				ret = strbuf_sprintf(where, " ( ");
				if ( ret < 0 )
					return ret;
			}

			ret = criterion_to_sql(conn, where, tables, entry);
			if ( ret < 0 )
				return ret;

			add_operator = 1;

		}

		/* add_operator = 0 signifies that the chain was empty */
		if ( add_operator ) {
			ret = strbuf_sprintf(where, " ) ");
			return ret;
		}

		return 0;

	} else {

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


		idmef_value_to_string(idmef_criterion_get_value(criterion), buf, VALLEN);
		value = prelude_sql_escape(conn, buf);

		ret = relation_to_sql(where,
				      field ? table_alias : NULL,
				      field ? field : function,
				      relation, value);

		free(value);

		if ( ret < 0 )
			return ret;

		return 0;
	}
}




/*
 * This function does not modify WHERE clause as for now, but we pass it the
 * relevant buffer just in case
 */
static int objects_to_sql(prelude_sql_connection_t *conn,
		   strbuf_t *fields,
		   strbuf_t *where,
		   table_list_t *tables,
		   idmef_selection_t *selection)
{
	idmef_object_t *obj;
	db_object_t *db;
	char *table;
	char *field;
	char *function;
	char *top_table;
	char *top_field;
	char *condition;
	char *ident_field;
	char *table_alias;
	int ret = -1;
	idmef_object_t *alert = NULL, *heartbeat = NULL;

	alert = idmef_object_new("alert.ident");
	if ( ! alert ) {
		log(LOG_ERR, "could not create alert.ident object!\n");
		ret = -1;
		goto error;
	}

	heartbeat = idmef_object_new("heartbeat.ident");
	if ( ! heartbeat ) {
		log(LOG_ERR, "could not create heartbeat.ident object!\n");
		ret = -1;
		goto error;
	}

	idmef_selection_set_object_iterator(selection);
	while ( (obj = idmef_selection_get_next_object(selection)) ) {

		db = db_object_find(obj);
		if ( ! db ) {
			const char *objname = idmef_object_get_name(obj);
			char *objnum = idmef_object_get_numeric(obj);

			log(LOG_ERR, "object %s [%s] not handled by database specification!\n",
				     objname, objnum);
			free(objnum);
			ret = -1;
			goto error;
		}

		table = db_object_get_table(db);
		field = db_object_get_field(db);
		function = db_object_get_function(db);
		top_table = db_object_get_top_table(db);
		top_field = db_object_get_top_field(db);
		condition = db_object_get_condition(db);
		ident_field = db_object_get_ident_field(db);

		/* Add object's table to LEFT JOIN list */
		table_alias = add_table(tables, table, top_table, top_field,
				        ident_field, condition);
		if ( ! table_alias )
			goto error;

		/*
		 * Add field to SELECT list, with alias if we're dealing with
		 * alert.ident or heartbeat.ident
		 */
		if ( ( idmef_object_compare(obj, alert) == 0 ) ||
		     ( idmef_object_compare(obj, heartbeat) == 0 ) )
		     	ret = add_field(fields, table, field, "ident", 
		     			idmef_selection_get_function(selection));
		else
			ret = add_field(fields,
					field ? table_alias : NULL,
					field ? field : function,
					NULL, 
					idmef_selection_get_function(selection));

		if ( ret < 0 )
			goto error;

	}

	ret = 0;

error:
	idmef_object_destroy(alert);
	idmef_object_destroy(heartbeat);

	return ret;
}




static strbuf_t *build_request(prelude_sql_connection_t *conn,
			       int distinct,
			       idmef_selection_t *selection,
			       idmef_criterion_t *criterion)
{
	strbuf_t *request = NULL;
	strbuf_t *str_tables = NULL, *where1 = NULL, *where2 = NULL, *fields = NULL;
	table_list_t *tables = NULL;
	int ret = -1;

	request = strbuf_new();
	if ( ! request )
		goto error;

	fields = strbuf_new();
	if ( ! fields )
		goto error;

	tables = table_list_new();
	if ( ! tables )
		goto error;

	where1 = strbuf_new();
	if ( ! where1 )
		goto error;

	where2 = strbuf_new();
	if ( ! where2 )
		goto error;

	ret = objects_to_sql(conn, fields, where1, tables, selection);
	if ( ret < 0 )
		goto error;

	/* criterion is optional */
	if ( criterion ) {
		ret = criterion_to_sql(conn, where2, tables, criterion);
		if ( ret < 0 )
			goto error;
	}

	str_tables = table_list_to_strbuf(tables);
	if ( ! str_tables )
		goto error;

	ret = strbuf_sprintf(request, "SELECT %s%s FROM %s %s %s %s %s ;",
			     distinct ? "DISTINCT " : "",
		             strbuf_string(fields),
		             strbuf_string(str_tables),
			     ( strbuf_empty(where1) && strbuf_empty(where2) ) ? "" : "WHERE",
		             strbuf_string(where1),
		             ( strbuf_empty(where1) || strbuf_empty(where2) ? "" : "AND" ),
		             strbuf_string(where2));
	if ( ret < 0 )
		goto error;

error:

	if ( tables )
		table_list_destroy(tables);

	if ( str_tables )
		strbuf_destroy(str_tables);

	if ( fields )
		strbuf_destroy(fields);

	if ( where1 )
		strbuf_destroy(where1);

	if ( where2 )
		strbuf_destroy(where2);

	if ( ret >= 0 )
		return request;

	if ( request )
		strbuf_destroy(request);

	return NULL;
}




prelude_sql_table_t * idmef_db_select(prelude_db_connection_t *conn,
				      int distinct,
				      idmef_selection_t *selection,
				      idmef_criterion_t *criterion)
{
	prelude_sql_connection_t *sql;
	strbuf_t *request;
	prelude_sql_table_t *table;

	if ( prelude_db_connection_get_type(conn) != prelude_db_type_sql ) {
		log(LOG_ERR, "SQL database required for classic format!\n");
		return NULL;
	}

	sql = prelude_db_connection_get(conn);

	request = build_request(sql, distinct, selection, criterion);
	if ( ! request )
		return NULL;

	table = prelude_sql_query(sql, strbuf_string(request));
	if ( ! table && prelude_sql_errno(sql) ) {
		log(LOG_ERR, "Query %s failed: %s\n",
		    strbuf_string(request),
		    prelude_sql_errno(sql) ? prelude_sql_error(sql) : "unknown error");
		strbuf_destroy(request);
		return NULL;
	}

#ifdef DEBUG
	log(LOG_INFO, "query returned %d rows\n", table ? prelude_sql_rows_num(table) : 0);
#endif

	strbuf_destroy(request);

	return table;
}

