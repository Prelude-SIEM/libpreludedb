/*****
*
* Copyright (C) 2001-2004 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2002, 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <assert.h>

#include <libprelude/prelude-list.h>
#include <libprelude/common.h>
#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-util.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "plugin-sql.h"
#include "db-type.h"
#include "db-connection.h"


/* 
 * maximum length of text representation of object value
 */
#define SQL_MAX_VALUE_LEN 262144


struct prelude_sql_connection {
        void *session;
	plugin_sql_t *plugin;
	prelude_sql_connection_data_t *data;
};

struct prelude_sql_table {
	prelude_sql_connection_t *conn;
	void *res;
	prelude_list_t row_list;
};

struct prelude_sql_row {
	prelude_list_t list;
	prelude_sql_table_t *table;
	void *res;
	prelude_list_t field_list;
};

struct prelude_sql_field {
	prelude_list_t list;
	prelude_sql_row_t *row;
	int num;
	const char *value;
	size_t len;
};

/*
 * va_copy() was introduced in C99.
 * in C89, it was available throught __va_copy().
 *
 * A safe bet is to fallback on memcpy() in case __va_copy()
 * isn't defined. Thought it won't work with some va_list
 * implementation.
 */
#ifndef va_copy

 #if defined (__va_copy)
  #define va_copy __va_copy
#else
  #warning "Both va_copy (C99) and __va_copy (C89) are undefined: working arround."
  #warning "Result might be undefined depending on your va_list implementation."
  #define va_copy(dst, src) memcpy(&(dst), &(src), sizeof(dst))
 #endif

#endif



/*
 * define what we believe should be enough for most of our query.
 */
#define DB_REQUEST_LENGTH 16384

/*
 * Maximum value of a dynamically allocated buffer in case
 * DB_REQUEST_LENGTH isn't enough (the length is computed taking
 * prelude_string_to_hex() function into account).
 * 1024 : "INSERT INTO table_name (field_names ...)
 * 65536 * (3+21/16) + 1 : a 2^16 length segment logged with its hexa dump based on prelude_string_to_hex()
 * 1 : the ')' at the end of an SQL request
 */
#define DB_MAX_INSERT_QUERY_LENGTH (1024 + 65536 * (3+21/16) + 1 + 1)



static prelude_sql_table_t *prelude_sql_table_new(prelude_sql_connection_t *conn, void *res)
{
	prelude_sql_table_t *prelude_sql_table;

	prelude_sql_table = malloc(sizeof (*prelude_sql_table));
	if ( ! prelude_sql_table ) {
		log(LOG_ERR, "memory exhausted.\n");
                return NULL;
	}
	
	prelude_sql_table->conn = conn;
	prelude_sql_table->res = res;
	PRELUDE_INIT_LIST_HEAD(&prelude_sql_table->row_list);
	
	return prelude_sql_table;
}



static prelude_sql_row_t *prelude_sql_row_new(prelude_sql_table_t *table, void *res)
{
	prelude_sql_row_t *prelude_sql_row;

	prelude_sql_row = malloc(sizeof (*prelude_sql_row));
	if ( ! prelude_sql_row ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}
	
	prelude_sql_row->table = table;
	prelude_sql_row->res = res;

        PRELUDE_INIT_LIST_HEAD(&prelude_sql_row->list);
	PRELUDE_INIT_LIST_HEAD(&prelude_sql_row->field_list);

        prelude_list_add_tail(&prelude_sql_row->list, &table->row_list);
	
	return prelude_sql_row;
}



static prelude_sql_field_t *prelude_sql_field_new(prelude_sql_row_t *row, int num, const char *value, size_t len)
{
	prelude_sql_field_t *prelude_sql_field;

	prelude_sql_field = malloc(sizeof (*prelude_sql_field));
	if ( ! prelude_sql_field ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}
	
	prelude_sql_field->row = row;
	prelude_sql_field->num = num;
	prelude_sql_field->value = value;
	prelude_sql_field->len = len;

        PRELUDE_INIT_LIST_HEAD(&prelude_sql_field->list);

        prelude_list_add_tail(&prelude_sql_field->list, &row->field_list);
	
	return prelude_sql_field;
}



