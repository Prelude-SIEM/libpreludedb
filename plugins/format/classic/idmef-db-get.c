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

#define	get_(type, name)											\
static int get_ ## name (prelude_sql_connection_t *sql, prelude_sql_row_t *row, int index, type *value)		\
{														\
	prelude_sql_field_t *field;										\
														\
	field = prelude_sql_field_fetch(row, index);								\
	if ( ! field ) {											\
		if ( prelude_sql_errno(sql) ) {									\
			db_log(sql);										\
			return -1;										\
		}												\
		*value = (type) 0;										\
	} else													\
		*value = prelude_sql_field_value_ ## name (field);						\
														\
	return 0;												\
}

get_(uint16_t, uint16)
get_(uint32_t, uint32)
get_(uint64_t, uint64)
get_(float, float)

static int get_string(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
				  int index, idmef_string_t **value)
{
	prelude_sql_field_t *field;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		*value = idmef_string_new();
	} else
		*value = prelude_sql_field_value_string(field);

	if ( ! *value ) {
		log_memory_exhausted();
		return -2;
	}

	return 0;
}

static int get_data(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
		    int index, idmef_data_t **value)
{
	prelude_sql_field_t *field;
	const char *tmp;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		*value = idmef_data_new();
		if ( ! *value ) {
			log_memory_exhausted();
			return -2;
		}

		return 0;
	}

	tmp = prelude_sql_field_value(field);
	if ( ! tmp )
		return -3;

	*value = idmef_data_new_dup(tmp, strlen(tmp) + 1);
	if ( ! *value ) {
		log_memory_exhausted();
		return -4;
	}

	return 0;
}

static int get_enum(prelude_sql_connection_t *sql, prelude_sql_row_t *row, 
		    int index, unsigned int *value, int (*convert_enum)(const char *))
{
	prelude_sql_field_t *field;
	const char *tmp;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		*value = 0;

	} else {
		tmp = prelude_sql_field_value(field);
		if ( ! tmp )
			return -2;

		*value = convert_enum(tmp);
	}

	return 0;
}

static int get_ntp_timestamp(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
			     int index, idmef_time_t **time)
{
	prelude_sql_field_t *field;
	const char *tmp;
	unsigned int sec, usec;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		*time = idmef_time_new();
		if ( ! *time ) {
			log_memory_exhausted();
			return -1;
		}

		return 0;
	}

	tmp = prelude_sql_field_value(field);
	if ( ! tmp )
		return -3;

	*time = idmef_time_new_ntp_timestamp(tmp);
	if ( ! *time )
		return -4;

	return 0;
}

static int get_timestamp(prelude_sql_connection_t *sql, prelude_sql_row_t *row,
			 int index, idmef_time_t **time)
{
	prelude_sql_field_t *field;
	const char *tmp;
	struct tm tm;
	unsigned int sec;

	field = prelude_sql_field_fetch(row, index);
	if ( ! field ) {
		if ( prelude_sql_errno(sql) ) {
			db_log(sql);
			return -1;
		}

		*time = idmef_time_new();
		if ( ! *time ) {
			log_memory_exhausted();
			return -2;
		}

		return 0;
	}

	tmp = prelude_sql_field_value(field);
	if ( ! tmp )
		return -3;

	memset(&tm, 0, sizeof (tm));

	if ( sscanf(tmp, "%d-%d-%d %d:%d:%d",
		    &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 6 )
		return -4;

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	sec = mktime(&tm);

	*time = idmef_time_new();
	if ( ! *time ) {
		log_memory_exhausted();
		return -5;
	}

	idmef_time_set_sec(*time, sec);

	return 0;
}

static int get_analyzer_time(prelude_sql_connection_t *sql,
			     uint64_t parent_ident,
			     char parent_type,
			     void *parent,
			     void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_time_t *analyzer_time = NULL;

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

	if ( get_ntp_timestamp(sql, row, 0, &analyzer_time) < 0 )
		goto error;

	prelude_sql_table_free(table);
	parent_set_field(parent, analyzer_time);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( analyzer_time )
		idmef_time_destroy(analyzer_time);

	return -1;
}

static int get_detect_time(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_time_t *detect_time = NULL;

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

	if ( get_ntp_timestamp(sql, row, 0, &detect_time) < 0 )
		goto error;

	prelude_sql_table_free(table);
	idmef_alert_set_detect_time(alert, detect_time);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( detect_time )
		idmef_time_destroy(detect_time);

	return -1;
}

static int get_create_time(prelude_sql_connection_t *sql,
			   uint64_t parent_ident,
			   char parent_type,
			   void *parent,
			   void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_time_t *create_time = NULL;

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

	if ( get_ntp_timestamp(sql, row, 0, &create_time) < 0 )
		goto error;

	prelude_sql_table_free(table);
	parent_set_field(parent, create_time);

	return 0;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( create_time )
		idmef_time_destroy(create_time);

	return -1;
}

static int get_userid(prelude_sql_connection_t *sql,
		      uint64_t alert_ident,
		      uint64_t parent_ident,
		      char parent_type,
		      void *parent,
		      void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_userid_t *userid = NULL;
	idmef_userid_type_t type;
        idmef_string_t *name;
        uint32_t number;
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

		userid = idmef_userid_new();
		if ( ! userid ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &type, idmef_userid_type_to_numeric) < 0 )
			goto error;
		idmef_userid_set_type(userid, type);

		if ( get_string(sql, row, 1, &name) < 0 )
			goto error;
		idmef_userid_set_name(userid, name);

		if ( get_uint32(sql, row, 2, &number) )
			goto error;
		idmef_userid_set_number(userid, number);

		parent_set_field(parent, userid);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( userid )
		idmef_userid_destroy(userid);

	return -1;	
}

