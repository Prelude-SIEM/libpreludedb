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
#include <libprelude/ntp.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "db-object.h"


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

get_(uint16_t, uint16)
get_(uint32_t, uint32)
get_(uint64_t, uint64)
get_(float, float)

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
		       void *parent, idmef_string_t *(*parent_new_child)(void *parent))
{
	prelude_sql_field_t *field;
	const char *tmp;
	idmef_string_t *string;

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

	if ( idmef_string_set_dup(string, tmp) < 0 )
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

static int _get_ntp_timestamp(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
			      int index,
			      void *parent, idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_field_t *field;
	const char *tmp;
	idmef_time_t *time;

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

	time = parent_new_child(parent);
	if ( ! time )
		return -1;

	if ( idmef_time_set_ntp_timestamp(time, tmp) < 0 )
		return -1;

	return 1;
}

static int _get_timestamp(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
			  int index,
			  void *parent, idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_field_t *field;
	const char *tmp;
	idmef_time_t *time;

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

	time = parent_new_child(parent);
	if ( ! time )
		return -1;

	if ( idmef_time_set_db_timestamp(time, tmp) < 0 )
		return -1;

	return 1;
}

#define get_string(sql, row, index, parent, parent_new_child) \
	_get_string(sql, row, index, parent, (idmef_string_t *(*)(void *)) parent_new_child)

#define get_data(sql, row, index, parent, parent_new_child) \
	_get_data(sql, row, index, parent, (idmef_data_t *(*)(void *)) parent_new_child)

#define get_enum(sql, row, index, parent, parent_new_child, convert_enum) \
	_get_enum(sql, row, index, parent, (int *(*)(void *)) parent_new_child, convert_enum)

#define get_ntp_timestamp(sql, row, index, parent, parent_new_child) \
	_get_ntp_timestamp(sql, row, index, parent, (idmef_time_t *(*)(void *)) parent_new_child)

#define get_timestamp(sql, row, index, parent, parent_new_child) \
	_get_timestamp(sql, row, index, parent, (idmef_time_t *(*)(void *)) parent_new_child)



static int get_analyzer_time(prelude_sql_connection_t *sql,
			     uint64_t parent_ident,
			     char parent_type,
			     void *parent,
			     idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT ntpstamp "
				  "FROM Prelude_AnalyzerTime "
				  "WHERE parent_type = '%c' AND parent_ident = %llu",
				  parent_type, parent_ident);
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

	if ( get_ntp_timestamp(sql, row, 0, parent, parent_new_child) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_detect_time(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT ntpstamp "
				  "FROM Prelude_DetectTime "
				  "WHERE alert_ident = %llu",
				  alert_ident);
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

	if ( get_ntp_timestamp(sql, row, 0, alert, idmef_alert_new_detect_time) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_create_time(prelude_sql_connection_t *sql,
			   uint64_t parent_ident,
			   char parent_type,
			   void *parent,
			   idmef_time_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT ntpstamp "
				  "FROM Prelude_CreateTime "
				  "WHERE parent_type = '%c' AND parent_ident = %llu",
				  parent_type, parent_ident);
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

	if ( get_ntp_timestamp(sql, row, 0, parent, parent_new_child) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_userid(prelude_sql_connection_t *sql,
		      uint64_t alert_ident,
		      uint64_t parent_ident,
		      char parent_type,
		      void *parent,
		      idmef_userid_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_userid_t *userid;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT type, name, number "
				  "FROM Prelude_UserId "
				  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		userid = parent_new_child(parent);
		if ( ! userid )
			goto error;

		if ( get_enum(sql, row, 0, userid, idmef_userid_new_type, idmef_userid_type_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, userid, idmef_userid_new_name) < 0 )
			goto error;

		if ( get_uint32(sql, row, 2, userid, idmef_userid_new_number) < 0 )
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
		    uint64_t alert_ident,
		    uint64_t parent_ident,
		    char parent_type,
		    void *parent,
		    idmef_user_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_user_t *user;

	table = prelude_sql_query(sql,
				  "SELECT category "
				  "FROM Prelude_User "
				  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);

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

	if ( get_enum(sql, row, 0, user, idmef_user_new_category, idmef_user_category_to_numeric) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_userid(sql, alert_ident, parent_ident, parent_type, user,
			(idmef_userid_t *(*)(void *)) idmef_user_new_userid) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;	
}

static int get_process_arg(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   uint64_t parent_ident,
			   char parent_type,
			   void *parent,
			   idmef_string_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	if ( parent_type == 'A' || parent_type == 'H' )
		table = prelude_sql_query(sql,
					  "SELECT arg "
					  "FROM Prelude_ProcessArg "
					  "WHERE parent_type = '%c' AND alert_ident = %llu",
					  parent_type, alert_ident);
	else
		table = prelude_sql_query(sql,
					  "SELECT arg "
					  "FROM Prelude_ProcessArg "
					  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
					  parent_type, parent_ident, alert_ident);

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
			   uint64_t alert_ident,
			   uint64_t parent_ident,
			   char parent_type,
			   void *parent,
			   idmef_string_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	if ( parent_type == 'A' || parent_type == 'H' )
		table = prelude_sql_query(sql,
					  "SELECT env "
					  "FROM Prelude_ProcessEnv "
					  "WHERE parent_type = '%c' AND alert_ident = %llu",
					  parent_type, alert_ident);
	else
		table = prelude_sql_query(sql,
					  "SELECT env "
					  "FROM Prelude_ProcessEnv "
					  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
					  parent_type, parent_ident, alert_ident);

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
		       uint64_t alert_ident,
		       uint64_t parent_ident,
		       char parent_type,
		       void *parent,
		       idmef_process_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_process_t *process;

	if ( parent_type == 'A' || parent_type == 'H' )
		table = prelude_sql_query(sql,
					  "SELECT name, pid, path "
					  "FROM Prelude_Process "
					  "WHERE parent_type = '%c' AND alert_ident = %llu",
					  parent_type, alert_ident);
	else
		table = prelude_sql_query(sql,
					  "SELECT name, pid, path "
					  "FROM Prelude_Process "
					  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
					  parent_type, parent_ident, alert_ident);

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

	if ( get_string(sql, row, 0, process, idmef_process_new_name) < 0 )
		goto error;

	if ( get_uint32(sql, row, 1, process, idmef_process_new_pid) < 0 )
		goto error;

	if ( get_string(sql, row, 2, process, idmef_process_new_path) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_process_arg(sql, alert_ident, parent_ident, parent_type, process,
			     (idmef_string_t *(*)(void *)) idmef_process_new_arg) < 0 )
		goto error;
	
	if ( get_process_env(sql, alert_ident, parent_ident, parent_type, process,
			     (idmef_string_t *(*)(void *)) idmef_process_new_env) < 0 )
		goto error;

	return 1;
	
 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_webservice_arg(prelude_sql_connection_t *sql,
			      uint64_t alert_ident,
			      uint64_t parent_ident,
			      char parent_type,
			      idmef_webservice_t *webservice)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT arg "
				  "FROM Prelude_WebServiceArg "
				  "WHERE parent_type = '%c' and parent_ident = %llu and alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		if ( get_string(sql, row, 0, webservice, idmef_webservice_new_arg) < 0 )
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

static int get_webservice(prelude_sql_connection_t *sql,
			  uint64_t alert_ident,
			  uint64_t parent_ident,
			  char parent_type,
			  idmef_service_t *service)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_webservice_t *webservice;

	table = prelude_sql_query(sql,
				  "SELECT url, cgi, http_method "
				  "FROM Prelude_WebService "
				  "WHERE parent_type = '%c' and parent_ident = %llu and alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);
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

	webservice = idmef_service_new_web(service);
	if ( ! webservice )
		goto error;

	if ( get_string(sql, row, 0, webservice, idmef_webservice_new_url) < 0 )
		goto error;

	if ( get_string(sql, row, 1, webservice, idmef_webservice_new_cgi) < 0 )
		goto error;

	if ( get_string(sql, row, 2, webservice, idmef_webservice_new_http_method) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_webservice_arg(sql, alert_ident, parent_ident, parent_type, webservice) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_snmpservice(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   uint64_t parent_ident,
			   char parent_type,
			   idmef_service_t *service)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_snmpservice_t *snmpservice;

	table = prelude_sql_query(sql,
				  "SELECT oid, community, command "
				  "FROM Prelude_SNMPService "
				  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);
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

	snmpservice = idmef_service_new_snmp(service);
	if ( ! snmpservice )
		goto error;

	if ( get_string(sql, row, 0, snmpservice, idmef_snmpservice_new_oid) < 0 )
		goto error;

	if ( get_string(sql, row, 1, snmpservice, idmef_snmpservice_new_community) < 0 )
		goto error;

	if ( get_string(sql, row, 2, snmpservice, idmef_snmpservice_new_command) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_service_portlist(prelude_sql_connection_t *sql,
				uint64_t alert_ident,
				uint64_t parent_ident,
				char parent_type,
				idmef_service_t *service)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;

	table = prelude_sql_query(sql,
				  "SELECT portlist "
				  "FROM Prelude_ServicePortlist "
				  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);
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

	if ( get_string(sql, row, 0, service, idmef_service_new_portlist) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_service(prelude_sql_connection_t *sql,
		       uint64_t alert_ident,
		       uint64_t parent_ident,
		       char parent_type,
		       void *parent,
		       idmef_service_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_service_t *service;

	table = prelude_sql_query(sql,
				  "SELECT name, port, protocol "
				  "FROM Prelude_Service "
				  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
				  parent_type, parent_ident, alert_ident);
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

	if ( get_string(sql, row, 0, service, idmef_service_new_name) < 0 )
		goto error;

	if ( get_uint16(sql, row, 1, service, idmef_service_new_port) < 0 )
		goto error;

	if ( get_string(sql, row, 2, service, idmef_service_new_protocol) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_service_portlist(sql, alert_ident, parent_ident, parent_type, service) < 0 )
		goto error;

	if ( get_webservice(sql, alert_ident, parent_ident, parent_type, service) < 0 )
		goto error;

	if ( get_snmpservice(sql, alert_ident, parent_ident, parent_type, service) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_address(prelude_sql_connection_t *sql,
		       uint64_t alert_ident,
		       uint64_t parent_ident,
		       char parent_type,
		       void *parent,
		       idmef_address_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_address_t *idmef_address;
	int cnt = 0;

	if ( parent_type == 'A' || parent_type == 'H' )
		table = prelude_sql_query(sql,
					  "SELECT category, vlan_name, vlan_num, address, netmask "
					  "FROM Prelude_Address "
					  "WHERE parent_type = '%c' AND alert_ident = %llu",
					  parent_type, alert_ident);
	else
		table = prelude_sql_query(sql,
					  "SELECT category, vlan_name, vlan_num, address, netmask "
					  "FROM Prelude_Address "
					  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
					  parent_type, parent_ident, alert_ident);

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

		if ( get_enum(sql, row, 0, idmef_address, idmef_address_new_category, idmef_address_category_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, idmef_address, idmef_address_new_vlan_name) < 0 )
			goto error;

		if ( get_uint32(sql, row, 2, idmef_address, idmef_address_new_vlan_num) < 0 )
			goto error;

		if ( get_string(sql, row, 3, idmef_address, idmef_address_new_address) < 0 )
			goto error;

		if ( get_string(sql, row, 4, idmef_address, idmef_address_new_netmask) < 0 )
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
		    uint64_t alert_ident,
		    uint64_t parent_ident,
		    char parent_type,
		    void *parent,
		    idmef_node_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_node_t *node;

	if ( parent_type == 'A' || parent_type == 'H' )
		table = prelude_sql_query(sql,
					  "SELECT category, location, name "
					  "FROM Prelude_Node "
					  "WHERE parent_type = '%c' AND alert_ident = %llu",
					  parent_type, alert_ident);
	else
		table = prelude_sql_query(sql,
					  "SELECT category, location, name "
					  "FROM Prelude_Node "
					  "WHERE parent_type = '%c' AND parent_ident = %llu AND alert_ident = %llu",
					  parent_type, parent_ident, alert_ident);

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

	if ( get_enum(sql, row, 0, node, idmef_node_new_category, idmef_node_category_to_numeric) < 0 )
		goto error;

	if ( get_string(sql, row, 1, node, idmef_node_new_location) < 0 )
		goto error;

	if ( get_string(sql, row, 2, node, idmef_node_new_name) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_address(sql, alert_ident, parent_ident, parent_type, node,
			 (idmef_address_t *(*)(void *)) idmef_node_new_address) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_analyzer(prelude_sql_connection_t *sql,
			uint64_t ident,
			char parent_type,
			void *parent,
			idmef_analyzer_t *(*parent_new_child)(void *parent))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_analyzer_t *analyzer;

	table = prelude_sql_query(sql,
				  "SELECT analyzerid, manufacturer, model, version, class, ostype, osversion "
				  "FROM Prelude_Analyzer "
				  "WHERE parent_type = '%c' AND parent_ident = %llu",
				  parent_type, ident);
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

	if ( get_string(sql, row, 1, analyzer, idmef_analyzer_new_manufacturer) < 0 )
		goto error;

	if ( get_string(sql, row, 2, analyzer, idmef_analyzer_new_model) < 0 )
		goto error;

	if ( get_string(sql, row, 3, analyzer, idmef_analyzer_new_version) < 0 )
		goto error;

	if ( get_string(sql, row, 4, analyzer, idmef_analyzer_new_class) < 0 )
		goto error;

	if ( get_string(sql, row, 5, analyzer, idmef_analyzer_new_ostype) < 0 )
		goto error;

	if ( get_string(sql, row, 6, analyzer, idmef_analyzer_new_osversion) < 0 )
		goto error;

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_node(sql, ident, 0, parent_type, analyzer,
		      (idmef_node_t *(*)(void *)) idmef_analyzer_new_node) < 0 )
		goto error;

	if ( get_process(sql, ident, 0, parent_type, analyzer,
			 (idmef_process_t *(*)(void *)) idmef_analyzer_new_process) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_action(prelude_sql_connection_t *sql,
		      uint64_t alert_ident,
		      idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_action_t *action;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT category, description "
				  "FROM Prelude_Action "
				  "WHERE alert_ident = %llu",
				  alert_ident);
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
			  uint64_t alert_ident,
			  idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_confidence_t *confidence;

	table = prelude_sql_query(sql,
				  "SELECT rating, confidence "
				  "FROM Prelude_Confidence "
				  "WHERE alert_ident = %llu",
				  alert_ident);
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
		      uint64_t alert_ident, 
		      idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_impact_t *impact;

	table = prelude_sql_query(sql, 
				  "SELECT severity, completion, type, description "
				  "FROM Prelude_Impact "
				  "WHERE alert_ident = %llu",
				  alert_ident);
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
			  uint64_t ident,
			  idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	idmef_assessment_t *assessment;

	table = prelude_sql_query(sql,
				  "SELECT alert_ident "
				  "FROM Prelude_Assessment "
				  "WHERE alert_ident = %llu",
				  ident);
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

	if ( get_impact(sql, ident, assessment) < 0 )
		goto error;

	if ( get_confidence(sql, ident, assessment) < 0 )
		goto error;

	if ( get_action(sql, ident, assessment) < 0 )
		goto error;

	return 1;

 error:
	return -1;
}

static int get_file_access(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   uint64_t target_ident,
			   uint64_t file_ident,
			   idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	prelude_sql_field_t *field;
	idmef_file_access_t *file_access;
	int file_access_num;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT COUNT(*) "
				  "FROM Prelude_FileAccess "
				  "WHERE file_ident = %llu AND target_ident = %llu AND alert_ident = %llu",
				  file_ident, target_ident, alert_ident);
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

	file_access_num = prelude_sql_field_value_int32(field);

	prelude_sql_table_free(table);
	table = NULL;

	for ( cnt = 0; cnt < file_access_num; cnt++ ) {

		file_access = idmef_file_new_file_access(file);
		if ( ! file_access )
			goto error;

		/*
		 * FIXME: that seems impossible to fetch userid the right way
		 * because we don't know exactly which node of "file" list and "file_access" list
		 * the Prelude_UserId refers to
		 */

		/*
		 * FIXME: permission_list is not supported by idmef-db-insert...
		 */

	}

	return file_access_num;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_linkage(prelude_sql_connection_t *sql,
		       uint64_t alert_ident,
		       uint64_t target_ident,
		       uint64_t file_ident,
		       idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_linkage_t *linkage;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT category, name, path "
				  "FROM Prelude_Linkage "
				  "WHERE file_ident = %llu AND target_ident = %llu AND alert_ident = %llu",
				  file_ident, target_ident, alert_ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		linkage = idmef_file_new_file_linkage(file);
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

	cnt = 0;
	linkage = NULL;

	while ( (linkage = idmef_file_get_next_file_linkage(file, linkage)) ) {

		/* 
		 * FIXME: there is no way to retrieve the file field from the DB since
		 * the parent_type field is not present in the Prelude_File table
		 * we cannot know if a file is a child of linkage or target
		 * so we assume that all files are a child of target
		 * in fact, it is almost always the true
		 */

		cnt++;
	}

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_inode(prelude_sql_connection_t *sql,
		     uint64_t alert_ident,
		     uint64_t target_ident,
		     uint64_t file_ident,
		     idmef_file_t *file)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_inode_t *inode;

	table = prelude_sql_query(sql,
				  "SELECT change_time, number, major_device, minor_device, c_major_device, c_minor_device "
				  "FROM Prelude_Inode "
				  "WHERE file_ident = %llu AND target_ident = %llu AND alert_ident = %llu",
				  file_ident, target_ident, alert_ident);
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

	if ( get_timestamp(sql, row, 0, inode, idmef_inode_new_change_time) < 0 )
		goto error;

	if ( get_uint32(sql, row, 1, inode, idmef_inode_new_number) < 0 )
		goto error;

	if ( get_uint32(sql, row, 2, inode, idmef_inode_new_major_device) < 0 )
		goto error;

	if ( get_uint32(sql, row, 3, inode, idmef_inode_new_minor_device) < 0 )
		goto error;

	if ( get_uint32(sql, row, 4, inode, idmef_inode_new_c_major_device) < 0 )
		goto error;

	if ( get_uint32(sql, row, 5, inode, idmef_inode_new_c_minor_device) < 0 )
		goto error;

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_file(prelude_sql_connection_t *sql,
		    uint64_t alert_ident,
		    uint64_t target_ident,
		    idmef_target_t *target)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_file_t *file = NULL;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT category, name, path, create_time, modify_time, access_time, data_size, disk_size "
				  "FROM Prelude_File "
				  "WHERE target_ident = %llu AND alert_ident = %llu",
				  target_ident, alert_ident);
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

		if ( get_enum(sql, row, 0, file, idmef_file_new_category, idmef_file_category_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, file, idmef_file_new_name) < 0 )
			goto error;

		if ( get_string(sql, row, 2, file, idmef_file_new_path) < 0 )
			goto error;

		if ( get_timestamp(sql, row, 3, file, idmef_file_new_create_time) < 0 )
			goto error;

		if ( get_timestamp(sql, row, 4, file, idmef_file_new_modify_time) < 0 )
			goto error;

		if ( get_timestamp(sql, row, 5, file, idmef_file_new_access_time) < 0 )
			goto error;

		if ( get_uint32(sql, row, 6, file, idmef_file_new_data_size) < 0 )
			goto error;

		if ( get_uint32(sql, row, 7, file, idmef_file_new_disk_size) < 0 )
			goto error;

		cnt++;
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	file = NULL;

	while ( (file = idmef_target_get_next_file(target, file)) ) {

		if ( get_file_access(sql, alert_ident, target_ident, cnt, file) < 0 )
			goto error;

		if ( get_linkage(sql, alert_ident, target_ident, cnt, file) < 0 )
			goto error;

		if ( get_inode(sql, alert_ident, target_ident, cnt, file) < 0 )
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
		      uint64_t ident,
		      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_source_t *source;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT spoofed, interface "
				  "FROM Prelude_Source "
				  "WHERE alert_ident = %llu",
				  ident);
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

		if ( get_enum(sql, row, 0, source, idmef_source_new_spoofed, idmef_spoofed_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, source, idmef_source_new_interface) < 0 )
			goto error;
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	source = NULL;

	while ( (source = idmef_alert_get_next_source(alert, source)) ) {

		if ( get_node(sql, ident, cnt, 'S', source, (idmef_node_t *(*)(void *)) idmef_source_new_node) < 0 )
			goto error;

		if ( get_user(sql, ident, cnt, 'S', source, (idmef_user_t *(*)(void *)) idmef_source_new_user) < 0 )
			goto error;

		if ( get_process(sql, ident, cnt, 'S', source, (idmef_process_t *(*)(void *)) idmef_source_new_process) < 0 )
			goto error;

		if ( get_service(sql, ident, cnt, 'S', source, (idmef_service_t *(*)(void *)) idmef_source_new_service) < 0 )
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
		      uint64_t ident,
		      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_target_t *target;
	int cnt;

	table = prelude_sql_query(sql,
				  "SELECT decoy, interface "
				  "FROM Prelude_Target "
				  "WHERE alert_ident = %llu",
				  ident);
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

		if ( get_enum(sql, row, 0, target, idmef_target_new_decoy, idmef_spoofed_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, target, idmef_target_new_interface) < 0 )
			goto error;
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	target = NULL;

	while ( (target = idmef_alert_get_next_target(alert, target)) ) {

		if ( get_node(sql, ident, cnt, 'T', target, (idmef_node_t *(*)(void *)) idmef_target_new_node) < 0 )
			goto error;

		if ( get_user(sql, ident, cnt, 'T', target, (idmef_user_t *(*)(void *)) idmef_target_new_user) < 0 )
			goto error;

		if ( get_process(sql, ident, cnt, 'T', target, (idmef_process_t *(*)(void *)) idmef_target_new_process) < 0 )
			goto error;

		if ( get_service(sql, ident, cnt, 'T', target, (idmef_service_t *(*)(void *)) idmef_target_new_service) < 0 )
			goto error;

		if ( get_file(sql, ident, cnt, target) < 0 )
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
			       uint64_t parent_ident,
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
				  "WHERE parent_type = '%c' AND parent_ident = %llu",
				  parent_type, parent_ident);
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

static int get_classification(prelude_sql_connection_t *sql,
			      uint64_t ident,
			      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_classification_t *classification;
	int cnt = 0;
	
	table = prelude_sql_query(sql,
				  "SELECT origin, name, url "
				  "FROM Prelude_Classification "
				  "WHERE alert_ident = %llu",
				  ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		classification = idmef_alert_new_classification(alert);
		if ( ! classification )
			goto error;

		if ( get_enum(sql, row, 0, classification, idmef_classification_new_origin,
			      idmef_classification_origin_to_numeric) < 0 )
			goto error;

		if ( get_string(sql, row, 1, classification, idmef_classification_new_name) < 0 )
			goto error;

		if ( get_string(sql, row, 2, classification, idmef_classification_new_url) < 0 )
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
			  uint64_t ident,
			  idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_tool_alert_t *tool_alert;

	table = prelude_sql_query(sql,
				  "SELECT name, command "
				  "FROM Prelude_ToolAlert "
				  "WHERE alert_ident = %llu",
				  ident);
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

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;	
}

static int get_correlation_alert_ident(prelude_sql_connection_t *sql,
				       uint64_t ident,
				       idmef_correlation_alert_t *correlation_alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_alertident_t *alertident = NULL;
	int cnt = 0;

	table = prelude_sql_query(sql,
				  "SELECT alert_ident "
				  "FROM Prelude_CorrelationAlert_Alerts "
				  "WHERE ident = %llu",
				  ident);
	if ( ! table ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			goto error;
		}

		return 0;
	}

	while ( (row = prelude_sql_row_fetch(table)) ) {

		alertident = idmef_correlation_alert_new_alertident(correlation_alert);
		if ( ! alertident )
			goto error;

		if ( get_uint64(sql, row, 0, alertident, idmef_alertident_new_alertident) < 0 )
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

static int get_correlation_alert(prelude_sql_connection_t *sql,
				 uint64_t ident,
				 idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_correlation_alert_t *correlation_alert;

	table = prelude_sql_query(sql,
				  "SELECT name "
				  "FROM Prelude_CorrelationAlert "
				  "WHERE ident = %llu",
				  ident);
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

	if ( get_correlation_alert_ident(sql, ident, correlation_alert) < 0 )
		goto error;

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	return -1;
}

static int get_overflow_alert(prelude_sql_connection_t *sql,
			      uint64_t ident,
			      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_overflow_alert_t *overflow_alert;

	table = prelude_sql_query(sql,
				  "SELECT program, size, buffer "
				  "FROM Prelude_OverflowAlert "
				  "WHERE alert_ident = %llu",
				  ident);
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

static int message_exists(prelude_sql_connection_t *sql,
			  const char *table_name, uint64_t ident)
{
	prelude_sql_table_t *table;
	int ret;

	table = prelude_sql_query(sql, "SELECT ident FROM %s WHERE ident = %llu",
				  table_name, ident);

	if ( table ) {
		ret = 1;
		prelude_sql_table_free(table);

	} else {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			ret = -1;

		} else {
			ret = 0;
		}
	}

	return ret;
}



static int alert_exists(prelude_sql_connection_t *sql,
			uint64_t ident)
{
	return message_exists(sql, "Prelude_Alert", ident);
}



static int heartbeat_exists(prelude_sql_connection_t *sql,
			    uint64_t ident)
{
	return message_exists(sql, "Prelude_Heartbeat", ident);
}



idmef_message_t	*get_alert(prelude_db_connection_t *connection,
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

	if ( alert_exists(sql, ident) <= 0 )
		return NULL;

	message = idmef_message_new();
	if ( ! message ) {
		log_memory_exhausted();
		return NULL;
	}

	alert = idmef_message_new_alert(message);
	if ( ! alert )
		goto error;

	idmef_alert_set_ident(alert, ident);

	if ( get_assessment(sql, ident, alert) < 0 )
		goto error;

	if ( get_analyzer(sql, ident, 'A', alert, (idmef_analyzer_t *(*)(void *)) idmef_alert_new_analyzer) < 0 )
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

	if ( heartbeat_exists(sql, ident) <= 0 )
		return NULL;

	message = idmef_message_new();
	if ( ! message ) {
		log_memory_exhausted();
		return NULL;
	}

	heartbeat = idmef_message_new_heartbeat(message);
	if ( ! heartbeat )
		goto error;
	
	idmef_heartbeat_set_ident(heartbeat, ident);

	if ( get_analyzer(sql, ident, 'H', heartbeat, (idmef_analyzer_t *(*)(void *)) idmef_heartbeat_new_analyzer) < 0 )
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
