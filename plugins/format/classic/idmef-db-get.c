/*****
*
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <time.h>

#include <libprelude/prelude-error.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"

#include "db-path.h"

#include "idmef-db-get.h"

#define db_log(sql) prelude_log(PRELUDE_LOG_ERR, "%s\n", prelude_sql_error(sql))
#define log_memory_exhausted() prelude_log(PRELUDE_LOG_ERR, "memory exhausted !\n")

#define get_(type, name)									\
static int _get_ ## name (preludedb_sql_t *sql, preludedb_sql_row_t *row,			\
			 int index,								\
			 void *parent, int (*parent_new_child)(void *parent, type **child))	\
{												\
	preludedb_sql_field_t *field;								\
	type *value;										\
	int ret;										\
												\
	ret = preludedb_sql_row_fetch_field(row, index, &field);				\
	if ( ret <= 0 )										\
		return ret;									\
												\
	ret = parent_new_child(parent, &value);							\
	if ( ret < 0 )										\
		return ret;									\
												\
	return preludedb_sql_field_to_ ## name (field, value);					\
}

get_(uint8_t, uint8)
get_(uint16_t, uint16)
get_(uint32_t, uint32)
get_(uint64_t, uint64)
get_(float, float)

#define get_uint8(sql, row, index, parent, parent_new_child) \
	_get_uint8(sql, row, index, parent, (int (*)(void *, uint8_t **)) parent_new_child)

#define get_uint16(sql, row, index, parent, parent_new_child) \
	_get_uint16(sql, row, index, parent, (int (*)(void *, uint16_t **)) parent_new_child)

#define get_uint32(sql, row, index, parent, parent_new_child) \
	_get_uint32(sql, row, index, parent, (int (*)(void *, uint32_t **)) parent_new_child)

#define get_uint64(sql, row, index, parent, parent_new_child) \
	_get_uint64(sql, row, index, parent, (int (*)(void *, uint64_t **)) parent_new_child)

#define get_float(sql, row, index, parent, parent_new_child) \
	_get_float(sql, row, index, parent, (int (*)(void *, float **)) parent_new_child)



static int _get_string(preludedb_sql_t *sql, preludedb_sql_row_t *row,
		       int index,
		       void *parent, int (*parent_new_child)(void *parent, prelude_string_t **child))
{
	preludedb_sql_field_t *field;
	prelude_string_t *string;
	int ret;

	ret = preludedb_sql_row_fetch_field(row, index, &field);
	if ( ret <= 0 )
		return ret;

	ret = parent_new_child(parent, &string);
	if ( ret < 0 )
		return ret;

	ret = prelude_string_set_dup_fast(string,
					  preludedb_sql_field_get_value(field),
					  preludedb_sql_field_get_len(field));
	
	return (ret < 0) ? ret : 1;
}



static int _get_enum(preludedb_sql_t *sql, preludedb_sql_row_t *row, 
		     int index,
		     void *parent, int (*parent_new_child)(void *parent, int **child), int (*convert_enum)(const char *))
{
	preludedb_sql_field_t *field;
	int *enum_val;
	int ret;

	ret = preludedb_sql_row_fetch_field(row, index, &field);
	if ( ret <= 0 )
		return ret;

	ret = parent_new_child(parent, &enum_val);
	if ( ret < 0 )
		return ret;

	*enum_val = convert_enum(preludedb_sql_field_get_value(field));

	return 1;
}



static int _get_timestamp(preludedb_sql_t *sql, preludedb_sql_row_t *row,
			  int time_index, int gmtoff_index, int usec_index,
			  void *parent, int (*parent_new_child)(void *parent, idmef_time_t **child))
{
	preludedb_sql_field_t *time_field, *gmtoff_field, *usec_field = NULL;
	const char *tmp;
	int32_t gmtoff;
	uint32_t usec = 0;
	idmef_time_t *time;
	int ret;

	ret = preludedb_sql_row_fetch_field(row, time_index, &time_field);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_row_fetch_field(row, gmtoff_index, &gmtoff_field);
	if ( ret <= 0 )
		return (ret < 0) ? ret : -1;

	if ( usec_index != -1 ) {
		ret = preludedb_sql_row_fetch_field(row, usec_index, &usec_field);
		if ( ret <= 0 )
			return (ret < 0) ? ret : -1;

		ret = preludedb_sql_field_to_uint32(usec_field, &usec);
		if ( ret < 0 )
			return ret;
	}

	tmp = preludedb_sql_field_get_value(time_field);

	ret = preludedb_sql_field_to_int32(gmtoff_field, &gmtoff);
	if ( ret < 0 )
		return ret;
	
	ret = parent_new_child(parent, &time);
	if ( ret < 0 )
		return ret;

	return preludedb_sql_time_from_timestamp(time, tmp, gmtoff, usec);
}

#define get_string(sql, row, index, parent, parent_new_child) \
	_get_string(sql, row, index, parent, (int (*)(void *, prelude_string_t **)) parent_new_child)

#define get_enum(sql, row, index, parent, parent_new_child, convert_enum) \
	_get_enum(sql, row, index, parent, (int (*)(void *, int **)) parent_new_child, convert_enum)

#define get_timestamp(sql, row, time_index, gmtoff_index, usec_index, parent, parent_new_child) \
	_get_timestamp(sql, row, time_index, gmtoff_index, usec_index, parent, (int (*)(void *, idmef_time_t **)) parent_new_child)



static int get_analyzer_time(preludedb_sql_t *sql,
			     uint64_t message_ident,
			     char parent_type,
			     void *parent,
			     int (*parent_new_child)(void *parent, idmef_time_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT time, gmtoff, usec "
					  "FROM Prelude_AnalyzerTime "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 "",
					  parent_type, message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		goto error;

	ret = get_timestamp(sql, row, 0, 1, 2, parent, parent_new_child);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_detect_time(preludedb_sql_t *sql,
			   uint64_t message_ident,
			   idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT time, gmtoff, usec "
					  "FROM Prelude_DetectTime "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);

	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		goto error;

	ret = get_timestamp(sql, row, 0, 1, 2, alert, idmef_alert_new_detect_time);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_create_time(preludedb_sql_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   void *parent,
			   int (*parent_new_child)(void *parent, idmef_time_t **time))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					 "SELECT time, gmtoff, usec "
					 "FROM Prelude_CreateTime "
					 "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 "",
					 parent_type, message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		goto error;

	ret = get_timestamp(sql, row, 0, 1, 2, parent, parent_new_child);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_user_id(preludedb_sql_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       int file_index,
		       int file_access_index,
		       void *parent,
		       int (*parent_new_child)(void *, idmef_user_id_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_user_id_t *user_id;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, type, name, number "
					  "FROM Prelude_UserId "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND "
					  "_parent_index = %d AND _file_index = %d AND _file_access_index = %d",
					  parent_type, message_ident, parent_index, file_index, file_access_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = parent_new_child(parent, &user_id);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 0, user_id, idmef_user_id_new_ident);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 1, user_id, idmef_user_id_new_type, idmef_user_id_type_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, user_id, idmef_user_id_new_name);
		if ( ret < 0 )
			goto error;

		ret = get_uint32(sql, row, 3, user_id, idmef_user_id_new_number);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_user(preludedb_sql_t *sql,
		    uint64_t message_ident,
		    char parent_type,
		    int parent_index,
		    void *parent,
		    int (*parent_new_child)(void *parent, idmef_user_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_user_t *user;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, category "
					  "FROM Prelude_User "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		goto error;

	ret = parent_new_child(parent, &user);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, user, idmef_user_new_ident);
	if ( ret < 0 )
		goto error;

	ret = get_enum(sql, row, 1, user, idmef_user_new_category, idmef_user_category_to_numeric);
	if ( ret < 0 )
		goto error;

	ret = get_user_id(sql, message_ident, parent_type, parent_index, 0, 0, user,
			  (int (*)(void *, idmef_user_id_t **)) idmef_user_new_user_id);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_process_arg(preludedb_sql_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   char parent_index,
			   void *parent,
			   int (*parent_new_child)(void *parent, prelude_string_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT arg "
					  "FROM Prelude_ProcessArg "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = get_string(sql, row, 0, parent, parent_new_child);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_process_env(preludedb_sql_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   int parent_index,
			   void *parent,
			   int (*parent_new_child)(void *parent, prelude_string_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT env "
					  "FROM Prelude_ProcessEnv "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = get_string(sql, row, 0, parent, parent_new_child);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_process(preludedb_sql_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       void *parent,
		       int (*parent_new_child)(void *parent, idmef_process_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_process_t *process;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, name, pid, path "
					  "FROM Prelude_Process "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = parent_new_child(parent, &process);
	if ( ret < 0 )
		return ret;

	ret = get_string(sql, row, 0, process, idmef_process_new_ident);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, process, idmef_process_new_name);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 2, process, idmef_process_new_pid);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 3, process, idmef_process_new_path);
	if ( ret < 0 )
		goto error;

	ret = get_process_arg(sql, message_ident, parent_type, parent_index, process,
			      (int (*)(void *, prelude_string_t **)) idmef_process_new_arg);
	if ( ret < 0 )
		goto error;
	
	ret = get_process_env(sql, message_ident, parent_type, parent_index, process,
			      (int (*)(void *, prelude_string_t **)) idmef_process_new_env);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_web_service_arg(preludedb_sql_t *sql,
			       uint64_t message_ident,
			       char parent_type,
			       int parent_index,
			       idmef_web_service_t *web_service)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT arg "
					  "FROM Prelude_WebServiceArg "
					  "WHERE _parent_type = '%c' and _message_ident = %" PRIu64 " and _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = get_string(sql, row, 0, web_service, idmef_web_service_new_arg);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_web_service(preludedb_sql_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   int parent_index,
			   idmef_service_t *service)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_web_service_t *web_service;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT url, cgi, http_method "
					  "FROM Prelude_WebService "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_service_new_web_service(service, &web_service);
	if ( ret < 0 )
		return ret;

	ret = get_string(sql, row, 0, web_service, idmef_web_service_new_url);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, web_service, idmef_web_service_new_cgi);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 2, web_service, idmef_web_service_new_http_method);
	if ( ret < 0 )
		goto error;

	ret = get_web_service_arg(sql, message_ident, parent_type, parent_index, web_service);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_snmp_service(preludedb_sql_t *sql,
			    uint64_t message_ident,
			    char parent_type,
			    int parent_index,
			    idmef_service_t *service)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_snmp_service_t *snmp_service;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT snmp_oid, community, security_name, context_name, context_engine_id, command "
					  "FROM Prelude_SNMPService "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_service_new_snmp_service(service, &snmp_service);
	if ( ret < 0 )
		return ret;

	ret = get_string(sql, row, 0, snmp_service, idmef_snmp_service_new_oid);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, snmp_service, idmef_snmp_service_new_community);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 2, snmp_service, idmef_snmp_service_new_security_name);
	if ( ret < 0 )
		goto error;

	ret =get_string(sql, row, 3, snmp_service, idmef_snmp_service_new_context_name);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 4, snmp_service, idmef_snmp_service_new_context_engine_id);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 5, snmp_service, idmef_snmp_service_new_command);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_service(preludedb_sql_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       void *parent,
		       int (*parent_new_child)(void *parent, idmef_service_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_service_t *service;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, name, port, iana_protocol_number, iana_protocol_name, portlist, protocol "
					  "FROM Prelude_Service "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return 0;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = parent_new_child(parent, &service);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, service, idmef_service_new_ident);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, service, idmef_service_new_name);
	if ( ret < 0 )	     
		goto error;

	ret = get_uint16(sql, row, 2, service, idmef_service_new_port);
	if ( ret < 0 )
		goto error;

	ret = get_uint8(sql, row, 3, service, idmef_service_new_iana_protocol_number);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 4, service, idmef_service_new_iana_protocol_name);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 5, service, idmef_service_new_portlist);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 6, service, idmef_service_new_protocol);
	if ( ret < 0 )
		goto error;

	ret = get_web_service(sql, message_ident, parent_type, parent_index, service);
	if ( ret < 0 )	     
		goto error;

	ret = get_snmp_service(sql, message_ident, parent_type, parent_index, service);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_address(preludedb_sql_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       void *parent,
		       int (*parent_new_child)(void *parent, idmef_address_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_address_t *idmef_address;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, category, vlan_name, vlan_num, address, netmask "
					  "FROM Prelude_Address "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = parent_new_child(parent, &idmef_address);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 0, idmef_address, idmef_address_new_ident);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 1, idmef_address, idmef_address_new_category, idmef_address_category_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, idmef_address, idmef_address_new_vlan_name);
		if ( ret < 0 )
			goto error;

		ret = get_uint32(sql, row, 3, idmef_address, idmef_address_new_vlan_num);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 4, idmef_address, idmef_address_new_address);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 5, idmef_address, idmef_address_new_netmask);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_node(preludedb_sql_t *sql,
		    uint64_t message_ident,
		    char parent_type,
		    int parent_index,
		    void *parent,
		    int (*parent_new_child)(void *parent, idmef_node_t **node))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_node_t *node;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, category, location, name "
					  "FROM Prelude_Node "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
					  parent_type, message_ident, parent_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = parent_new_child(parent, &node);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, node, idmef_node_new_ident);
	if ( ret < 0 )
		goto error;

	ret = get_enum(sql, row, 1, node, idmef_node_new_category, idmef_node_category_to_numeric);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 2, node, idmef_node_new_location);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 3, node, idmef_node_new_name);
	if ( ret < 0 )
		goto error;

	ret = get_address(sql, message_ident, parent_type, parent_index, node,
			  (int (*)(void *, idmef_address_t **)) idmef_node_new_address);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_analyzer(preludedb_sql_t *sql,
			uint64_t message_ident,
			char parent_type,
			int depth,
			void *parent,
			int (*parent_new_child)(void *parent, idmef_analyzer_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_analyzer_t *analyzer;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT analyzerid, name, manufacturer, model, version, class, ostype, osversion "
					  "FROM Prelude_Analyzer "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _depth = %d",
					  parent_type, message_ident, depth);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = parent_new_child(parent, &analyzer);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, analyzer, idmef_analyzer_new_analyzerid);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, analyzer, idmef_analyzer_new_name);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 2, analyzer, idmef_analyzer_new_manufacturer);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 3, analyzer, idmef_analyzer_new_model);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 4, analyzer, idmef_analyzer_new_version);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 5, analyzer, idmef_analyzer_new_class);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 6, analyzer, idmef_analyzer_new_ostype);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 7, analyzer, idmef_analyzer_new_osversion);
	if ( ret < 0 )
		goto error;

	ret = get_node(sql, message_ident, parent_type, depth, analyzer,
		       (int (*)(void *, idmef_node_t **)) idmef_analyzer_new_node);
	if ( ret < 0 )
		goto error;

	ret = get_process(sql, message_ident, parent_type, depth, analyzer,
			  (int (*)(void *, idmef_process_t **)) idmef_analyzer_new_process);
	if ( ret < 0 )
		goto error;

	ret = get_analyzer(sql, message_ident, parent_type, depth + 1, analyzer,
			   (int (*)(void *, idmef_analyzer_t **)) idmef_analyzer_new_analyzer);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_action(preludedb_sql_t *sql,
		      uint64_t message_ident,
		      idmef_assessment_t *assessment)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_action_t *action;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT category, description "
					  "FROM Prelude_Action "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_assessment_new_action(assessment, &action);
		if ( ret < 0 )
			return ret;

		ret = get_enum(sql, row, 0, action, idmef_action_new_category, idmef_action_category_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 1, action, idmef_action_new_description);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;	
}

static int get_confidence(preludedb_sql_t *sql,
			  uint64_t message_ident,
			  idmef_assessment_t *assessment)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_confidence_t *confidence;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT rating, confidence "
					  "FROM Prelude_Confidence "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_assessment_new_confidence(assessment, &confidence);
	if ( ret < 0 )
		goto error;

	ret = get_enum(sql, row, 0, confidence, idmef_confidence_new_rating, idmef_confidence_rating_to_numeric);
	if ( ret < 0 )
		goto error;

	ret = get_float(sql, row, 1, confidence, idmef_confidence_new_confidence);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_impact(preludedb_sql_t *sql, 
		      uint64_t message_ident, 
		      idmef_assessment_t *assessment)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_impact_t *impact;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT severity, completion, type, description "
					  "FROM Prelude_Impact "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_assessment_new_impact(assessment, &impact);
	if ( ret < 0 )
		goto error;

	ret = get_enum(sql, row, 0, impact, idmef_impact_new_severity, idmef_impact_severity_to_numeric);
	if ( ret < 0 )
		goto error;

	ret = get_enum(sql, row, 1, impact, idmef_impact_new_completion, idmef_impact_completion_to_numeric);
	if ( ret < 0 )
		goto error;

	ret = get_enum(sql, row, 2, impact, idmef_impact_new_type, idmef_impact_type_to_numeric);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 3, impact, idmef_impact_new_description);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_assessment(preludedb_sql_t *sql, 
			  uint64_t message_ident,
			  idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	idmef_assessment_t *assessment;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT _message_ident "
					  "FROM Prelude_Assessment "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	preludedb_sql_table_destroy(table);

	ret = idmef_alert_new_assessment(alert, &assessment);
	if ( ret < 0 )
		goto error;

	ret = get_impact(sql, message_ident, assessment);
	if ( ret < 0 )
		goto error;

	ret = get_confidence(sql, message_ident, assessment);
	if ( ret < 0 )
		goto error;

	ret = get_action(sql, message_ident, assessment);
	if ( ret < 0 )
		goto error;

 error:
	return ret;
}

static int get_file_access_permission(preludedb_sql_t *sql,
				      uint64_t message_ident,
				      int target_index,
				      int file_index,
				      int file_access_index,
				      idmef_file_access_t *parent)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT perm "
					  "FROM Prelude_FileAccess_Permission "
					  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND "
					  "_file_index = %d AND _file_access_index = %d",
					  message_ident, target_index, file_index, file_access_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = get_string(sql, row, 0, parent, idmef_file_access_new_permission);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_file_access(preludedb_sql_t *sql,
			   uint64_t message_ident,
			   int target_index,
			   int file_index,
			   idmef_file_t *file)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	preludedb_sql_field_t *field;
	idmef_file_access_t *file_access;
	uint32_t file_access_count;
	int cnt;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT COUNT(*) "
					  "FROM Prelude_FileAccess "
					  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
					  message_ident, target_index, file_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		goto error;

	ret = preludedb_sql_row_fetch_field(row, 0, &field);
	if ( ret <= 0 )
		goto error;

	ret = preludedb_sql_field_to_int32(field, &file_access_count);
	if ( ret < 0 )
		goto error;

	for ( cnt = 0; cnt < file_access_count; cnt++ ) {

		ret = idmef_file_new_file_access(file, &file_access);
		if ( ret < 0 )
			goto error;

		ret = get_user_id(sql, message_ident, 'F', target_index, file_index, cnt,
				  file_access, (int (*)(void *, idmef_user_id_t **)) idmef_file_access_new_user_id);
		if ( ret < 0 )
			goto error;

		ret = get_file_access_permission(sql, message_ident, target_index, file_index, cnt, file_access);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_linkage(preludedb_sql_t *sql,
		       uint64_t message_ident,
		       int target_index,
		       int file_index,
		       idmef_file_t *file)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_linkage_t *linkage;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT category, name, path "
					  "FROM Prelude_Linkage "
					  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
					  message_ident, target_index, file_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_file_new_linkage(file, &linkage);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 0, linkage, idmef_linkage_new_category, idmef_linkage_category_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 1, linkage, idmef_linkage_new_name);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, linkage, idmef_linkage_new_path);
		if ( ret < 0 )
			goto error;
	}

	/* FIXME: file in linkage is not currently supported  */

 error:
	preludedb_sql_table_destroy(table);
	
	return ret;
}

