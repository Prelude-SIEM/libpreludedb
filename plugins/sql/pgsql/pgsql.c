/*****
*
* Copyright (C) 2001-2004 Vandoorselaere Yoann <yoann@prelude-ids.org>
* Copyright (C) 2002-2005 Nicolas Delon <nicolas@prelude-ids.org>
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

#include "config.h"

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"
#include "preludedb-plugin-sql.h"

prelude_plugin_generic_t *pgsql_LTX_prelude_plugin_init(void);

struct pg_session {
	PGconn *pgsql;

	/* query dependant variable */
	int row;
};



static int sql_open(preludedb_sql_settings_t *settings, void **session)
{
	struct pg_session *s;

	s = calloc(1, sizeof (struct pg_session));
	if ( ! s )
		return preludedb_error_from_errno(errno);

        s->pgsql = PQsetdbLogin(preludedb_sql_settings_get_host(settings),
				preludedb_sql_settings_get_port(settings),
				NULL,
				NULL,
				preludedb_sql_settings_get_name(settings),
				preludedb_sql_settings_get_user(settings),
				preludedb_sql_settings_get_pass(settings));

        if ( PQstatus(s->pgsql) == CONNECTION_BAD ) {
                PQfinish(s->pgsql);
		free(s);
		return preludedb_error(PRELUDEDB_ERROR_CONNECTION);
        }

	*session = s;

        return 0;
}



static void sql_close(void *session)
{
        PQfinish(((struct pg_session *) session)->pgsql);
	free(session);
}



static const char *sql_get_error(void *session)
{
	PQerrorMessage(((struct pg_session *) session)->pgsql);
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

        ptr = PQescapeBytea((unsigned char *) input, input_size, &dummy);

        string = prelude_string_new();
	if ( ! string )
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        ret = prelude_string_sprintf(string, "'%s'", ptr);
        free(ptr);
        if ( ret < 0 ) {
                prelude_string_destroy(string);
                return ret;
        }

        *output = prelude_string_get_string_released(string);

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
			return prelude_string_sprintf(output, "LIMIT %d OFFSET %d", limit, offset);
		
		return prelude_string_sprintf(output, "LIMIT %d", limit);
	}

	return 0;
}



static int sql_query(void *session, const char *query, void **resource)
{
        int ret;
        PGresult *res;
	PGconn *pgsql = ((struct pg_session *) session)->pgsql;

	res = PQexec(pgsql, query);
	if ( ! res )
		return preludedb_error(PRELUDEDB_ERROR_QUERY);

	ret = PQresultStatus(res);
	switch ( ret ) {
		
	case PGRES_COMMAND_OK:
		PQclear(res);
		return 0;

	case PGRES_TUPLES_OK:
		if ( PQntuples(res) == 0 ) {
			PQclear(res);
			return 0;
		}

		((struct pg_session *)session)->row = 0;
		*resource = res;
		return 1;

	default:
		PQclear(res);
	}

	return preludedb_error(PRELUDEDB_ERROR_QUERY);
}



static void sql_resource_destroy(void *session, void *resource)
{
	if ( resource )
		PQclear(resource);
}



static const char *sql_get_column_name(void *session, void *resource, unsigned int column_num)
{
	return PQfname(resource, column_num);
}



static int sql_get_column_num(void *session, void *resource, const char *column_name)
{
	return PQfnumber(resource, column_name);
}



static unsigned int sql_get_column_count(void *session, void *resource)
{
	return PQnfields(resource);
}



static unsigned int sql_get_row_count(void *session, void *resource)
{
	return PQntuples(resource);
}



static int sql_fetch_row(void *s, void *resource, void **row)
{
	struct pg_session *session = s;

	if ( session->row < PQntuples(((PGresult *) resource)) ) {
		*row = (void *) (session->row++ + 1);
		return 1;
	}

	return 0;
}



static int sql_fetch_field(void *session, void *resource, void *r,
			   unsigned int column_num, const char **value, size_t *len)
{
	unsigned int row = (unsigned int) r - 1;
	
	if ( column_num >= PQnfields(resource) )
		return preludedb_error(PRELUDEDB_ERROR_INVALID_COLUMN_NUM);

	if ( PQgetisnull(resource, row, column_num) )
		return 0;

	*value = PQgetvalue(resource, row, column_num);
	*len = PQgetlength(resource, row, column_num);

	return 1;
}



