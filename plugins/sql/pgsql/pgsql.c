/*****
*
* Copyright (C) 2003-2005 PreludeIDS Technologies. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/prelude-error.h>
#include <libprelude/idmef.h>
#include <libpq-fe.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"
#include "preludedb-plugin-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"


int pgsql_LTX_prelude_plugin_version(void);
int pgsql_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data);


struct pg_result {
	PGresult *result;
	int row;
};


static int sql_open(preludedb_sql_settings_t *settings, void **session, char *errbuf, size_t size)
{
	PGconn *conn;

        conn = PQsetdbLogin(preludedb_sql_settings_get_host(settings),
			    preludedb_sql_settings_get_port(settings),
			    NULL,
			    NULL,
			    preludedb_sql_settings_get_name(settings),
			    preludedb_sql_settings_get_user(settings),
			    preludedb_sql_settings_get_pass(settings));

        if ( PQstatus(conn) == CONNECTION_BAD ) {
		if ( PQerrorMessage(conn) )
			snprintf(errbuf, size, "%s", PQerrorMessage(conn));
                PQfinish(conn);

		return preludedb_error(PRELUDEDB_ERROR_CONNECTION);
        }

	*session = conn;

        return 0;
}



static void sql_close(void *session)
{
        PQfinish(session);
}



static const char *sql_get_error(void *session)
{
	return PQerrorMessage(session);
}



static int sql_escape(void *session, const char *input, size_t input_size, char **output)
{
	size_t rsize;

        rsize = input_size * 2 + 3;
        if ( rsize <= input_size )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);
        
        *output = malloc(rsize);
        if ( ! *output )
		return preludedb_error_from_errno(errno);

        (*output)[0] = '\'';

	rsize = PQescapeString((*output) + 1, input, input_size);

        (*output)[rsize + 1] = '\'';
        (*output)[rsize + 2] = '\0';

        return 0;
}



static int sql_escape_binary(void *session, const unsigned char *input, size_t input_size, char **output)
{
        prelude_string_t *string;
        size_t rsize, dummy;
        char *ptr;
        int ret;

        rsize = input_size * 2 + 3;
        if ( rsize <= input_size )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        ptr = PQescapeBytea(input, input_size, &dummy);

        ret = prelude_string_new(&string);
	if ( ret < 0 )
		return ret;

        ret = prelude_string_sprintf(string, "'%s'", ptr);
        free(ptr);
        if ( ret < 0 ) {
                prelude_string_destroy(string);
                return ret;
        }

        ret = prelude_string_get_string_released(string, output);

        prelude_string_destroy(string);

        return 0;
}



static int sql_unescape_binary(void *session, const char *input, unsigned char **output, size_t *output_size)
{
	*output = PQunescapeBytea((const unsigned char *) input, output_size);
	if ( ! *output )
		return preludedb_error_from_errno(errno);

	return 0;
}



static int sql_build_limit_offset_string(void *session, int limit, int offset, prelude_string_t *output)
{
	if ( limit >= 0 ) {
		if ( offset >= 0 )
			return prelude_string_sprintf(output, " LIMIT %d OFFSET %d", limit, offset);
		
		return prelude_string_sprintf(output, " LIMIT %d", limit);
	}

	return 0;
}



static int sql_query(void *session, const char *query, void **resource)
{
        int ret;
        struct pg_result *res;

	res = calloc(1, sizeof (*res));
	if ( ! res )
		return prelude_error_from_errno(errno);

	res->row = -1;

	res->result = PQexec(session, query);
	if ( ! res->result ) {
		free(res);
		return preludedb_error(PRELUDEDB_ERROR_QUERY);
	}

	ret = PQresultStatus(res->result);

        if ( ret == PGRES_TUPLES_OK && PQntuples(res->result) != 0 ) {
                *resource = res;
                return 1;
        }
        
        PQclear(res->result);
	free(res);
        if ( ret == PGRES_TUPLES_OK || ret == PGRES_COMMAND_OK )
                return 0;
        
	return preludedb_error(PRELUDEDB_ERROR_QUERY);
}



static void sql_resource_destroy(void *session, void *resource)
{
	if ( resource ) {
		PQclear(((struct pg_result *) resource)->result);
		free(resource);
	}
}



static const char *sql_get_column_name(void *session, void *resource, unsigned int column_num)
{
	return PQfname(((struct pg_result *) resource)->result, column_num);
}



static int sql_get_column_num(void *session, void *resource, const char *column_name)
{
	return PQfnumber(((struct pg_result *) resource)->result, column_name);
}



static unsigned int sql_get_column_count(void *session, void *resource)
{
	return PQnfields(((struct pg_result *) resource)->result);
}



static unsigned int sql_get_row_count(void *session, void *resource)
{
	return PQntuples(((struct pg_result *) resource)->result);
}



static int sql_fetch_row(void *s, void *resource, void **row)
{
	struct pg_result *res = resource;

	/* 
	 * initialize *row, but we won't use it since we access row's fields directly
	 * through the PGresult structure
	 */
	*row = NULL;

	if ( res->row + 1 < PQntuples(res->result) ) {
		res->row++;
		return 1;
	}

	return 0;
}



