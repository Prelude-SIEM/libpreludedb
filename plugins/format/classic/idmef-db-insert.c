/*****
*
* Copyright (C) 2001-2004 Yoann Vandoorselaere <yoann@prelude-ids.org>
* Copyright (C) 2003,2004 Nicolas Delon <nicolas@prelude-ids.org>
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
#include <inttypes.h>
#include <string.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>
#include <libprelude/idmef-util.h>
#include <libprelude/prelude-plugin.h>

#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "idmef-db-insert.h"

#ifdef DEBUG

/*
 * for an unknow reason, we don't see warning about
 * invalid fmt arguments when using db_plugin_insert().
 */
#define db_plugin_insert(conn, tbl, field, fmt, args...) \
        printf(fmt, args); db_plugin_insert(conn, tbl, field, fmt, args)

#endif


static inline const char *get_string(prelude_string_t *string)
{
        const char *s;

        if ( ! string )
                return NULL;

        s = prelude_string_get_string(string);

        return s ? s : "";
}


static inline char *get_data(prelude_sql_connection_t *conn, idmef_data_t *data)
{
	switch ( idmef_data_get_type(data) ) {
	case IDMEF_DATA_TYPE_BYTE: case IDMEF_DATA_TYPE_BYTE_STRING:
	case IDMEF_DATA_TYPE_CHAR: case IDMEF_DATA_TYPE_CHAR_STRING:
		return prelude_sql_escape_fast(conn, idmef_data_get_data(data), idmef_data_get_len(data));

	default: {
		char buf[32];

		if ( idmef_data_to_string(data, buf, sizeof (buf)) < 0 )
			return NULL;
		return strdup(buf);		
	}
	}

	return NULL;
}



static inline char *get_optional_enum(int *value, char *(*convert_func)(int))
{
	if ( ! value )
		return NULL;

	return convert_func(*value);
}



#define get_optional_integer(name, type, format)			\
static inline void get_optional_ ## name(char *dst, type *value)	\
{									\
	if ( ! value )							\
		strcpy(dst, "NULL");					\
	else								\
		sprintf(dst, format, *value);				\
}


get_optional_integer(uint8, uint8_t, "%hhu")
get_optional_integer(uint16, uint16_t, "%hu")
get_optional_integer(int32, int32_t, "%d")
get_optional_integer(uint32, uint32_t, "%u")
get_optional_integer(uint64, uint64_t, "%" PRIu64)
get_optional_integer(float, float, "%f")


static int insert_address(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
			  idmef_address_t *address) 
{
        int ret;
        char *vlan_name, vlan_num[16], *addr, *netmask, *category;

        if ( ! address )
                return 0;

        category = prelude_sql_escape(conn, idmef_address_category_to_string(idmef_address_get_category(address)));
        if ( ! category )
                return -1;

        addr = prelude_sql_escape(conn, get_string(idmef_address_get_address(address)));
        if ( ! addr ) {
                free(category);
                return -1;
        }

        netmask = prelude_sql_escape(conn, get_string(idmef_address_get_netmask(address)));
        if ( ! netmask ) {
                free(addr);
                free(category);
                return -1;
        }

        vlan_name = prelude_sql_escape(conn, get_string(idmef_address_get_vlan_name(address)));
        if ( ! vlan_name ) {
                free(addr);
                free(netmask);
                free(category);
                return -1;
        }

	get_optional_int32(vlan_num, idmef_address_get_vlan_num(address));

        ret = prelude_sql_insert(conn, "Prelude_Address", "_message_ident, _parent_type, _parent_index, "
                                 "ident, category, vlan_name, vlan_num, address, netmask",
                                 "%" PRIu64 ", '%c', %d, %" PRIu64 ", %s, %s, %s, %s, %s",
                                 message_ident, parent_type, parent_index, idmef_address_get_ident(address),
				 category, vlan_name, vlan_num, addr, netmask);
        
        free(addr);
        free(netmask);
        free(category);
        free(vlan_name);

        return ret;
}



static int insert_node(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
                       idmef_node_t *node)
{
        int ret;
        idmef_address_t *address;
        char *location, *name, *category;

        if ( ! node )
                return 0;

        category = prelude_sql_escape(conn, idmef_node_category_to_string(idmef_node_get_category(node)));
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, get_string(idmef_node_get_name(node)));
        if ( ! name ) {
                free(category);
                return -1;
        }
        
        location = prelude_sql_escape(conn, get_string(idmef_node_get_location(node)));
        if ( ! location ) {
                free(name);
                free(category);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Node",
				 "_message_ident, _parent_type, _parent_index, ident, category, location, name",
                                 "%" PRIu64 ", '%c', %d, %" PRIu64 ", %s, %s, %s", 
                                 message_ident, parent_type, parent_index, idmef_node_get_ident(node),
				 category, location, name);

        free(name);
        free(location);
        free(category);
        
        if ( ret < 0 )
                return ret;

        address = NULL;
        while ( (address = idmef_node_get_next_address(node, address)) ) {

                if ( insert_address(conn, message_ident, parent_type, parent_index, address) < 0 )
                        return -1;
        }

        return 0;
}



static int insert_user_id(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
			  int file_index, int file_access_index, idmef_user_id_t *user_id) 
{
        int ret;
        char *name, *type, number[16];
        
        type = prelude_sql_escape(conn, idmef_user_id_type_to_string(idmef_user_id_get_type(user_id)));
        if ( ! type )
                return -1;

        name = prelude_sql_escape(conn, get_string(idmef_user_id_get_name(user_id)));
        if ( ! name ) {
                free(type);
                return -1;
        }

	get_optional_uint32(number, idmef_user_id_get_number(user_id));
        
        ret = prelude_sql_insert(conn, "Prelude_UserId", "_message_ident, _parent_type, _parent_index, _file_index, "
				 "_file_access_index, ident, type, name, number",
                                 "%" PRIu64 ", '%c', %d, %d, %d, %" PRIu64 ", %s, %s, %s", 
                                 message_ident, parent_type, parent_index, file_index, file_access_index,
                                 idmef_user_id_get_ident(user_id), type, name, number);

        free(type);
        free(name);

        return ret;
}