static char *generate_dynamic_query(const char *old, size_t olen,
                                    int *nlen, const char *fmt, va_list ap) 
{
        int ret;
        char *query;

        assert(olen < *nlen);
        
        query = malloc(*nlen);
        if ( ! query ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
                
        strncpy(query, old, olen);
        ret = vsnprintf(query + olen, *nlen - olen, fmt, ap);
        
        if ( (ret + 2) > (*nlen - olen) || ret < 0 ) {
                log(LOG_ERR, "query %s doesn't fit in %d bytes.\n", query, *nlen);
                free(query);
                return NULL;
        }

        *nlen = ret + olen;
        
        return query;
}




static char *generate_query(char *buf, size_t size, int *len, const char *fmt, va_list ap)
{
        char *query;
        va_list copy;
        int query_length;

        va_copy(copy, ap);
        
        /*
         * These  functions  return  the number of characters printed
         * (not including the trailing `\0' used  to  end  output  to
         * strings).   snprintf  and vsnprintf do not write more than
         * size bytes (including the trailing '\0'), and return -1 if
         * the  output  was truncated due to this limit.
         */
        query_length = vsnprintf(buf + *len, size - *len, fmt, ap);

        if ( (query_length + 2) <= (size - *len) ) {
                *len = query_length + *len;
                va_end(copy);
                return buf;
        }
        
        if ( query_length < 0 ) 
                query_length = DB_MAX_INSERT_QUERY_LENGTH;
        else 
                query_length += *len + 2;

        query = generate_dynamic_query(buf, *len, &query_length, fmt, copy);
        va_end(copy);
        
        if ( ! query )
                return NULL;

        *len = query_length;
                
        return query;
}


prelude_sql_table_t *prelude_sql_query(prelude_sql_connection_t *conn, const char *fmt, ...)
{
        va_list ap;
	void *res;
        int query_length = 0;
        char query_static[DB_REQUEST_LENGTH], *query;
        
        va_start(ap, fmt);
        query = generate_query(query_static, sizeof(query_static), &query_length, fmt, ap);
        va_end(ap);

        res = conn->plugin->db_query(conn->session, query);
	
	if ( query != query_static )
                free(query);

        return res ? prelude_sql_table_new(conn, res) : NULL;
}



int prelude_sql_insert(prelude_sql_connection_t *conn, const char *table, const char *fields,
		       const char *fmt, ...)
{
        va_list ap;
        int len, ret;
        char query_static[DB_REQUEST_LENGTH], *query;

        len = snprintf(query_static, sizeof(query_static), "INSERT INTO %s (%s) VALUES(", table, fields);
        if ( (len + 1) > sizeof(query_static) || len < 0 ) {
                log(LOG_ERR, "start of query doesn't fit in %d bytes.\n", sizeof(query_static));
                return -1;
        }
        
        va_start(ap, fmt);
        query = generate_query(query_static, sizeof(query_static), &len, fmt, ap);
        va_end(ap);
        
        if ( ! query )
                return -1;

        /*
         * generate_query() allow us to add one byte at the end of the buffer.
         */
        query[len] = ')';
        query[len + 1] = '\0';

        conn->plugin->db_query(conn->session, query);
	ret = -prelude_sql_errno(conn);

        if ( query != query_static )
                free(query);
        
        return ret;
}




char *prelude_sql_escape_fast(prelude_sql_connection_t *conn, const char *buf, size_t len) 
{
	if ( ! buf ) 
		return strdup("NULL");
	
	return (conn && conn->plugin) ? 
		conn->plugin->db_escape(conn->session, buf, len) : NULL;
}




char *prelude_sql_escape(prelude_sql_connection_t *conn, const char *string) 
{
        if ( ! string )
                return strdup("NULL");
        
        return (conn && conn->plugin) ? 
		conn->plugin->db_escape(conn->session, string, strlen(string)) : NULL;
}




const char *prelude_sql_limit_offset(prelude_sql_connection_t *conn, int limit, int offset)
{
	return (conn && conn->plugin) ?
		conn->plugin->db_limit_offset(conn->session, limit, offset): 
		NULL;
}



int prelude_sql_begin(prelude_sql_connection_t *conn)
{
	return (conn && conn->plugin) ? conn->plugin->db_begin(conn->session) : -1;
}



int prelude_sql_commit(prelude_sql_connection_t *conn)
{
	return (conn && conn->plugin) ? conn->plugin->db_commit(conn->session) : -1;
}



int prelude_sql_rollback(prelude_sql_connection_t *conn)
{
	return (conn && conn->plugin) ? conn->plugin->db_rollback(conn->session) : -1;
}



void prelude_sql_close(prelude_sql_connection_t *conn)
{
	if (conn && conn->plugin)
    		conn->plugin->db_close(conn->session);

	free(conn);
}



prelude_sql_connection_t *prelude_sql_connect(prelude_sql_connection_data_t *data) 
{
	int ret;
        void *session;
	plugin_sql_t *plugin;
	prelude_sql_connection_t *conn;
	char *dbtype, *dbhost, *dbport, *dbname, *dbuser, *dbpass;
		
	dbtype = prelude_sql_connection_data_get_type(data);
	dbhost = prelude_sql_connection_data_get_host(data);
	dbport = prelude_sql_connection_data_get_port(data);
	dbname = prelude_sql_connection_data_get_name(data);
	dbuser = prelude_sql_connection_data_get_user(data);
	dbpass = prelude_sql_connection_data_get_pass(data);

	if ( ! dbtype ) {
		log(LOG_ERR, "dbtype not specified\n");
		return NULL;
	}
	
	plugin = (plugin_sql_t *) prelude_plugin_search_by_name(dbtype);
	if ( ! plugin ) {
		errno = -ERR_DB_PLUGIN_NOT_FOUND;
		return NULL;
	}
	
	/*
         * any needed parameter translation should go in here
         */
	session = plugin->db_setup(dbhost, dbport, dbname, dbuser, dbpass);
	if ( ! session ) 
		return NULL; /* FIXME: better error detection ? */
	
	ret = plugin->db_connect(session);
	if ( ret < 0 ) {
                plugin->db_close(session);
		return NULL; /* FIXME: better error detection ? */
        }
        
	conn = calloc(1, sizeof(prelude_sql_connection_t));
	if ( ! conn ) {
		log(LOG_ERR, "memory exhausted.\n");
                plugin->db_close(session);
		return NULL;
	}
	
	conn->plugin = plugin;
	conn->session = session;
	
	return conn;
}



int prelude_sql_errno(prelude_sql_connection_t *conn)
{
	return conn->plugin->db_errno(conn->session);
}



const char *prelude_sql_error(prelude_sql_connection_t *conn)
{
	static char *error_strings[] = {
		"Success",
		"Plugin not enabled",
		"Sessions exhausted",
		"Not connected",
		"Query error",
		"No transaction",
		"Connection failed",
		"Result table error",
		"Result row error",
		"Result field error",
		"Incorrect parameters",
		"Already connected",
		"Memory exhausted",
		"Illegal field num",
		"Illegal field name",
		"Connection lost",
		"Two queries in the same time"
	};
	int db_errno;

	db_errno = prelude_sql_errno(conn);
	if ( db_errno >= (sizeof (error_strings) / sizeof (error_strings[0])) )
		return NULL;

	return error_strings[db_errno];
}


void prelude_sql_table_free(prelude_sql_table_t *table)
{
	prelude_sql_connection_t *conn = table->conn;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
        prelude_list_t *tmp, *next_row, *next_field;

	prelude_list_for_each_safe(tmp, next_row, &table->row_list) {
		row = prelude_list_entry(tmp, prelude_sql_row_t, list);

                prelude_list_for_each_safe(tmp, next_field, &row->field_list) {
			field = prelude_list_entry(tmp, prelude_sql_field_t, list);
			free(field);
		}

                free(row);
	}
        
	conn->plugin->db_table_free(conn->session, table->res);
	free(table);
}



const char *prelude_sql_field_name(prelude_sql_table_t *table, unsigned int i)
{
	prelude_sql_connection_t *conn = table->conn;

	return conn->plugin->db_field_name(conn->session, table->res, i);
}



int prelude_sql_field_num(prelude_sql_table_t *table, const char *name)
{
	prelude_sql_connection_t *conn = table->conn;

	return conn->plugin->db_field_num(conn->session, table->res, name);
}


prelude_sql_field_type_t prelude_sql_field_type(prelude_sql_table_t *table, unsigned int i)
{
	prelude_sql_connection_t *conn = table->conn;
	
	return conn->plugin->db_field_type(conn->session, table->res, i);
}



prelude_sql_field_type_t prelude_sql_field_type_by_name(prelude_sql_table_t *table, const char *name)
{
	prelude_sql_connection_t *conn = table->conn;

	return conn->plugin->db_field_type_by_name(conn->session, table->res, name);
}



unsigned int prelude_sql_fields_num(prelude_sql_table_t *table)
{
	prelude_sql_connection_t *conn = table->conn;

	return conn->plugin->db_fields_num(conn->session, table->res);
}



unsigned int prelude_sql_rows_num(prelude_sql_table_t *table)
{
	prelude_sql_connection_t *conn = table->conn;	

	return conn->plugin->db_rows_num(conn->session, table->res);
}



prelude_sql_row_t *prelude_sql_row_fetch(prelude_sql_table_t *table)
{
	prelude_sql_connection_t *conn = table->conn;
	void *res;

	res = conn->plugin->db_row_fetch(conn->session, table->res);
	return res ? prelude_sql_row_new(table, res) : NULL;
}


void prelude_sql_row_free(prelude_sql_row_t *row)
{
	prelude_list_t *tmp, *next;
	prelude_sql_field_t *field;

	if ( ! row )
		return;

	prelude_list_for_each_safe(tmp, next, &row->field_list) {
		field = prelude_list_entry(tmp, prelude_sql_field_t, list);
		free(field);
	}

	prelude_list_del(&row->list);
	free(row);
}


prelude_sql_field_t *prelude_sql_field_fetch(prelude_sql_row_t *row, unsigned int i)
{
	prelude_sql_table_t *table = row->table;
	prelude_sql_connection_t *conn = table->conn;
	const char *value;
	size_t len;

	if ( conn->plugin->db_field_fetch(conn->session, table->res, row->res, i, &value, &len) <= 0 )
		return NULL;

	return prelude_sql_field_new(row, i, value, len);
}



prelude_sql_field_t *prelude_sql_field_fetch_by_name(prelude_sql_row_t *row, const char *name)
{
	prelude_sql_table_t *table = row->table;
	prelude_sql_connection_t *conn = table->conn;
	const char *value;
	size_t len;
	int num;

	num = prelude_sql_field_num(table, name);
	if ( num == - 1)
		return NULL;

	if ( conn->plugin->db_field_fetch_by_name(conn->session, table->res, row->res, name, &value, &len) <= 0 )
		return NULL;

	return prelude_sql_field_new(row, num, value, len);
}



const char *prelude_sql_field_value(prelude_sql_field_t *field)
{
	return field->value;
}



size_t prelude_sql_field_len(prelude_sql_field_t *field)
{
	return field->len;
}



prelude_sql_field_type_t prelude_sql_field_info_type(prelude_sql_field_t *field)
{
	return prelude_sql_field_type(field->row->table, field->num);
}



uint8_t prelude_sql_field_value_uint8(prelude_sql_field_t *field)
{
	const char *s;
	uint8_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%hhd", &i);
	return i;
}



int16_t prelude_sql_field_value_int16(prelude_sql_field_t *field)
{
	const char *s;
	int16_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%hd", &i);
	return i;
}



uint16_t prelude_sql_field_value_uint16(prelude_sql_field_t *field)
{
	const char *s;
	uint16_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%hu", &i);
	return i;
}



int32_t prelude_sql_field_value_int32(prelude_sql_field_t *field)
{
	const char *s;
	int32_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%d", &i);
	return i;
}



uint32_t prelude_sql_field_value_uint32(prelude_sql_field_t *field)
{
	const char *s;
	uint32_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%u", &i);
	return i;
}



int64_t prelude_sql_field_value_int64(prelude_sql_field_t *field)
{
	const char *s;
	int64_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%" PRId64, &i);
	return i;
}



uint64_t prelude_sql_field_value_uint64(prelude_sql_field_t *field)
{
	const char *s;
	uint64_t i;

	s = prelude_sql_field_value(field);
	sscanf(s, "%" PRIu64, &i);
	return i;
}



float prelude_sql_field_value_float(prelude_sql_field_t *field)
{
	const char *s;
	float f;

	s = prelude_sql_field_value(field);
	sscanf(s, "%f", &f);
	return f;
}



double prelude_sql_field_value_double(prelude_sql_field_t *field)
{
	const char *s;
	double d;

	s = prelude_sql_field_value(field);
	sscanf(s, "%lf", &d);
	return d;
}



prelude_string_t *prelude_sql_field_value_string(prelude_sql_field_t *field)
{
	const char *s;

	s = prelude_sql_field_value(field);
	return prelude_string_new_dup(s);
}



idmef_value_t *prelude_sql_field_value_idmef(prelude_sql_field_t *field)
{
	idmef_value_t *value = NULL;
	prelude_sql_field_type_t type;

	type = prelude_sql_field_type(field->row->table, field->num);

	switch ( type ) {
		
	case dbtype_int32:
		value = idmef_value_new_int32(prelude_sql_field_value_int32(field));
		break;

	case dbtype_uint32:
		value = idmef_value_new_uint32(prelude_sql_field_value_uint32(field));
		break;

	case dbtype_int64:
		value = idmef_value_new_int64(prelude_sql_field_value_int64(field));
		break;

	case dbtype_uint64:
		value = idmef_value_new_uint64(prelude_sql_field_value_uint64(field));
		break;

	case dbtype_float:
		value = idmef_value_new_float(prelude_sql_field_value_float(field));
		break;

	case dbtype_double:
		value = idmef_value_new_double(prelude_sql_field_value_double(field));
		break;

	case dbtype_string:
		value = idmef_value_new_string(prelude_sql_field_value_string(field));
		break;

	default:
		break;
	}

	return value;
}



const char *prelude_sql_idmef_relation_to_string(idmef_value_relation_t relation)
{
        int i;
        struct tbl {
                idmef_value_relation_t relation;
                const char *name;
        } tbl[] = {
                { IDMEF_VALUE_RELATION_SUBSTR,                            "LIKE" },
                { IDMEF_VALUE_RELATION_REGEX,                               NULL },
                { IDMEF_VALUE_RELATION_GREATER,                              ">" },
                { IDMEF_VALUE_RELATION_GREATER|IDMEF_VALUE_RELATION_EQUAL,  ">=" },
                { IDMEF_VALUE_RELATION_LESSER,                               "<" },
                { IDMEF_VALUE_RELATION_LESSER|IDMEF_VALUE_RELATION_EQUAL,   "<=" },
                { IDMEF_VALUE_RELATION_EQUAL,                                "=" },
                { IDMEF_VALUE_RELATION_NOT_EQUAL,                           "!=" },
                { IDMEF_VALUE_RELATION_IS_NULL,                        "IS NULL" },
                { IDMEF_VALUE_RELATION_IS_NOT_NULL,                "IS NOT NULL" },
                { 0, NULL },
        };

        for ( i = 0; tbl[i].relation != 0; i++ )
                if ( relation == tbl[i].relation )
                        return tbl[i].name;
        
	return NULL;
}



static int build_time_constraint(prelude_sql_connection_t *conn,
				 prelude_string_t *output, const char *field,
				 prelude_sql_time_constraint_type_t type,
				 idmef_value_relation_t relation, int value)
{
	uint32_t gmt_offset;

	if ( prelude_get_gmt_offset(&gmt_offset) < 0 )
		return -1;

	return conn->plugin->db_build_time_constraint(output, field, type, relation, value, gmt_offset);
}



static int build_time_interval(prelude_sql_connection_t *conn,
			       prelude_sql_time_constraint_type_t type, int value,
			       char *buf, size_t size)

{
	return conn->plugin->db_build_time_interval(type, value, buf, size);
}



static const struct {
	prelude_sql_time_constraint_type_t type;
	int (*get_value)(idmef_criterion_value_non_linear_time_t *);
} non_linear_time_values[] = {
	{ dbconstraint_year,	idmef_criterion_value_non_linear_time_get_year	},
	{ dbconstraint_month,	idmef_criterion_value_non_linear_time_get_month	},
	{ dbconstraint_yday,	idmef_criterion_value_non_linear_time_get_yday	},
	{ dbconstraint_mday,	idmef_criterion_value_non_linear_time_get_mday	},
	{ dbconstraint_wday,	idmef_criterion_value_non_linear_time_get_wday	},
	{ dbconstraint_hour,	idmef_criterion_value_non_linear_time_get_hour	},
	{ dbconstraint_min,	idmef_criterion_value_non_linear_time_get_min	},
	{ dbconstraint_sec,	idmef_criterion_value_non_linear_time_get_sec	}
};



static int build_criterion_non_linear_time_value_relation_equal(prelude_sql_connection_t *conn,
								prelude_string_t *output,
								const char *field,
								idmef_value_relation_t relation,
								idmef_criterion_value_non_linear_time_t *time)
{
	int i;
	int value;
	int prev = 0;

	for ( i = 0; i < sizeof (non_linear_time_values) / sizeof (non_linear_time_values[0]); i++ ) {
		value = non_linear_time_values[i].get_value(time);
		if ( value == -1 )
			continue;

		if ( prev++ )
			if ( prelude_string_cat(output, " AND ") < 0 )
				return -1;

		if ( build_time_constraint(conn, output, field, 
					   non_linear_time_values[i].type,
					   relation, value) < 0 )
			return -1;
	}

	return 0;
}


static int build_criterion_non_linear_time_value_relation_not_equal(prelude_sql_connection_t *conn,
								    prelude_string_t *output,
								    const char *field,
								    idmef_criterion_value_non_linear_time_t *time)
{
	if ( prelude_string_cat(output, "NOT(") < 0 )
		return -1;

	if ( build_criterion_non_linear_time_value_relation_equal(conn, output,
								  field, IDMEF_VALUE_RELATION_EQUAL, time) < 0 )
		return -1;

	return prelude_string_cat(output, ")");
}



static int build_criterion_timestamp(prelude_sql_connection_t *conn,
				     prelude_string_t *output,
				     const char *field,
				     idmef_value_relation_t relation,
				     idmef_criterion_value_non_linear_time_t *time)
{
	struct tm tm;
	int month, day, hour, min, sec;
	idmef_time_t t;
	char buf[IDMEF_TIME_MAX_STRING_SIZE];
	int offset = 0;

	if ( relation == IDMEF_VALUE_RELATION_GREATER || relation == (IDMEF_VALUE_RELATION_LESSER|IDMEF_VALUE_RELATION_EQUAL) )
		offset = 1;

	month = idmef_criterion_value_non_linear_time_get_month(time);
	day = idmef_criterion_value_non_linear_time_get_mday(time);
	hour = idmef_criterion_value_non_linear_time_get_hour(time);
	min = idmef_criterion_value_non_linear_time_get_min(time);
	sec = idmef_criterion_value_non_linear_time_get_sec(time);

	memset(&tm, 0, sizeof (tm));
	tm.tm_mday = 1;
	tm.tm_isdst = -1;

	tm.tm_year = idmef_criterion_value_non_linear_time_get_year(time) - 1900;

	/*
	 * This is not ascii art ;)
	 */

	if ( month != -1 ) {
		tm.tm_mon = month - 1;

		if ( day != -1 ) {
			tm.tm_mday = day;

			if ( hour != -1 ) {
				tm.tm_hour = hour;

				if ( min != -1 ) {
					tm.tm_min = min;

					if ( sec != -1 ) {
						tm.tm_sec = sec + offset;

					} else {
						tm.tm_min += offset;
					}
				} else {
					tm.tm_hour += offset;
				}
			} else {
				tm.tm_mday += offset;
			}
		} else {
			tm.tm_mon += offset;
		}
	} else {
		tm.tm_year += offset;
	}

	if ( relation & IDMEF_VALUE_RELATION_LESSER )
		tm.tm_sec -= 1;

	t.sec = mktime(&tm);
	t.usec = 0;

	if ( prelude_sql_time_to_timestamp(&t, buf, sizeof (buf), NULL, 0, NULL, 0) < 0 )
		return -1;

	if ( prelude_string_sprintf(output, "%s %s %s",
				    field,
				    prelude_sql_idmef_relation_to_string(relation|IDMEF_VALUE_RELATION_EQUAL),
				    buf) < 0 )
		return -1;

	return 0;
}



static int build_criterion_hour(prelude_sql_connection_t *conn,
				prelude_string_t *output,
				const char *field,
				idmef_value_relation_t relation,
				idmef_criterion_value_non_linear_time_t *timeval)
{
	int hour, min, sec;
	unsigned int total_seconds;
	int relation_offset;
	uint32_t gmt_offset;
	char interval[128];

	hour = idmef_criterion_value_non_linear_time_get_hour(timeval);
	min = idmef_criterion_value_non_linear_time_get_min(timeval);
	sec = idmef_criterion_value_non_linear_time_get_sec(timeval);

	if ( relation & IDMEF_VALUE_RELATION_EQUAL )
		relation_offset = 0;
	else
		relation_offset = 1;

	total_seconds = hour * 3600;
	if ( min != -1 ) {
		total_seconds += min * 60;
		if ( sec != -1 ) {
			total_seconds += sec;
		} else {
			total_seconds += relation_offset * 60;
		}
	} else {
		total_seconds += relation_offset * 3600;
	}

	if ( prelude_get_gmt_offset(&gmt_offset) < 0 )
		return -1;

	if ( build_time_interval(conn, dbconstraint_hour, gmt_offset / 3600, interval, sizeof (interval)) < 0 )
		return -1;

	return prelude_string_sprintf(output,
				      "EXTRACT(HOUR FROM %s + %s) * 3600 + "
				      "EXTRACT(MINUTE FROM %s + %s) * 60 + "
				      "EXTRACT(SECOND FROM %s + %s) %s %d",
				      field, interval,
				      field, interval,
				      field, interval,
				      prelude_sql_idmef_relation_to_string(relation | IDMEF_VALUE_RELATION_EQUAL),
				      total_seconds);
}



static int build_criterion_non_linear_time_value_relation_lesser_or_greater(prelude_sql_connection_t *conn,
									    prelude_string_t *output,
									    const char *field,
									    idmef_value_relation_t relation,
									    idmef_criterion_value_non_linear_time_t *time)
{
	int tmp;

	if ( idmef_criterion_value_non_linear_time_get_year(time) != -1 )
		return build_criterion_timestamp(conn, output, field, relation, time);

	if ( idmef_criterion_value_non_linear_time_get_hour(time) != -1 )
		return build_criterion_hour(conn, output, field, relation, time);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_yday(time)) != -1 )
		return build_time_constraint(conn, output, field, dbconstraint_yday, relation, tmp);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_wday(time)) != -1 )
		return build_time_constraint(conn, output, field, dbconstraint_wday, relation, tmp);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_month(time)) != -1 )
		return build_time_constraint(conn, output, field, dbconstraint_month, relation, tmp);

	if ( (tmp = idmef_criterion_value_non_linear_time_get_mday(time)) != -1 )
		return build_time_constraint(conn, output, field, dbconstraint_mday, relation, tmp);

	return -1;
}