static int get_user(prelude_sql_connection_t *sql,
		    uint64_t alert_ident,
		    uint64_t parent_ident,
		    char parent_type,
		    void *parent,
		    void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_user_t *user = NULL;
	idmef_user_category_t category;

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

	user = idmef_user_new();
	if ( ! user ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_enum(sql, row, 0, &category, idmef_user_category_to_numeric) < 0 )
		goto error;
	idmef_user_set_category(user, category);

	prelude_sql_table_free(table);

	if ( get_userid(sql, alert_ident, parent_ident, parent_type, user, (void (*)(void *, void *)) idmef_user_set_userid) < 0 )
		goto error;

	parent_set_field(parent, user);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( user )
		idmef_user_destroy(user);

	return -1;	
}

static int get_process_arg(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   uint64_t parent_ident,
			   char parent_type,
			   void *parent,
			   void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_string_t *arg = NULL;
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

		if ( get_string(sql, row, 0, &arg) < 0 )
			goto error;

		parent_set_field(parent, arg);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( arg )
		idmef_string_destroy(arg);

	return -1;
}

static int get_process_env(prelude_sql_connection_t *sql,
			   uint64_t alert_ident,
			   uint64_t parent_ident,
			   char parent_type,
			   void *parent,
			   void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_string_t *env = NULL;
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

		if ( get_string(sql, row, 0, &env) < 0 )
			goto error;

		parent_set_field(parent, env);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( env )
		idmef_string_destroy(env);

	return -1;
}

