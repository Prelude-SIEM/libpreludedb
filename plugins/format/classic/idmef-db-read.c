/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#include "idmef-db-read.h"

/* maximum length of text representation of object value */
#define VALLEN 262144


int add_table(strbuf_t *tables, char *table)
{
	int ret;
	
	/* Add the table to list if not already there */
	if ( strbuf_empty(tables) ) {
		/* list empty */
		ret = strbuf_sprintf(tables, "%s", table);
		if ( ret < 0 )
			return ret;
			
	} else {
		/* check if table has already been added or not */
		if ( strstr(strbuf_string(tables), table ) == NULL ) {
			ret = strbuf_sprintf(tables, ", %s", table);
			if ( ret < 0 )
				return ret;
		}
	}
	
	return 0;
}





int add_field(strbuf_t *fields, char *table, char *field, char *alias)
{
	strbuf_t *tmp;
	int ret;
	
	tmp = strbuf_new();
	if ( ! tmp )
		return -1;
			
	if ( table ) {
		ret = strbuf_sprintf(tmp, "%s.", table);
		if ( ret < 0 )
			goto error;
	}
		
	ret = strbuf_sprintf(tmp, "%s", field);
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

	return ret;
}



/* NOTE: This function assumes thet val is already escaped! */
int relation_to_sql(strbuf_t *where, char *table, char *field, idmef_relation_t rel, char *val)
{	
	switch (rel) {
	case relation_substring:
		if (table)
			return strbuf_sprintf(where, "%s.%s LIKE '%%%s%'", table, field, val);
		else
			return strbuf_sprintf(where, "%s LIKE '%%%s%'", field, val);

	case relation_regexp:
		return -1; /* unsupported */

	case relation_greater_then: 
		if (table)
			return strbuf_sprintf(where, "%s.%s > '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s > '%s'", field, val);
				      
	case relation_greater_or_equal: 
		if (table)
			return strbuf_sprintf(where, "%s.%s >= '%s'", table, field, val);
		else
			return strbuf_sprintf(where, "%s >= '%s'", field, val);

	case relation_less_then:
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



int criterion_to_sql(prelude_sql_connection_t *conn, strbuf_t *where, strbuf_t *tables, idmef_criterion_t *criterion)
{
	idmef_object_t *object;
	db_object_t *db;
	char buf[VALLEN];
	char *table, *field, *function, *top_table, *top_field, *ident_field, *condition;
	char *operator;
	idmef_relation_t relation;
	idmef_criterion_t *entry;
	char *value;
	int ret, add_operator;
	
	if ( idmef_criterion_is_chain(criterion) ) {

		ret = strbuf_sprintf(where, " ( ");
		if ( ret < 0 )
			return ret;

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
			}
			
			ret = criterion_to_sql(conn, where, tables, entry);
			if ( ret < 0 )
				return ret;
				
			add_operator = 1;
				
		}
		
		ret = strbuf_sprintf(where, " ) ");
		return ret;
				
	} else {
		
		object = idmef_criterion_get_object(criterion);
		db = db_object_find(object);
		
		table = db_object_get_table(db);
		field = db_object_get_field(db);
		function = db_object_get_function(db);
		top_table = db_object_get_top_table(db);
		top_field = db_object_get_top_field(db);
		condition = db_object_get_condition(db);
		ident_field = db_object_get_ident_field(db);

		relation = idmef_criterion_get_relation(criterion);
		
		/* Add table to JOIN list */
		ret = add_table(tables, table);
		if ( ret < 0 )
			return ret;
		
		
		idmef_value_to_string(idmef_criterion_get_value(criterion), buf, VALLEN);
		value = prelude_sql_escape(conn, buf);


		if ( condition || ( top_table && top_field && ident_field ) ) {
			ret = strbuf_sprintf(where, " ( ");
			if ( ret < 0 )
				return ret;			
		}


		if ( top_table && top_field && ident_field ) {

			/* Add top table to JOIN list */
			ret = add_table(tables, top_table);
			if ( ret < 0 )
				return ret;		
			
			/* Add WHERE clause for tables' relation */
			ret = strbuf_sprintf(where, "%s.%s = %s.%s AND ", 
				     	     top_table, top_field, table, ident_field);
			if ( ret < 0 ) 
				return ret;
		}
		
		if ( condition ) {
			ret = strbuf_sprintf(where,"%s AND ", condition);
			if ( ret < 0 )
				return ret;
		}
							
		ret = relation_to_sql(where, 
				      field ? table : NULL, 
				      field ? field : function, 
				      relation, value);
		if ( ret < 0 )
			return ret;

		if ( condition || ( top_table && top_field && ident_field ) ) {					
			ret = strbuf_sprintf(where, " ) ");
			if ( ret < 0 )
				return ret;
		}
		
		free(value);
		
		return 0;
	}
}