static int sql_build_time_constraint_string(prelude_string_t *output, const char *field,
					    preludedb_sql_time_constraint_type_t type,
					    idmef_value_relation_t relation, int value, int gmt_offset)
{
	char buf[128];
	const char *sql_relation;
	int ret;

	ret = snprintf(buf, sizeof (buf), "%s + INTERVAL '%d HOUR'", field, gmt_offset / 3600);
	if ( ret < 0 || ret >= sizeof (buf) )
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);

	sql_relation = preludedb_sql_get_relation_string(relation);
	if ( ! sql_relation )
		return preludedb_error(PRELUDEDB_ERROR_GENERIC);

	switch ( type ) {
	case PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR:
		return prelude_string_sprintf(output, "EXTRACT(YEAR FROM %s) %s %d",
					      buf, sql_relation, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH:
		return  prelude_string_sprintf(output, "EXTRACT(MONTH FROM %s) %s %d",
					       buf, sql_relation, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY:
		return prelude_string_sprintf(output, "EXTRACT(DOY FROM %s) %s %d",
					      buf, sql_relation, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY:
		return prelude_string_sprintf(output, "EXTRACT(DAY FROM %s) %s %d",
					      buf, sql_relation, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY:
		return prelude_string_sprintf(output, "EXTRACT(DOW FROM %s) %s %d",
					      buf, sql_relation, value % 7 + 1);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR:
		return prelude_string_sprintf(output, "EXTRACT(HOUR FROM %s) %s %d",
					      buf, sql_relation, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_MIN:
		return prelude_string_sprintf(output, "EXTRACT(MINUTE FROM %s) %s %d",
					      buf, sql_relation, value);

	case PRELUDEDB_SQL_TIME_CONSTRAINT_SEC:
		return prelude_string_sprintf(output, "EXTRACT(SECOND FROM %s) %s %d",
					      buf, sql_relation, value);
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



prelude_plugin_generic_t *pgsql_LTX_prelude_plugin_init(void)
{
	static preludedb_plugin_sql_t plugin;

	memset(&plugin, 0, sizeof (plugin));

        prelude_plugin_set_name(&plugin, "PgSQL");
        prelude_plugin_set_desc(&plugin, "SQL plugin for PostgreSQL database.");
	prelude_plugin_set_author(&plugin, "Nicolas Delon");
        prelude_plugin_set_contact(&plugin, "nicolas@prelude-ids.org");

        preludedb_plugin_sql_set_open_func(&plugin, sql_open);
        preludedb_plugin_sql_set_close_func(&plugin, sql_close);
        preludedb_plugin_sql_set_get_error_func(&plugin, sql_get_error);
        preludedb_plugin_sql_set_escape_func(&plugin, sql_escape);
        preludedb_plugin_sql_set_escape_binary_func(&plugin, sql_escape_binary);
        preludedb_plugin_sql_set_unescape_binary_func(&plugin, sql_unescape_binary);
        preludedb_plugin_sql_set_query_func(&plugin, sql_query);
	preludedb_plugin_sql_set_resource_destroy_func(&plugin, sql_resource_destroy);
	preludedb_plugin_sql_set_get_column_count_func(&plugin, sql_get_column_count);
	preludedb_plugin_sql_set_get_row_count_func(&plugin, sql_get_row_count);
	preludedb_plugin_sql_set_get_column_name_func(&plugin, sql_get_column_name);
	preludedb_plugin_sql_set_get_column_num_func(&plugin, sql_get_column_num);
	preludedb_plugin_sql_set_fetch_row_func(&plugin, sql_fetch_row);
	preludedb_plugin_sql_set_fetch_field_func(&plugin, sql_fetch_field);
	preludedb_plugin_sql_set_build_time_constraint_string_func(&plugin, sql_build_time_constraint_string);
	preludedb_plugin_sql_set_build_time_interval_string_func(&plugin, sql_build_time_interval_string);
        preludedb_plugin_sql_set_build_limit_offset_string_func(&plugin, sql_build_limit_offset_string);

	return (void *) &plugin;
}
