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
#include "libmissing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>
#include <libprelude/prelude-plugin.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "classic-insert.h"


static inline const char *get_string(prelude_string_t *string)
{
        const char *s;

        if ( ! string )
                return NULL;
        
        s = prelude_string_get_string(string);

        return s ? s : "";
}


static inline int get_data(preludedb_sql_t *sql, idmef_data_t *data, char **output)
{
        int ret;
        prelude_string_t *string;

	switch ( idmef_data_get_type(data) ) {
        case IDMEF_DATA_TYPE_BYTE:
        case IDMEF_DATA_TYPE_BYTE_STRING:
                return preludedb_sql_escape_binary(sql, idmef_data_get_data(data), idmef_data_get_len(data), output);
                
        case IDMEF_DATA_TYPE_CHAR_STRING:
                return preludedb_sql_escape_binary(sql, idmef_data_get_data(data), idmef_data_get_len(data) - 1, output);

        case IDMEF_DATA_TYPE_CHAR:
		return preludedb_sql_escape_binary(sql, idmef_data_get_data(data), 1, output);
                
	default:
		ret = prelude_string_new(&string);
		if ( ret < 0 )
			return ret;

		ret = idmef_data_to_string(data, string);
		if (  ret < 0 ) {
			prelude_string_destroy(string);
			return ret;
		}

		ret = preludedb_sql_escape_binary(sql, (const unsigned char *) prelude_string_get_string(string),
                                                  prelude_string_get_len(string), output);
		prelude_string_destroy(string);

		return ret;
	}
}



static inline char *get_optional_enum(int *value, char *(*convert_func)(int))
{
	if ( ! value )
		return NULL;

	return convert_func(*value);
}



#define get_optional_integer(name, type, format)			        \
static inline void get_optional_ ## name(char *dst, size_t size, type *value)	\
{									        \
	if ( ! value )							        \
		strncpy(dst, "NULL", size);					\
	else								        \
		snprintf(dst, size, format, *value);				\
}


/*
 * %hh convertion specifier is not portable.
 */
static inline void get_optional_uint8(char *dst, size_t size, uint8_t *value)
{        
        if ( ! value )
                strncpy(dst, "NULL", size);
        else {
                unsigned int tmp = (unsigned int) *value;
                snprintf(dst, size, "%u", tmp);
        }
}

get_optional_integer(uint16, uint16_t, "%hu")
get_optional_integer(int32, int32_t, "%d")
get_optional_integer(uint32, uint32_t, "%u")
get_optional_integer(uint64, uint64_t, "%" PRELUDE_PRIu64)
get_optional_integer(float, float, "%f")


static int insert_address(preludedb_sql_t *sql,
			  char parent_type, uint64_t message_ident, int parent_index, int address_index,
			  idmef_address_t *address)
{
        int ret;
        char *vlan_name, vlan_num[16], *addr, *netmask, *category, *ident;

        if ( ! address )
                return 0;

        ret = preludedb_sql_escape(sql, idmef_address_category_to_string(idmef_address_get_category(address)), &category);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_address_get_ident(address)), &ident);
        if ( ret < 0 ) {
                free(category);
                return ret;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_address_get_address(address)), &addr);
        if ( ret < 0 ) {
                free(ident);
                free(category);
                return ret;
        }

        ret = preludedb_sql_escape(sql, get_string(idmef_address_get_netmask(address)), &netmask);
        if ( ret < 0 ) {
                free(ident);
                free(addr);
                free(category);
                return ret;
        }

        ret = preludedb_sql_escape(sql, get_string(idmef_address_get_vlan_name(address)), &vlan_name);
        if ( ret < 0 ) {
                free(ident);
                free(addr);
                free(netmask);
                free(category);
                return ret;
        }

	get_optional_int32(vlan_num, sizeof(vlan_num), idmef_address_get_vlan_num(address));

        ret = preludedb_sql_insert(sql, "Prelude_Address",
				   "_parent_type, _message_ident, _parent0_index, _index,"
				   "ident, category, vlan_name, vlan_num, address, netmask",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %d, %s, %s, %s, %s, %s, %s",
				   parent_type, message_ident, parent_index, address_index,
				   ident, category, vlan_name, vlan_num, addr, netmask);

        free(ident);
        free(addr);
        free(netmask);
        free(category);
        free(vlan_name);

        return ret;
}



static int insert_node(preludedb_sql_t *sql,
		       char parent_type, uint64_t message_ident, int parent_index,
                       idmef_node_t *node)
{
        int ret;
        idmef_address_t *address, *last_address;
        char *location, *name, *category, *ident;
	int index;

        if ( ! node )
                return 0;

        ret = preludedb_sql_escape(sql, idmef_node_category_to_string(idmef_node_get_category(node)), &category);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_node_get_ident(node)), &ident);
        if ( ret < 0 ) {
                free(category);
                return ret;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_node_get_name(node)), &name);
        if ( ret < 0 ) {
                free(ident);
                free(category);
                return ret;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_node_get_location(node)), &location);
        if ( ret < 0 ) {
                free(name);
                free(ident);
                free(category);
                return -1;
        }
        
        ret = preludedb_sql_insert(sql, "Prelude_Node",
				   "_parent_type, _message_ident, _parent0_index, "
				   "ident, category, location, name",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s, %s",
                                   parent_type, message_ident, parent_index,
				   ident, category, location, name);

        free(name);
        free(ident);
        free(location);
        free(category);
        
        if ( ret < 0 )
                return ret;

	index = 0;
        last_address = address = NULL;
        while ( (address = idmef_node_get_next_address(node, address)) ) {

                ret = insert_address(sql, parent_type, message_ident, parent_index, index++, address);
		if ( ret < 0 )
                        return ret;

		last_address = address;
        }

	if ( last_address ) {
		ret = insert_address(sql, parent_type, message_ident, parent_index, -1, last_address);
		if ( ret < 0 )
			return ret;
	}

        return 0;
}



static int insert_user_id(preludedb_sql_t *sql,
			  char parent_type, uint64_t message_ident,
			  int parent_index, int file_index, int file_access_index, int index,
			  idmef_user_id_t *user_id) 
{
        int ret;
        char *name, *type, *ident, *tty, number[16];

        ret = preludedb_sql_escape(sql, get_string(idmef_user_id_get_ident(user_id)), &ident);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_user_id_get_tty(user_id)), &tty);
        if ( ret < 0 ) {
                free(ident);
                return ret;
        }
        
        ret = preludedb_sql_escape(sql, idmef_user_id_type_to_string(idmef_user_id_get_type(user_id)), &type);
        if ( ret < 0 ) {
                free(tty);
                free(ident);
                return ret;
        }

        ret = preludedb_sql_escape(sql, get_string(idmef_user_id_get_name(user_id)), &name);
        if ( ret < 0 ) {
                free(tty);
                free(type);
                free(ident);
                return ret;
        }

	get_optional_uint32(number, sizeof(number), idmef_user_id_get_number(user_id));
        
        ret = preludedb_sql_insert(sql, "Prelude_UserId",
				   "_parent_type, _message_ident, _parent0_index, _parent1_index, _parent2_index, _index, "
				   "ident, type, name, number, tty",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %d, %d, %d, %s, %s, %s, %s, %s",
				   parent_type, message_ident, parent_index, file_index, file_access_index, index,
				   ident, type, name, number, tty);

        free(tty);
        free(type);
        free(name);
        free(ident);

        return ret;
}