static int build_criterion_non_linear_time_value(prelude_sql_connection_t *conn,
						 prelude_string_t *output,
						 const char *field,
						 idmef_value_relation_t relation,
						 idmef_criterion_value_non_linear_time_t *time)
{
	if ( relation == IDMEF_VALUE_RELATION_EQUAL )
		return build_criterion_non_linear_time_value_relation_equal
			(conn, output, field, relation, time);

	if ( relation == IDMEF_VALUE_RELATION_NOT_EQUAL )
		return build_criterion_non_linear_time_value_relation_not_equal
			(conn, output, field, time);

	if ( relation & (IDMEF_VALUE_RELATION_LESSER | IDMEF_VALUE_RELATION_GREATER) )
		return build_criterion_non_linear_time_value_relation_lesser_or_greater
			(conn, output, field, relation, time);

	return -1;
}



static int build_criterion_fixed_sql_time_value(idmef_value_t *value, char *buf, size_t size)
{
	idmef_time_t *time;

	time = idmef_value_get_time(value);
	if ( ! time )
		return -1;

	return prelude_sql_time_to_timestamp(time, buf, IDMEF_TIME_MAX_STRING_SIZE, NULL, 0, NULL, 0);
}



static int build_criterion_fixed_sql_like_value(idmef_value_t *value, char *buf, size_t size)
{
        size_t i = 0;
        const char *input;
        prelude_string_t *string;

	string = idmef_value_get_string(value);
	if ( ! string )
		return -1;

        input = prelude_string_get_string(string);
        if ( ! input )
                return -1;
        
        while ( *input ) {

                if ( i == (size - 1) )
                        return -1;
                
                if ( *input == '*' )
                        buf[i++] = '%';
                
                else if ( *input == '\\' && *(input + 1) == '*' ) {
                        input++;
                        buf[i++] = '*';
                }
                
                else buf[i++] = *input;

                input++;
        }

        buf[i] = 0;
        
	return 0;
}