static int get_process(prelude_sql_connection_t *sql,
		       uint64_t alert_ident,
		       uint64_t parent_ident,
		       char parent_type,
		       void *parent,
		       void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_process_t *process = NULL;
	idmef_string_t *name;
        uint32_t pid;
        idmef_string_t *path;

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

	process = idmef_process_new();
	if ( ! process ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &name) < 0 )
		goto error;
	idmef_process_set_name(process, name);

	if ( get_uint32(sql, row, 1, &pid) < 0 )
		goto error;
	idmef_process_set_pid(process, pid);

	if ( get_string(sql, row, 2, &path) < 0 )
		goto error;
	idmef_process_set_path(process, path);

	prelude_sql_table_free(table);

	if ( get_process_arg(sql, alert_ident, parent_ident, parent_type, process, (void (*)(void *, void *)) idmef_process_set_arg) < 0 )
		goto error;
	
	if ( get_process_env(sql, alert_ident, parent_ident, parent_type, process, (void (*)(void *, void *)) idmef_process_set_env) < 0 )
		goto error;
	
	parent_set_field(parent, process);

	return 1;
	
 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( process )
		idmef_process_destroy(process);

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
	idmef_string_t *arg = NULL;
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

		if ( get_string(sql, row, 0, &arg) < 0 )
			goto error;

		idmef_webservice_set_arg(webservice, arg);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( arg )
		idmef_string_destroy(arg);

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
	idmef_webservice_t *webservice = NULL;
	idmef_string_t *url;
        idmef_string_t *cgi;
        idmef_string_t *http_method;

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

	webservice = idmef_webservice_new();
	if ( ! webservice ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &url) < 0 )
		goto error;
	idmef_webservice_set_url(webservice, url);

	if ( get_string(sql, row, 1, &cgi) < 0 )
		goto error;
	idmef_webservice_set_cgi(webservice, cgi);

	if ( get_string(sql, row, 2, &http_method) < 0 )
		goto error;
	idmef_webservice_set_http_method(webservice, http_method);

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_webservice_arg(sql, alert_ident, parent_ident, parent_type, webservice) < 0 )
		goto error;

	idmef_service_set_web(service, webservice);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( webservice )
		idmef_webservice_destroy(webservice);

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
	idmef_snmpservice_t *snmpservice = NULL;
	idmef_string_t *oid;
        idmef_string_t *community;
        idmef_string_t *command;

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

	snmpservice = idmef_snmpservice_new();
	if ( ! snmpservice ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &oid) < 0 )
		goto error;
	idmef_snmpservice_set_oid(snmpservice, oid);

	if ( get_string(sql, row, 1, &community) < 0 )
		goto error;
	idmef_snmpservice_set_community(snmpservice, community);

	if ( get_string(sql, row, 2, &command) < 0 )
		goto error;
	idmef_snmpservice_set_command(snmpservice, command);

	prelude_sql_table_free(table);
	idmef_service_set_snmp(service, snmpservice);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( snmpservice )
		idmef_snmpservice_destroy(snmpservice);

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
	idmef_string_t *portlist = NULL;

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

	if ( get_string(sql, row, 0, &portlist) < 0 )
		goto error;
	idmef_service_set_portlist(service, portlist);

	prelude_sql_table_free(table);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( portlist )
		idmef_string_destroy(portlist);

	return -1;
}

static int get_service(prelude_sql_connection_t *sql,
		       uint64_t alert_ident,
		       uint64_t parent_ident,
		       char parent_type,
		       void *parent,
		       void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_service_t *service = NULL;
	idmef_string_t *name;
        uint16_t port;
        idmef_string_t *protocol;

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

	service = idmef_service_new();
	if ( ! service ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &name) < 0 )
		goto error;
	idmef_service_set_name(service, name);

	if ( get_uint16(sql, row, 1, &port) < 0 )
		goto error;
	idmef_service_set_port(service, port);

	if ( get_string(sql, row, 2, &protocol) < 0 )
		goto error;
	idmef_service_set_protocol(service, protocol);

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_service_portlist(sql, alert_ident, parent_ident, parent_type, service) < 0 )
		goto error;

	if ( get_webservice(sql, alert_ident, parent_ident, parent_type, service) < 0 )
		goto error;

	if ( get_snmpservice(sql, alert_ident, parent_ident, parent_type, service) < 0 )
		goto error;

	parent_set_field(parent, service);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( service )
		idmef_service_destroy(service);

	return -1;
}

static int get_address(prelude_sql_connection_t *sql,
		       uint64_t alert_ident,
		       uint64_t parent_ident,
		       char parent_type,
		       void *parent,
		       void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_address_t *idmef_address = NULL;
	idmef_address_category_t category;
        idmef_string_t *vlan_name;
        uint32_t vlan_num;
        idmef_string_t *address;
        idmef_string_t *netmask;
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

		idmef_address = idmef_address_new();
		if ( ! idmef_address ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &category, idmef_address_category_to_numeric) < 0 )
			goto error;
		idmef_address_set_category(idmef_address, category);

		if ( get_string(sql, row, 1, &vlan_name) < 0 )
			goto error;
		idmef_address_set_vlan_name(idmef_address, vlan_name);

		if ( get_uint32(sql, row, 2, &vlan_num) < 0 )
			goto error;
		idmef_address_set_vlan_num(idmef_address, vlan_num);

		if ( get_string(sql, row, 3, &address) < 0 )
			goto error;
		idmef_address_set_address(idmef_address, address);

		if ( get_string(sql, row, 4, &netmask) < 0 )
			goto error;
		idmef_address_set_netmask(idmef_address, netmask);

		parent_set_field(parent, idmef_address);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( idmef_address )
		idmef_address_destroy(idmef_address);

	return -1;
}