static int sql_fetch_field(void *session, void *resource, void *r,
			   unsigned int column_num, const char **value, size_t *len)
{
	struct pg_result *res = resource;
	
	if ( column_num >= PQnfields(res->result) )
		return preludedb_error(PRELUDEDB_ERROR_INVALID_COLUMN_NUM);

	if ( PQgetisnull(res->result, res->row, column_num) )
		return 0;

	*value = PQgetvalue(res->result, res->row, column_num);
	*len = PQgetlength(res->result, res->row, column_num);

	return 1;
}



static int sql_build_time_constraint_string(prelude_string_t *output, const char *field,
					    preludedb_sql_time_constraint_type_t type,
					    idmef_criterion_operator_t operator, int value, int gmt_offset)
{
	char buf[128];
	const char *sql_operator;
	int ret;

	ret = snprintf(buf, sizeof (buf), "%s + INTERVAL '%d HOUR'", field, gmt_offset / 3600);
	if ( ret < 0 || ret >= sizeof (buf) )
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);

	sql_operator = preludedb_sql_get_operator_string(operator);
	if ( ! sql_operator )
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);

	switch ( type ) {
	case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
		return prelude_string_sprintf(output, "EXTRACT(YEAR FROM %s) %s %d",
					      buf, sql_operator, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
		return  prelude_string_sprintf(output, "EXTRACT(MONTH FROM %s) %s %d",
					       buf, sql_operator, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY:
		return prelude_string_sprintf(output, "EXTRACT(DOY FROM %s) %s %d",
					      buf, sql_operator, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
		return prelude_string_sprintf(output, "EXTRACT(DAY FROM %s) %s %d",
					      buf, sql_operator, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY:
		return prelude_string_sprintf(output, "EXTRACT(DOW FROM %s) %s %d",
					      buf, sql_operator, value % 7 + 1);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
		return prelude_string_sprintf(output, "EXTRACT(HOUR FROM %s) %s %d",
					      buf, sql_operator, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
		return prelude_string_sprintf(output, "EXTRACT(MINUTE FROM %s) %s %d",
					      buf, sql_operator, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
		return prelude_string_sprintf(output, "EXTRACT(SECOND FROM %s) %s %d",
					      buf, sql_operator, value);
	}

	/* not reached */

	return preludedb_error(PRELUDEDB_ERROR_GENERIC);
}



static int sql_build_time_interval_string(preludedb_sql_time_constraint_type_t type, int value,
					  char *buf, size_t size)
{
	char *type_str;
	int ret;

	switch ( type ) {
	case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
		type_str = "YEAR";
		break;

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
		type_str = "MONTH";
		break;

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
		type_str = "DAY";
		break;

	case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
		type_str = "HOUR";
		break;

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
		type_str = "MINUTE";
		break;

	case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
		type_str = "SECOND";
		break;

	default:
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);
	}

	ret = snprintf(buf, size, "INTERVAL '%d %s'", value, type_str);

	return (ret < 0 || ret >= size) ? preludedb_error(PRELUDEDB_ERROR_GENERIC) : 0;
}



int pgsql_LTX_preludedb_plugin_init(prelude_plugin_entry_t *pe, void *data)
{
	static preludedb_plugin_sql_t sql_plugin;
        
	memset(&sql_plugin, 0, sizeof(sql_plugin));
        prelude_plugin_set_name(&sql_plugin, "PgSQL");
        prelude_plugin_entry_set_plugin(pe, (void *) &sql_plugin);
        
        preludedb_plugin_sql_set_open_func(&sql_plugin, sql_open);
        preludedb_plugin_sql_set_close_func(&sql_plugin, sql_close);
        preludedb_plugin_sql_set_get_error_func(&sql_plugin, sql_get_error);
        preludedb_plugin_sql_set_escape_func(&sql_plugin, sql_escape);
        preludedb_plugin_sql_set_escape_binary_func(&sql_plugin, sql_escape_binary);
        preludedb_plugin_sql_set_unescape_binary_func(&sql_plugin, sql_unescape_binary);
        preludedb_plugin_sql_set_query_func(&sql_plugin, sql_query);
	preludedb_plugin_sql_set_resource_destroy_func(&sql_plugin, sql_resource_destroy);
	preludedb_plugin_sql_set_get_column_count_func(&sql_plugin, sql_get_column_count);
	preludedb_plugin_sql_set_get_row_count_func(&sql_plugin, sql_get_row_count);
	preludedb_plugin_sql_set_get_column_name_func(&sql_plugin, sql_get_column_name);
	preludedb_plugin_sql_set_get_column_num_func(&sql_plugin, sql_get_column_num);
	preludedb_plugin_sql_set_fetch_row_func(&sql_plugin, sql_fetch_row);
	preludedb_plugin_sql_set_fetch_field_func(&sql_plugin, sql_fetch_field);
	preludedb_plugin_sql_set_build_time_constraint_string_func(&sql_plugin, sql_build_time_constraint_string);
	preludedb_plugin_sql_set_build_time_interval_string_func(&sql_plugin, sql_build_time_interval_string);
        preludedb_plugin_sql_set_build_limit_offset_string_func(&sql_plugin, sql_build_limit_offset_string);

	return 0;
}



int pgsql_LTX_prelude_plugin_version(void)
{
        return PRELUDE_PLUGIN_API_VERSION;
}