static int get_inode(preludedb_sql_t *sql,
		     uint64_t message_ident,
		     int target_index,
		     int file_index,
		     idmef_file_t *file)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_inode_t *inode;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT change_time, change_time_gmtoff, number, major_device, minor_device, "
					  "c_major_device, c_minor_device "
					  "FROM Prelude_Inode "
					  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
					  message_ident, target_index, file_index);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_file_new_inode(file, &inode);
	if ( ret < 0 )
		goto error;

	ret = get_timestamp(sql, row, 0, 1, -1, inode, idmef_inode_new_change_time);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 2, inode, idmef_inode_new_number);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 3, inode, idmef_inode_new_major_device);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 4, inode, idmef_inode_new_minor_device);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 5, inode, idmef_inode_new_c_major_device);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 6, inode, idmef_inode_new_c_minor_device);
	if ( ret < 0 )
		goto error;

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}


static int get_checksum(preludedb_sql_t *sql,
			uint64_t message_ident,
			int target_index,
			int file_index,
			idmef_file_t *file)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_checksum_t *checksum;
	int ret;
	
	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT value, checksum_key, algorithm "
					  "FROM Prelude_Checksum "
					  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
					  message_ident, target_index, file_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_file_new_checksum(file, &checksum);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 0, checksum, idmef_checksum_new_value);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 1, checksum, idmef_checksum_new_key);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 2, checksum, idmef_checksum_new_algorithm, idmef_checksum_algorithm_to_numeric);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}