static int insert_user(preludedb_sql_t *sql, char parent_type, uint64_t message_ident, int parent_index,
		       idmef_user_t *user) 
{
        char *category, *ident;
        idmef_user_id_t *user_id, *last_user_id;
	int index;
        int ret;
        
        if ( ! user )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_user_get_ident(user)), &ident);
        if ( ret < 0 )
                return ret;
        
        ret = preludedb_sql_escape(sql, idmef_user_category_to_string(idmef_user_get_category(user)), &category);
        if ( ret < 0 ) {
                free(ident);
                return ret;
        }
        
        ret = preludedb_sql_insert(sql, "Prelude_User",
				   "_parent_type, _message_ident, _parent0_index, "
				   "ident, category",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s",
				   parent_type, message_ident, parent_index,
				   ident, category);

        free(ident);
        free(category);
        
        if ( ret < 0 )
                return ret;

	index = 0;
        last_user_id = user_id = NULL;
        while ( (user_id = idmef_user_get_next_user_id(user, user_id)) ) {

                ret = insert_user_id(sql, parent_type, message_ident, parent_index, 0, 0, index++, user_id);
		if ( ret < 0 )
                        return ret;

		last_user_id = user_id;
        }

	if ( last_user_id ) {
		ret = insert_user_id(sql, parent_type, message_ident, parent_index, 0, 0, -1, last_user_id);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



static int insert_process_arg(preludedb_sql_t *sql,
			      char parent_type, uint64_t message_ident, int parent_index, int arg_index,
			      prelude_string_t *arg)
{
	char *tmp;
	int ret;

	ret = preludedb_sql_escape(sql, get_string(arg), &tmp);
	if ( ret < 0 )
		return ret;
                
	ret = preludedb_sql_insert(sql, "Prelude_ProcessArg",
				   "_parent_type, _message_ident, _parent0_index, _index, arg",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %d, %s",
				   parent_type, message_ident, parent_index, arg_index,
				   tmp);

	free(tmp);

	return ret;	
}



static int insert_process_env(preludedb_sql_t *sql,
			      char parent_type, uint64_t message_ident, int parent_index, int env_index,
			      prelude_string_t *env)
{
	char *tmp;
	int ret;

	ret = preludedb_sql_escape(sql, get_string(env), &tmp);
	if ( ret < 0 )
		return ret;
                
	ret = preludedb_sql_insert(sql, "Prelude_ProcessEnv",
				   "_parent_type, _message_ident, _parent0_index, _index, arg",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %d, %s",
				   parent_type, message_ident, parent_index, env_index,
				   tmp);

	free(tmp);

	return ret;
}



static int insert_process(preludedb_sql_t *sql, char parent_type, uint64_t message_ident, int parent_index,
                          idmef_process_t *process) 
{
        prelude_string_t *process_arg, *last_process_arg;
        prelude_string_t *process_env, *last_process_env;
        char *name, *path, pid[16], *ident;
	int index;
        int ret;

        if ( ! process )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_process_get_ident(process)), &ident);
        if ( ret < 0 )
                return ret;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_process_get_name(process)), &name);
        if ( ret < 0 ) {
                free(ident);
                return ret;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_process_get_path(process)), &path);
        if ( ret < 0 ) {
                free(ident);
                free(name);
                return ret;
        }

	get_optional_uint32(pid, sizeof(pid), idmef_process_get_pid(process));

        ret = preludedb_sql_insert(sql, "Prelude_Process",
				   "_parent_type, _message_ident, _parent0_index, ident, name, pid, path",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s, %s", 
				   parent_type, message_ident, parent_index,
				   ident, name, pid, path);

        free(name);
        free(path);
        free(ident);

        if ( ret < 0 )
                return ret;

	index = 0;
        last_process_arg = process_arg = NULL;
        while ( (process_arg = idmef_process_get_next_arg(process, process_arg)) ) {
		ret = insert_process_arg(sql, parent_type, message_ident, parent_index, index++, process_arg);
		if ( ret < 0 )
			return ret;

		last_process_arg = process_arg;
        }

	index = 0;
        last_process_env = process_env = NULL;
        while ( (process_env = idmef_process_get_next_env(process, process_env)) ) {
		ret = insert_process_env(sql, parent_type, message_ident, parent_index, index++, process_env);
		if ( ret < 0 )
			return ret;

		last_process_env = process_env;
        }
	
        return 1;
}



static int insert_snmp_service(preludedb_sql_t *sql, char parent_type, uint64_t message_ident, int parent_index,
                               idmef_snmp_service_t *snmp_service) 
{
        char *oid = NULL, *community = NULL, *security_name = NULL, *context_name = NULL,
                *context_engine_id = NULL, *command = NULL, mpm[12], sem[12], sel[12];
        int ret = -1;

        if ( ! snmp_service )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_snmp_service_get_oid(snmp_service)), &oid);
	if ( ret < 0 )
		goto error;
        
        get_optional_uint32(mpm, sizeof(mpm), idmef_snmp_service_get_message_processing_model(snmp_service));
        get_optional_uint32(sem, sizeof(sem), idmef_snmp_service_get_security_model(snmp_service));
                
        ret = preludedb_sql_escape(sql, get_string(idmef_snmp_service_get_security_name(snmp_service)), &security_name);
	if ( ret < 0 )
		goto error;

        get_optional_uint32(sel, sizeof(sel), idmef_snmp_service_get_security_level(snmp_service));

        ret = preludedb_sql_escape(sql, get_string(idmef_snmp_service_get_context_name(snmp_service)), &context_name);
	if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_snmp_service_get_context_engine_id(snmp_service)), &context_engine_id);
	if ( ret < 0 )
		goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_snmp_service_get_command(snmp_service)), &command);
	if ( ret < 0 )
		goto error;
        
        ret = preludedb_sql_insert(sql, "Prelude_SnmpService",
				   "_parent_type, _message_ident, _parent0_index, snmp_oid, message_processing_model, "
                                   "security_model, security_name, security_level, context_name, "
				   "context_engine_id, command",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s, %s, %s, %s, %s, %s", 
				   parent_type, message_ident, parent_index, oid, mpm, sem, security_name, sel, context_name,
				   context_engine_id, command);

 error:
	if ( oid )
		free(oid);

	if ( community )
		free(community);

	if ( security_name )
		free(security_name);

	if ( context_name )
		free(context_name);

	if ( context_engine_id )
		free(context_engine_id);

	if ( command )
		free(command);

        return ret;
}



static int insert_web_service_arg(preludedb_sql_t *sql,
				  char parent_type, uint64_t message_ident, int parent_index, int arg_index,
				  prelude_string_t *arg)
{
	char *tmp;
	int ret;

	ret = preludedb_sql_escape(sql, get_string(arg), &tmp);
	if ( ret < 0 )
		return ret;
                
	ret = preludedb_sql_insert(sql, "Prelude_WebServiceArg",
				   "_parent_type, _message_ident, _parent0_index, _index, arg",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %d, %s",
				   parent_type, message_ident, parent_index, arg_index,
				   arg);

	free(arg);

	return ret;
}