static int get_node(prelude_sql_connection_t *sql,
		    uint64_t alert_ident,
		    uint64_t parent_ident,
		    char parent_type,
		    void *parent,
		    void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_node_t *node = NULL;
	idmef_node_category_t category;
        idmef_string_t *location;
        idmef_string_t *name;

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

	node = idmef_node_new();
	if ( ! node ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_enum(sql, row, 0, &category, idmef_node_category_to_numeric) < 0 )
		goto error;
	idmef_node_set_category(node, category);

	if ( get_string(sql, row, 1, &location) < 0 )
		goto error;
	idmef_node_set_location(node, location);

	if ( get_string(sql, row, 2, &name) < 0 )
		goto error;
	idmef_node_set_name(node, name);

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_address(sql, alert_ident, parent_ident, parent_type, node, (void (*)(void *, void *)) idmef_node_set_address) < 0 )
		goto error;

	parent_set_field(parent, node);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( node )
		idmef_node_destroy(node);

	return -1;
}

static int get_analyzer(prelude_sql_connection_t *sql,
			uint64_t ident,
			char parent_type,
			void *parent,
			void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_analyzer_t *analyzer = NULL;
	uint64_t analyzerid;
        idmef_string_t *manufacturer;
        idmef_string_t *model;
        idmef_string_t *version;
        idmef_string_t *class;
        idmef_string_t *ostype;
        idmef_string_t *osversion;

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

	analyzer = idmef_analyzer_new();
	if ( ! analyzer ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_uint64(sql, row, 0, &analyzerid) < 0 )
		goto error;
	idmef_analyzer_set_analyzerid(analyzer, analyzerid);

	if ( get_string(sql, row, 1, &manufacturer) < 0 )
		goto error;
	idmef_analyzer_set_manufacturer(analyzer, manufacturer);

	if ( get_string(sql, row, 2, &model) < 0 )
		goto error;
	idmef_analyzer_set_model(analyzer, model);

	if ( get_string(sql, row, 3, &version) < 0 )
		goto error;
	idmef_analyzer_set_version(analyzer, version);

	if ( get_string(sql, row, 4, &class) < 0 )
		goto error;
	idmef_analyzer_set_class(analyzer, class);

	if ( get_string(sql, row, 5, &ostype) < 0 )
		goto error;
	idmef_analyzer_set_ostype(analyzer, ostype);

	if ( get_string(sql, row, 6, &osversion) < 0 )
		goto error;
	idmef_analyzer_set_osversion(analyzer, osversion);

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_node(sql, ident, 0, parent_type, analyzer, (void (*)(void *, void *)) idmef_analyzer_set_node) < 0 )
		goto error;

	if ( get_process(sql, ident, 0, parent_type, analyzer, (void (*)(void *, void *)) idmef_analyzer_set_process) < 0 )
		goto error;

	parent_set_field(parent, analyzer);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( analyzer )
		idmef_analyzer_destroy(analyzer);

	return -1;
}

static int get_action(prelude_sql_connection_t *sql,
		      uint64_t alert_ident,
		      idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_action_t *action = NULL;
	idmef_action_category_t category;
	idmef_string_t *description;
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

		action = idmef_action_new();
		if ( ! action ) {
			log_memory_exhausted();
			goto error;
		}
		
		if ( get_enum(sql, row, 0, &category, idmef_action_category_to_numeric) < 0 )
			goto error;
		idmef_action_set_category(action, category);

		if ( get_string(sql, row, 1, &description) < 0 )
			goto error;
		idmef_action_set_description(action, description);

		idmef_assessment_set_action(assessment, action);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( action )
		idmef_action_destroy(action);

	return -1;
	
}