static int get_file(preludedb_sql_t *sql,
		    uint64_t message_ident,
		    int target_index,
		    idmef_target_t *target)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_file_t *file = NULL;
	int cnt;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, category, name, path, create_time, create_time_gmtoff, "
					  "modify_time, modify_time_gmtoff, access_time, "
					  "access_time_gmtoff, data_size, disk_size, fstype "
					  "FROM Prelude_File "
					  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d",
					  message_ident, target_index);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_target_new_file(target, &file);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 0, file, idmef_file_new_ident);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 1, file, idmef_file_new_category, idmef_file_category_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, file, idmef_file_new_name);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 3, file, idmef_file_new_path);
		if ( ret < 0 )
			goto error;

		ret = get_timestamp(sql, row, 4, 5, -1, file, idmef_file_new_create_time);
		if ( ret < 0 )
			goto error;

		ret = get_timestamp(sql, row, 6, 7, -1, file, idmef_file_new_modify_time);
		if ( ret < 0 )
			goto error;

		ret = get_timestamp(sql, row, 8, 9, -1, file, idmef_file_new_access_time);
		if ( ret < 0 )
			goto error;

		ret = get_uint32(sql, row, 10, file, idmef_file_new_data_size);
		if ( ret < 0 )
			goto error;

		ret = get_uint32(sql, row, 11, file, idmef_file_new_disk_size);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 12, file, idmef_file_new_fstype, idmef_file_fstype_to_numeric);
		if ( ret < 0 )
			goto error;
	}

	file = NULL;
	cnt = 0;
	while ( (file = idmef_target_get_next_file(target, file)) ) {

		ret = get_file_access(sql, message_ident, target_index, cnt, file);
		if ( ret < 0 )
			goto error;

		ret = get_linkage(sql, message_ident, target_index, cnt, file);
		if ( ret < 0 )
			goto error;

		ret = get_inode(sql, message_ident, target_index, cnt, file);
		if ( ret < 0 )
			goto error;

		ret = get_checksum(sql, message_ident, target_index, cnt, file);
		if ( ret < 0 )
			goto error;

		cnt++;
	}
	
 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_source(preludedb_sql_t *sql,
		      uint64_t message_ident,
		      idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_source_t *source;
	int cnt;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, spoofed, interface "
					  "FROM Prelude_Source "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_alert_new_source(alert, &source);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 0, source, idmef_source_new_ident);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 1, source, idmef_source_new_spoofed, idmef_source_spoofed_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, source, idmef_source_new_interface);
		if ( ret < 0 )
			goto error;
	}

	source = NULL;
	cnt = 0;
	while ( (source = idmef_alert_get_next_source(alert, source)) ) {

		ret = get_node(sql, message_ident, 'S', cnt, source, (int (*)(void *, idmef_node_t **)) idmef_source_new_node);
		if ( ret < 0 )
			goto error;

		ret = get_user(sql, message_ident, 'S', cnt, source, (int (*)(void *, idmef_user_t **)) idmef_source_new_user);
		if ( ret < 0 )
			goto error;

		ret = get_process(sql, message_ident, 'S', cnt, source, (int (*)(void *, idmef_process_t **)) idmef_source_new_process);
		if ( ret < 0 )
			goto error;

		ret = get_service(sql, message_ident, 'S', cnt, source, (int (*)(void *, idmef_service_t **)) idmef_source_new_service);
		if ( ret < 0 )
			goto error;

		cnt++;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_target(preludedb_sql_t *sql,
		      uint64_t message_ident,
		      idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_target_t *target;
	int cnt;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, decoy, interface "
					  "FROM Prelude_Target "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_alert_new_target(alert, &target);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 0, target, idmef_target_new_ident);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 1, target, idmef_target_new_decoy, idmef_target_decoy_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, target, idmef_target_new_interface);
		if ( ret < 0 )
			goto error;
	}

	target = NULL;
	cnt = 0;
	while ( (target = idmef_alert_get_next_target(alert, target)) ) {

		ret = get_node(sql, message_ident, 'T', cnt, target, (int (*)(void *, idmef_node_t **)) idmef_target_new_node);
		if ( ret < 0 )		     
			goto error;

		ret = get_user(sql, message_ident, 'T', cnt, target, (int (*)(void *, idmef_user_t **)) idmef_target_new_user);
		if ( ret < 0 )
			goto error;

		ret = get_process(sql, message_ident, 'T', cnt, target, (int (*)(void *, idmef_process_t **)) idmef_target_new_process);
		if ( ret < 0 )
			goto error;

		ret = get_service(sql, message_ident, 'T', cnt, target, (int (*)(void *, idmef_service_t **)) idmef_target_new_service);
		if ( ret < 0 )
			goto error;

		ret = get_file(sql, message_ident, cnt, target);
		if ( ret < 0 )
			goto error;

		cnt++;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_additional_data(preludedb_sql_t *sql,
			       uint64_t message_ident,
			       char parent_type,
			       void *parent,
			       int (*parent_new_child)(void *, idmef_additional_data_t **))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_additional_data_t *additional_data;
	idmef_data_t *data;
	preludedb_sql_field_t *field;
	int ret = 0;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT type, meaning, data "
					  "FROM Prelude_AdditionalData "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 "",
					  parent_type, message_ident);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = parent_new_child(parent, &additional_data);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 0, additional_data, idmef_additional_data_new_type,
			       idmef_additional_data_type_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 1, additional_data, idmef_additional_data_new_meaning);
		if ( ret < 0 )
			goto error;

		ret = preludedb_sql_row_fetch_field(row, 2, &field);
		if ( ret <= 0 ) {
			if ( ret == 0 )
				ret = -1;
			goto error;
		}

		ret = idmef_additional_data_new_data(additional_data, &data);
		if ( ret < 0 )
			goto error;

		switch ( idmef_additional_data_get_type(additional_data) ) {
		case IDMEF_ADDITIONAL_DATA_TYPE_BOOLEAN: case IDMEF_ADDITIONAL_DATA_TYPE_BYTE: {
			uint8_t value;

			ret = preludedb_sql_field_to_uint8(field, &value);
			if ( ret < 0 )
				goto error;

			idmef_data_set_byte(data, value);
			break;
		}

		case IDMEF_ADDITIONAL_DATA_TYPE_CHARACTER: {
			uint8_t value;

			ret = preludedb_sql_field_to_uint8(field, &value);
			if ( ret < 0 )
				goto error;
			
			idmef_data_set_char(data, (char) value);
			break;
		}

		case IDMEF_ADDITIONAL_DATA_TYPE_DATE_TIME:
		case IDMEF_ADDITIONAL_DATA_TYPE_PORTLIST:
		case IDMEF_ADDITIONAL_DATA_TYPE_STRING:
		case IDMEF_ADDITIONAL_DATA_TYPE_XML:
			ret = idmef_data_set_char_string_dup(data, preludedb_sql_field_get_value(field));
			break;

		case IDMEF_ADDITIONAL_DATA_TYPE_REAL: {
			float value;

			ret = preludedb_sql_field_to_float(field, &value);
			if ( ret < 0 )
				goto error;

			idmef_data_set_float(data, value);
			break;
		}

		case IDMEF_ADDITIONAL_DATA_TYPE_INTEGER: {
			uint32_t value;

			ret = preludedb_sql_field_to_uint32(field, &value);
			if ( ret < 0 )
				goto error;
			
			idmef_data_set_uint32(data, value);
			break;
		}

		case IDMEF_ADDITIONAL_DATA_TYPE_BYTE_STRING: {
			unsigned char *value;
			size_t value_size;

			ret = preludedb_sql_unescape_binary(sql,
							    preludedb_sql_field_get_value(field),
							    preludedb_sql_field_get_len(field),
							    &value, &value_size);
			if ( ret < 0 )
				break;

			ret = idmef_data_set_byte_string_nodup(data, value, value_size);

			break;
		}

		case IDMEF_ADDITIONAL_DATA_TYPE_NTPSTAMP: {
			uint64_t value;

			ret = preludedb_sql_field_to_uint64(field, &value);
			if ( ret < 0 )
				goto error;

			idmef_data_set_uint64(data, value);
			break;
		}
			
		case IDMEF_ADDITIONAL_DATA_TYPE_ERROR:
			ret = -1;
		}

		if ( ret < 0 )
			goto error;

	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_reference(preludedb_sql_t *sql,
			 uint64_t message_ident,
			 idmef_classification_t *classification)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_reference_t *reference;
	int ret;
	
	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT origin, name, url, meaning "
					  "FROM Prelude_Reference "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = idmef_classification_new_reference(classification, &reference);
		if ( ret < 0 )
			goto error;

		ret = get_enum(sql, row, 0, reference, idmef_reference_new_origin,
			       idmef_reference_origin_to_numeric);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 1, reference, idmef_reference_new_name);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 2, reference, idmef_reference_new_url);
		if ( ret < 0 )
			goto error;

		ret = get_string(sql, row, 3, reference, idmef_reference_new_meaning);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_classification(preludedb_sql_t *sql,
			      uint64_t message_ident,
			      idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_classification_t *classification;
	int ret;
	
	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT ident, text "
					  "FROM Prelude_Classification "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		goto error;

	ret = idmef_alert_new_classification(alert, &classification);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, classification, idmef_classification_new_ident);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, classification, idmef_classification_new_text);
	if ( ret < 0 )
		goto error;

	ret = get_reference(sql, message_ident, classification);
	if ( ret < 0 )
		goto error;

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_alertident(preludedb_sql_t *sql,
			  uint64_t message_ident,
			  char parent_type,
			  void *parent,
			  int (*parent_new_child)(void *parent, idmef_alertident_t **child))
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_alertident_t *alertident = NULL;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT alertident, analyzerid "
					  "FROM Prelude_AlertIdent "
					  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64,
					  parent_type, message_ident);
	if ( ret <= 0 )
		return ret;

	while ( (ret = preludedb_sql_table_fetch_row(table, &row)) > 0 ) {

		ret = parent_new_child(parent, &alertident);
		if ( ret < 0 )
			goto error;

		ret = get_uint64(sql, row, 0, alertident, idmef_alertident_new_alertident);
		if ( ret < 0 )
			goto error;

		ret = get_uint64(sql, row, 1, alertident, idmef_alertident_new_analyzerid);
		if ( ret < 0 )
			goto error;
	}

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_tool_alert(preludedb_sql_t *sql,
			  uint64_t message_ident,
			  idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_tool_alert_t *tool_alert;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT name, command "
					  "FROM Prelude_ToolAlert "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_alert_new_tool_alert(alert, &tool_alert);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, tool_alert, idmef_tool_alert_new_name);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 1, tool_alert, idmef_tool_alert_new_command);
	if ( ret < 0 )
		goto error;

	ret = get_alertident(sql, message_ident, 'T', tool_alert,
			     (int (*)(void *, idmef_alertident_t **)) idmef_tool_alert_new_alertident);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}