static int insert_web_service(preludedb_sql_t *sql,
			      char parent_type, uint64_t message_ident, int parent_index,
			      idmef_web_service_t *web_service) 
{
	prelude_string_t *web_service_arg, *last_web_service_arg;
        char *url, *cgi, *method;
	int index = 0;
        int ret;
        
        if ( ! web_service )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_web_service_get_url(web_service)), &url);
        if ( ret < 0 )
                return ret;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_web_service_get_cgi(web_service)), &cgi);
        if ( ret < 0 ) {
                free(url);
                return -1;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_web_service_get_http_method(web_service)), &method);
        if ( ret < 0 ) {
                free(url);
                free(cgi);
                return ret;
        }
        
        ret = preludedb_sql_insert(sql,
				   "Prelude_WebService",
				   "_parent_type, _message_ident, _parent0_index, "
				   "url, cgi, http_method",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s",
				   parent_type, message_ident, parent_index,
				   url, cgi, method);

        free(url);
        free(cgi);
        free(method);

	index = 0;
	last_web_service_arg = web_service_arg = NULL;
	while ( (web_service_arg = idmef_web_service_get_next_arg(web_service, web_service_arg)) ) {
		ret = insert_web_service_arg(sql, parent_type, message_ident, parent_index, index++, web_service_arg);
		if ( ret < 0 )
			return ret;

		last_web_service_arg = web_service_arg;		
        }

	if ( last_web_service_arg ) {
		ret = insert_web_service_arg(sql, parent_type, message_ident, parent_index, -1, last_web_service_arg);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



static int insert_service(preludedb_sql_t *sql, char parent_type, uint64_t message_ident, int parent_index,
                          idmef_service_t *service) 
{
        int ret = -1;
        char ip_version[8], *name = NULL, port[8], iana_protocol_number[8], *iana_protocol_name = NULL,
		*portlist = NULL, *protocol = NULL, *ident;

        if ( ! service )
                return 0;

	get_optional_uint8(ip_version, sizeof(ip_version), idmef_service_get_ip_version(service));

        ret = preludedb_sql_escape(sql, get_string(idmef_service_get_ident(service)), &ident);
        if ( ret < 0 )
		goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_service_get_name(service)), &name);
        if ( ret < 0 )
		goto error;

	get_optional_uint16(port, sizeof(port), idmef_service_get_port(service));
	get_optional_uint8(iana_protocol_number, sizeof(iana_protocol_number), idmef_service_get_iana_protocol_number(service));

        ret = preludedb_sql_escape(sql, get_string(idmef_service_get_iana_protocol_name(service)), &iana_protocol_name);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_service_get_portlist(service)), &portlist);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_service_get_protocol(service)), &protocol);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_insert(sql, "Prelude_Service",
				   "_parent_type, _message_ident, _parent0_index, "
				   "ident, ip_version, name, port, iana_protocol_number, iana_protocol_name, portlist, protocol",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s, %s, %s, %s, %s, %s", 
				   parent_type, message_ident, parent_index,
				   ident, ip_version, name, port, iana_protocol_number,
				   iana_protocol_name, portlist, protocol);

        if ( ret < 0 )
		goto error;

        switch ( idmef_service_get_type(service)) {

        case IDMEF_SERVICE_TYPE_DEFAULT:
                break;
                
        case IDMEF_SERVICE_TYPE_WEB:
                ret = insert_web_service(sql, parent_type, message_ident, parent_index, idmef_service_get_web_service(service));
                break;

        case IDMEF_SERVICE_TYPE_SNMP:
                ret = insert_snmp_service(sql, parent_type, message_ident, parent_index, idmef_service_get_snmp_service(service));
                break;

        default:
                ret = -1;
        }

 error:
	if ( name )
		free(name);

	if ( iana_protocol_name )
		free(iana_protocol_name);

	if ( portlist )
		free(portlist);

	if ( protocol )
		free(protocol);

        if ( ident )
                free(ident);

        return ret;
}



static int insert_inode(preludedb_sql_t *sql,
			uint64_t message_ident, int target_index, int file_index,
			idmef_inode_t *inode) 
{
        char ctime[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE], ctime_gmtoff[16];
	char number[16], major_device[16], minor_device[16], c_major_device[16], c_minor_device[16];
        int ret;

        if ( ! inode )
                return 0;

        if ( preludedb_sql_time_to_timestamp(sql, idmef_inode_get_change_time(inode),
					     ctime, sizeof (ctime), ctime_gmtoff, sizeof (ctime_gmtoff), NULL, 0) < 0 )
                return -1;

	get_optional_uint32(number, sizeof(number), idmef_inode_get_number(inode));
	get_optional_uint32(major_device, sizeof(major_device), idmef_inode_get_major_device(inode));
	get_optional_uint32(minor_device, sizeof(minor_device), idmef_inode_get_minor_device(inode));
	get_optional_uint32(c_major_device, sizeof(c_major_device), idmef_inode_get_c_major_device(inode));
	get_optional_uint32(c_minor_device, sizeof(c_minor_device), idmef_inode_get_c_minor_device(inode));

        ret = preludedb_sql_insert(sql, "Prelude_Inode",
				   "_message_ident, _parent0_index, _parent1_index, "
				   "change_time, change_time_gmtoff, number, major_device, minor_device, c_major_device, "
				   "c_minor_device",
				   "%" PRELUDE_PRIu64 ", %d, %d, %s, %s, %s, %s, %s, %s, %s",
				   message_ident, target_index, file_index, ctime, ctime_gmtoff, number, major_device,
				   minor_device, c_major_device, c_minor_device);

        return ret;
}



static int insert_linkage(preludedb_sql_t *sql, uint64_t message_ident, int target_index, int file_index, int index,
                          idmef_linkage_t *linkage) 
{
        int ret;
        char *name, *path, *category;

        if ( ! linkage )
                return 0;

        ret = preludedb_sql_escape(sql, idmef_linkage_category_to_string(idmef_linkage_get_category(linkage)), &category);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_linkage_get_name(linkage)), &name);
        if ( ret < 0 ) {
                free(category);
                return ret;
        }

        ret = preludedb_sql_escape(sql, get_string(idmef_linkage_get_path(linkage)), &path);
        if ( ret < 0 ) {
                free(name);
                free(category);
                return ret;
        }

        ret = preludedb_sql_insert(sql, "Prelude_Linkage",
				   "_message_ident, _parent0_index, _parent1_index, _index, category, name, path",
				   "%" PRELUDE_PRIu64 ", %d, %d, %s, %s, %s",
				   message_ident, target_index, file_index, index,
				   category, name, path);

        free(name);
        free(path);
        free(category);

	/* FIXME: idmef_file in idmef_linkage is not currently supported by the db */

	return ret;
}