static int get_confidence(prelude_sql_connection_t *sql,
			  uint64_t alert_ident,
			  idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_confidence_t *idmef_confidence = NULL;
	idmef_confidence_rating_t rating;
        float confidence;

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

	idmef_confidence = idmef_confidence_new();
	if ( ! idmef_confidence ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_enum(sql, row, 0, &rating, idmef_confidence_rating_to_numeric) < 0 )
		goto error;
	idmef_confidence_set_rating(idmef_confidence, rating);

	if ( get_float(sql, row, 1, &confidence) < 0 )
		goto error;
	idmef_confidence_set_confidence(idmef_confidence, confidence);

	prelude_sql_table_free(table);
	idmef_assessment_set_confidence(assessment, idmef_confidence);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( idmef_confidence )
		idmef_confidence_destroy(idmef_confidence);

	return -1;
}

static int get_impact(prelude_sql_connection_t *sql, 
		      uint64_t alert_ident, 
		      idmef_assessment_t *assessment)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_impact_t *impact = NULL;
	idmef_impact_severity_t severity;
        idmef_impact_completion_t completion;
        idmef_impact_type_t type;
        idmef_string_t *description;

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

	impact = idmef_impact_new();
	if ( ! impact ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_enum(sql, row, 0, &severity, idmef_impact_severity_to_numeric) < 0  )
		goto error;
	idmef_impact_set_severity(impact, severity);
	

	if ( get_enum(sql, row, 1, &completion, idmef_impact_completion_to_numeric) < 0 )
		goto error;
	idmef_impact_set_completion(impact, completion);


	if ( get_enum(sql, row, 2, &type, idmef_impact_type_to_numeric) < 0 )
		goto error;
	idmef_impact_set_type(impact, type);

	if ( get_string(sql, row, 3, &description) < 0 )
		goto error;
	idmef_impact_set_description(impact, description);

	prelude_sql_table_free(table);
	idmef_assessment_set_impact(assessment, impact);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( impact )
		idmef_impact_destroy(impact);

	return -1;
}

static int get_assessment(prelude_sql_connection_t *sql, 
			  uint64_t ident,
			  idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	idmef_assessment_t *assessment = NULL;

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

	assessment = idmef_assessment_new();
	if ( ! assessment ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_impact(sql, ident, assessment) < 0 )
		goto error;

	if ( get_confidence(sql, ident, assessment) < 0 )
		goto error;

	if ( get_action(sql, ident, assessment) < 0 )
		goto error;

	idmef_alert_set_assessment(alert, assessment);

	return 1;

 error:
	if ( assessment )
		idmef_assessment_destroy(assessment);

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
	idmef_file_access_t *file_access = NULL;
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

		file_access = idmef_file_access_new();
		if ( ! file_access ) {
			log_memory_exhausted();
			goto error;
		}

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

	if ( file_access )
		idmef_file_access_destroy(file_access);

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
	idmef_linkage_t *linkage = NULL;
	idmef_linkage_category_t category;
        idmef_string_t *name;
        idmef_string_t *path;
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

		linkage = idmef_linkage_new();
		if ( ! linkage ) {
			log_memory_exhausted();
			return -1;
		}

		if ( get_enum(sql, row, 0, &category, idmef_linkage_category_to_numeric) < 0 )
			goto error;
		idmef_linkage_set_category(linkage, category);

		if ( get_string(sql, row, 1, &name) < 0 )
			goto error;
		idmef_linkage_set_name(linkage, name);

		if ( get_string(sql, row, 2, &path) < 0 )
			goto error;
		idmef_linkage_set_path(linkage, path);

		idmef_file_set_file_linkage(file, linkage);
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

	if ( linkage )
		idmef_linkage_destroy(linkage);

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
	idmef_inode_t *inode = NULL;
	idmef_time_t *change_time;
        uint32_t number;
        uint32_t major_device;
        uint32_t minor_device;
        uint32_t c_major_device;
        uint32_t c_minor_device;

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

	inode = idmef_inode_new();
	if ( ! inode ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_timestamp(sql, row, 0, &change_time) < 0 )
		goto error;
	idmef_inode_set_change_time(inode, change_time);

	if ( get_uint32(sql, row, 1, &number) < 0 )
		goto error;
	idmef_inode_set_number(inode, number);

	if ( get_uint32(sql, row, 2, &major_device) < 0 )
		goto error;
	idmef_inode_set_major_device(inode, major_device);

	if ( get_uint32(sql, row, 3, &minor_device) < 0 )
		goto error;
	idmef_inode_set_minor_device(inode, minor_device);

	if ( get_uint32(sql, row, 4, &c_major_device) < 0 )
		goto error;
	idmef_inode_set_c_major_device(inode, c_major_device);

	if ( get_uint32(sql, row, 5, &c_minor_device) < 0 )
		goto error;
	idmef_inode_set_c_minor_device(inode, c_minor_device);

	prelude_sql_table_free(table);
	idmef_file_set_inode(file, inode);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( inode )
		idmef_inode_destroy(inode);

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
	idmef_file_category_t category;
        idmef_string_t *name;
        idmef_string_t *path;
        idmef_time_t *create_time;
        idmef_time_t *modify_time;
        idmef_time_t *access_time;
        uint32_t data_size;
        uint32_t disk_size;
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

		file = idmef_file_new();
		if ( ! file ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &category, idmef_file_category_to_numeric) < 0 )
			goto error;
		idmef_file_set_category(file, category);

		if ( get_string(sql, row, 1, &name) < 0 )
			goto error;
		idmef_file_set_name(file, name);

		if ( get_string(sql, row, 2, &path) < 0 )
			goto error;
		idmef_file_set_path(file, path);

		if ( get_timestamp(sql, row, 3, &create_time) < 0 )
			goto error;
		idmef_file_set_create_time(file, create_time);

		if ( get_timestamp(sql, row, 4, &modify_time) < 0 )
			goto error;
		idmef_file_set_modify_time(file, modify_time);

		if ( get_timestamp(sql, row, 5, &access_time) < 0 )
			goto error;
		idmef_file_set_access_time(file, access_time);

		if ( get_uint32(sql, row, 6, &data_size) < 0 )
			goto error;
		idmef_file_set_data_size(file, data_size);

		if ( get_uint32(sql, row, 7, &disk_size) < 0 )
			goto error;
		idmef_file_set_disk_size(file, disk_size);

		idmef_target_set_file(target, file);

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

	if ( file )
		idmef_file_destroy(file);

	return -1;
}