int objects_to_sql(prelude_sql_connection_t *conn, 
		   strbuf_t *fields,
		   strbuf_t *where, strbuf_t *tables, 
		   idmef_object_t **objects)
{
	idmef_object_t *obj, **obj_list;
	db_object_t *db;
	char *table;
	char *field;
	char *function;
	char *first_object = NULL;
	char *current_object, *end;
	char *top_table;
	char *top_field;
	char *condition;
	char *ident_field;
	int written, ret;
	strbuf_t *tmp;
	idmef_object_t *alert, *heartbeat;

	tmp = strbuf_new();
	if ( ! tmp ) {
		ret = -1;
		goto error;
	}
	
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

	
	/* Start our part of WHERE statement */

	obj_list = objects;
	written = 0;
	while ( ( obj = *obj_list++ ) ) {
		db = db_object_find(obj);
		table = db_object_get_table(db);
		field = db_object_get_field(db);
		function = db_object_get_function(db);
		top_table = db_object_get_top_table(db);
		top_field = db_object_get_top_field(db);
		condition = db_object_get_condition(db);
		ident_field = db_object_get_ident_field(db);

		/*
		 * Check if all objects have the same top level object 
		 * (alert or heartbeat). strcmp() only first field
		 * of numeric identifiers
		 */
		 
		if ( first_object ) {
			current_object = idmef_object_get_numeric(obj);
			
			if ( ! current_object ) {
				log(LOG_ERR, "could not get numeric address for object!\n");
				ret = -1;
				goto error;
			}
			
			end = strchr(current_object, '.');
			if ( ! end ) {
				log(LOG_ERR, "top-level object requested!\n");
				free(current_object);
				ret = -1;
				goto error;
			}
			
			*end = '\0';
			
			if ( strcmp(current_object, first_object) != 0 ) {
				log(LOG_ERR, "different top-level objects in the same query!\n");
				free(current_object);
				ret = -1;
				goto error;
			}
			
			free(current_object);
			
		} else {
			first_object = idmef_object_get_numeric(obj);
			
			if ( ! first_object ) {
				log(LOG_ERR, "could not get numeric address for object!\n");
				goto error;
			}
			
			end = strchr(first_object, '.');
			if ( ! end ) {
				log(LOG_ERR, "top-level object requested!\n");
				goto error;
			}
			
			*end = '\0';		
		}

		/* Add top table to JOIN list */
		if ( top_table ) {
			ret = add_table(tables, top_table);
			if ( ret < 0 )
				goto error;
		}
		
		/* Add object's table to JOIN list */
		ret = add_table(tables, table);
		if ( ret < 0 )
			goto error;
		
		/* 
		 * Add field to SELECT list, with alias if we're dealing with 
		 * alert.ident or heartbeat.ident 
		 */
		if ( ( idmef_object_compare(obj, alert) == 0 ) || 
		     ( idmef_object_compare(obj, heartbeat) == 0 ) )
		     	ret = add_field(fields, table, field, "ident");
		else 
			ret = add_field(fields, 
					field ? table : NULL, 
					field ? field : function, 
					NULL);
		
		if ( ret < 0 )
			goto error;

		if ( top_table && top_field && ident_field ) {
			ret = strbuf_sprintf(tmp, "%s.%s = %s.%s", 
				     	     top_table, top_field, table, ident_field);
			if ( ret < 0 )
				goto error;
		
			
			if ( strstr(strbuf_string(where), strbuf_string(tmp)) == NULL ) {
				ret = strbuf_sprintf(where, " %s ( %s ) ", 
						     written++ ? "AND" : "(",
						     strbuf_string(tmp));
				if ( ret < 0 )
					goto error;
			}
		
		}
				
		/* Add condition to WHERE statement */
		if ( condition && ( strstr(strbuf_string(where), condition) == NULL ) ) {
			ret = strbuf_sprintf(where, " %s ( %s ) ", 
    					     written++ ? "AND" : "(",
					     condition);
			if ( ret < 0 )
				goto error;
		}
		
		strbuf_clear(tmp);
		
		
	}
	
	/* Close our part of WHERE statement */
	if ( written ) {
		ret = strbuf_sprintf(where, " ) ");
		if ( ret < 0 )
			goto error;
	}

	ret = 0;
	
error:
	strbuf_destroy(tmp);

	if ( first_object )
		free(first_object);

	idmef_object_destroy(alert);
	idmef_object_destroy(heartbeat);

	return ret;
}