static int insert_file_access_permission(preludedb_sql_t *sql,
					 uint64_t message_ident, int target_index, int file_index, int file_access_index, int perm_index,
					 prelude_string_t *perm)
{
	char *tmp;
	int ret;

	ret = preludedb_sql_escape(sql, get_string(perm), &tmp);
	if ( ret < 0 )
		return ret;

	ret = preludedb_sql_insert(sql, "Prelude_FileAccess_Permission",
				   "_message_ident, _parent0_index, _parent1_index, _parent2_index, _index, permission",
				   "%" PRELUDE_PRIu64 ", %d, %d, %d, %d, %s",
				   message_ident, target_index, file_index, file_access_index, perm_index, tmp);

	free(tmp);

	return ret;
}



static int insert_file_access(preludedb_sql_t *sql,
			      uint64_t message_ident, int target_index, int file_index, int file_access_index,
                              idmef_file_access_t *file_access)
{
	prelude_string_t *file_access_permission, *last_file_access_permission;
	int index;
        int ret;

        if ( ! file_access )
                return 0;
        
        ret = preludedb_sql_insert(sql, "Prelude_FileAccess", "_message_ident, _parent0_index, _parent1_index, _index",
				   "%" PRELUDE_PRIu64 ", %d, %d, %d", message_ident, target_index, file_index, file_access_index);
	if ( ret < 0 )
                return ret;

	index = 0;
	last_file_access_permission = file_access_permission = NULL;
	while ( (file_access_permission = idmef_file_access_get_next_permission(file_access, file_access_permission)) ) {
		ret = insert_file_access_permission(sql,
						    message_ident, target_index, file_index, file_access_index, index++,
						    file_access_permission);
		if ( ret < 0 )
			return ret;

		last_file_access_permission = file_access_permission;
        }

	if ( last_file_access_permission ) {
		ret = insert_file_access_permission(sql,
						    message_ident, target_index, file_index, file_access_index, -1,
						    last_file_access_permission);
		if ( ret < 0 )
			return ret;
	}

        ret = insert_user_id(sql, 'F', message_ident, target_index, file_index, file_access_index, 0,
			     idmef_file_access_get_user_id(file_access));

        return ret;
}



static int insert_checksum(preludedb_sql_t *sql,
			   uint64_t message_ident, int target_index, int file_index, int checksum_index,
			   idmef_checksum_t *checksum)
{
	char *value = NULL, *key = NULL, *algorithm = NULL;
	int ret = -1;

	ret = preludedb_sql_escape(sql, get_string(idmef_checksum_get_value(checksum)), &value);
	if ( ret < 0 )
		return ret;

	ret = preludedb_sql_escape(sql, get_string(idmef_checksum_get_key(checksum)), &key);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_escape(sql, idmef_checksum_algorithm_to_string(idmef_checksum_get_algorithm(checksum)), &algorithm);
        if ( ret < 0 )
                goto error;

	ret = preludedb_sql_insert(sql, "Prelude_Checksum",
				   "_message_ident, _parent0_index, _parent1_index, _index, value, checksum_key, algorithm",
				   "%" PRELUDE_PRIu64 ", %d, %d, %d, %s, %s, %s",
				   message_ident, target_index, file_index, checksum_index,
				   value, key, algorithm);

error:
	if ( value )
		free(value);

	if ( key )
		free(key);

	if ( algorithm )
		free(algorithm);

	return ret;
}



static int insert_file(preludedb_sql_t *sql, uint64_t message_ident, int target_index, int file_index,
                       idmef_file_t *file) 
{
        int ret = -1;
        idmef_linkage_t *linkage, *last_linkage;
	idmef_checksum_t *checksum, *last_checksum;
        idmef_file_access_t *file_access, *last_file_access;
	int index;
        char *name = NULL, *path = NULL, *category = NULL, *fstype = NULL,
                *ident = NULL, *file_type = NULL, data_size[32], disk_size[32];
        char ctime[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE], ctime_gmtoff[16];
        char mtime[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE], mtime_gmtoff[16];
        char atime[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE], atime_gmtoff[16];

        ret = preludedb_sql_time_to_timestamp(sql, idmef_file_get_create_time(file),
					      ctime, sizeof (ctime), ctime_gmtoff, sizeof (ctime_gmtoff), NULL, 0);
	if ( ret < 0 )
                return ret;

        ret = preludedb_sql_time_to_timestamp(sql, idmef_file_get_modify_time(file),
					      mtime, sizeof (mtime), mtime_gmtoff, sizeof (mtime_gmtoff), NULL, 0);
	if ( ret < 0 )
                return ret;

        ret = preludedb_sql_time_to_timestamp(sql, idmef_file_get_access_time(file),
					      atime, sizeof(atime), atime_gmtoff, sizeof (atime_gmtoff), NULL, 0);
	if ( ret < 0 )
		return ret;

        ret = preludedb_sql_escape(sql, idmef_file_category_to_string(idmef_file_get_category(file)), &category);
	if ( ret < 0 )
		return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_file_get_ident(file)), &ident);
	if ( ret < 0 )
		goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_file_get_name(file)), &name);
	if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_file_get_path(file)), &path);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_file_get_file_type(file)), &file_type);
        if ( ret < 0 )
		goto error;
        
	get_optional_uint64(data_size, sizeof(data_size), idmef_file_get_data_size(file));
	get_optional_uint64(disk_size, sizeof(disk_size), idmef_file_get_disk_size(file));

	ret = preludedb_sql_escape(sql, get_optional_enum((int *) idmef_file_get_fstype(file),
							  (char *(*)(int)) idmef_file_fstype_to_string), &fstype);
	if ( ret < 0 )
		goto error;
	
        ret = preludedb_sql_insert(sql, "Prelude_File", "_message_ident, _parent0_index, _index, ident, category, name, path, "
				   "create_time, create_time_gmtoff, modify_time, modify_time_gmtoff, access_time, access_time_gmtoff, "
				   "data_size, disk_size, fstype, file_type",
				   "%" PRELUDE_PRIu64 ", %d, %d, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s",
				   message_ident, target_index, file_index,
				   ident, category, name, path,
				   ctime, ctime_gmtoff, mtime, mtime_gmtoff, atime, atime_gmtoff,
                                   data_size, disk_size, fstype, file_type);

        if ( ret < 0 )
                goto error;

	index = 0;
        last_file_access = file_access = NULL;
        while ( (file_access = idmef_file_get_next_file_access(file, file_access)) ) {

                ret = insert_file_access(sql, message_ident, target_index, file_index, index++, file_access);
		if ( ret < 0 )
			goto error;

		last_file_access = file_access;
        }

	if ( last_file_access ) {
		ret = insert_file_access(sql, message_ident, target_index, file_index, -1, last_file_access);
		if ( ret < 0 )
			goto error;
	}

	index = 0;
        last_linkage = linkage = NULL;
        while ( (linkage = idmef_file_get_next_linkage(file, linkage)) ) {

                ret = insert_linkage(sql, message_ident, target_index, file_index, index++, linkage);
		if ( ret < 0 )
			goto error;

		last_linkage = linkage;
        }

	if ( last_linkage ) {
		ret = insert_linkage(sql, message_ident, target_index, file_index, -1, last_linkage);
		if ( ret < 0 )
			return ret;
	}

        ret = insert_inode(sql, message_ident, target_index, file_index, idmef_file_get_inode(file));
	if ( ret < 0 )
		goto error;

	index = 0;
	last_checksum = checksum = NULL;
	while ( (checksum = idmef_file_get_next_checksum(file, checksum)) ) {

		ret = insert_checksum(sql, message_ident, target_index, file_index, index++, checksum);
		if ( ret < 0 )
			goto error;

		last_checksum = checksum;
	}

	if ( last_checksum ) {
		ret = insert_checksum(sql, message_ident, target_index, file_index, -1, last_checksum);
		if ( ret < 0 )
			goto error;
	}

 error:
        if ( ident )
                free(ident);
        
	if ( name )
		free(name);

	if ( path )
		free(path);

	if ( category )
		free(category);

	if ( fstype )
		free(fstype);

        if ( file_type )
                free(file_type);
        
        return ret;
}



