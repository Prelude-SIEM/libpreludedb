/*****
*
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <time.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object.h"
#include "db-message-ident.h"

#include "idmef-db-get.h"

#define db_log(sql) log(LOG_ERR, "%s\n", prelude_sql_error(sql))
#define log_memory_exhausted() log(LOG_ERR, "memory exhausted !\n")

#define get_(type, name)								\
static int _get_ ## name (prelude_sql_connection_t *sql, prelude_sql_row_t *row,	\
			 int index,							\
			 void *parent, type *(*parent_new_child)(void *parent))		\
{											\
	prelude_sql_field_t *field;							\
	type *value;									\
											\
	field = prelude_sql_field_fetch(row, index);					\
	if ( ! field ) {								\
		if ( prelude_sql_errno(sql) ) {						\
			db_log(sql);							\
			return -1;							\
		}									\
											\
		return 0;								\
	}										\
											\
	value = parent_new_child(parent);						\
	if ( ! value )									\
		return -1;								\
											\
	*value = prelude_sql_field_value_ ## name (field);				\
											\
	return 1;									\
}

get_(uint8_t, uint8)
get_(uint16_t, uint16)
get_(uint32_t, uint32)
get_(uint64_t, uint64)
get_(float, float)

#define get_uint8(sql, row, index, parent, parent_new_child) \
	_get_uint8(sql, row, index, parent, (uint8_t *(*)(void *)) parent_new_child)

#define get_uint16(sql, row, index, parent, parent_new_child) \
	_get_uint16(sql, row, index, parent, (uint16_t *(*)(void *)) parent_new_child)

#define get_uint32(sql, row, index, parent, parent_new_child) \
	_get_uint32(sql, row, index, parent, (uint32_t *(*)(void *)) parent_new_child)

#define get_uint64(sql, row, index, parent, parent_new_child) \
	_get_uint64(sql, row, index, parent, (uint64_t *(*)(void *)) parent_new_child)

#define get_float(sql, row, index, parent, parent_new_child) \
	_get_float(sql, row, index, parent, (float *(*)(void *)) parent_new_child)



static int _get_string(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
		       int index,
		       void *parent, prelude_string_t *(*parent_new_child)(void *parent))
{
	prelude_sql_field_t *field;
	const char *tmp;
	prelude_string_t *string;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		return 0;
	}

	tmp = prelude_sql_field_value(field);
	if ( ! tmp )
		return -1;

	string = parent_new_child(parent);
	if ( ! string )
		return -1;

	if ( prelude_string_set_dup(string, tmp) < 0 )
		return -1;

	return 1;
}

static int _get_data(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
		     int index,
		     void *parent, idmef_data_t *(parent_new_child)(void *parent))
{
	prelude_sql_field_t *field;
	const char *tmp;
	idmef_data_t *data;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		return 0;
	}

	tmp = prelude_sql_field_value(field);
	if ( ! tmp )
		return -1;

	data = parent_new_child(parent);
	if ( ! data )
		return -1;

	if ( idmef_data_set_dup(data, tmp, strlen(tmp) + 1) < 0 )
		return -1;

	return 1;
}

static int _get_enum(prelude_sql_connection_t *sql, prelude_sql_row_t *row, 
		     int index,
		     void *parent, int *(*parent_new_child)(void *parent), int (*convert_enum)(const char *))
{
	prelude_sql_field_t *field;
	const char *tmp;
	int *enum_val;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		return 0;
	}

	tmp = prelude_sql_field_value(field);
	if ( ! tmp )
		return -1;

	enum_val = parent_new_child(parent);
	if ( ! enum_val )
		return -1;

	*enum_val = convert_enum(tmp);

	return 1;
}



static int _get_timestamp(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
			  int time_index, int gmtoff_index, int usec_index,
			  void *parent, idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_field_t *time_field, *gmtoff_field, *usec_field = NULL;
	const char *tmp;
	uint32_t gmtoff;
	uint32_t usec = 0;
	idmef_time_t *time;

	time_field = prelude_sql_field_fetch(row, time_index);
	gmtoff_field = prelude_sql_field_fetch(row, gmtoff_index);
	if ( usec_index != -1 )
		usec_field = prelude_sql_field_fetch(row, usec_index);
	
	if ( ! time_field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		return 0;
	} else {
		if ( ! gmtoff_field )
			return -1;
		if ( usec_index != -1 && ! usec_field )
			return -1;
	}	

	tmp = prelude_sql_field_value(time_field);
	if ( ! tmp )
		return -1;

	gmtoff = prelude_sql_field_value_uint32(gmtoff_field);

	if ( usec_index != -1 )
		usec = prelude_sql_field_value_uint32(usec_field);

	time = parent_new_child(parent);
	if ( ! time )
		return -1;

	return prelude_sql_time_from_timestamp(time, tmp, gmtoff, usec_index != -1 ? usec : 0);
}

#define get_string(sql, row, index, parent, parent_new_child) \
	_get_string(sql, row, index, parent, (prelude_string_t *(*)(void *)) parent_new_child)

#define get_data(sql, row, index, parent, parent_new_child) \
	_get_data(sql, row, index, parent, (idmef_data_t *(*)(void *)) parent_new_child)

#define get_enum(sql, row, index, parent, parent_new_child, convert_enum) \
	_get_enum(sql, row, index, parent, (int *(*)(void *)) parent_new_child, convert_enum)

#define get_timestamp(sql, row, time_index, gmtoff_index, usec_index, parent, parent_new_child) \
	_get_timestamp(sql, row, time_index, gmtoff_index, usec_index, parent, (idmef_time_t *(*)(void *)) parent_new_child)



static int get_analyzer_time(prelude_sql_connection_t *sql,
			     uint64_t message_ident,
			     char parent_type,
			     void *parent,
			     idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT time, gmtoff, usec "
				  "FROM Prelude_AnalyzerTime "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 "",
				  parent_type, message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	if ( get_timestamp(sql, row, 0, 1, 2, parent, parent_new_child) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_detect_time(prelude_sql_connection_t *sql,
			   uint64_t message_ident,
			   idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT time, gmtoff, usec "
				  "FROM Prelude_DetectTime "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	if ( get_timestamp(sql, row, 0, 1, 2, alert, idmef_alert_new_detect_time) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_create_time(prelude_sql_connection_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   void *parent,
			   idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT time, gmtoff, usec "
				  "FROM Prelude_CreateTime "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 "",
				  parent_type, message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	if ( get_timestamp(sql, row, 0, 1, 2, parent, parent_new_child) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_user_id(prelude_sql_connection_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       int file_index,
		       int file_access_index,
		       void *parent,
		       idmef_user_id_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_user_id_t *user_id;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT ident, type, name, number "
				  "FROM Prelude_UserId "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND "
				  "_parent_index = %d AND _file_index = %d AND _file_access_index = %d",
				  parent_type, message_ident, parent_index, file_index, file_access_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		user_id = parent_new_child(parent);
		if ( ! user_id )
			goto error;

		if ( get_uint64(sql, row, 0, user_id, idmef_user_id_new_ident) < 0 )
			goto error;

		if ( get_enum(sql, row, 1, user_id, idmef_user_id_new_type, idmef_user_id_type_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 2, user_id, idmef_user_id_new_name) < 0 )
			goto error;

		if ( get_uint32(sql, row, 3, user_id, idmef_user_id_new_number) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_user(prelude_sql_connection_t *sql,
		    uint64_t message_ident,
		    char parent_type,
		    int parent_index,
		    void *parent,
		    idmef_user_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_user_t *user;

	table = prelude_sql_query(sql,
				  "SELECT ident, category "
				  "FROM Prelude_User "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	user = parent_new_child(parent);
	if ( ! user )
		goto error;

	if ( get_uint64(sql, row, 0, user, idmef_user_new_ident) < 0 )
		goto error;

	if ( get_enum(sql, row, 1, user, idmef_user_new_category, idmef_user_category_to_numeric) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_user_id(sql, message_ident, parent_type, parent_index, 0, 0, user,
			 (idmef_user_id_t *(*)(void *)) idmef_user_new_user_id) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;	
}

static int get_process_arg(prelude_sql_connection_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   char parent_index,
			   void *parent,
			   prelude_string_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT arg "
				  "FROM Prelude_ProcessArg "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		if ( get_string(sql, row, 0, parent, parent_new_child) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_process_env(prelude_sql_connection_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   int parent_index,
			   void *parent,
			   prelude_string_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT env "
				  "FROM Prelude_ProcessEnv "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		if ( get_string(sql, row, 0, parent, parent_new_child) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_process(prelude_sql_connection_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       void *parent,
		       idmef_process_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_process_t *process;

	table = prelude_sql_query(sql,
				  "SELECT ident, name, pid, path "
				  "FROM Prelude_Process "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	process = parent_new_child(parent);
	if ( ! process )
		goto error;

	if ( get_uint64(sql, row, 0, process, idmef_process_new_ident) < 0 )
		goto error;

	if ( get_string(sql, row, 1, process, idmef_process_new_name) < 0 )
		goto error;

	if ( get_uint32(sql, row, 2, process, idmef_process_new_pid) < 0 )
		goto error;

	if ( get_string(sql, row, 3, process, idmef_process_new_path) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_process_arg(sql, message_ident, parent_type, parent_index, process,
			     (prelude_string_t *(*)(void *)) idmef_process_new_arg) < 0 )
		goto error;
	
	if ( get_process_env(sql, message_ident, parent_type, parent_index, process,
			     (prelude_string_t *(*)(void *)) idmef_process_new_env) < 0 )
		goto error;

	return 1;
	
 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_web_service_arg(prelude_sql_connection_t *sql,
			       uint64_t message_ident,
			       char parent_type,
			       int parent_index,
			       idmef_web_service_t *web_service)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT arg "
				  "FROM Prelude_WebServiceArg "
				  "WHERE _parent_type = '%c' and _message_ident = %" PRIu64 " and _parent_index = %d",
				  parent_type, message_ident, parent_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		if ( get_string(sql, row, 0, web_service, idmef_web_service_new_arg) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);


	return -1;
}

static int get_web_service(prelude_sql_connection_t *sql,
			   uint64_t message_ident,
			   char parent_type,
			   int parent_index,
			   idmef_service_t *service)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_web_service_t *web_service;

	table = prelude_sql_query(sql,
				  "SELECT url, cgi, http_method "
				  "FROM Prelude_WebService "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	web_service = idmef_service_new_web_service(service);
	if ( ! web_service )
		goto error;

	if ( get_string(sql, row, 0, web_service, idmef_web_service_new_url) < 0 )
		goto error;

	if ( get_string(sql, row, 1, web_service, idmef_web_service_new_cgi) < 0 )
		goto error;

	if ( get_string(sql, row, 2, web_service, idmef_web_service_new_http_method) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_web_service_arg(sql, message_ident, parent_type, parent_index, web_service) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_snmp_service(prelude_sql_connection_t *sql,
			    uint64_t message_ident,
			    char parent_type,
			    int parent_index,
			    idmef_service_t *service)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_snmp_service_t *snmp_service;

	table = prelude_sql_query(sql,
				  "SELECT oid, community, security_name, context_name, context_engine_id, command "
				  "FROM Prelude_SNMPService "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	snmp_service = idmef_service_new_snmp_service(service);
	if ( ! snmp_service )
		goto error;

	if ( get_string(sql, row, 0, snmp_service, idmef_snmp_service_new_oid) < 0 )
		goto error;

	if ( get_string(sql, row, 1, snmp_service, idmef_snmp_service_new_community) < 0 )
		goto error;

	if ( get_string(sql, row, 2, snmp_service, idmef_snmp_service_new_security_name) < 0 )
		goto error;

	if ( get_string(sql, row, 3, snmp_service, idmef_snmp_service_new_context_name) < 0 )
		goto error;

	if ( get_string(sql, row, 4, snmp_service, idmef_snmp_service_new_context_engine_id) < 0 )
		goto error;

	if ( get_string(sql, row, 5, snmp_service, idmef_snmp_service_new_command) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_service(prelude_sql_connection_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       void *parent,
		       idmef_service_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_service_t *service;

	table = prelude_sql_query(sql,
				  "SELECT ident, name, port, iana_protocol_number, iana_protocol_name, portlist, protocol "
				  "FROM Prelude_Service "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	service = parent_new_child(parent);
	if ( ! service )
		goto error;

	if ( get_uint64(sql, row, 0, service, idmef_service_new_ident) < 0 )
		goto error;

	if ( get_string(sql, row, 1, service, idmef_service_new_name) < 0 )
		goto error;

	if ( get_uint16(sql, row, 2, service, idmef_service_new_port) < 0 )
		goto error;

	if ( get_uint8(sql, row, 3, service, idmef_service_new_iana_protocol_number) < 0 )
		goto error;

	if ( get_string(sql, row, 4, service, idmef_service_new_iana_protocol_name) < 0 )
		goto error;

	if ( get_string(sql, row, 5, service, idmef_service_new_portlist) < 0 )
		goto error;

	if ( get_string(sql, row, 6, service, idmef_service_new_protocol) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_web_service(sql, message_ident, parent_type, parent_index, service) < 0 )
		goto error;

	if ( get_snmp_service(sql, message_ident, parent_type, parent_index, service) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_address(prelude_sql_connection_t *sql,
		       uint64_t message_ident,
		       char parent_type,
		       int parent_index,
		       void *parent,
		       idmef_address_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_address_t *idmef_address;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT ident, category, vlan_name, vlan_num, address, netmask "
				  "FROM Prelude_Address "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		idmef_address = parent_new_child(parent);
		if ( ! idmef_address )
			goto error;

		if ( get_uint64(sql, row, 0, idmef_address, idmef_address_new_ident) < 0 )
			goto error;

		if ( get_enum(sql, row, 1, idmef_address, idmef_address_new_category, idmef_address_category_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 2, idmef_address, idmef_address_new_vlan_name) < 0 )
			goto error;

		if ( get_uint32(sql, row, 3, idmef_address, idmef_address_new_vlan_num) < 0 )
			goto error;

		if ( get_string(sql, row, 4, idmef_address, idmef_address_new_address) < 0 )
			goto error;

		if ( get_string(sql, row, 5, idmef_address, idmef_address_new_netmask) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_node(prelude_sql_connection_t *sql,
		    uint64_t message_ident,
		    char parent_type,
		    int parent_index,
		    void *parent,
		    idmef_node_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_node_t *node;

	table = prelude_sql_query(sql,
				  "SELECT ident, category, location, name "
				  "FROM Prelude_Node "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _parent_index = %d",
				  parent_type, message_ident, parent_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	node = parent_new_child(parent);
	if ( ! node )
		goto error;

	if ( get_uint64(sql, row, 0, node, idmef_node_new_ident) < 0 )
		goto error;

	if ( get_enum(sql, row, 1, node, idmef_node_new_category, idmef_node_category_to_numeric) < 0 )
		goto error;

	if ( get_string(sql, row, 2, node, idmef_node_new_location) < 0 )
		goto error;

	if ( get_string(sql, row, 3, node, idmef_node_new_name) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_address(sql, message_ident, parent_type, parent_index, node,
			 (idmef_address_t *(*)(void *)) idmef_node_new_address) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_analyzer(prelude_sql_connection_t *sql,
			uint64_t message_ident,
			char parent_type,
			int depth,
			void *parent,
			idmef_analyzer_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_analyzer_t *analyzer;

	table = prelude_sql_query(sql,
				  "SELECT analyzerid, name, manufacturer, model, version, class, ostype, osversion "
				  "FROM Prelude_Analyzer "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 " AND _depth = %d",
				  parent_type, message_ident, depth);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	analyzer = parent_new_child(parent);
	if ( ! analyzer )
		goto error;

	if ( get_uint64(sql, row, 0, analyzer, idmef_analyzer_new_analyzerid) < 0 )
		goto error;

	if ( get_string(sql, row, 1, analyzer, idmef_analyzer_new_name) < 0 )
		goto error;

	if ( get_string(sql, row, 2, analyzer, idmef_analyzer_new_manufacturer) < 0 )
		goto error;

	if ( get_string(sql, row, 3, analyzer, idmef_analyzer_new_model) < 0 )
		goto error;

	if ( get_string(sql, row, 4, analyzer, idmef_analyzer_new_version) < 0 )
		goto error;

	if ( get_string(sql, row, 5, analyzer, idmef_analyzer_new_class) < 0 )
		goto error;

	if ( get_string(sql, row, 6, analyzer, idmef_analyzer_new_ostype) < 0 )
		goto error;

	if ( get_string(sql, row, 7, analyzer, idmef_analyzer_new_osversion) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_node(sql, message_ident, parent_type, depth, analyzer,
		      (idmef_node_t *(*)(void *)) idmef_analyzer_new_node) < 0 )
		goto error;

	if ( get_process(sql, message_ident, parent_type, depth, analyzer,
			 (idmef_process_t *(*)(void *)) idmef_analyzer_new_process) < 0 )
		goto error;

	if ( get_analyzer(sql, message_ident, parent_type, depth + 1, analyzer,
			  (idmef_analyzer_t *(*)(void *)) idmef_analyzer_new_analyzer) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_action(prelude_sql_connection_t *sql,
		      uint64_t message_ident,
		      idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_action_t *action;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT category, description "
				  "FROM Prelude_Action "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		action = idmef_assessment_new_action(assessment);
		if ( ! action )
			goto error;

		if ( get_enum(sql, row, 0, action, idmef_action_new_category, idmef_action_category_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, action, idmef_action_new_description) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
	
}

static int get_confidence(prelude_sql_connection_t *sql,
			  uint64_t message_ident,
			  idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_confidence_t *confidence;

	table = prelude_sql_query(sql,
				  "SELECT rating, confidence "
				  "FROM Prelude_Confidence "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	confidence = idmef_assessment_new_confidence(assessment);
	if ( ! confidence )
		goto error;

	if ( get_enum(sql, row, 0, confidence, idmef_confidence_new_rating, idmef_confidence_rating_to_numeric) < 0 )
		goto error;

	if ( get_float(sql, row, 1, confidence, idmef_confidence_new_confidence) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_impact(prelude_sql_connection_t *sql, 
		      uint64_t message_ident, 
		      idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_impact_t *impact;

	table = prelude_sql_query(sql, 
				  "SELECT severity, completion, type, description "
				  "FROM Prelude_Impact "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	impact = idmef_assessment_new_impact(assessment);
	if ( ! impact )
		goto error;

	if ( get_enum(sql, row, 0, impact, idmef_impact_new_severity, idmef_impact_severity_to_numeric) < 0  )
		goto error;

	if ( get_enum(sql, row, 1, impact, idmef_impact_new_completion, idmef_impact_completion_to_numeric) < 0 )
		goto error;

	if ( get_enum(sql, row, 2, impact, idmef_impact_new_type, idmef_impact_type_to_numeric) < 0 )
		goto error;

	if ( get_string(sql, row, 3, impact, idmef_impact_new_description) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_assessment(prelude_sql_connection_t *sql, 
			  uint64_t message_ident,
			  idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	idmef_assessment_t *assessment;

	table = prelude_sql_query(sql,
				  "SELECT _message_ident "
				  "FROM Prelude_Assessment "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	prelude_sql_table_free(table);

	assessment = idmef_alert_new_assessment(alert);
	if ( ! assessment )
		goto error;

	if ( get_impact(sql, message_ident, assessment) < 0 )
		goto error;

	if ( get_confidence(sql, message_ident, assessment) < 0 )
		goto error;

	if ( get_action(sql, message_ident, assessment) < 0 )
		goto error;

	return 1;

 error:
	return -1;
}

static int get_file_access_permission(prelude_sql_connection_t *sql,
				      uint64_t message_ident,
				      int target_index,
				      int file_index,
				      int file_access_index,
				      idmef_file_access_t *parent)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT perm "
				  "FROM Prelude_FileAccess_Permission "
				  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND "
				  "_file_index = %d AND _file_access_index = %d",
				  message_ident, target_index, file_index, file_access_index);

	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		if ( get_string(sql, row, 0, parent, idmef_file_access_new_permission) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_file_access(prelude_sql_connection_t *sql,
			   uint64_t message_ident,
			   int target_index,
			   int file_index,
			   idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
	idmef_file_access_t *file_access;
	int file_access_count;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT COUNT(*) "
				  "FROM Prelude_FileAccess "
				  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
				  message_ident, target_index, file_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return -1;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	field = prelude_sql_field_fetch(row, 0);
	if ( ! field ) {
		db_log(sql);
		goto error;
	}

	file_access_count = prelude_sql_field_value_int32(field);

	prelude_sql_table_free(table);
	table = NULL;

	for ( cnt = 0; cnt < file_access_count; cnt++ ) {

		file_access = idmef_file_new_file_access(file);
		if ( ! file_access )
			goto error;

		if ( get_user_id(sql, message_ident, 'F', target_index, file_index, cnt,
				 file_access, (idmef_user_id_t *(*)(void *)) idmef_file_access_new_user_id) < 0 )
			goto error;

		if ( get_file_access_permission(sql, message_ident, target_index, file_index, cnt, file_access) < 0 )
			goto error;
	}

	return file_access_count;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_linkage(prelude_sql_connection_t *sql,
		       uint64_t message_ident,
		       int target_index,
		       int file_index,
		       idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_linkage_t *linkage;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT category, name, path "
				  "FROM Prelude_Linkage "
				  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
				  message_ident, target_index, file_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		linkage = idmef_file_new_linkage(file);
		if ( ! linkage )
			goto error;

		if ( get_enum(sql, row, 0, linkage, idmef_linkage_new_category, idmef_linkage_category_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, linkage, idmef_linkage_new_name) < 0 )
			goto error;

		if ( get_string(sql, row, 2, linkage, idmef_linkage_new_path) < 0 )
			goto error;
	}

	prelude_sql_table_free(table);
	table = NULL;

	/* FIXME: file in linkage is not currently supported  */

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_inode(prelude_sql_connection_t *sql,
		     uint64_t message_ident,
		     int target_index,
		     int file_index,
		     idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_inode_t *inode;

	table = prelude_sql_query(sql,
				  "SELECT change_time, change_time_gmtoff, number, major_device, minor_device, "
				  "c_major_device, c_minor_device "
				  "FROM Prelude_Inode "
				  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
				  message_ident, target_index, file_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	inode = idmef_file_new_inode(file);
	if ( ! inode )
		goto error;

	if ( get_timestamp(sql, row, 0, 1, -1, inode, idmef_inode_new_change_time) < 0 )
		goto error;

	if ( get_uint32(sql, row, 2, inode, idmef_inode_new_number) < 0 )
		goto error;

	if ( get_uint32(sql, row, 3, inode, idmef_inode_new_major_device) < 0 )
		goto error;

	if ( get_uint32(sql, row, 4, inode, idmef_inode_new_minor_device) < 0 )
		goto error;

	if ( get_uint32(sql, row, 5, inode, idmef_inode_new_c_major_device) < 0 )
		goto error;

	if ( get_uint32(sql, row, 6, inode, idmef_inode_new_c_minor_device) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}


static int get_checksum(prelude_sql_connection_t *sql,
			uint64_t message_ident,
			int target_index,
			int file_index,
			idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_checksum_t *checksum;
	int cnt = 0;
	
	table = prelude_sql_query(sql,
				  "SELECT value, key, algorithm "
				  "FROM Prelude_Checksum "
				  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d AND _file_index = %d",
				  message_ident, target_index, file_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	cnt = 0;
	while ( (row = prelude_sql_row_fetch(table)) ) {

		checksum = idmef_file_new_checksum(file);
		if ( ! checksum )
			goto error;

		if ( get_string(sql, row, 0, checksum, idmef_checksum_new_value) < 0 )
			goto error;

		if ( get_string(sql, row, 1, checksum, idmef_checksum_new_key) < 0 )
			goto error;

		if ( get_enum(sql, row, 2, checksum, idmef_checksum_new_algorithm, idmef_checksum_algorithm_to_numeric) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);
	table = NULL;

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}


static int get_file(prelude_sql_connection_t *sql,
		    uint64_t message_ident,
		    int target_index,
		    idmef_target_t *target)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_file_t *file = NULL;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT ident, category, name, path, create_time, create_time_gmtoff, modify_time, modify_time_gmtoff, "
				  "access_time, access_time_gmtoff, data_size, disk_size, fstype "
				  "FROM Prelude_File "
				  "WHERE _message_ident = %" PRIu64 " AND _target_index = %d",
				  message_ident, target_index);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		file = idmef_target_new_file(target);
		if ( ! file )
			goto error;

		if ( get_uint64(sql, row, 0, file, idmef_file_new_ident) < 0 )
			goto error;

		if ( get_enum(sql, row, 1, file, idmef_file_new_category, idmef_file_category_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 2, file, idmef_file_new_name) < 0 )
			goto error;

		if ( get_string(sql, row, 3, file, idmef_file_new_path) < 0 )
			goto error;

		if ( get_timestamp(sql, row, 4, 5, -1, file, idmef_file_new_create_time) < 0 )
			goto error;

		if ( get_timestamp(sql, row, 6, 7, -1, file, idmef_file_new_modify_time) < 0 )
			goto error;

		if ( get_timestamp(sql, row, 8, 9, -1, file, idmef_file_new_access_time) < 0 )
			goto error;

		if ( get_uint32(sql, row, 10, file, idmef_file_new_data_size) < 0 )
			goto error;

		if ( get_uint32(sql, row, 11, file, idmef_file_new_disk_size) < 0 )
			goto error;

		if ( get_enum(sql, row, 12, file, idmef_file_new_fstype, idmef_file_fstype_to_numeric) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	file = NULL;

	while ( (file = idmef_target_get_next_file(target, file)) ) {

		if ( get_file_access(sql, message_ident, target_index, cnt, file) < 0 )
			goto error;

		if ( get_linkage(sql, message_ident, target_index, cnt, file) < 0 )
			goto error;

		if ( get_inode(sql, message_ident, target_index, cnt, file) < 0 )
			goto error;

		if ( get_checksum(sql, message_ident, target_index, cnt, file) < 0 )
			goto error;

		cnt++;
	}

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_source(prelude_sql_connection_t *sql,
		      uint64_t message_ident,
		      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_source_t *source;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT ident, spoofed, interface "
				  "FROM Prelude_Source "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		source = idmef_alert_new_source(alert);
		if ( ! source )
			goto error;

		if ( get_uint64(sql, row, 0, source, idmef_source_new_ident) < 0 )
			goto error;

		if ( get_enum(sql, row, 1, source, idmef_source_new_spoofed, idmef_source_spoofed_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 2, source, idmef_source_new_interface) < 0 )
			goto error;
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	source = NULL;

	while ( (source = idmef_alert_get_next_source(alert, source)) ) {

		if ( get_node(sql, message_ident, 'S', cnt, source, (idmef_node_t *(*)(void *)) idmef_source_new_node) < 0 )
			goto error;

		if ( get_user(sql, message_ident, 'S', cnt, source, (idmef_user_t *(*)(void *)) idmef_source_new_user) < 0 )
			goto error;

		if ( get_process(sql, message_ident, 'S', cnt, source, (idmef_process_t *(*)(void *)) idmef_source_new_process) < 0 )
			goto error;

		if ( get_service(sql, message_ident, 'S', cnt, source, (idmef_service_t *(*)(void *)) idmef_source_new_service) < 0 )
			goto error;

		cnt++;
	}

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_target(prelude_sql_connection_t *sql,
		      uint64_t message_ident,
		      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_target_t *target;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT ident, decoy, interface "
				  "FROM Prelude_Target "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		target = idmef_alert_new_target(alert);
		if ( ! target )
			goto error;

		if ( get_uint64(sql, row, 0, target, idmef_target_new_ident) < 0 )
			goto error;

		if ( get_enum(sql, row, 1, target, idmef_target_new_decoy, idmef_target_decoy_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 2, target, idmef_target_new_interface) < 0 )
			goto error;
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	target = NULL;

	while ( (target = idmef_alert_get_next_target(alert, target)) ) {

		if ( get_node(sql, message_ident, 'T', cnt, target, (idmef_node_t *(*)(void *)) idmef_target_new_node) < 0 )
			goto error;

		if ( get_user(sql, message_ident, 'T', cnt, target, (idmef_user_t *(*)(void *)) idmef_target_new_user) < 0 )
			goto error;

		if ( get_process(sql, message_ident, 'T', cnt, target, (idmef_process_t *(*)(void *)) idmef_target_new_process) < 0 )
			goto error;

		if ( get_service(sql, message_ident, 'T', cnt, target, (idmef_service_t *(*)(void *)) idmef_target_new_service) < 0 )
			goto error;

		if ( get_file(sql, message_ident, cnt, target) < 0 )
			goto error;

		cnt++;
	}

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_additional_data(prelude_sql_connection_t *sql,
			       uint64_t message_ident,
			       char parent_type,
			       void *parent,
			       idmef_additional_data_t *(*parent_new_child)(void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_additional_data_t *additional_data;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT type, meaning, data "
				  "FROM Prelude_AdditionalData "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64 "",
				  parent_type, message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		additional_data = parent_new_child(parent);
		if ( ! additional_data )
			goto error;

		if ( get_enum(sql, row, 0, additional_data, idmef_additional_data_new_type,
			      idmef_additional_data_type_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, additional_data, idmef_additional_data_new_meaning) < 0 )
			goto error;

		if ( get_data(sql, row, 2, additional_data, idmef_additional_data_new_data) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_reference(prelude_sql_connection_t *sql,
			 uint64_t message_ident,
			 idmef_classification_t *classification)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_reference_t *reference;
	int cnt = 0;
	
	table = prelude_sql_query(sql,
				  "SELECT origin, name, url, meaning "
				  "FROM Prelude_Reference "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		reference = idmef_classification_new_reference(classification);
		if ( ! reference )
			goto error;

		if ( get_enum(sql, row, 0, reference, idmef_reference_new_origin,
			      idmef_reference_origin_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, reference, idmef_reference_new_name) < 0 )
			goto error;

		if ( get_string(sql, row, 2, reference, idmef_reference_new_url) < 0 )
			goto error;

		if ( get_string(sql, row, 3, reference, idmef_reference_new_meaning) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_classification(prelude_sql_connection_t *sql,
			      uint64_t message_ident,
			      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_classification_t *classification;
	
	table = prelude_sql_query(sql,
				  "SELECT ident, text "
				  "FROM Prelude_Classification "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	classification = idmef_alert_new_classification(alert);
	if ( ! classification )
		goto error;

	if ( get_uint64(sql, row, 0, classification, idmef_classification_new_ident) < 0 )
		goto error;

	if ( get_string(sql, row, 1, classification, idmef_classification_new_text) < 0 )
		goto error;

	if ( get_reference(sql, message_ident, classification) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_alertident(prelude_sql_connection_t *sql,
			  uint64_t message_ident,
			  char parent_type,
			  void *parent,
			  idmef_alertident_t *(*parent_new_child)(void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_alertident_t *alertident = NULL;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT alertident, analyzerid "
				  "FROM Prelude_AlertIdent "
				  "WHERE _parent_type = '%c' AND _message_ident = %" PRIu64,
				  parent_type, message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		alertident = parent_new_child(parent);

		if ( get_uint64(sql, row, 0, alertident, idmef_alertident_new_alertident) < 0 )
			goto error;

		if ( get_uint64(sql, row, 1, alertident, idmef_alertident_new_analyzerid) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;	
}

static int get_tool_alert(prelude_sql_connection_t *sql,
			  uint64_t message_ident,
			  idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_tool_alert_t *tool_alert;

	table = prelude_sql_query(sql,
				  "SELECT name, command "
				  "FROM Prelude_ToolAlert "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	tool_alert = idmef_alert_new_tool_alert(alert);
	if ( ! tool_alert )
		goto error;

	if ( get_string(sql, row, 0, tool_alert, idmef_tool_alert_new_name) < 0 )
		goto error;

	if ( get_string(sql, row, 1, tool_alert, idmef_tool_alert_new_command) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return get_alertident(sql, message_ident, 'T', tool_alert,
			      (idmef_alertident_t *(*)(void *)) idmef_tool_alert_new_alertident);

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;	
}

static int get_correlation_alert(prelude_sql_connection_t *sql,
				 uint64_t message_ident,
				 idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_correlation_alert_t *correlation_alert;

	table = prelude_sql_query(sql,
				  "SELECT name "
				  "FROM Prelude_CorrelationAlert "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	correlation_alert = idmef_alert_new_correlation_alert(alert);
	if ( ! correlation_alert )
		goto error;

	if ( get_string(sql, row, 0, correlation_alert, idmef_correlation_alert_new_name) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	return get_alertident(sql, message_ident, 'C', correlation_alert,
			      (idmef_alertident_t *(*)(void *)) idmef_correlation_alert_new_alertident);

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_overflow_alert(prelude_sql_connection_t *sql,
			      uint64_t message_ident,
			      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_overflow_alert_t *overflow_alert;

	table = prelude_sql_query(sql,
				  "SELECT program, size, buffer "
				  "FROM Prelude_OverflowAlert "
				  "WHERE _message_ident = %" PRIu64 "",
				  message_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	row = prelude_sql_row_fetch(table);
	if ( ! row ) {
		db_log(sql);
		goto error;
	}

	overflow_alert = idmef_alert_new_overflow_alert(alert);
	if ( ! overflow_alert )
		goto error;

	if ( get_string(sql, row, 0, overflow_alert, idmef_overflow_alert_new_program) < 0 )
		goto error;

	if ( get_uint32(sql, row, 1, overflow_alert, idmef_overflow_alert_new_size) < 0 )
		goto error;

	if ( get_data(sql, row, 2, overflow_alert, idmef_overflow_alert_new_buffer) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;
	
 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}



idmef_message_t	*get_alert(prelude_db_connection_t *connection,
			   prelude_db_message_ident_t *message_ident,
			   uint64_t ident)
{
	prelude_sql_connection_t *sql;
	idmef_message_t *message;
	idmef_alert_t *alert;

	sql = prelude_db_connection_get(connection);
	if ( ! sql ) {
		log(LOG_ERR, "not an SQL connection !\n");
		return NULL;
	}

	message = idmef_message_new();
	if ( ! message ) {
		log_memory_exhausted();
		return NULL;
	}

	alert = idmef_message_new_alert(message);
	if ( ! alert )
		goto error;

	idmef_alert_set_messageid(alert, prelude_db_message_ident_get_ident(message_ident));

	if ( get_assessment(sql, ident, alert) < 0 )
		goto error;

	if ( get_analyzer(sql, ident, 'A', 0, alert, (idmef_analyzer_t *(*)(void *)) idmef_alert_new_analyzer) < 0 )
		goto error;

	if ( get_create_time(sql, ident, 'A', alert, (idmef_time_t *(*)(void *)) idmef_alert_new_create_time) < 0 )
		goto error;

	if ( get_detect_time(sql, ident, alert) < 0 )
		goto error;

	if ( get_analyzer_time(sql, ident, 'A', alert, (idmef_time_t *(*)(void *)) idmef_alert_new_analyzer_time) < 0 )
		goto error;

	if ( get_source(sql, ident, alert) < 0 )
		goto error;

	if ( get_target(sql, ident, alert) < 0 )
		goto error;

	if ( get_classification(sql, ident, alert) < 0 )
		goto error;

	if ( get_additional_data(sql, ident, 'A', alert, 
				 (idmef_additional_data_t *(*)(void *)) idmef_alert_new_additional_data) < 0 )
		goto error;

	if ( get_tool_alert(sql, ident, alert) < 0 )
		goto error;

	if ( get_correlation_alert(sql, ident, alert) < 0 )
		goto error;

	if ( get_overflow_alert(sql, ident, alert) < 0 )
		goto error;

	return message;

 error:
	if ( message )
		idmef_message_destroy(message);

	return NULL;
}



idmef_message_t *get_heartbeat(prelude_db_connection_t *connection,
			       prelude_db_message_ident_t *message_ident,
			       uint64_t ident)
{
	prelude_sql_connection_t *sql;
	idmef_message_t *message;
	idmef_heartbeat_t *heartbeat;

	sql = prelude_db_connection_get(connection);
	if ( ! sql ) {
		log(LOG_ERR, "not an SQL connection !\n");
		return NULL;
	}

	message = idmef_message_new();
	if ( ! message ) {
		log_memory_exhausted();
		return NULL;
	}

	heartbeat = idmef_message_new_heartbeat(message);
	if ( ! heartbeat )
		goto error;
	
	idmef_heartbeat_set_messageid(heartbeat, prelude_db_message_ident_get_ident(message_ident));

	if ( get_analyzer(sql, ident, 'H', 0, heartbeat, (idmef_analyzer_t *(*)(void *)) idmef_heartbeat_new_analyzer) < 0 )
		goto error;

	if ( get_create_time(sql, ident, 'H', heartbeat, (idmef_time_t *(*)(void *)) idmef_heartbeat_new_create_time) < 0 )
		goto error;

	if ( get_analyzer_time(sql, ident, 'H', heartbeat, (idmef_time_t *(*)(void *)) idmef_heartbeat_new_analyzer_time) < 0 )
		goto error;

	if ( get_additional_data(sql, ident, 'H', heartbeat,
				 (idmef_additional_data_t *(*)(void *)) idmef_heartbeat_new_additional_data) < 0 )
		goto error;

	return message;

 error:
	if ( message )
		idmef_message_destroy(message);

	return NULL;
}