static int get_correlation_alert(preludedb_sql_t *sql,
				 uint64_t message_ident,
				 idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_correlation_alert_t *correlation_alert;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT name "
					  "FROM Prelude_CorrelationAlert "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_alert_new_correlation_alert(alert, &correlation_alert);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, correlation_alert, idmef_correlation_alert_new_name);
	if ( ret < 0 )
		goto error;

	ret = get_alertident(sql, message_ident, 'C', correlation_alert,
			     (int (*)(void *, idmef_alertident_t **)) idmef_correlation_alert_new_alertident);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}


static int get_overflow_alert(preludedb_sql_t *sql,
			      uint64_t message_ident,
			      idmef_alert_t *alert)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	idmef_overflow_alert_t *overflow_alert;
	preludedb_sql_field_t *field;
	idmef_data_t *buffer;
	unsigned char *data;
	size_t data_size;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table,
					  "SELECT program, size, buffer "
					  "FROM Prelude_OverflowAlert "
					  "WHERE _message_ident = %" PRIu64 "",
					  message_ident);
	if ( ret <= 0 )
		return ret;

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret <= 0 )
		return ret;

	ret = idmef_alert_new_overflow_alert(alert, &overflow_alert);
	if ( ret < 0 )
		goto error;

	ret = get_string(sql, row, 0, overflow_alert, idmef_overflow_alert_new_program);
	if ( ret < 0 )
		goto error;

	ret = get_uint32(sql, row, 1, overflow_alert, idmef_overflow_alert_new_size);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_row_fetch_field(row, 2, &field);
	if ( ret < 0 )
		goto error;

	ret = idmef_overflow_alert_new_buffer(overflow_alert, &buffer);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_unescape_binary(sql,
					    preludedb_sql_field_get_value(field),
					    preludedb_sql_field_get_len(field),
					    &data, &data_size);

	if ( ret < 0 )
		goto error;

	ret = idmef_data_set_byte_string_nodup(buffer, data, data_size);

 error:
	preludedb_sql_table_destroy(table);

	return ret;
}