static int insert_source(preludedb_sql_t *sql, uint64_t message_ident, int index, idmef_source_t *source)
{
        int ret;
        char *interface, *spoofed, *ident;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_source_get_ident(source)), &ident);
	if ( ret < 0 )
		return ret;

        ret = preludedb_sql_escape(sql, idmef_source_spoofed_to_string(idmef_source_get_spoofed(source)), &spoofed);
	if ( ret < 0 ) {
                free(ident);
		return ret;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_source_get_interface(source)), &interface);
        if ( ret < 0 ) {
                free(ident);
                free(spoofed);
                return ret;
        }

        ret = preludedb_sql_insert(sql, "Prelude_Source",
				   "_message_ident, _index, ident, spoofed, interface",
				   "%" PRELUDE_PRIu64 ", %d, %s, %s, %s",
				   message_ident, index,
				   ident, spoofed, interface);

        free(ident);
        free(spoofed);
        free(interface);
        
        if ( ret < 0 )
                return ret;
        
        ret = insert_node(sql, 'S', message_ident, index, idmef_source_get_node(source));
	if ( ret < 0 )
                return ret;

        ret = insert_user(sql, 'S', message_ident, index, idmef_source_get_user(source));
	if ( ret < 0 )
		return ret;
        
        ret = insert_process(sql, 'S', message_ident, index, idmef_source_get_process(source));
	if ( ret < 0 )
		return ret;
        
        ret = insert_service(sql, 'S', message_ident, index, idmef_source_get_service(source));
	if ( ret < 0 )
		return ret;

        return 1;
}



static int insert_target(preludedb_sql_t *sql, uint64_t message_ident, int target_index, idmef_target_t *target)
{
        int ret;
        idmef_file_t *file, *last_file;
	int index;
        char *interface, *decoy, *ident;
        
        ret = preludedb_sql_escape(sql, idmef_target_decoy_to_string(idmef_target_get_decoy(target)), &decoy);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_target_get_ident(target)), &ident);
        if ( ret < 0 ) {
                free(decoy);
                return -2;
        }
        
        ret = preludedb_sql_escape(sql, get_string(idmef_target_get_interface(target)), &interface);
        if ( ret < 0 ) {
                free(ident);
                free(decoy);
                return -2;
        }

        ret = preludedb_sql_insert(sql, "Prelude_Target",
				   "_message_ident, _index, ident, decoy, interface",
				   "%" PRELUDE_PRIu64 ", %d, %s, %s, %s", 
				   message_ident, target_index,
				   ident, decoy, interface);

        free(ident);
        free(decoy);
        free(interface);

        if ( ret < 0 )
                return -1;
        
        ret = insert_node(sql, 'T', message_ident, target_index, idmef_target_get_node(target));
	if ( ret < 0 )
		return ret;
        
        ret = insert_user(sql, 'T', message_ident, target_index, idmef_target_get_user(target));
	if ( ret < 0 )
                return ret;
        
        ret = insert_process(sql, 'T', message_ident, target_index, idmef_target_get_process(target));
	if ( ret < 0 )
                return ret;
        
        ret = insert_service(sql, 'T', message_ident, target_index, idmef_target_get_service(target));
	if ( ret < 0 )
                return ret;

        index = 0;
        last_file = file = NULL;
        while ( (file = idmef_target_get_next_file(target, file)) ) {

                ret = insert_file(sql, message_ident, target_index, index++, file);
		if ( ret < 0 )
                        return ret;

		last_file = file;
        }

	if ( last_file ) {
		ret = insert_file(sql, message_ident, target_index, -1, last_file);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



static int insert_analyzer(preludedb_sql_t *sql,
			   char parent_type, uint64_t message_ident, int analyzer_index,
			   idmef_analyzer_t *analyzer) 
{
        int ret = -1;
        char *name = NULL, *manufacturer = NULL, *model = NULL, *version = NULL, *class = NULL,
		*ostype = NULL, *osversion = NULL, *analyzerid = NULL;

        if ( ! analyzer )
                return 0;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_analyzerid(analyzer)), &analyzerid);
        if ( ret < 0 )
                return ret;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_class(analyzer)), &class);
        if ( ret < 0 )
                goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_name(analyzer)), &name);
        if ( ret < 0 )
                goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_model(analyzer)), &model);
        if ( ret < 0 )
		goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_version(analyzer)), &version);
        if ( ret < 0 )
		goto error;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_manufacturer(analyzer)), &manufacturer);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_ostype(analyzer)), &ostype);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_analyzer_get_osversion(analyzer)), &osversion);
        if ( ret < 0 )
		goto error;
	
        ret = preludedb_sql_insert(sql, "Prelude_Analyzer",
				   "_parent_type, _message_ident, _index, analyzerid, name, manufacturer, "
				   "model, version, class, "
				   "ostype, osversion",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s, %s, %s, %s, %s, %s", 
				   parent_type, message_ident, analyzer_index,
				   analyzerid, name, manufacturer, model, version, class, ostype, osversion);
        
        if ( ret < 0 )
                goto error;
        
        ret = insert_node(sql, parent_type, message_ident, analyzer_index, idmef_analyzer_get_node(analyzer));
	if ( ret < 0 )
                goto error;

        ret = insert_process(sql, parent_type, message_ident, analyzer_index, idmef_analyzer_get_process(analyzer));

 error:
	if ( class )
		free(class);

	if ( name )
		free(name);

	if ( model )
		free(model);

	if ( version )
		free(version);

	if ( manufacturer )
		free(manufacturer);

	if ( ostype )
		free(ostype);

	if ( osversion )
		free(osversion);

        if ( analyzerid )
                free(analyzerid);

        return ret;
}



static int insert_reference(preludedb_sql_t *sql,
			    uint64_t message_ident, int reference_index,
			    idmef_reference_t *reference)
{
	char *origin = NULL, *name = NULL, *url = NULL, *meaning = NULL;
	int ret = -1;

        ret = preludedb_sql_escape(sql, idmef_reference_origin_to_string(idmef_reference_get_origin(reference)), &origin);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_reference_get_url(reference)), &url);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_reference_get_name(reference)), &name);
        if ( ret < 0 )
		goto error;

	ret = preludedb_sql_escape(sql, get_string(idmef_reference_get_meaning(reference)), &meaning);
	if ( ret < 0 )
		goto error;

	ret = preludedb_sql_insert(sql, "Prelude_Reference", "_message_ident, _index, origin, name, url, meaning",
				   "%" PRELUDE_PRIu64 ", %d, %s, %s, %s, %s",
				   message_ident, reference_index, origin, name, url, meaning);

 error:
	if ( origin )
		free(origin);

	if ( url )
		free(url);

	if ( name )
		free(name);

	if ( meaning )
		free(meaning);

	return ret;
}