static int build_criterion_fixed_sql_value(prelude_sql_connection_t *conn,
					   prelude_string_t *output,
					   idmef_value_t *value,
					   idmef_value_relation_t relation)
{
	int ret;
	char buf[SQL_MAX_VALUE_LEN];
	prelude_string_t *string;
	char *tmp;

	if ( idmef_value_get_type(value) == IDMEF_VALUE_TYPE_TIME ) {
		ret = build_criterion_fixed_sql_time_value(value, buf, sizeof (buf));
		if ( ret < 0 )
			return ret;

		return prelude_string_cat(output, buf);
	}

	if ( relation == IDMEF_VALUE_RELATION_SUBSTR ) {
		ret = build_criterion_fixed_sql_like_value(value, buf, sizeof (buf));
		if ( ret < 0 )
			return ret;

		tmp = prelude_sql_escape(conn, buf);
		if ( ! tmp )
			return -1;

		ret = prelude_string_cat(output, tmp);

		free(tmp);

		return ret;		
	}

	string = prelude_string_new();
	if ( ! string )
		return -1;

	ret = idmef_value_to_string(value, string);
	if ( ret < 0 ) {
		prelude_string_destroy(string);
		return ret;
	}

	tmp = prelude_sql_escape(conn, prelude_string_get_string(string));

	prelude_string_destroy(string);

	if ( ! tmp )
		return -1;

	ret = prelude_string_cat(output, tmp);

	free(tmp);

	return 0;
}