static int get_source(prelude_sql_connection_t *sql,
		      uint64_t ident,
		      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_source_t *source = NULL;
	idmef_spoofed_t spoofed;
	idmef_string_t *interface;
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

		source = idmef_source_new();
		if ( ! source ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &spoofed, idmef_spoofed_to_numeric) < 0 )
			goto error;
		idmef_source_set_spoofed(source, spoofed);

		if ( get_string(sql, row, 1, &interface) < 0 )
			goto error;
		idmef_source_set_interface(source, interface);

		idmef_alert_set_source(alert, source);
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	source = NULL;

	while ( (source = idmef_alert_get_next_source(alert, source)) ) {

		if ( get_node(sql, ident, cnt, 'S', source, (void (*)(void *, void *)) idmef_source_set_node) < 0 )
			goto error;

		if ( get_user(sql, ident, cnt, 'S', source, (void (*)(void *, void *)) idmef_source_set_user) < 0 )
			goto error;

		if ( get_process(sql, ident, cnt, 'S', source, (void (*)(void *, void *)) idmef_source_set_process) < 0 )
			goto error;

		if ( get_service(sql, ident, cnt, 'S', source, (void (*)(void *, void *)) idmef_source_set_service) < 0 )
			goto error;

		cnt++;
	}

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( source )
		idmef_source_destroy(source);

	return -1;
}

static int get_target(prelude_sql_connection_t *sql,
		      uint64_t ident,
		      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_target_t *target = NULL;
	idmef_spoofed_t decoy;
	idmef_string_t *interface;
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

		target = idmef_target_new();
		if ( ! target ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &decoy, idmef_spoofed_to_numeric) < 0 )
			goto error;
		idmef_target_set_decoy(target, decoy);

		if ( get_string(sql, row, 1, &interface) < 0 )
			goto error;
		idmef_target_set_interface(target, interface);

		idmef_alert_set_target(alert, target);
	}

	prelude_sql_table_free(table);
	table = NULL;

	cnt = 0;
	target = NULL;

	while ( (target = idmef_alert_get_next_target(alert, target)) ) {

		if ( get_node(sql, ident, cnt, 'T', target, (void (*)(void *, void *)) idmef_target_set_node) < 0 )
			goto error;

		if ( get_user(sql, ident, cnt, 'T', target, (void (*)(void *, void *)) idmef_target_set_user) < 0 )
			goto error;

		if ( get_process(sql, ident, cnt, 'T', target, (void (*)(void *, void *)) idmef_target_set_process) < 0 )
			goto error;

		if ( get_service(sql, ident, cnt, 'T', target, (void (*)(void *, void *)) idmef_target_set_service) < 0 )
			goto error;

		if ( get_file(sql, ident, cnt, target) < 0 )
			goto error;

		cnt++;
	}

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( target )
		idmef_target_destroy(target);

	return -1;
}