static int insert_classification(preludedb_sql_t *sql, uint64_t message_ident, idmef_classification_t *classification) 
{
        char *text, *ident;
	idmef_reference_t *reference, *last_reference;
	int index;
        int ret;

	if ( ! classification )
		return 0;

	ret = preludedb_sql_escape(sql, get_string(idmef_classification_get_ident(classification)), &ident);
	if ( ret < 0 )
		return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_classification_get_text(classification)), &text);
	if ( ret < 0 ) {
                free(ident);
		return ret;
        }
        
        ret = preludedb_sql_insert(sql, "Prelude_Classification",
				   "_message_ident, ident, text",
				   "%" PRELUDE_PRIu64 ", %s, %s",
				   message_ident, ident, text);
        
	free(text);
        free(ident);

	index = 0;
	last_reference = reference = NULL;
	while ( (reference = idmef_classification_get_next_reference(classification, reference)) ) {

		ret = insert_reference(sql, message_ident, index++, reference);
		if ( ret < 0 )
			return ret;

		last_reference = reference;
	}

	if ( last_reference ) {
		ret = insert_reference(sql, message_ident, -1, last_reference);
		if ( ret < 0 )
			return ret;
	}

	return 1;
}



static int insert_additional_data(preludedb_sql_t *sql, 
				  char parent_type, uint64_t message_ident, int ad_index,
				  idmef_additional_data_t *additional_data) 
{
        int ret;
        char *meaning, *type, *data;
                
        if ( ! additional_data )
                return 0;

        ret = preludedb_sql_escape(sql, idmef_additional_data_type_to_string(idmef_additional_data_get_type(additional_data)), &type);
        if ( ret < 0 )
                return ret;
        
        ret = preludedb_sql_escape(sql, get_string(idmef_additional_data_get_meaning(additional_data)), &meaning);
        if ( ret < 0 ) {
                free(type);
                return ret;
        }
        
	ret = get_data(sql, idmef_additional_data_get_data(additional_data), &data);
	if ( ret < 0 ) {
		free(type);
		free(meaning);
		return ret;
	}
        
        ret = preludedb_sql_insert(sql, "Prelude_AdditionalData",
				   "_parent_type, _message_ident, _index, type, meaning, data",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s, %s",
				   parent_type, message_ident, ad_index, type, meaning, data);
        
        free(type);
        free(meaning);        
        free(data);
        
        return ret;
}



static int insert_createtime(preludedb_sql_t *sql, char parent_type, uint64_t message_ident, idmef_time_t *time) 
{
        char utc_time[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE];
	char utc_time_usec[16];
	char utc_time_gmtoff[16];
        int ret;

        ret = preludedb_sql_time_to_timestamp(sql, time, utc_time, sizeof (utc_time), 
					      utc_time_gmtoff, sizeof (utc_time_gmtoff),
					      utc_time_usec, sizeof (utc_time_usec));
	if ( ret < 0 )
		return ret;

        return preludedb_sql_insert(sql, "Prelude_CreateTime", "_parent_type, _message_ident, time, gmtoff, usec", 
				    "'%c', %" PRELUDE_PRIu64 ", %s, %s, %s",
				    parent_type, message_ident, utc_time, utc_time_gmtoff, utc_time_usec);
}



static int insert_detecttime(preludedb_sql_t *sql, uint64_t message_ident, idmef_time_t *time) 
{        
        char utc_time[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE];
	char utc_time_usec[16];
	char utc_time_gmtoff[16];
        int ret;

        if ( ! time )
                return 0;

        ret = preludedb_sql_time_to_timestamp(sql, time, utc_time, sizeof (utc_time),
					      utc_time_gmtoff, sizeof (utc_time_gmtoff),
					      utc_time_usec, sizeof (utc_time_usec));
	if ( ret < 0 )
                return ret;

        return preludedb_sql_insert(sql, "Prelude_DetectTime", "_message_ident, time, gmtoff, usec",
				    "%" PRELUDE_PRIu64 ", %s, %s, %s", message_ident, utc_time, utc_time_gmtoff, utc_time_usec);
}



static int insert_analyzertime(preludedb_sql_t *sql, char parent_type, uint64_t message_ident, idmef_time_t *time) 
{
        char utc_time[PRELUDEDB_SQL_TIMESTAMP_STRING_SIZE];
	char utc_time_usec[16];
	char utc_time_gmtoff[16];
        int ret;

        if ( ! time )
                return 0;

        ret = preludedb_sql_time_to_timestamp(sql, time, utc_time, sizeof(utc_time),
                                              utc_time_gmtoff, sizeof(utc_time_gmtoff),
					      utc_time_usec, sizeof (utc_time_usec));
	if ( ret < 0 )
                return ret;

        return preludedb_sql_insert(sql, "Prelude_AnalyzerTime", "_parent_type, _message_ident, time, gmtoff, usec",
				    "'%c', %" PRELUDE_PRIu64 ", %s, %s, %s",
				    parent_type, message_ident, utc_time, utc_time_gmtoff, utc_time_usec);
}



static int insert_impact(preludedb_sql_t *sql, uint64_t message_ident, idmef_impact_t *impact) 
{
        int ret = -1;
        char *completion = NULL, *type = NULL, *severity = NULL, *description = NULL;

        if ( ! impact )
                return 0;

        ret = preludedb_sql_escape(sql, get_optional_enum((int *) idmef_impact_get_completion(impact),
							  (char *(*)(int)) idmef_impact_completion_to_string), &completion);
        if ( ret < 0 )
		return ret;

        ret = preludedb_sql_escape(sql, idmef_impact_type_to_string(idmef_impact_get_type(impact)), &type);
        if ( ret < 0 ) 
                goto error;

        ret = preludedb_sql_escape(sql, get_optional_enum((int *) idmef_impact_get_severity(impact),
							  (char *(*)(int))idmef_impact_severity_to_string), &severity);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_escape(sql, get_string(idmef_impact_get_description(impact)), &description);
        if ( ret < 0 )
		goto error;

        ret = preludedb_sql_insert(sql, "Prelude_Impact", "_message_ident, severity, completion, type, description", 
				   "%" PRELUDE_PRIu64 ", %s, %s, %s, %s", 
				   message_ident, severity, completion, type, description);

 error:
	if ( completion )
		free(completion);

	if ( type )
		free(type);

	if ( severity )
		free(severity);

	if ( description )
		free(description);

        return ret;
}



static int insert_action(preludedb_sql_t *sql, uint64_t message_ident, int action_index, idmef_action_t *action)
{
        int ret;
        char *category, *description;

	ret = preludedb_sql_escape(sql, idmef_action_category_to_string(idmef_action_get_category(action)), &category);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_action_get_description(action)), &description);
        if ( ret < 0 ) {
                free(category);
                return ret;
        }

        ret = preludedb_sql_insert(sql, "Prelude_Action",
				   "_message_ident, _index, category, description",
				   "%" PRELUDE_PRIu64 ", %d, %s, %s", 
				   message_ident, action_index, category, description);

        free(category);
        free(description);

        return ret;
}