static int build_criterion_relation(prelude_string_t *output, idmef_value_relation_t relation)
{
	const char *tmp;

	tmp = prelude_sql_idmef_relation_to_string(relation);
	if ( ! tmp )
		return -1;

	return prelude_string_sprintf(output, "%s ", tmp);
}


static int build_criterion_fixed_value(prelude_sql_connection_t *conn,
				       prelude_string_t *output,
				       const char *field,
				       idmef_value_relation_t relation,
				       idmef_value_t *value)
{
	if ( prelude_string_sprintf(output, "%s ", field) < 0 )
		return -1;

	if ( build_criterion_relation(output, relation) < 0 )
		return -1;

	if ( build_criterion_fixed_sql_value(conn, output, value, relation) < 0 )
		return -1;

	return 0;
}


int prelude_sql_build_criterion(prelude_sql_connection_t *conn,
				prelude_string_t *output,
				const char *field,
				idmef_value_relation_t relation, idmef_criterion_value_t *value)
{
	if ( relation == IDMEF_VALUE_RELATION_IS_NULL )
		return prelude_string_sprintf(output, "%s = NULL", field);

	if ( relation == IDMEF_VALUE_RELATION_IS_NOT_NULL )
		return prelude_string_sprintf(output, "%s != NULL", field);

	switch ( idmef_criterion_value_get_type(value) ) {
	case idmef_criterion_value_type_fixed:
		return build_criterion_fixed_value(conn, output, field, relation,
						   idmef_criterion_value_get_fixed(value));

	case idmef_criterion_value_type_non_linear_time:
		return build_criterion_non_linear_time_value(conn, output, field, relation,
							     idmef_criterion_value_get_non_linear_time(value));
	}

	return -1;
}