static int insert_user(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
		       idmef_user_t *user) 
{
        int ret;
        char *category;
        idmef_user_id_t *user_id;
        
        if ( ! user )
                return 0;

        category = prelude_sql_escape(conn, idmef_user_category_to_string(idmef_user_get_category(user)));
        if ( ! category )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_User", "_message_ident, _parent_type, _parent_index, ident, category",
                                 "%" PRIu64 ", '%c', %d, %" PRIu64 ", %s",
                                 message_ident, parent_type, parent_index, idmef_user_get_ident(user), category);

        free(category);
        
        if ( ret < 0 )
                return ret;
        
        user_id = NULL;
        while ( (user_id = idmef_user_get_next_user_id(user, user_id)) ) {

                if ( insert_user_id(conn, message_ident, parent_type, parent_index, 0, 0, user_id) < 0 )
                        return -1;
        }

        return 0;
}



static int insert_process(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
                          idmef_process_t *process) 
{
        prelude_string_t *process_arg;
        prelude_string_t *process_env;
        char *name, *path, *arg, *env, pid[16];
        int ret;

        if ( ! process )
                return 0;
        
        name = prelude_sql_escape(conn, get_string(idmef_process_get_name(process)));
        if ( ! name )
                return -1;

        path = prelude_sql_escape(conn, get_string(idmef_process_get_path(process)));
        if ( ! path ) {
                free(name);
                return -1;
        }

	get_optional_uint32(pid, idmef_process_get_pid(process));

        ret = prelude_sql_insert(conn, "Prelude_Process", "_message_ident, _parent_type, _parent_index, ident, name, pid, path",
                                 "%" PRIu64 ", '%c', %d, %" PRIu64 ", %s, %s, %s", 
                                 message_ident, parent_type, parent_index, idmef_process_get_ident(process),
				 name, pid, path);

        free(name);
        free(path);

        if ( ret < 0 )
                return ret;

        process_arg = NULL;
        while ( (process_arg = idmef_process_get_next_arg(process, process_arg)) ) {

                arg = prelude_sql_escape(conn, get_string(process_arg));
                if ( ! arg )
                        return -1;
                
                ret = prelude_sql_insert(conn, "Prelude_ProcessArg", "_message_ident, _parent_type, _parent_index, arg",
                                         "%" PRIu64 ", '%c', %d, %s", message_ident, parent_type, parent_index, arg);

                free(arg);

                if ( ret < 0 )
                        return ret;
        }

        process_env = NULL;
        while ( (process_env = idmef_process_get_next_env(process, process_env)) ) {

                env = prelude_sql_escape(conn, get_string(process_env));
                if ( ! env )
                        return -1;

                ret = prelude_sql_insert(conn, "Prelude_ProcessEnv", "_message_ident, _parent_type, _parent_index, env",
                                         "%" PRIu64 ", '%c', %d, %s", message_ident, parent_type, parent_index, env);
                                 
                free(env);

                if ( ret < 0 )
                        return ret;
        }

        return 1;
}