static int get_messageid(preludedb_sql_t *sql, const char *table_name, uint64_t ident, prelude_string_t *messageid)
{
	preludedb_sql_table_t *table;
	preludedb_sql_row_t *row;
	preludedb_sql_field_t *field;
	int ret;

	ret = preludedb_sql_query_sprintf(sql, &table, "SELECT messageid FROM %s WHERE _ident = %" PRIu64 "", 
					  table_name, ident);
	if ( ret < 0 )
		return ret;

	if ( ret == 0 )
		return preludedb_error(PRELUDEDB_ERROR_INVALID_MESSAGE_IDENT);

	ret = preludedb_sql_table_fetch_row(table, &row);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_row_fetch_field(row, 0, &field);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_field_to_string(field, messageid);

 error:
	preludedb_sql_table_destroy(table);

	return (ret < 0) ? ret : 1;
}


int get_alert(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message)
{
	idmef_alert_t *alert;
        prelude_string_t *messageid;
	int ret;

	ret = idmef_message_new(message);
	if ( ret < 0 )
		return ret;

	ret = idmef_message_new_alert(*message, &alert);
	if ( ret < 0 )
		goto error;

	ret = idmef_alert_new_messageid(alert, &messageid);
	if ( ret < 0 )
		goto error;

	ret = get_messageid(sql, "Prelude_Alert", ident, messageid);
	if ( ret < 0 )
		goto error;
        
	if ( ret == 0 ) {
		ret = preludedb_error(PRELUDEDB_ERROR_INVALID_MESSAGE_IDENT);
		goto error;
	}

	ret = get_assessment(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_analyzer(sql, ident, 'A', 0, alert, (int (*)(void *, idmef_analyzer_t **)) idmef_alert_new_analyzer);
	if ( ret < 0 )
		goto error;

	ret = get_create_time(sql, ident, 'A', alert, (int (*)(void *, idmef_time_t **)) idmef_alert_new_create_time);
	if ( ret < 0 )
		goto error;

	ret = get_detect_time(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_analyzer_time(sql, ident, 'A', alert, (int (*)(void *, idmef_time_t **)) idmef_alert_new_analyzer_time);
	if ( ret < 0 )
		goto error;

	ret = get_source(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_target(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_classification(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_additional_data(sql, ident, 'A', alert, 
				  (int (*)(void *, idmef_additional_data_t **)) idmef_alert_new_additional_data);
	if ( ret < 0 )
		goto error;

	ret = get_tool_alert(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_correlation_alert(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	ret = get_overflow_alert(sql, ident, alert);
	if ( ret < 0 )
		goto error;

	return 0;

 error:
	idmef_message_destroy(*message);

	return ret;
}



int get_heartbeat(preludedb_sql_t *sql, uint64_t ident, idmef_message_t **message)
{
	idmef_heartbeat_t *heartbeat;
	prelude_string_t *messageid;
	int ret;

	ret = idmef_message_new(message);
	if ( ret < 0 ) 
		return ret;

	ret = idmef_message_new_heartbeat(*message, &heartbeat);
	if ( ret < 0 )
		goto error;

	ret = idmef_heartbeat_new_messageid(heartbeat, &messageid);
	if ( ret < 0 )
		goto error;

	ret = get_messageid(sql, "Prelude_Heartbeat", ident, messageid);
	if ( ret <= 0 )
		goto error;

	ret = get_analyzer(sql, ident, 'H', 0, heartbeat, (int (*)(void *, idmef_analyzer_t **)) idmef_heartbeat_new_analyzer);
	if ( ret < 0 )
		goto error;

	ret = get_create_time(sql, ident, 'H', heartbeat, (int (*)(void *, idmef_time_t **)) idmef_heartbeat_new_create_time);
	if ( ret < 0 )
		goto error;

	ret = get_analyzer_time(sql, ident, 'H', heartbeat, (int (*)(void *, idmef_time_t **)) idmef_heartbeat_new_analyzer_time);
	if ( ret < 0 )
		goto error;

	ret = get_additional_data(sql, ident, 'H', heartbeat,
				  (int (*)(void *, idmef_additional_data_t **)) idmef_heartbeat_new_additional_data);
	if ( ret < 0 )
		goto error;

	return 0;

 error:
	idmef_message_destroy(*message);

	return ret;
}