strbuf_t *build_request(prelude_sql_connection_t *conn, 
   		        idmef_object_t **objects, 
       		        idmef_criterion_t *criterion)
{
	strbuf_t *request = NULL;
	strbuf_t *tables = NULL, *where1 = NULL, *where2 = NULL, *fields = NULL;
	int ret;
	
	request = strbuf_new();
	if ( ! request )
		goto error;

	fields = strbuf_new();
	if ( ! fields )
		goto error;
		
	tables = strbuf_new();
	if ( ! tables )
		goto error;
		
	where1 = strbuf_new();
	if ( ! where1 )
		goto error;
	
	where2 = strbuf_new();
	if ( ! where2 )
		goto error;
		
	ret = objects_to_sql(conn, fields, where1, tables, objects);
	if ( ret < 0 )
		goto error;

	/* criterion is optional */	
	if ( criterion ) {	
		ret = criterion_to_sql(conn, where2, tables, criterion);
		if ( ret < 0 )
			goto error;
	}
	
	ret = strbuf_sprintf(request, "SELECT DISTINCT %s FROM %s WHERE %s %s %s ", 
		             strbuf_string(fields), 
		             strbuf_string(tables), 
		             strbuf_string(where1), 
		             ( strbuf_empty(where1) || strbuf_empty(where2) ? "" : "AND" ), 
		             strbuf_string(where2));
	if ( ret < 0 )
		goto error;

error:	
	
	if ( tables ) 
		strbuf_destroy(tables);

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




struct db_handle {
	prelude_sql_connection_t *conn;
	prelude_sql_table_t *table;
	idmef_cache_t *cache;
	idmef_cached_object_t **cached;
	idmef_object_t **objects;
	int object_count;
	int line;
	prelude_sql_row_t *row;
	uint64_t prev_ident;
};


int idmef_db_read(void *handle)
{
	struct db_handle *data = handle;
	prelude_sql_field_t *field;
	idmef_value_t *val;
	uint64_t ident;
	int i, ret;	

	do {
	
		if ( ! data->row ) {
			data->row = prelude_sql_row_fetch(data->table);
			if ( ! data->row ) {
				prelude_sql_table_free(data->table);
				free(data->cached);
				free(data->objects);
				free(data);
				/* cache will be freed by the caller */
				return 0; /* no more data */
			}
		} else 		
			idmef_cache_purge(data->cache);
	
		field = prelude_sql_field_fetch_by_name(data->row, "ident");
		if ( ! field ) {
			log(LOG_ERR, "could not find 'ident' field!\n");
			return -1;
		}
	
		ident = prelude_sql_field_value_uint64(field);

		if ( ( data->line++ ) && ( data->prev_ident != ident ) ) {
			data->prev_ident = ident;
			return 1;
		}

		data->prev_ident = ident;

		i = 0;		
		for ( i = 0; i < data->object_count; i++ ) {
			field = prelude_sql_field_fetch(data->row, i);
			val = prelude_sql_field_value_idmef(field);
			
			ret = idmef_cached_object_set(data->cached[i], val);
			if ( ret < 0 ) {
				log(LOG_ERR, "error in IDMEF cache operation!\n");
				return -i;
			}
		} 

		prelude_sql_row_free(data->row);
		data->row = NULL;
	
	} while(1);
	
	/* not reached */
	
	return 1;

}



void *idmef_db_prepare(prelude_db_connection_t *conn, idmef_cache_t *cache, 
       		       idmef_criterion_t *criterion)
{
	prelude_sql_connection_t *sql;
	strbuf_t *request;
	prelude_sql_table_t *table;
	int ret, i, fields, object_count = 0;
	idmef_cached_object_t **cached;
	idmef_object_t **objects, *first, *ident_obj;
	char *first_name, *end;
	db_object_t *ident_db;
	char *ident_table, *ident_field;
	struct db_handle *handle;
	char buf[128];

	if ( prelude_db_connection_get_type(conn) != prelude_db_type_sql ) {
		log(LOG_ERR, "SQL database required for classic format!\n");
		return NULL;
	}

	sql = prelude_db_connection_get(conn);

	ret = idmef_cache_index(cache);
	if ( ret < 0 ) {
		log(LOG_ERR, "cache indexing error!\n");
		return NULL;
	}

	object_count = idmef_cache_get_object_count(cache);
	if ( object_count == 0 ) {
		log(LOG_ERR, "no objects requested!\n");
		return NULL;
	}
	
	if ( object_count < 0 ) {
		log(LOG_ERR, "idmef_cache_get_object_count() failed!\n");
		return NULL;		
	}
	
	/* 
	 * check if the first object is a subclass of alert or heartbeat 
	 * and add 'alert.ident' or 'heartbeat.ident' to cache if not already there
	 */
	first = idmef_cached_object_get_object(idmef_cache_get_object(cache, 0));
	if ( ! first ) {
		log(LOG_ERR, "could not get first object from cache!\n");
		return NULL;
	}
	
	first_name = idmef_object_get_name(first);
	end = strchr(first_name, '.');
	if ( end )
		*end = '\0';
	
	buf[sizeof(buf)-1]='\0'; /* ensure that result will be null-terminated */
	strncpy(buf, first_name, sizeof(buf)-1);
	strncat(buf, ".ident", sizeof(buf)-1);
		
	free(first_name);
	
	ident_obj = idmef_object_new(buf);
	if ( ! ident_obj ) {
		log(LOG_ERR, "could not create %s object!\n");
		return NULL;
	}
	
	if ( ! idmef_cache_register_object(cache, ident_obj) ) {
		log(LOG_ERR, "could not register %s object!\n");
		idmef_object_destroy(ident_obj);
		return NULL;
	}
	
	idmef_cache_reindex(cache);
	
	ident_db = db_object_find(ident_obj);
	ident_table = db_object_get_table(ident_db);
	ident_field = db_object_get_field(ident_db);
	
	idmef_object_destroy(ident_obj);
	
	/* get updated object count */
	object_count = idmef_cache_get_object_count(cache);
	
	/* allocate needed tables */
	
	cached = malloc((object_count+1)*sizeof(idmef_cached_object_t *));
	if ( ! cached ) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}
	
	objects = malloc((object_count+1)*sizeof(idmef_cached_object_t *));
	if ( ! objects ) {
		log(LOG_ERR, "out of memory!\n");
		free(cached);
		return NULL;
	}
	
	cached[object_count] = NULL;
	objects[object_count] = NULL;

	for ( i = 0; i < object_count; i++ ) {
		cached[i] = idmef_cache_get_object(cache, i);
		objects[i] = idmef_cached_object_get_object(cached[i]);
	}

	request = build_request(sql, objects, criterion);
	if ( ! request ) {
		free(cached);
		free(objects);
		return NULL;
	}

	ret = strbuf_sprintf(request, " ORDER BY %s.%s ;", ident_table, ident_field);
	if ( ret < 0 )
		return NULL;

	table = prelude_sql_query(sql, strbuf_string(request));	
	if ( ! table ) {
		log(LOG_ERR, "Query %s failed: %s\n", strbuf_string(request), 
				prelude_sql_errno(sql) ? 
				prelude_sql_error(sql) : "unknown error");
		strbuf_destroy(request);
		return NULL;
	}

	strbuf_destroy(request);

	fields = prelude_sql_fields_num(table);

	if ( fields != object_count ) {
		log(LOG_ERR, "Requested %d objects, read %d fields: something is wrong\n", 
		    object_count, fields);
		return NULL;
	}
	
	handle = malloc(sizeof(*handle));
	if ( ! handle ) {
		log(LOG_ERR, "out of memory\n");
		return NULL;
	}
	
	handle->conn = sql;
	handle->table = table;
	handle->cache = cache;
	handle->cached = cached;
	handle->objects = objects;	
	handle->object_count = object_count;
	handle->line = 0;
	handle->row = NULL;

	return handle;
}