static int insert_snmp_service(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
                               idmef_snmp_service_t *snmp_service) 
{
        char *oid = NULL, *community = NULL, *security_name = NULL, *context_name = NULL, *context_engine_id = NULL,
		*command = NULL;
        int ret = -1;

        if ( ! snmp_service )
                return 0;

        if ( !(oid = prelude_sql_escape(conn, get_string(idmef_snmp_service_get_oid(snmp_service)))) )
		goto error;

        if ( !(community = prelude_sql_escape(conn, get_string(idmef_snmp_service_get_community(snmp_service)))) )
		goto error;

        if ( !(security_name = prelude_sql_escape(conn, get_string(idmef_snmp_service_get_security_name(snmp_service)))) )
		goto error;

        if ( !(context_name = prelude_sql_escape(conn, get_string(idmef_snmp_service_get_context_name(snmp_service)))) )
		goto error;

        if ( !(context_engine_id = prelude_sql_escape(conn, get_string(idmef_snmp_service_get_context_engine_id(snmp_service)))) )
		goto error;
        
        if ( !(command = prelude_sql_escape(conn, get_string(idmef_snmp_service_get_command(snmp_service)))) )
		goto error;
        
        ret = prelude_sql_insert(conn, "Prelude_Snmp_Service",
				 "_message_ident, _parent_type, _parent_index, oid, community, security_name, context_name, "
				 "context_engine_id, command",
                                 "%" PRIu64 ", '%c', %d, %s, %s, %s, %s, %s, %s", 
                                 message_ident, parent_type, parent_index, oid, community, security_name, context_name,
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



static int insert_web_service(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
			      idmef_web_service_t *web_service) 
{
	prelude_string_t *web_service_arg;
        char *url, *cgi, *method, *arg;
        int ret;
        
        if ( ! web_service )
                return 0;

        url = prelude_sql_escape(conn, get_string(idmef_web_service_get_url(web_service)));
        if ( ! url )
                return -1;
        
        cgi = prelude_sql_escape(conn, get_string(idmef_web_service_get_cgi(web_service)));
        if ( ! cgi ) {
                free(url);
                return -1;
        }
        
        method = prelude_sql_escape(conn, get_string(idmef_web_service_get_http_method(web_service)));
        if ( ! method ) {
                free(url);
                free(cgi);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_WebService", "_message_ident, _parent_type, _parent_index, url, cgi, http_method",
                                 "%" PRIu64 ", '%c', %d, %s, %s, %s",
                                 message_ident, parent_type, parent_index, url, cgi, method);

        free(url);
        free(cgi);
        free(method);

	web_service_arg = NULL;
	while ( (web_service_arg = idmef_web_service_get_next_arg(web_service, web_service_arg)) ) {

                arg = prelude_sql_escape(conn, get_string(web_service_arg));
                if ( ! arg )
                        return -1;
                
                ret = prelude_sql_insert(conn, "Prelude_WebServiceArg", "_message_ident, _parent_type, _parent_index, arg",
                                         "%" PRIu64 ", '%c', %d, %s", message_ident, parent_type, parent_index, arg);

                free(arg);

                if ( ret < 0 )
                        return ret;
        }


        return ret;
}



static int insert_service(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, int parent_index,
                          idmef_service_t *service) 
{
        int ret = -1;
        char ip_version[8], *name = NULL, port[8], iana_protocol_number[8], *iana_protocol_name = NULL,
		*portlist = NULL, *protocol = NULL;

        if ( ! service )
                return 0;

	get_optional_uint8(ip_version, idmef_service_get_ip_version(service));

        name = prelude_sql_escape(conn, get_string(idmef_service_get_name(service)));
        if ( ! name )
		goto error;

	get_optional_uint16(port, idmef_service_get_port(service));

	get_optional_uint8(iana_protocol_number, idmef_service_get_iana_protocol_number(service));

        iana_protocol_name = prelude_sql_escape(conn, get_string(idmef_service_get_iana_protocol_name(service)));
        if ( ! iana_protocol_name )
		goto error;

        portlist = prelude_sql_escape(conn, get_string(idmef_service_get_portlist(service)));
        if ( ! portlist )
		goto error;

        protocol = prelude_sql_escape(conn, get_string(idmef_service_get_protocol(service)));
        if ( ! protocol )
		goto error;

        ret = prelude_sql_insert(conn, "Prelude_Service", "_message_ident, _parent_type, _parent_index, "
				 "ident, ip_version, name, port, iana_protocol_number, iana_protocol_name, portlist, protocol",
                                 "%" PRIu64 ", '%c', %d, %" PRIu64 ", %s, %s, %s, %s, %s, %s, %s", 
                                 message_ident, parent_type, parent_index,
				 idmef_service_get_ident(service), ip_version, name, port, iana_protocol_number,
				 iana_protocol_name, portlist, protocol);

        if ( ret < 0 )
		goto error;

        switch ( idmef_service_get_type(service)) {

        case IDMEF_SERVICE_TYPE_DEFAULT:
                break;
                
        case IDMEF_SERVICE_TYPE_WEB:
                ret = insert_web_service(conn, message_ident, parent_type, parent_index, idmef_service_get_web_service(service));
                break;

        case IDMEF_SERVICE_TYPE_SNMP:
                ret = insert_snmp_service(conn, message_ident, parent_type, parent_index, idmef_service_get_snmp_service(service));
                break;

        default:
                log(LOG_ERR, "unknown service type %d.\n");
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

        return ret;
}



static int insert_inode(prelude_sql_connection_t *conn, uint64_t message_ident, int target_index, int file_index,
			idmef_inode_t *inode) 
{
        char ctime[IDMEF_TIME_MAX_STRING_SIZE], ctime_gmtoff[16];
	char number[16], major_device[16], minor_device[16], c_major_device[16], c_minor_device[16];
        int ret;

        if ( ! inode )
                return 0;

        if ( prelude_sql_time_to_timestamp(idmef_inode_get_change_time(inode),
				       ctime, sizeof (ctime), ctime_gmtoff, sizeof (ctime_gmtoff), NULL, 0) < 0 )
                return -1;

	get_optional_uint32(number, idmef_inode_get_number(inode));

	get_optional_uint32(major_device, idmef_inode_get_major_device(inode));

	get_optional_uint32(minor_device, idmef_inode_get_minor_device(inode));

	get_optional_uint32(c_major_device, idmef_inode_get_c_major_device(inode));

	get_optional_uint32(c_minor_device, idmef_inode_get_c_minor_device(inode));

	get_optional_uint32(number, idmef_inode_get_number(inode));

        ret = prelude_sql_insert(conn, "Prelude_Inode", "_message_ident, _target_index, _file_index, "
                                 "change_time, change_time_gmtoff, number, major_device, minor_device, c_major_device, "
                                 "c_minor_device",
				 "%" PRIu64 ", %d, %d, %s, %s, %s, %s, %s, %s, %s",
				 message_ident, target_index, file_index, ctime, ctime_gmtoff, number, major_device,
				 minor_device, c_major_device, c_minor_device);

        return ret;
}



static int insert_linkage(prelude_sql_connection_t *conn, uint64_t message_ident, int target_index, int file_index,
                          idmef_linkage_t *linkage) 
{
        int ret;
        char *name, *path, *category;

        if ( ! linkage )
                return 0;
        
        category = prelude_sql_escape(conn, idmef_linkage_category_to_string(idmef_linkage_get_category(linkage)));
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, get_string(idmef_linkage_get_name(linkage)));
        if ( ! name ) {
                free(category);
                return -1;
        }
        
        path = prelude_sql_escape(conn, get_string(idmef_linkage_get_path(linkage)));
        if ( ! path ) {
                free(name);
                free(category);
                return -1;
        }

        ret = prelude_sql_insert(conn, "Prelude_Linkage", "message_ident, target_index, file_index, category, name, path",
                                 "%" PRIu64 ", %d, %d, %s, %s, %s", 
                                 message_ident, target_index, file_index, category, name, path);

        free(name);
        free(path);
        free(category);

	/* FIXME: idmef_file in idmef_linkage is not currently supported by the db */

	return ret;
}



static int insert_file_access(prelude_sql_connection_t *conn, uint64_t message_ident, int target_index, int file_index,
                              idmef_file_access_t *file_access, int index)
{
        int ret;
	prelude_string_t *file_access_permission;
	char *permission;

        if ( ! file_access )
                return 0;
        
        if ( prelude_sql_insert(conn, "Prelude_FileAccess", "_message_ident, _target_index, _file_index, _index",
                                "%" PRIu64 "%d, %d, %d", message_ident, target_index, file_index, index) < 0 )
                return -1;

	file_access_permission = NULL;
	while ( (file_access_permission = idmef_file_access_get_next_permission(file_access, file_access_permission)) ) {

                permission = prelude_sql_escape(conn, get_string(file_access_permission));
                if ( ! permission )
                        return -1;
                
                ret = prelude_sql_insert(conn, "Prelude_FileAccess_Permission",
					 "_message_ident, _target_index, _file_index, _file_access_index, perm",
                                         "%" PRIu64 ", %d, %d, %d, %s",
					 message_ident, target_index, file_index, index, permission);

                free(permission);

                if ( ret < 0 )
                        return ret;
        }

        ret = insert_user_id(conn, message_ident, 'F', target_index, file_index, index, idmef_file_access_get_user_id(file_access));

        return ret;
}



static int insert_checksum(prelude_sql_connection_t *conn, uint64_t message_ident, int target_index, int file_index,
			   idmef_checksum_t *checksum)
{
	char *value = NULL, *key = NULL, *algorithm = NULL;
	int ret = -1;

	value = prelude_sql_escape(conn, get_string(idmef_checksum_get_value(checksum)));
	if ( ! value )
		return -1;

	key = prelude_sql_escape(conn, get_string(idmef_checksum_get_key(checksum)));
	if ( ! key )
		goto error;

	algorithm = prelude_sql_escape(conn, idmef_checksum_algorithm_to_string(idmef_checksum_get_algorithm(checksum)));
        if ( ! algorithm )
                goto error;

	ret = prelude_sql_insert(conn, "Prelude_Checksum",
				 "_message_ident, _target_index, _file_index, value, checksum_key, algorithm",
				 "%" PRIu64 ", %d, %d, %s, %s, %s",
                                 message_ident, target_index, file_index, value, key, algorithm);

error:
	if ( value )
		free(value);

	if ( key )
		free(key);

	if ( algorithm )
		free(algorithm);

	return ret;
}



static int insert_file(prelude_sql_connection_t *conn, uint64_t message_ident, int target_index,
                       idmef_file_t *file, int index) 
{
        int ret = -1;
        idmef_linkage_t *linkage;
	idmef_checksum_t *checksum;
        idmef_file_access_t *file_access;
	int file_access_index;
        char *name = NULL, *path = NULL, *category = NULL, *fstype = NULL, data_size[32], disk_size[32];
        char ctime[IDMEF_TIME_MAX_STRING_SIZE], ctime_gmtoff[16];
        char mtime[IDMEF_TIME_MAX_STRING_SIZE], mtime_gmtoff[16];
        char atime[IDMEF_TIME_MAX_STRING_SIZE], atime_gmtoff[16];

        if ( prelude_sql_time_to_timestamp(idmef_file_get_create_time(file),
					 ctime, sizeof (ctime), ctime_gmtoff, sizeof (ctime_gmtoff), NULL, 0) < 0 )
                return -1;

        if ( prelude_sql_time_to_timestamp(idmef_file_get_modify_time(file),
					 mtime, sizeof (mtime), mtime_gmtoff, sizeof (mtime_gmtoff), NULL, 0) < 0 )
                return -1;

        if ( prelude_sql_time_to_timestamp(idmef_file_get_access_time(file),
					 atime, sizeof(atime), atime_gmtoff, sizeof (atime_gmtoff), NULL, 0) < 0 )
                return -1;

        category = prelude_sql_escape(conn, idmef_file_category_to_string(idmef_file_get_category(file)));
        if ( ! category )
                return -1;

        name = prelude_sql_escape(conn, get_string(idmef_file_get_name(file)));
        if ( ! name )
		return -1;

        path = prelude_sql_escape(conn, get_string(idmef_file_get_path(file)));
        if ( ! path )
		goto error;

	get_optional_uint64(data_size, idmef_file_get_data_size(file));

	get_optional_uint64(disk_size, idmef_file_get_disk_size(file));

	fstype = prelude_sql_escape(conn, get_optional_enum((int *) idmef_file_get_fstype(file),
							    (char *(*)(int)) idmef_file_fstype_to_string));
	if ( ! fstype )
		goto error;
	
        ret = prelude_sql_insert(conn, "Prelude_File", "_message_ident, _target_index, _index, ident, category, name, path, "
                                 "create_time, create_time_gmtoff, modify_time, modify_time_gmtoff, access_time, access_time_gmtoff, "
				 "data_size, disk_size, fstype",
				 "%" PRIu64 ", %d, %d, %" PRIu64 ", %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s",
                                 message_ident, target_index, index, idmef_file_get_ident(file), category, name, path,
				 ctime, ctime_gmtoff, mtime, mtime_gmtoff, atime, atime_gmtoff, data_size, disk_size, fstype);

        if ( ret < 0 )
                goto error;

        file_access = NULL;
	file_access_index = 0;
        while ( (file_access = idmef_file_get_next_file_access(file, file_access)) ) {

                ret = insert_file_access(conn, message_ident, target_index, index, file_access, file_access_index++);
		if ( ret < 0 )
			goto error;
        }

        linkage = NULL;
        while ( (linkage = idmef_file_get_next_linkage(file, linkage)) ) {

                ret = insert_linkage(conn, message_ident, target_index, index, linkage);
		if ( ret < 0 )
			goto error;
        }

        ret = insert_inode(conn, message_ident, target_index, index, idmef_file_get_inode(file));
	if ( ret < 0 )
		goto error;

	checksum = NULL;
	while ( (checksum = idmef_file_get_next_checksum(file, checksum)) ) {

		ret = insert_checksum(conn, message_ident, target_index, index, checksum);
		if ( ret < 0 )
			goto error;
	}

 error:
	if ( name )
		free(name);

	if ( path )
		free(path);

	if ( category )
		free(category);

	if ( fstype )
		free(fstype);

        return ret;
}



static int insert_source(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_source_t *source, int index)
{
        int ret;
        char *interface, *spoofed;

        spoofed = prelude_sql_escape(conn, idmef_source_spoofed_to_string(idmef_source_get_spoofed(source)));
        if ( ! spoofed )
                return -1;

        interface = prelude_sql_escape(conn, get_string(idmef_source_get_interface(source)));
        if ( ! interface ) {
                free(spoofed);
                return -1;
        }

        ret = prelude_sql_insert(conn, "Prelude_Source", "_message_ident, _index, ident, spoofed, interface",
				 "%" PRIu64 ", %d, %" PRIu64 ", %s, %s",
                                 message_ident, index, idmef_source_get_ident(source), spoofed, interface);

        free(spoofed);
        free(interface);
        
        if ( ret < 0 )
                return -1;
        
        if ( insert_node(conn, message_ident, 'S', index, idmef_source_get_node(source)) < 0 )
                return -1;

        if ( insert_user(conn, message_ident, 'S', index, idmef_source_get_user(source)) < 0 )
                return -1;
        
        if ( insert_process(conn, message_ident, 'S', index, idmef_source_get_process(source)) < 0 )
                return -1;
        
        if ( insert_service(conn, message_ident, 'S', index, idmef_source_get_service(source)) < 0 )
                return -1;

        return 1;
}



static int insert_target(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_target_t *target, int index)
{
        int ret;
        idmef_file_t *file;
	int file_index;
        char *interface, *decoy;

        decoy = prelude_sql_escape(conn, idmef_target_decoy_to_string(idmef_target_get_decoy(target)));
        if ( ! decoy )
                return -1;

        interface = prelude_sql_escape(conn, get_string(idmef_target_get_interface(target)));
        if ( ! interface ) {
                free(decoy);
                return -2;
        }

        ret = prelude_sql_insert(conn, "Prelude_Target",
				 "_message_ident, _index, ident, decoy, interface",
				 "%" PRIu64 ", %d, %" PRIu64 ", %s, %s", 
                                 message_ident, index, idmef_target_get_ident(target), decoy, interface);

        free(decoy);
        free(interface);

        if ( ret < 0 )
                return -1;
        
        if ( insert_node(conn, message_ident, 'T', index, idmef_target_get_node(target)) < 0 )
                return -1;
        
        if ( insert_user(conn, message_ident, 'T', index, idmef_target_get_user(target)) < 0 )
                return -1;
        
        if ( insert_process(conn, message_ident, 'T', index, idmef_target_get_process(target)) < 0 )
                return -1;
        
        if ( insert_service(conn, message_ident, 'T', index, idmef_target_get_service(target)) < 0 )
                return -1;

        file = NULL;
        file_index = 0;
        while ( (file = idmef_target_get_next_file(target, file)) ) {

                if ( insert_file(conn, message_ident, index, file, file_index++) < 0 )
                        return -1;
        }

        return 1;
}



static int insert_analyzer(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type,
			   idmef_analyzer_t *analyzer, int depth) 
{
        int ret = -1;
        char *name = NULL, *manufacturer = NULL, *model = NULL, *version = NULL, *class = NULL,
		*ostype = NULL, *osversion = NULL;
	idmef_analyzer_t *sub_analyzer;

        if ( ! analyzer )
                return 0;

        class = prelude_sql_escape(conn, get_string(idmef_analyzer_get_class(analyzer)));
        if ( ! class )
                return -1;

        name = prelude_sql_escape(conn, get_string(idmef_analyzer_get_name(analyzer)));
        if ( ! name )
                return -1;
        
        model = prelude_sql_escape(conn, get_string(idmef_analyzer_get_model(analyzer)));
        if ( ! model )
		goto error;
        
        version = prelude_sql_escape(conn, get_string(idmef_analyzer_get_version(analyzer)));
        if ( ! version )
		goto error;
        
        manufacturer = prelude_sql_escape(conn, get_string(idmef_analyzer_get_manufacturer(analyzer)));
        if ( ! manufacturer )
		goto error;

        ostype = prelude_sql_escape(conn, get_string(idmef_analyzer_get_ostype(analyzer)));
        if ( ! ostype )
		goto error;

        osversion = prelude_sql_escape(conn, get_string(idmef_analyzer_get_osversion(analyzer)));
        if ( ! osversion )
		goto error;
	

        ret = prelude_sql_insert(conn, "Prelude_Analyzer",
				 "_message_ident, _parent_type, _depth, analyzerid, name, manufacturer, "
				 "model, version, class, "
                                 "ostype, osversion",
				 "%" PRIu64 ", '%c', %d, %" PRIu64 ", %s, %s, %s, %s, %s, %s, %s", 
                                 message_ident, parent_type, depth, idmef_analyzer_get_analyzerid(analyzer),
				 name, manufacturer, model, version, class, ostype, osversion);
        
        if ( ret < 0 )
                goto error;
        
        ret = insert_node(conn, message_ident, parent_type, depth, idmef_analyzer_get_node(analyzer));
	if ( ret < 0 )
                goto error;

        ret = insert_process(conn, message_ident, parent_type, depth, idmef_analyzer_get_process(analyzer));
	if ( ret < 0 )
		goto error;

	sub_analyzer = idmef_analyzer_get_analyzer(analyzer);
	if ( sub_analyzer )
		ret = insert_analyzer(conn, message_ident, parent_type, sub_analyzer, depth + 1);

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

        return ret;
}



static int insert_reference(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_reference_t *reference)
{
	char *origin = NULL, *name = NULL, *url = NULL, *meaning = NULL;
	int ret = -1;

        origin = prelude_sql_escape(conn, idmef_reference_origin_to_string(idmef_reference_get_origin(reference)));
        if ( ! origin )
                return -1;

        url = prelude_sql_escape(conn, get_string(idmef_reference_get_url(reference)));
        if ( ! url )
		goto error;

        name = prelude_sql_escape(conn, get_string(idmef_reference_get_name(reference)));
        if ( ! name )
		goto error;

	meaning = prelude_sql_escape(conn, get_string(idmef_reference_get_meaning(reference)));
	if ( ! meaning )
		goto error;

	ret = prelude_sql_insert(conn, "Prelude_Reference", "_message_ident, origin, name, url, meaning",
				 "%" PRIu64 ", %s, %s, %s, %s",
				 message_ident, origin, name, url, meaning);

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


static int insert_classification(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_classification_t *classification) 
{
        int ret;
        char *text;
	idmef_reference_t *reference;

	if ( ! classification )
		return 0;

	text = prelude_sql_escape(conn, get_string(idmef_classification_get_text(classification)));
	if ( ! text )
		return -1;

        ret = prelude_sql_insert(conn, "Prelude_Classification", "_message_ident, ident, text",
                                 "%" PRIu64 ", %" PRIu64 ", %s",
				 message_ident, idmef_classification_get_ident(classification), text);

	free(text);

	reference = NULL;
	while ( (reference = idmef_classification_get_next_reference(classification, reference)) ) {

		ret = insert_reference(conn, message_ident, reference);
		if ( ret < 0 )
			return -1;

	}

	return 1;
}



static int insert_additional_data(prelude_sql_connection_t *conn, uint64_t message_ident,
                                  char parent_type, idmef_additional_data_t *additional_data) 
{
        int ret;
        char *meaning, *type, *data;
                
        if ( ! additional_data )
                return 0;

        type = prelude_sql_escape(conn, idmef_additional_data_type_to_string(idmef_additional_data_get_type(additional_data)));
        if ( ! type )
                return -1;
        
        meaning = prelude_sql_escape(conn, get_string(idmef_additional_data_get_meaning(additional_data)));
        if ( ! meaning ) {
                free(type);
                return -1;
        }

	data = get_data(conn, idmef_additional_data_get_data(additional_data));
	if ( ! data ) {
		free(type);
		free(meaning);
		return -1;
	}
        
        ret = prelude_sql_insert(conn, "Prelude_AdditionalData", "_message_ident, _parent_type, type, meaning, data",
                                 "%" PRIu64 ", '%c', %s, %s, %s", message_ident, parent_type, type, meaning, data);

        free(type);
        free(meaning);        
        free(data);
        
        return ret;
}



static int insert_createtime(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, idmef_time_t *time) 
{
        char utc_time[IDMEF_TIME_MAX_STRING_SIZE];
	char utc_time_usec[16];
	char utc_time_gmtoff[16];
        int ret;

        if ( prelude_sql_time_to_timestamp(time, utc_time, sizeof (utc_time), 
					   utc_time_gmtoff, sizeof (utc_time_gmtoff),
					   utc_time_usec, sizeof (utc_time_usec)) < 0 )
                return -1;

        ret = prelude_sql_insert(conn, "Prelude_CreateTime", "_message_ident, _parent_type, time, gmtoff, usec", 
                                 "%" PRIu64 ", '%c', %s, %s, %s",
				 message_ident, parent_type, utc_time, utc_time_gmtoff, utc_time_usec);

        return ret;
}



static int insert_detecttime(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_time_t *time) 
{        
        char utc_time[IDMEF_TIME_MAX_STRING_SIZE];
	char utc_time_usec[16];
	char utc_time_gmtoff[16];
        int ret;

        if ( ! time )
                return 0;

        if ( prelude_sql_time_to_timestamp(time, utc_time, sizeof (utc_time),
					   utc_time_gmtoff, sizeof (utc_time_gmtoff),
					   utc_time_usec, sizeof (utc_time_usec)) < 0 )
                return -1;

        ret = prelude_sql_insert(conn, "Prelude_DetectTime", "_message_ident, time, gmtoff, usec",
                                 "%" PRIu64 ", %s, %s, %s", message_ident, utc_time, utc_time_gmtoff, utc_time_usec);

        return ret;
}



static int insert_analyzertime(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type, idmef_time_t *time) 
{
        char utc_time[IDMEF_TIME_MAX_STRING_SIZE];
	char utc_time_usec[16];
	char utc_time_gmtoff[16];
        int ret;

        if ( ! time )
                return 0;

        if ( prelude_sql_time_to_timestamp(time, utc_time, sizeof (utc_time), utc_time_gmtoff, sizeof (utc_time_gmtoff),
					 utc_time_usec, sizeof (utc_time_usec)) < 0 )
                return -1;

        ret = prelude_sql_insert(conn, "Prelude_AnalyzerTime", "_message_ident, _parent_type, time, gmtoff, usec",
                                 "%" PRIu64 ", '%c', %s, %s, %s",
				 message_ident, parent_type, utc_time, utc_time_gmtoff, utc_time_usec);

        return ret;
}



static int insert_impact(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_impact_t *impact) 
{
        int ret = -1;
        char *completion = NULL, *type = NULL, *severity = NULL, *description = NULL;

        if ( ! impact )
                return 0;

        completion = prelude_sql_escape(conn, get_optional_enum((int *) idmef_impact_get_completion(impact),
								(char *(*)(int)) idmef_impact_completion_to_string));
        if ( ! completion )
		goto error;

        type = prelude_sql_escape(conn, idmef_impact_type_to_string(idmef_impact_get_type(impact)));
        if ( ! type ) 
                return -1;

        severity = prelude_sql_escape(conn, get_optional_enum((int *) idmef_impact_get_severity(impact),
							      (char *(*)(int))idmef_impact_severity_to_string));
        if ( ! severity )
		goto error;
        
        description = prelude_sql_escape(conn, get_string(idmef_impact_get_description(impact)));
        if ( ! description )
		goto error;
        
        ret = prelude_sql_insert(conn, "Prelude_Impact", "_message_ident, severity, completion, type, description", 
                                 "%" PRIu64 ", %s, %s, %s, %s", 
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



static int insert_action(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_action_t *action)
{
        int ret;
        char *category, *description;

        category = prelude_sql_escape(conn, idmef_action_category_to_string(idmef_action_get_category(action)));
        if ( ! category )
                return -1;

        description = prelude_sql_escape(conn, get_string(idmef_action_get_description(action)));
        if ( ! description ) {
                free(category);
                return -2;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Action", "_message_ident, category, description",
				 "%" PRIu64 ", %s, %s", 
                                 message_ident, category, description);

        free(category);
        free(description);

        return ret;
}



static int insert_confidence(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_confidence_t *confidence) 
{
        int ret;
        char *rating;

        if ( ! confidence )
                return 0;

        rating = prelude_sql_escape(conn, idmef_confidence_rating_to_string(idmef_confidence_get_rating(confidence)));
        if ( ! rating )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_Confidence", "_message_ident, rating, confidence",
				 "%" PRIu64 ", %s, %f",
                                 message_ident, rating, idmef_confidence_get_confidence(confidence));

        free(rating);
        
        return ret;
}



static int insert_assessment(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_assessment_t *assessment) 
{
        idmef_action_t *action;

        if ( ! assessment )
                return 0;

        if ( prelude_sql_insert(conn, "Prelude_Assessment", "_message_ident", "%" PRIu64, message_ident) < 0 )
                return -1;

        if ( insert_impact(conn, message_ident, idmef_assessment_get_impact(assessment)) < 0 )
                return -1;
        
        if ( insert_confidence(conn, message_ident, idmef_assessment_get_confidence(assessment)) < 0 )
                return -1;

        action = NULL;
        while ( (action = idmef_assessment_get_next_action(assessment, action)) ) {

                if ( insert_action(conn, message_ident, action) < 0 )
                        return -1;
        }
        
        return 1;
}



static int insert_overflow_alert(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_overflow_alert_t *overflow_alert) 
{
        char *program, *buffer, size[16];
        int ret;

        program = prelude_sql_escape(conn, get_string(idmef_overflow_alert_get_program(overflow_alert)));
        if ( ! program )
                return -1;

	buffer = get_data(conn, idmef_overflow_alert_get_buffer(overflow_alert));
	if ( ! buffer ) {
		free(buffer);
		return -1;
	}

	get_optional_uint32(size, idmef_overflow_alert_get_size(overflow_alert));

        ret = prelude_sql_insert(conn, "Prelude_OverflowAlert", "_message_ident, program, size, buffer",
				 "%" PRIu64 ", %s, %s, %s", 
                                 message_ident, program, size, buffer);

        free(program);
        free(buffer);

        return ret;
}


static int insert_alertident(prelude_sql_connection_t *conn, uint64_t message_ident, char parent_type,
			     idmef_alertident_t *alertident)
{
	char analyzerid[32];
	int ret;

	get_optional_uint64(analyzerid, idmef_alertident_get_analyzerid(alertident));

	ret = prelude_sql_insert(conn, "Prelude_AlertIdent", "_message_ident, _parent_type, alertident, analyzerid",
				 "%" PRIu64 ", '%c', %" PRIu64 ", %s",
				 message_ident, parent_type, idmef_alertident_get_alertident(alertident), analyzerid);

	return ret;
}


static int insert_tool_alert(prelude_sql_connection_t *conn, uint64_t message_ident, idmef_tool_alert_t *tool_alert) 
{
        char *name, *command;
	idmef_alertident_t *alertident;
        int ret;

        if ( ! tool_alert )
                return 0;

        name = prelude_sql_escape(conn, get_string(idmef_tool_alert_get_name(tool_alert)));
        if ( ! name )
                return -1;

        command = prelude_sql_escape(conn, get_string(idmef_tool_alert_get_command(tool_alert)));
        if ( ! command ) {
                free(name);
                return -2;
        }

        ret = prelude_sql_insert(conn, "Prelude_ToolAlert", "_message_ident, name, command",
				 "%" PRIu64 ", %s, %s",
                                 message_ident, name, command);

        free(name);
        free(command);

	alertident = NULL;
	while ( (alertident = idmef_tool_alert_get_next_alertident(tool_alert, alertident)) ) {
		if ( insert_alertident(conn, message_ident, 'T', alertident) < 0 )
			return -1;
	}

        return 1;
}



static int insert_correlation_alert(prelude_sql_connection_t *conn, uint64_t message_ident,
				    idmef_correlation_alert_t *correlation_alert) 
{
        char *name;
	idmef_alertident_t *alertident;
        int ret;

        if ( ! correlation_alert )
                return 0;

        name = prelude_sql_escape(conn, get_string(idmef_correlation_alert_get_name(correlation_alert)));
        if ( ! name )
                return -1;

        ret = prelude_sql_insert(conn, "Prelude_CorrelationAlert", "_message_ident, name",
				 "%" PRIu64 ", %s",
				 message_ident, name);

        free(name);
 
        if ( ret < 0 )
                return -1;

	alertident = NULL;
	while ( (alertident = idmef_correlation_alert_get_next_alertident(correlation_alert, alertident)) ) {

		if ( insert_alertident(conn, message_ident, 'C', alertident) < 0 )
			return -1;
	}

        return 1;
}



static int get_last_insert_ident(prelude_sql_connection_t *conn, const char *table_name, uint64_t *result)
{
        prelude_sql_table_t *table;
        prelude_sql_row_t *row;
        prelude_sql_field_t *field;

        table = prelude_sql_query(conn, "SELECT max(_ident) FROM %s;", table_name);
        if ( ! table )
                return -1;

        row = prelude_sql_row_fetch(table);
        if ( ! row )
                goto error;

        field = prelude_sql_field_fetch(row, 0);
        if ( ! field )
                goto error;

        *result = prelude_sql_field_value_uint64(field);

        prelude_sql_table_free(table);

        return 0;       

 error:
        prelude_sql_table_free(table);
        return -1;
}



static int insert_heartbeat_ident(prelude_sql_connection_t *conn, idmef_heartbeat_t *heartbeat, uint64_t *result)
{
        uint64_t messageid;
        uint64_t analyzerid;
        idmef_analyzer_t *analyzer;

        messageid = idmef_heartbeat_get_messageid(heartbeat);
        analyzer = idmef_heartbeat_get_analyzer(heartbeat);
        analyzerid = idmef_analyzer_get_analyzerid(analyzer);

        if ( prelude_sql_insert(conn, "Prelude_Heartbeat", "analyzerid, messageid", "%" PRIu64 ", %" PRIu64,
                                analyzerid, messageid) < 0 )
                return -1;

        return get_last_insert_ident(conn, "Prelude_Heartbeat", result);
}



static int insert_alert_ident(prelude_sql_connection_t *conn, idmef_alert_t *alert, uint64_t *result)
{
        uint64_t messageid;
        uint64_t analyzerid;
        idmef_analyzer_t *analyzer;

        messageid = idmef_alert_get_messageid(alert);
        analyzer = idmef_alert_get_analyzer(alert);
        analyzerid = idmef_analyzer_get_analyzerid(analyzer);

        if ( prelude_sql_insert(conn, "Prelude_Alert", "analyzerid, messageid", "%" PRIu64 ", %" PRIu64,
                                analyzerid, messageid) < 0 )
                return -1;

        return get_last_insert_ident(conn, "Prelude_Alert", result);
}



static int insert_alert(prelude_sql_connection_t *conn, idmef_alert_t *alert) 
{
        uint64_t ident;
        idmef_source_t *source;
        idmef_target_t *target;
        idmef_additional_data_t *additional_data;
	int index;

        if ( ! alert )
                return 0;

        if ( insert_alert_ident(conn, alert, &ident) < 0 )
                return -1;
        
        if ( insert_assessment(conn, ident, idmef_alert_get_assessment(alert)) < 0 )
             return -1;
             
        if ( insert_analyzer(conn, ident, 'A', idmef_alert_get_analyzer(alert), 0) < 0 )
                return -1;

        if ( insert_createtime(conn, ident, 'A', idmef_alert_get_create_time(alert)) < 0 )
                return -1;
        
        if ( insert_detecttime(conn, ident, idmef_alert_get_detect_time(alert)) < 0 )
                return -1;
        
        if ( insert_analyzertime(conn, ident, 'A', idmef_alert_get_analyzer_time(alert)) < 0 )
                return -1;

        switch ( idmef_alert_get_type(alert) ) {

        case IDMEF_ALERT_TYPE_DEFAULT:
                break;
                
        case IDMEF_ALERT_TYPE_TOOL:
                if ( insert_tool_alert(conn, ident, idmef_alert_get_tool_alert(alert)) < 0 )
                        return -1;
                break;

        case IDMEF_ALERT_TYPE_OVERFLOW:
                if ( insert_overflow_alert(conn, ident, idmef_alert_get_overflow_alert(alert)) < 0 )
                        return -1;
                break;

        case IDMEF_ALERT_TYPE_CORRELATION:
                if ( insert_correlation_alert(conn, ident, idmef_alert_get_correlation_alert(alert)) < 0 )
                        return -1;
                break;
        }

        index = 0;
        source = NULL;
        while ( (source = idmef_alert_get_next_source(alert, source)) ) {
                
                if ( insert_source(conn, ident, source, index++) < 0 )
                        return -1;
        }

        index = 0;
        target = NULL;
        while ( (target = idmef_alert_get_next_target(alert, target)) ) {

                if ( insert_target(conn, ident, target, index++) < 0 )
                        return -1;
        }

	if ( insert_classification(conn, ident, idmef_alert_get_classification(alert)) < 0 )
		return -1;

        additional_data = NULL;
        while ( (additional_data = idmef_alert_get_next_additional_data(alert, additional_data)) ) {

                if ( insert_additional_data(conn, ident, 'A', additional_data) < 0 )
                        return -1;
        }

        return 1;
}



static int insert_heartbeat(prelude_sql_connection_t *conn, idmef_heartbeat_t *heartbeat) 
{
        uint64_t ident;
        idmef_additional_data_t *additional_data;

        if ( ! heartbeat )
                return 0;

        if ( insert_heartbeat_ident(conn, heartbeat, &ident) < 0 )
                return -1;

        if ( insert_analyzer(conn, ident, 'H', idmef_heartbeat_get_analyzer(heartbeat), 0) < 0 )
                return -2;

        if ( insert_createtime(conn, ident, 'H', idmef_heartbeat_get_create_time(heartbeat)) < 0 )
                return -3;
        
        if ( insert_analyzertime(conn, ident, 'H', idmef_heartbeat_get_analyzer_time(heartbeat)) < 0 )
                return -4;

        additional_data = NULL;
        while ( (additional_data = idmef_heartbeat_get_next_additional_data(heartbeat, additional_data)) ) {

                if ( insert_additional_data(conn, ident, 'H', additional_data) < 0 )
                        return -5;
        }

        return 1;
}



int idmef_db_insert(prelude_db_connection_t *conn, idmef_message_t *message)
{
        prelude_sql_connection_t *sql;
        int ret, ret2;

        if ( ! message )
                return 0;

        ret = -1;

        if ( prelude_db_connection_get_type(conn) != prelude_db_type_sql ) {
                log(LOG_ERR, "SQL database required for classic format!\n");
                return -1;
        }

        sql = prelude_db_connection_get(conn);

        ret2 = prelude_sql_begin(sql);
        if ( ret2 < 0 ) {
                log(LOG_ERR, "error in BEGIN: SQL error code=%d: %s\n",
                prelude_sql_errno(sql), prelude_sql_error(sql));
                
                /* Looks like we won't get very far */
                return ret;
        }
        
        switch ( idmef_message_get_type(message) ) {

        case IDMEF_MESSAGE_TYPE_ALERT:
                ret = insert_alert(sql, idmef_message_get_alert(message));
                break;

        case IDMEF_MESSAGE_TYPE_HEARTBEAT:
                ret = insert_heartbeat(sql, idmef_message_get_heartbeat(message));
                break;

        default:
                log(LOG_ERR, "unknown message type: %d.\n", idmef_message_get_type(message));
                break;
        }

        if ( ret < 0 ) {
                log(LOG_ERR, "error processing IDMEF message: SQL error code=%d: %s\n",
                        prelude_sql_errno(sql), prelude_sql_error(sql));
                        
                ret2 = prelude_sql_rollback(sql);
                if ( ret2 < 0 )
                        /* You're not serious, are you? */
                        log(LOG_ERR, "error in ROLLBACK: SQL error code=%d: %s\n",
                        prelude_sql_errno(sql), prelude_sql_error(sql));
                        
                return ret2;
        }
        
        ret2 = prelude_sql_commit(sql);
        if ( ret2 < 0 ) {
                log(LOG_ERR, "error in COMMIT: SQL error code=%d: %s\n",
                prelude_sql_errno(sql), prelude_sql_error(sql));
                
                /* Error at the very last moment. How sad. */
                if ( ret >= 0 )
                        ret = ret2;
        }


        return ret;
}