static int get_additional_data(prelude_sql_connection_t *sql,
			       uint64_t parent_ident,
			       char parent_type,
			       void *parent,
			       void (*parent_set_field)(void *, void *))
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_additional_data_t *additional_data = NULL;
	idmef_additional_data_type_t type;
        idmef_string_t *meaning;
	idmef_data_t *data;
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

		additional_data = idmef_additional_data_new();
		if ( ! additional_data ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &type, idmef_additional_data_type_to_numeric) < 0 )
			goto error;
		idmef_additional_data_set_type(additional_data, type);

		if ( get_string(sql, row, 1, &meaning) < 0 )
			goto error;
		idmef_additional_data_set_meaning(additional_data, meaning);

		if ( get_data(sql, row, 2, &data) < 0 )
			goto error;
		idmef_additional_data_set_data(additional_data, data);

		parent_set_field(parent, additional_data);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( additional_data )
		idmef_additional_data_destroy(additional_data);

	return -1;
}

static int get_classification(prelude_sql_connection_t *sql,
			      uint64_t ident,
			      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_classification_t *classification = NULL;
	idmef_classification_origin_t origin;
        idmef_string_t *name;
        idmef_string_t *url;
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

		classification = idmef_classification_new();
		if ( ! classification ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_enum(sql, row, 0, &origin, idmef_classification_origin_to_numeric) < 0 )
			goto error;
		idmef_classification_set_origin(classification, origin);

		if ( get_string(sql, row, 1, &name) < 0 )
			goto error;
		idmef_classification_set_name(classification, name);

		if ( get_string(sql, row, 2, &url) < 0 )
			goto error;
		idmef_classification_set_url(classification, url);

		idmef_alert_set_classification(alert, classification);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( classification )
		idmef_classification_destroy(classification);

	return -1;
}

static int get_tool_alert(prelude_sql_connection_t *sql,
			  uint64_t ident,
			  idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_tool_alert_t *tool_alert = NULL;
	idmef_string_t *name;
        idmef_string_t *command;

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

	tool_alert = idmef_tool_alert_new();
	if ( ! tool_alert ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &name) < 0 )
		goto error;
	idmef_tool_alert_set_name(tool_alert, name);

	if ( get_string(sql, row, 1, &command) < 0 )
		goto error;
	idmef_tool_alert_set_command(tool_alert, command);

	prelude_sql_table_free(table);
	idmef_alert_set_tool_alert(alert, tool_alert);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( tool_alert )
		idmef_tool_alert_destroy(tool_alert);

	return -1;	
}

static int get_correlation_alert_ident(prelude_sql_connection_t *sql,
				       uint64_t ident,
				       idmef_correlation_alert_t *correlation_alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_alertident_t *idmef_alertident = NULL;
	uint64_t alert_ident;
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

		idmef_alertident = idmef_alertident_new();
		if ( ! idmef_alertident ) {
			log_memory_exhausted();
			goto error;
		}

		if ( get_uint64(sql, row, 0, &alert_ident) < 0 )
			goto error;
		idmef_alertident_set_alertident(idmef_alertident, alert_ident);

		idmef_correlation_alert_set_alertident(correlation_alert, idmef_alertident);

		cnt++;
	}

	prelude_sql_table_free(table);

	return cnt;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( idmef_alertident )
		idmef_alertident_destroy(idmef_alertident);

	return -1;	
}