int prelude_sql_time_from_timestamp(idmef_time_t *time, const char *time_buf, uint32_t gmtoff, uint32_t usec)
{
	int ret;
        struct tm tm;
        
        memset(&tm, 0, sizeof (tm));
        
        ret = sscanf(time_buf, "%d-%d-%d %d:%d:%d",
                     &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                     &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

        if ( ret < 6 )
                return -1;

        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
	tm.tm_isdst = -1;

        idmef_time_set_sec(time, prelude_timegm(&tm));
        idmef_time_set_usec(time, usec);
	idmef_time_set_gmt_offset(time, gmtoff);
        
        return 0;
}


int prelude_sql_time_to_timestamp(const idmef_time_t *time,
				  char *time_buf, size_t time_buf_size,
				  char *gmtoff_buf, size_t gmtoff_buf_size,
				  char *usec_buf, size_t usec_buf_size)
{
        struct tm utc;
	time_t t;
        
        if ( ! time ) {
                snprintf(time_buf, time_buf_size,  "NULL");
		if ( gmtoff_buf )
			snprintf(gmtoff_buf, gmtoff_buf_size, "NULL");
		if ( usec_buf )
			snprintf(usec_buf, usec_buf_size, "NULL");
		return 0;
        }

	t = idmef_time_get_sec(time);
        
        if ( ! gmtime_r(&t, &utc) ) {
                log(LOG_ERR, "error converting timestamp to gmt time.\n");
                return -1;
        }
        
        snprintf(time_buf, time_buf_size, "'%d-%.2d-%.2d %.2d:%.2d:%.2d'",
		 utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
		 utc.tm_hour, utc.tm_min, utc.tm_sec);

	if ( gmtoff_buf )
		snprintf(gmtoff_buf, gmtoff_buf_size, "%d", idmef_time_get_gmt_offset(time));

	if ( usec_buf )
		snprintf(usec_buf, usec_buf_size, "%d", idmef_time_get_usec(time));
        
        return 0;
}