static int insert_confidence(preludedb_sql_t *sql, uint64_t message_ident, idmef_confidence_t *confidence) 
{
        int ret;
        char *rating;

        if ( ! confidence )
                return 0;

        ret = preludedb_sql_escape(sql, idmef_confidence_rating_to_string(idmef_confidence_get_rating(confidence)), &rating);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_insert(sql, "Prelude_Confidence", "_message_ident, rating, confidence",
				   "%" PRELUDE_PRIu64 ", %s, %f", message_ident, rating, idmef_confidence_get_confidence(confidence));

        free(rating);
        
        return ret;
}



static int insert_assessment(preludedb_sql_t *sql, uint64_t message_ident, idmef_assessment_t *assessment) 
{
        idmef_action_t *action, *last_action;
	int index;
	int ret;

        if ( ! assessment )
                return 0;

        ret = preludedb_sql_insert(sql, "Prelude_Assessment", "_message_ident", "%" PRELUDE_PRIu64, message_ident);
	if ( ret < 0 )
                return ret;

        ret = insert_impact(sql, message_ident, idmef_assessment_get_impact(assessment));
	if ( ret < 0 )
                return ret;
        
        ret = insert_confidence(sql, message_ident, idmef_assessment_get_confidence(assessment));
	if ( ret < 0 )
                return ret;

	index = 0;
        last_action = action = NULL;
        while ( (action = idmef_assessment_get_next_action(assessment, action)) ) {

                ret = insert_action(sql, message_ident, index++, action);
		if ( ret < 0 )
                        return ret;

		last_action = action;
        }

	if ( last_action ) {
		ret = insert_action(sql, message_ident, -1, last_action);
		if ( ret < 0 )
			return ret;
	}

	return 1;
}



static int insert_overflow_alert(preludedb_sql_t *sql, uint64_t message_ident, idmef_overflow_alert_t *overflow_alert) 
{
        char *program, *buffer, size[16];
        int ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_overflow_alert_get_program(overflow_alert)), &program);
	if ( ret < 0 )
		return ret;

	ret = get_data(sql, idmef_overflow_alert_get_buffer(overflow_alert), &buffer);
	if ( ret < 0 ) {
		free(program);
		return ret;
	}

	get_optional_uint32(size, sizeof(size), idmef_overflow_alert_get_size(overflow_alert));

        ret = preludedb_sql_insert(sql, "Prelude_OverflowAlert", "_message_ident, program, size, buffer",
				   "%" PRELUDE_PRIu64 ", %s, %s, %s", 
				   message_ident, program, size, buffer);

        free(program);
        free(buffer);

        return ret;
}


static int insert_alertident(preludedb_sql_t *sql,
			     char parent_type, uint64_t message_ident, int alertident_index,
			     idmef_alertident_t *alertident)
{
	int ret;
        char *analyzerid, *messageid;

        ret = preludedb_sql_escape(sql, get_string(idmef_alertident_get_analyzerid(alertident)), &analyzerid);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_alertident_get_alertident(alertident)), &messageid);
        if ( ret < 0 ) {
                free(analyzerid);
                return ret;
        }
        
	ret = preludedb_sql_insert(sql, "Prelude_Alertident",
				   "_parent_type, _message_ident, _index, alertident, analyzerid",
				   "'%c', %" PRELUDE_PRIu64 ", %d, %s, %s",
				   parent_type, message_ident, alertident_index,
				   messageid, analyzerid);

        free(analyzerid);
        free(messageid);
        
	return ret;
}