static int get_correlation_alert(prelude_sql_connection_t *sql,
				 uint64_t ident,
				 idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_correlation_alert_t *correlation_alert = NULL;
	idmef_string_t *name;

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

	correlation_alert = idmef_correlation_alert_new();
	if ( ! correlation_alert ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &name) < 0 )
		goto error;
	idmef_correlation_alert_set_name(correlation_alert, name);

	prelude_sql_table_free(table);
	table = NULL;

	if ( get_correlation_alert_ident(sql, ident, correlation_alert) < 0 )
		goto error;

	idmef_alert_set_correlation_alert(alert, correlation_alert);

	return 1;

 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( correlation_alert )
		idmef_correlation_alert_destroy(correlation_alert);

	return -1;
}

static int get_overflow_alert(prelude_sql_connection_t *sql,
			      uint64_t ident,
			      idmef_alert_t *alert)
{
	prelude_sql_table_t *table;
	prelude_sql_row_t *row;
	idmef_overflow_alert_t *overflow_alert = NULL;
	idmef_string_t *program;
        uint32_t size;
        idmef_data_t *buffer;

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

	overflow_alert = idmef_overflow_alert_new();
	if ( ! overflow_alert ) {
		log_memory_exhausted();
		goto error;
	}

	if ( get_string(sql, row, 0, &program) < 0 )
		goto error;
	idmef_overflow_alert_set_program(overflow_alert, program);

	if ( get_uint32(sql, row, 1, &size) < 0 )
		goto error;
	idmef_overflow_alert_set_size(overflow_alert, size);

	if ( get_data(sql, row, 2, &buffer) < 0 )
		goto error;
	idmef_overflow_alert_set_buffer(overflow_alert, buffer);

	prelude_sql_table_free(table);
	idmef_alert_set_overflow_alert(alert, overflow_alert);

	return 1;
	
 error:
	if ( table )
		prelude_sql_table_free(table);

	if ( overflow_alert )
		idmef_overflow_alert_destroy(overflow_alert);

	return -1;
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

	message = idmef_message_new();
	if ( ! message ) {
		log_memory_exhausted();
		return NULL;
	}

	alert = idmef_alert_new();
	if ( ! alert ) {
		log_memory_exhausted();
		goto error;
	}
	
	idmef_message_set_alert(message, alert);
	idmef_alert_set_ident(alert, ident);

	if ( get_assessment(sql, ident, alert) < 0 )
		goto error;

	if ( get_analyzer(sql, ident, 'A', alert, (void (*)(void *, void *)) idmef_alert_set_analyzer) < 0 )
		goto error;

	if ( get_create_time(sql, ident, 'A', alert, (void (*)(void *, void *)) idmef_alert_set_create_time) < 0 )
		goto error;

	if ( get_detect_time(sql, ident, alert) < 0 )
		goto error;

	if ( get_analyzer_time(sql, ident, 'A', alert, (void (*)(void *, void *)) idmef_alert_set_analyzer_time) < 0 )
		goto error;

	if ( get_source(sql, ident, alert) < 0 )
		goto error;

	if ( get_target(sql, ident, alert) < 0 )
		goto error;

	if ( get_classification(sql, ident, alert) < 0 )
		goto error;

	if ( get_additional_data(sql, ident, 'A', alert, (void (*)(void *, void *)) idmef_alert_set_additional_data) < 0 )
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

	message = idmef_message_new();
	if ( ! message ) {
		log_memory_exhausted();
		return NULL;
	}

	heartbeat = idmef_heartbeat_new();
	if ( ! heartbeat ) {
		log_memory_exhausted();
		goto error;
	}
	
	idmef_message_set_heartbeat(message, heartbeat);
	idmef_heartbeat_set_ident(heartbeat, ident);

	if ( get_analyzer(sql, ident, 'H', heartbeat, (void (*)(void *, void *)) idmef_heartbeat_set_analyzer) < 0 )
		goto error;

	if ( get_create_time(sql, ident, 'H', heartbeat, (void (*)(void *, void *)) idmef_heartbeat_set_create_time) < 0 )
		goto error;

	if ( get_analyzer_time(sql, ident, 'H', heartbeat, (void (*)(void *, void *)) idmef_heartbeat_set_analyzer_time) < 0 )
		goto error;

	if ( get_additional_data(sql, ident, 'H', heartbeat, (void (*)(void *, void *)) idmef_heartbeat_set_additional_data) < 0 )
		goto error;

	return message;

 error:
	if ( message )
		idmef_message_destroy(message);

	return NULL;
}