static int insert_tool_alert(preludedb_sql_t *sql, uint64_t message_ident, idmef_tool_alert_t *tool_alert) 
{
        char *name, *command;
	idmef_alertident_t *alertident, *last_alertident;
	int index;
        int ret;

        if ( ! tool_alert )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_tool_alert_get_name(tool_alert)), &name);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_escape(sql, get_string(idmef_tool_alert_get_command(tool_alert)), &command);
        if ( ret < 0 ) {
                free(name);
                return ret;
        }

        ret = preludedb_sql_insert(sql, "Prelude_ToolAlert", "_message_ident, name, command",
				   "%" PRELUDE_PRIu64 ", %s, %s",
				   message_ident, name, command);

        free(name);
        free(command);

	index = 0;
	last_alertident = alertident = NULL;
	while ( (alertident = idmef_tool_alert_get_next_alertident(tool_alert, alertident)) ) {
		ret = insert_alertident(sql, 'T', message_ident, index++, alertident);
		if ( ret < 0 )
			return ret;

		last_alertident = alertident;
	}

	if ( ret < 0 ) {
		ret = insert_alertident(sql, 'T', message_ident, -1, alertident);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



static int insert_correlation_alert(preludedb_sql_t *sql, uint64_t message_ident,
				    idmef_correlation_alert_t *correlation_alert) 
{
        char *name;
	idmef_alertident_t *alertident, *last_alertident;
	int index;
        int ret;

        if ( ! correlation_alert )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_correlation_alert_get_name(correlation_alert)), &name);
	if ( ret < 0 )
		return ret;

        ret = preludedb_sql_insert(sql, "Prelude_CorrelationAlert", "_message_ident, name",
				   "%" PRELUDE_PRIu64 ", %s", message_ident, name);

        free(name);
 
        if ( ret < 0 )
                return ret;

	index = 0;
	last_alertident = alertident = NULL;
	while ( (alertident = idmef_correlation_alert_get_next_alertident(correlation_alert, alertident)) ) {

		ret = insert_alertident(sql, 'C', message_ident, index++, alertident);
		if ( ret < 0 )
			return ret;

		last_alertident = alertident;
	}

	if ( last_alertident ) {
		ret = insert_alertident(sql, 'C', message_ident, -1, last_alertident);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



static int get_last_insert_ident(preludedb_sql_t *sql, const char *table_name, uint64_t *result)
{
        preludedb_sql_table_t *table;
        preludedb_sql_row_t *row;
        preludedb_sql_field_t *field;
	int ret;

        ret = preludedb_sql_query_sprintf(sql, &table, "SELECT max(_ident) FROM %s;", table_name);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_table_fetch_row(table, &row);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_row_fetch_field(row, 0, &field);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_field_to_uint64(field, result);
	if ( ret < 0 )
		goto error;

 error:
        preludedb_sql_table_destroy(table);

        return ret;
}



static int insert_message_messageid(preludedb_sql_t *sql, const char *table_name,
                                    prelude_string_t *messageid, uint64_t *result)
{
	int ret;
        char *tmp;

	ret = preludedb_sql_escape(sql, get_string(messageid), &tmp);
	if ( ret < 0 )
		return ret;

        ret = preludedb_sql_insert(sql, table_name, "messageid", "%s", tmp);
	free(tmp);
	if ( ret < 0 )
                return ret;

        return get_last_insert_ident(sql, table_name, result);
}



static int insert_alert(preludedb_sql_t *sql, idmef_alert_t *alert) 
{
        uint64_t ident;
        idmef_source_t *source, *last_source;
        idmef_target_t *target, *last_target;
        idmef_analyzer_t *analyzer, *last_analyzer;
        idmef_additional_data_t *additional_data, *last_additional_data;
	int index;
	int ret;

        if ( ! alert )
                return 0;

        ret = insert_message_messageid(sql, "Prelude_Alert", idmef_alert_get_messageid(alert), &ident);
	if ( ret < 0 )
                return ret;
                
        ret = insert_createtime(sql, 'A', ident, idmef_alert_get_create_time(alert));
	if ( ret < 0 )
                return ret;
        
        ret = insert_detecttime(sql, ident, idmef_alert_get_detect_time(alert));
	if ( ret < 0 )
                return ret;
        
        ret = insert_analyzertime(sql, 'A', ident, idmef_alert_get_analyzer_time(alert));
	if ( ret < 0 )
		return ret;

        ret = insert_assessment(sql, ident, idmef_alert_get_assessment(alert));
	if ( ret < 0 )
             return ret;

        switch ( idmef_alert_get_type(alert) ) {

        case IDMEF_ALERT_TYPE_DEFAULT:
                break;
                
        case IDMEF_ALERT_TYPE_TOOL:
                ret = insert_tool_alert(sql, ident, idmef_alert_get_tool_alert(alert));
		if ( ret < 0 )
                        return ret;
                break;

        case IDMEF_ALERT_TYPE_OVERFLOW:
                ret = insert_overflow_alert(sql, ident, idmef_alert_get_overflow_alert(alert));
		if ( ret < 0 )
                        return ret;
                break;

        case IDMEF_ALERT_TYPE_CORRELATION:
                ret = insert_correlation_alert(sql, ident, idmef_alert_get_correlation_alert(alert));
		if ( ret < 0 )
                        return ret;
                break;

	default:
		return -1;
        }

        index = 0;
        last_analyzer = analyzer = NULL;
        while ( (analyzer = idmef_alert_get_next_analyzer(alert, analyzer)) ) {
                
                ret = insert_analyzer(sql, 'A', ident, index++, analyzer);
		if ( ret < 0 )
                        return ret;

		last_analyzer = analyzer;
        }

	if ( last_analyzer ) {
		ret = insert_analyzer(sql, 'A', ident, -1, last_analyzer);
		if ( ret < 0 )
			return ret;
	}

        index = 0;
        last_source = source = NULL;
        while ( (source = idmef_alert_get_next_source(alert, source)) ) {
                
                ret = insert_source(sql, ident, index++, source);
		if ( ret < 0 )
                        return ret;

		last_source = source;
        }

	if ( last_source ) {
		ret = insert_source(sql, ident, -1, last_source);
		if ( ret < 0 )
			return ret;
	}

        index = 0;
        last_target = target = NULL;
        while ( (target = idmef_alert_get_next_target(alert, target)) ) {

                ret = insert_target(sql, ident, index++, target);
		if ( ret < 0 )
                        return ret;

		last_target = target;
        }

	if ( last_target ) {
		ret = insert_target(sql, ident, -1, last_target);
		if ( ret < 0 )
			return ret;
	}

	ret = insert_classification(sql, ident, idmef_alert_get_classification(alert));
	if ( ret < 0 )
		return ret;

	index = 0;
        last_additional_data = additional_data = NULL;
        while ( (additional_data = idmef_alert_get_next_additional_data(alert, additional_data)) ) {

                ret = insert_additional_data(sql, 'A', ident, index++, additional_data);
		if ( ret < 0 )
                        return ret;

		last_additional_data = additional_data;
        }

	if ( last_additional_data ) {
		ret = insert_additional_data(sql, 'A', ident, -1, last_additional_data);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



static int insert_heartbeat(preludedb_sql_t *sql, idmef_heartbeat_t *heartbeat) 
{
        uint64_t ident;
        idmef_analyzer_t *analyzer, *last_analyzer;
        char heartbeat_interval[16], *messageid;
        idmef_additional_data_t *additional_data, *last_additional_data;
        unsigned int index;
	int ret;

        if ( ! heartbeat )
                return 0;

        ret = preludedb_sql_escape(sql, get_string(idmef_heartbeat_get_messageid(heartbeat)), &messageid);
	if ( ret < 0 )
		return ret;
        
        get_optional_uint32(heartbeat_interval, sizeof(heartbeat_interval),
                            idmef_heartbeat_get_heartbeat_interval(heartbeat));
        
        ret = preludedb_sql_insert(sql, "Prelude_Heartbeat", "messageid, heartbeat_interval",
                                   "%s, %s", messageid, heartbeat_interval);
        
        free(messageid);
        if ( ret < 0 )
                return ret;

        ret = get_last_insert_ident(sql, "Prelude_Heartbeat", &ident);
        if ( ret < 0 )
                return ret;

        index = 0;
        last_analyzer = analyzer = NULL;
        while ( (analyzer = idmef_heartbeat_get_next_analyzer(heartbeat, analyzer)) ) {
                
                ret = insert_analyzer(sql, 'H', ident, index++, analyzer);
		if ( ret < 0 )
                        return ret;

		last_analyzer = analyzer;
        }

	if ( last_analyzer ) {
		ret = insert_analyzer(sql, 'H', ident, -1, last_analyzer);
		if ( ret < 0 )
			return ret;
	}
        
        ret = insert_createtime(sql, 'H', ident, idmef_heartbeat_get_create_time(heartbeat));
	if ( ret < 0 )
                return ret;
        
        ret = insert_analyzertime(sql, 'H', ident, idmef_heartbeat_get_analyzer_time(heartbeat));
	if ( ret < 0 )
                return ret;

	index = 0;
        last_additional_data = additional_data = NULL;
        while ( (additional_data = idmef_heartbeat_get_next_additional_data(heartbeat, additional_data)) ) {

                ret = insert_additional_data(sql, 'H', ident, index++, additional_data);
		if ( ret < 0 )
			return ret;

		last_additional_data = additional_data;
        }

	if ( last_additional_data ) {
		ret = insert_additional_data(sql, 'H', ident, -1, last_additional_data);
		if ( ret < 0 )
			return ret;
	}

        return 1;
}



int classic_insert(preludedb_sql_t *sql, idmef_message_t *message)
{
        int ret;

        if ( ! message )
                return 0;

        ret = preludedb_sql_transaction_start(sql);
        if ( ret < 0 )
		return ret;
        
        switch ( idmef_message_get_type(message) ) {

        case IDMEF_MESSAGE_TYPE_ALERT:
                ret = insert_alert(sql, idmef_message_get_alert(message));
                break;

        case IDMEF_MESSAGE_TYPE_HEARTBEAT:
                ret = insert_heartbeat(sql, idmef_message_get_heartbeat(message));
                break;

        default:
                return -1;
        }

        if ( ret < 0 ) {
		int tmp;

                tmp = preludedb_sql_transaction_abort(sql);

		return (tmp < 0) ? tmp : ret;
        }

        return preludedb_sql_transaction_end(sql);
}
