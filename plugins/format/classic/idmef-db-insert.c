/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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
#include <stdlib.h>
#include <inttypes.h>

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>
#include <libprelude/plugin-common.h>
#include <libprelude/idmef.h>
#include <libprelude/idmef-tree-wrap.h>
#include <libprelude/idmef-util.h>

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

static int insert_file(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident, int file_ident, char parent_type, idmef_file_t *file);

static int insert_address(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                          char parent_type, uint64_t node_ident, idmef_address_t *address) 
{
        const char *category;
        char *vlan_name, *addr, *netmask;
        int ret;

	if ( ! address )
		return 0;

        category = idmef_address_category_to_string(idmef_address_get_category(address));
        if ( ! category )
                return -1;

        addr = prelude_sql_escape(conn, idmef_string(idmef_address_get_address(address)));
        if ( ! addr )
                return -2;

        netmask = prelude_sql_escape(conn, idmef_string(idmef_address_get_netmask(address)));
        if ( ! netmask ) {
                free(addr);
                return -3;
        }

        vlan_name = prelude_sql_escape(conn, idmef_string(idmef_address_get_vlan_name(address)));
        if ( ! vlan_name ) {
                free(addr);
                free(netmask);
                return -4;
        }

        ret = prelude_sql_insert(conn, "Prelude_Address", "alert_ident, parent_type, parent_ident, "
				 "category, vlan_name, vlan_num, address, netmask",
				 "%llu, '%c', %llu, '%s', '%s', '%d', '%s', '%s'",
				 alert_ident, parent_type, parent_ident, category, vlan_name, 
				 idmef_address_get_vlan_num(address), addr, netmask);
        
        free(addr);
        free(netmask);
        free(vlan_name);

        return (ret < 0) ? -5 : 1;
}



static int insert_node(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                       char parent_type, idmef_node_t *node) 
{
        int ret;
        const char *category;
        idmef_address_t *address;
        char *location, *name;

        if ( ! node )
                return 0;

        category = idmef_node_category_to_string(idmef_node_get_category(node));
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(idmef_node_get_name(node)));
        if ( ! name )
                return -2;
        
        location = prelude_sql_escape(conn, idmef_string(idmef_node_get_location(node)));
        if ( ! location ) {
                free(name);
                return -3;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Node", "alert_ident, parent_type, parent_ident, category, location, name",
				 "%llu, '%c', %llu, '%s', '%s', '%s'", 
				 alert_ident, parent_type, parent_ident, category, location, name);

        free(name);
        free(location);
        
        if ( ret < 0 )
        	return -4;

	address = NULL;
	while ( (address = idmef_node_get_next_address(node, address)) ) {

                if ( insert_address(conn, alert_ident, parent_ident, parent_type, idmef_node_get_ident(node), address) < 0 )
			return -5;
	}

        return 0;
}



static int insert_userid(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                         char parent_type, idmef_userid_t *userid) 
{
        const char *type;
        char *name;
        int ret;
        
        type = idmef_userid_type_to_string(idmef_userid_get_type(userid));
        if ( ! type )
                return -1;

        name = prelude_sql_escape(conn, idmef_string(idmef_userid_get_name(userid)));
        if ( ! name )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_UserId", "alert_ident, parent_type, parent_ident, ident, type, name, number",
				 "%llu, '%c', %llu, %llu, '%s', '%s', '%u'", 
				 alert_ident, parent_type, parent_ident,
				 idmef_userid_get_ident(userid), type, name, idmef_userid_get_number(userid));

        free(name);

        return ret;
}



static int insert_user(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                       char parent_type, idmef_user_t *user) 
{
        uint64_t tmpid = 0;
        idmef_userid_t *userid;
        const char *category;
        
        if ( ! user )
                return 0;

        category = idmef_user_category_to_string(idmef_user_get_category(user));
        if ( ! category )
                return -1;
        
        if ( prelude_sql_insert(conn, "Prelude_User", "alert_ident, parent_type, parent_ident, category",
				"%llu, '%c', %llu, '%s'", 
				alert_ident, parent_type, parent_ident,	category) < 0 )
		return -2;

	userid = NULL;
	while ( (userid = idmef_user_get_next_userid(user, userid)) ) {

		if ( idmef_userid_get_ident(userid) == 0 )
			idmef_userid_set_ident(userid, tmpid++);

                if ( insert_userid(conn, alert_ident, idmef_userid_get_ident(userid), parent_type, userid) < 0 )
			return -3;
        }

        return 0;
}



static int insert_process(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                          char parent_type, idmef_process_t *process) 
{
	idmef_string_t *process_arg;
	idmef_string_t *process_env;
        char *name, *path, *arg, *env;
        int ret;

        if ( ! process )
                return 0;
        
        name = prelude_sql_escape(conn, idmef_string(idmef_process_get_name(process)));
        if ( ! name )
                return -1;

        path = prelude_sql_escape(conn, idmef_string(idmef_process_get_path(process)));
        if ( ! path ) {
                free(name);
                return -2;
        }

        ret = prelude_sql_insert(conn, "Prelude_Process", "alert_ident, parent_type, parent_ident, name, pid, path",
				 "%llu, '%c', %llu, '%s', '%u', '%s'", 
				 alert_ident, parent_type, parent_ident, name, idmef_process_get_pid(process), path);
        
        free(name);
        free(path);
        
        if ( ret < 0 )
        	return -3;

	process_arg = NULL;
	while ( (process_arg = idmef_process_get_next_arg(process, process_arg)) ) {

                arg = prelude_sql_escape(conn, idmef_string(process_arg));
                if ( ! arg )
                        return -4;
                
                ret = prelude_sql_insert(conn, "Prelude_ProcessArg", "alert_ident, parent_type, parent_ident, arg",
					 "%llu, '%c', %llu, '%s'", 
					 alert_ident, parent_type, parent_ident, arg);

                free(arg);

		if ( ret < 0 )
			return -5;
        }

	process_env = NULL;
	while ( (process_env = idmef_process_get_next_env(process, process_env)) ) {

                env = prelude_sql_escape(conn, idmef_string(process_env));
                if ( ! env )
                        return -6;

                ret = prelude_sql_insert(conn, "Prelude_ProcessEnv", "alert_ident, parent_type, parent_ident, env",
					 "%llu, '%c', %llu, '%s'", 
					 alert_ident, parent_type, parent_ident, env);
                                 
                free(env);

		if ( ret < 0 )
			return -7;
        }

        return 1;
}



static int insert_snmp_service(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t service_ident,
                               uint64_t parent_ident, char parent_type, idmef_snmpservice_t *snmpservice) 
{
        char *oid, *community, *command;
        int ret;

	if ( ! snmpservice )
		return 0;

        oid = prelude_sql_escape(conn, idmef_string(idmef_snmpservice_get_oid(snmpservice)));
        if ( ! oid )
                return -1;
        
        command = prelude_sql_escape(conn, idmef_string(idmef_snmpservice_get_command(snmpservice)));
        if ( ! command ) {
                free(oid);
                return -2;
        }

        community = prelude_sql_escape(conn, idmef_string(idmef_snmpservice_get_community(snmpservice)));
        if ( ! community ) {
                free(oid);
                free(command);
                return -3;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_SnmpService", "alert_ident, parent_type, parent_ident, service_ident, oid, community, command",
				 "%llu, '%c', %llu, %llu, '%s', '%s', '%s'", 
				 alert_ident, parent_type, parent_ident, service_ident, oid, community, command);

        free(oid);
        free(command);
        free(community);

        return (ret < 0) ? -4 : 1;
}



static int insert_web_service(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t service_ident,
                              uint64_t parent_ident, char parent_type, idmef_webservice_t *webservice) 
{
        char *url, *cgi, *method;
        int ret;
        
        if ( ! webservice )
                return 0;

        url = prelude_sql_escape(conn, idmef_string(idmef_webservice_get_url(webservice)));
        if ( ! url )
                return -1;
        
        cgi = prelude_sql_escape(conn, idmef_string(idmef_webservice_get_cgi(webservice)));
        if ( ! cgi ) {
                free(url);
                return -2;
        }
        
        method = prelude_sql_escape(conn, idmef_string(idmef_webservice_get_http_method(webservice)));
        if ( ! method ) {
                free(url);
                free(cgi);
                return -3;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_WebService", "alert_ident, parent_type, parent_ident, service_ident, url, cgi, http_method",
				 "%llu, '%c', %llu, %llu, '%s', '%s', '%s'",
				 alert_ident, parent_type, parent_ident, service_ident, url, cgi, method);

        free(url);
        free(cgi);
        free(method);

        return (ret < 0) ? -4 : 1;
}



static int insert_service(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                          char parent_type, idmef_service_t *service) 
{
        int ret;
        char *name, *protocol, *portlist;
	uint64_t ident;

        if ( ! service )
                return 0;

	ident = idmef_service_get_ident(service);

        name = prelude_sql_escape(conn, idmef_string(idmef_service_get_name(service)));
        if ( ! name )
                return -1;

        protocol = prelude_sql_escape(conn, idmef_string(idmef_service_get_protocol(service)));
        if ( ! protocol ) {
                free(name);
                return -2;
        }

        ret = prelude_sql_insert(conn, "Prelude_Service", "alert_ident, parent_type, parent_ident, name, port, protocol",
				 "%llu, '%c', %llu, '%s', '%u', '%s'", 
				 alert_ident, parent_type, parent_ident, name, idmef_service_get_port(service), protocol);

	free(name);
	free(protocol);

	if ( ret < 0 )
		return -3;

	if ( idmef_string(idmef_service_get_portlist(service)) ) {
		portlist = prelude_sql_escape(conn, idmef_string(idmef_service_get_portlist(service)));
		if ( ! portlist )
			return -3;

		ret = prelude_sql_insert(conn, "Prelude_ServicePortlist", "alert_ident, parent_type, parent_ident, portlist",
					 "%llu, '%c', %llu, '%s'", alert_ident, parent_type, parent_ident, portlist);

		free(portlist);

		if ( ret < 0 )
			return -4;
	}

        switch ( idmef_service_get_type(service)) {

        case web_service:
                if ( insert_web_service(conn, alert_ident, ident, parent_ident, parent_type, idmef_service_get_web(service)) < 0 )
			return -5;
                break;

        case snmp_service:
                if ( insert_snmp_service(conn, alert_ident, ident, parent_ident, parent_type, idmef_service_get_snmp(service)) < 0 )
			return -6;
                break;

        case no_specific_service:
		/* nop */;
                break;
                
        default:
		return -7;
        }

        return 1;
}



static int insert_inode(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                        int file_ident, idmef_inode_t *inode) 
{
        char ctime[MAX_UTC_DATETIME_SIZE] = { '\0' };
	idmef_time_t *time;
	int ret;

        if ( ! inode )
                return 0;

	time = idmef_inode_get_change_time(inode);
        if ( time )
                if ( idmef_time_get_db_timestamp(time, ctime, sizeof(ctime)) < 0 )
			return -1;

	ret = prelude_sql_insert(conn, "Prelude_Inode", "alert_ident, target_ident, file_ident, "
				 "change_time, major_device, minor_device, c_major_device, "
				 "c_minor_device", "%llu, %llu, %d, '%s', %d, %d, %d, %d",
				 alert_ident, target_ident, file_ident, ctime, 
				 idmef_inode_get_major_device(inode),
				 idmef_inode_get_minor_device(inode),
				 idmef_inode_get_c_major_device(inode), 
				 idmef_inode_get_c_minor_device(inode));

	return (ret < 0) ? -2 : 1;
}



static int insert_linkage(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                          int file_ident, idmef_linkage_t *linkage) 
{
        char *name, *path;
        const char *category;
        int ret;

	if ( ! linkage )
		return 0;
        
        category = idmef_linkage_category_to_string(idmef_linkage_get_category(linkage));
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(idmef_linkage_get_name(linkage)));
        if ( ! name )
                return -2;
        
        path = prelude_sql_escape(conn, idmef_string(idmef_linkage_get_path(linkage)));
        if ( ! path ) {
                free(name);
                return -3;
        }

        ret = prelude_sql_insert(conn, "Prelude_Linkage", "alert_ident, target_ident, file_ident, category, name, path",
				 "%llu, %llu, %d, '%s', '%s', '%s'", 
				 alert_ident, target_ident, file_ident, category, name, path);

        free(name);
        free(path);
        
        if ( ret < 0 )
        	return -4;
        
        ret = insert_file(conn, alert_ident, target_ident, file_ident, 'L', idmef_linkage_get_file(linkage));

	return (ret < 0) ? -5 : 1;
}



static int insert_file_access(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                              int file_ident, idmef_file_access_t *file_access)
{
	int ret;

	if ( ! file_access )
		return 0;
	
	if ( prelude_sql_insert(conn, "Prelude_FileAccess", "alert_ident, target_ident, file_ident",
				"%llu, %llu, %llu", alert_ident, target_ident, file_ident) < 0 )
        	return -1;

        /*
         * FIXME: access permission ?
         */

	ret = insert_userid(conn, alert_ident, target_ident, 'F', idmef_file_access_get_userid(file_access));

	return (ret < 0) ? -2 : 1;
}



static int insert_file(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                       int file_ident, char parent_type, idmef_file_t *file) 
{
        int ret;
        const char *category;
        char *name, *path;
        idmef_linkage_t *linkage;
        idmef_file_access_t *file_access;
	idmef_time_t *time;
        char ctime[MAX_UTC_DATETIME_SIZE] = { '\0' };
        char mtime[MAX_UTC_DATETIME_SIZE]= { '\0' }, atime[MAX_UTC_DATETIME_SIZE] = { '\0' };

        if ( ! file )
                return 0;

        /*
         * why no parent_ident ???
         */
        category = idmef_file_category_to_string(idmef_file_get_category(file));
        if ( ! category )
                return -1;

        name = prelude_sql_escape(conn, idmef_string(idmef_file_get_name(file)));
        if ( ! name )
                return -2;

        path = prelude_sql_escape(conn, idmef_string(idmef_file_get_path(file)));
        if ( ! path ) {
                free(name);
                return -3;
        }

	time = idmef_file_get_create_time(file);
	if ( time )
		if ( idmef_time_get_db_timestamp(time, ctime, sizeof (ctime)) < 0 )
			return -4;

	time = idmef_file_get_modify_time(file);
	if ( time )
		if ( idmef_time_get_db_timestamp(time, mtime, sizeof (mtime)) < 0 )
			return -5;

	time = idmef_file_get_access_time(file);
	if ( time )
		if ( idmef_time_get_db_timestamp(time, atime, sizeof (atime)) < 0 )
			return -6;

        ret = prelude_sql_insert(conn, "Prelude_File", "alert_ident, target_ident, ident, category, name, path, "
				 "create_time, modify_time, access_time, data_size, disk_size", "%llu, %llu, %d, '%s', "
				 "'%s', '%s', '%s', '%s', '%s', '%d', %d", 
				 alert_ident, target_ident, file_ident, category, name, path, ctime, mtime, atime,
				 idmef_file_get_data_size(file), idmef_file_get_disk_size(file));

        free(name);
        free(path);
        
        if ( ret < 0 )
        	return -4;

	file_access = NULL;
	while ( (file_access = idmef_file_get_next_file_access(file, file_access)) ) {

		if ( insert_file_access(conn, alert_ident, target_ident, file_ident, file_access) < 0 )
			return -7;
	}

	linkage = NULL;
	while ( (linkage = idmef_file_get_next_file_linkage(file, linkage)) ) {

                if ( insert_linkage(conn, alert_ident, target_ident, file_ident, linkage) < 0 )
			return -8;
	}

        if ( insert_inode(conn, alert_ident, target_ident, file_ident, idmef_file_get_inode(file)) < 0 )
		return -9;
        
        return 0;
}



static int insert_source(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_source_t *source)
{
        int ret;
        char *interface;
        const char *spoofed;
	uint64_t ident;

        if ( ! source )
                return 0;

	ident = idmef_source_get_ident(source);

        spoofed = idmef_spoofed_to_string(idmef_source_get_spoofed(source));
        if ( ! spoofed )
                return -1;

        interface = prelude_sql_escape(conn, idmef_string(idmef_source_get_interface(source)));
        if ( ! interface )
                return -2;

        ret = prelude_sql_insert(conn, "Prelude_Source", "alert_ident, ident, spoofed, interface", "%llu, %llu, '%s', '%s'",
				 alert_ident, ident, spoofed, interface);

        free(interface);
        
        if ( ret < 0 )
        	return -3;
        
        if ( insert_node(conn, alert_ident, ident, 'S', idmef_source_get_node(source)) < 0 )
		return -4;

        if ( insert_user(conn, alert_ident, ident, 'S', idmef_source_get_user(source)) < 0 )
		return -5;
        
        if ( insert_process(conn, alert_ident, ident, 'S', idmef_source_get_process(source)) < 0 )
		return -6;
        
        if ( insert_service(conn, alert_ident, ident, 'S', idmef_source_get_service(source)) < 0 )
		return -7;

        return 1;
}



static int insert_target(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_target_t *target)
{
        int ret;
        char *interface;
        const char *decoy;
	uint64_t ident;
	idmef_file_t *file;
	uint64_t fileid;
        
        if ( ! target )
                return 0;

	ident = idmef_target_get_ident(target);

        decoy = idmef_spoofed_to_string(idmef_target_get_decoy(target));
        if ( ! decoy )
                return -1;

        interface = prelude_sql_escape(conn, idmef_string(idmef_target_get_interface(target)));
        if ( ! interface )
                return -2;

        ret = prelude_sql_insert(conn, "Prelude_Target", "alert_ident, ident, decoy, interface", "%llu, %llu, '%s', '%s'", 
				 alert_ident, ident, decoy, interface);

	free(interface);

        if ( ret < 0 )
        	return -3;
        
        if ( insert_node(conn, alert_ident, ident, 'T', idmef_target_get_node(target)) < 0 )
		return -4;
        
        if ( insert_user(conn, alert_ident, ident, 'T', idmef_target_get_user(target)) < 0 )
		return -5;
        
        if ( insert_process(conn, alert_ident, ident, 'T', idmef_target_get_process(target)) < 0 )
		return -6;
        
        if ( insert_service(conn, alert_ident, ident, 'T', idmef_target_get_service(target)) < 0 )
		return -7;

	if ( prelude_sql_insert(conn, "Prelude_FileList", "alert_ident, target_ident", "%llu, %llu", 
				alert_ident, ident) < 0 )
		return -8;

	file = NULL;
	fileid = 0;
	while ( (file = idmef_target_get_next_file(target, file)) ) {

		if ( idmef_file_get_ident(file) == 0 )
			idmef_file_set_ident(file, fileid++);

		if ( insert_file(conn, alert_ident, ident, idmef_file_get_ident(file), 'T', file) < 0 )
			return -9;
	}

        return 1;
}



static int insert_analyzer(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_analyzer_t *analyzer) 
{
        int ret;
        char *manufacturer, *model, *version, *class, *ostype, *osversion;
	uint64_t analyzerid;

	if ( ! analyzer )
		return 0;

	analyzerid = idmef_analyzer_get_analyzerid(analyzer);

        class = prelude_sql_escape(conn, idmef_string(idmef_analyzer_get_class(analyzer)));
        if ( ! class )
                return -1;
        
        model = prelude_sql_escape(conn, idmef_string(idmef_analyzer_get_model(analyzer)));
        if ( ! model ) {
                free(class);
                return -2;
        }
        
        version = prelude_sql_escape(conn, idmef_string(idmef_analyzer_get_version(analyzer)));
        if ( ! version ) {
                free(class);
                free(model);
                return -3;
        }
        
        manufacturer = prelude_sql_escape(conn, idmef_string(idmef_analyzer_get_manufacturer(analyzer)));
        if ( ! manufacturer ) {
                free(class);
                free(model);
                free(version);
                return -4;
        }

        ostype = prelude_sql_escape(conn, idmef_string(idmef_analyzer_get_ostype(analyzer)));
        if ( ! ostype ) {
                free(class);
                free(model);
                free(version);
                free(manufacturer);
                return -5;
        }

        osversion = prelude_sql_escape(conn, idmef_string(idmef_analyzer_get_osversion(analyzer)));
        if ( ! osversion ) {
                free(class);
                free(model);
                free(version);
                free(manufacturer);
                free(ostype);
                return -6;
        }

        ret = prelude_sql_insert(conn, "Prelude_Analyzer", "parent_ident, parent_type, analyzerid, manufacturer, model, version, class, "
				 "ostype, osversion", "%llu, '%c', %llu, '%s', '%s', '%s', '%s', '%s', '%s'", 
				 parent_ident, parent_type, analyzerid, manufacturer, model, version, class, ostype, osversion);

        free(manufacturer);
        free(model);
        free(class);
        free(version);
        free(ostype);
        free(osversion);
        
        if ( ret < 0 )
        	return -7;
        
        if ( insert_node(conn, parent_ident, analyzerid, parent_type, idmef_analyzer_get_node(analyzer)) < 0 )
		return -8;

        if ( insert_process(conn, parent_ident, analyzerid, parent_type, idmef_analyzer_get_process(analyzer)) < 0 )
		return -9;

        return 1;
}



static int insert_classification(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_classification_t *classification) 
{
        char *name, *url;
        const char *origin;
        int ret;

        origin = idmef_classification_origin_to_string(idmef_classification_get_origin(classification));
        if ( ! origin )
                return -1;

        url = prelude_sql_escape(conn, idmef_string(idmef_classification_get_url(classification)));
        if ( ! url )
                return -2;
        
        name = prelude_sql_escape(conn, idmef_string(idmef_classification_get_name(classification)));
        if ( ! name ) {
                free(url);
                return -3;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Classification", "alert_ident, origin, name, url",
				 "%llu, '%s', '%s', '%s'", alert_ident, origin, name, url);

        free(url);
        free(name);

        return (ret < 0) ? -4: 1;
}



static int insert_additional_data(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_additional_data_t *additional_data) 
{
        const char *type;
        char buf[1024], *meaning, *data;
        int ret;

	if ( ! additional_data )
		return 0;

        type = idmef_additional_data_type_to_string(idmef_additional_data_get_type(additional_data));
        if ( ! type )
                return -1;

        if ( idmef_additionaldata_data_to_string(additional_data, buf, sizeof (buf)) < 0 )
                return -2;

        meaning = prelude_sql_escape(conn, idmef_string(idmef_additional_data_get_meaning(additional_data)));
        if ( ! meaning )
                return -3;

        data = prelude_sql_escape(conn, buf);
        if ( ! data ) {
                free(meaning);
                return -4;
        }

        ret = prelude_sql_insert(conn, "Prelude_AdditionalData", "parent_ident, parent_type, type, meaning, data",
				 "%llu, '%c', '%s', '%s', '%s'", parent_ident, parent_type, type, meaning, data);

        free(data);
        free(meaning);

        return (ret < 0) ? -5 : 1;
}



static int insert_createtime(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_time_t *time) 
{
        char utc_time[MAX_UTC_DATETIME_SIZE], ntpstamp[MAX_NTP_TIMESTAMP_SIZE], *u, *n;
        int ret;
        
        if ( idmef_time_get_db_timestamp(time, utc_time, sizeof(utc_time)) < 0 )
		return -1;

        if ( idmef_time_get_ntp_timestamp(time, ntpstamp, sizeof(ntpstamp)) < 0 )
		return -2;

        u = prelude_sql_escape(conn, utc_time);
        if ( ! u )
                return -3;

        n = prelude_sql_escape(conn, ntpstamp);
        if ( ! n ) {
                free(u);
                return -4;
        }

        ret = prelude_sql_insert(conn, "Prelude_CreateTime", "parent_ident, parent_type, time, ntpstamp", 
				 "%llu, '%c', '%s', '%s'", parent_ident, parent_type, u, n);

        free(u);
        free(n);

        return (ret < 0) ? -5 : 1;
}



static int insert_detecttime(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_time_t *time) 
{        
        char utc_time[MAX_UTC_DATETIME_SIZE], ntpstamp[MAX_NTP_TIMESTAMP_SIZE], *u, *n;
        int ret;
        
        if ( ! time )
                return 0;

        if ( idmef_time_get_db_timestamp(time, utc_time, sizeof(utc_time)) < 0 )
		return -1;

        if ( idmef_time_get_ntp_timestamp(time, ntpstamp, sizeof(ntpstamp)) < 0 )
		return -2;

        u = prelude_sql_escape(conn, utc_time);
        if ( ! u )
                return -3;

        n = prelude_sql_escape(conn, ntpstamp);
        if ( ! n ) {
                free(u);
                return -4;
        }

        ret = prelude_sql_insert(conn, "Prelude_DetectTime", "alert_ident, time, ntpstamp",
				 "%llu, '%s', '%s'", alert_ident, u, n);

        free(u);
        free(n);

        return (ret < 0) ? -5 : 1;
}



static int insert_analyzertime(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_time_t *time) 
{
        char utc_time[MAX_UTC_DATETIME_SIZE], ntpstamp[MAX_NTP_TIMESTAMP_SIZE], *u, *n;
        int ret;

        if ( ! time )
                return 0;

        if ( idmef_time_get_db_timestamp(time, utc_time, sizeof(utc_time)) < 0 )
		return -1;

        if ( idmef_time_get_ntp_timestamp(time, ntpstamp, sizeof(ntpstamp)) < 0 )
		return -2;

        u = prelude_sql_escape(conn, utc_time);
        if ( ! u )
                return -3;

        n = prelude_sql_escape(conn, ntpstamp);
        if ( ! n ) {
                free(u);
                return -4;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_AnalyzerTime", "parent_ident, parent_type, time, ntpstamp",
				 "%llu, '%c', '%s', '%s'", parent_ident, parent_type, u, n);

        free(u);
        free(n);

        return (ret < 0) ? -5 : 1;
}



static int insert_impact(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_impact_t *impact) 
{
        const char *completion, *type, *severity;
        char *description;
        int ret;

        if ( ! impact )
                return 0;

        completion = idmef_impact_completion_to_string(idmef_impact_get_completion(impact));
        if ( ! completion ) {
		
                return -1;
	}

        type = idmef_impact_type_to_string(idmef_impact_get_type(impact));
        if ( ! type )
                return -2;

        severity = idmef_impact_severity_to_string(idmef_impact_get_severity(impact));
        if ( ! severity )
                return -3;

        description = prelude_sql_escape(conn, idmef_string(idmef_impact_get_description(impact)));
        if ( ! description )
                return -4;
        
        ret = prelude_sql_insert(conn, "Prelude_Impact", "alert_ident, severity, completion, type, description", 
				 "%llu, '%s', '%s', '%s', '%s'", 
				 alert_ident, severity, completion, type, description);

        free(description);

        return (ret < 0) ? -5 : 1;
}



static int insert_action(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_action_t *action)
{
        const char *category;
        char *description;
        int ret;

        category = idmef_action_category_to_string(idmef_action_get_category(action));
        if ( ! category )
                return -1;

        description = prelude_sql_escape(conn, idmef_string(idmef_action_get_description(action)));
        if ( ! description )
                return -2;
        
        ret = prelude_sql_insert(conn, "Prelude_Action", "alert_ident, category, description", "%llu, '%s', '%s'", 
				 alert_ident, category, description);

        free(description);

        return (ret < 0) ? -1 : 1;
}



static int insert_confidence(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_confidence_t *confidence) 
{
	int ret;

        if ( ! confidence )
                return 0;
        
        ret =  prelude_sql_insert(conn, "Prelude_Confidence", "alert_ident, rating, confidence", "%llu, '%s', %f",
				  alert_ident, idmef_confidence_rating_to_string(idmef_confidence_get_rating(confidence)),
				  idmef_confidence_get_confidence(confidence));

	return (ret < 0) ? -1 : 1;
}



static int insert_assessment(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_assessment_t *assessment) 
{
        idmef_action_t *action;

        if ( ! assessment )
                return 0;

        if ( prelude_sql_insert(conn, "Prelude_Assessment", "alert_ident", "%llu", alert_ident) < 0 )
		return -1;

        if ( insert_impact(conn, alert_ident, idmef_assessment_get_impact(assessment)) < 0 )
		return -2;
        
	if ( insert_confidence(conn, alert_ident, idmef_assessment_get_confidence(assessment)) < 0 )
		return -3;

	action = NULL;
	while ( (action = idmef_assessment_get_next_action(assessment, action)) ) {

                if ( insert_action(conn, alert_ident, action) < 0 )
			return -4;
        }
        
        return 1;
}



static int insert_overflow_alert(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_overflow_alert_t *overflow_alert) 
{
        char *program, *buffer;
        int ret;

        program = prelude_sql_escape(conn, idmef_string(idmef_overflow_alert_get_program(overflow_alert)));
        if ( ! program )
                return -1;

	/* FIXME: data will not be null byte terminated forever  */
        buffer = prelude_sql_escape(conn, idmef_data(idmef_overflow_alert_get_buffer(overflow_alert)));
        if ( ! buffer ) {
                free(program);
                return -2;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_OverflowAlert", "alert_ident, program, size, buffer", "%llu, '%s', %d, '%s'", 
				 alert_ident, program, idmef_overflow_alert_get_size(overflow_alert), buffer);

        free(buffer);
        free(program);

        return (ret < 0) ? -3 : 1;
}



static int insert_tool_alert(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_tool_alert_t *tool_alert) 
{
        char *name, *command;
        int ret;

	if ( ! tool_alert )
		return 0;

        /*
         * FIXME use alert_ident ?
         */
        name = prelude_sql_escape(conn, idmef_string(idmef_tool_alert_get_name(tool_alert)));
        if ( ! name )
                return -1;

        command = prelude_sql_escape(conn, idmef_string(idmef_tool_alert_get_command(tool_alert)));
        if ( ! command ) {
                free(name);
                return -2;
        }

        ret = prelude_sql_insert(conn, "Prelude_ToolAlert", "alert_ident, name, command", "%llu, '%s', '%s'",
				 alert_ident, name, command);

        free(name);
        free(command);

        return (ret < 0) ? -3 : 1;
}



static int insert_correlation_alert(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_correlation_alert_t *correlation_alert) 
{
        char *name;
        idmef_alertident_t *idmef_alertident;
	int ret;

	if ( ! correlation_alert )
		return 0;

        /*
         * FIXME: use alert_ident ?
         */
        name = prelude_sql_escape(conn, idmef_string(idmef_correlation_alert_get_name(correlation_alert)));
        if ( ! name )
                return -1;

        ret = prelude_sql_insert(conn, "Prelude_CorrelationAlert", "ident, name", "%llu, '%s'", alert_ident, name);

	free(name);
 
	if ( ret < 0 )
		return -2;

	idmef_alertident = NULL;
	while ( (idmef_alertident = idmef_correlation_alert_get_next_alertident(correlation_alert, idmef_alertident)) ) {

                if ( prelude_sql_insert(conn,
					"Prelude_CorrelationAlert_Alerts", "ident, alert_ident", "%llu, %llu",
					alert_ident, idmef_alertident_get_alertident(idmef_alertident)) < 0 )
			return -3;
        }

	return 1;
}



static int insert_alert(prelude_sql_connection_t *conn, idmef_alert_t *alert) 
{
	uint64_t ident;
        uint64_t tmpid;
        idmef_source_t *source;
        idmef_target_t *target;
        idmef_classification_t *classification;
        idmef_additional_data_t *additional_data;

	if ( ! alert )
		return 0;

	ident = idmef_alert_get_ident(alert);
        
        if ( prelude_sql_insert(conn, "Prelude_Alert", "ident", "%llu", ident) < 0 )
		return -1;

        if ( insert_assessment(conn, ident, idmef_alert_get_assessment(alert)) < 0 )
	     return -2;
	     
	if ( insert_analyzer(conn, ident, 'A', idmef_alert_get_analyzer(alert)) < 0 )
		return -3;

        if ( insert_createtime(conn, ident, 'A', idmef_alert_get_create_time(alert)) < 0 )
		return -4;
        
        if ( insert_detecttime(conn, ident, idmef_alert_get_detect_time(alert)) < 0 )
		return -5;
        
        if ( insert_analyzertime(conn, ident, 'A', idmef_alert_get_analyzer_time(alert)) < 0 )
		return -6;

        switch ( idmef_alert_get_type(alert) ) {

        case idmef_default:
		/* nop */
                break;

        case idmef_tool_alert:
                if ( insert_tool_alert(conn, ident, idmef_alert_get_tool_alert(alert)) < 0 )
			return -7;
                break;

        case idmef_overflow_alert:
                if ( insert_overflow_alert(conn, ident, idmef_alert_get_overflow_alert(alert)) < 0 )
			return -8;
                break;

        case idmef_correlation_alert:
		if ( insert_correlation_alert(conn, ident, idmef_alert_get_correlation_alert(alert)) < 0 )
			return -9;
                break;

	default:
		/* nop */;
        }

        tmpid = 0;
	source = NULL;
	while ( (source = idmef_alert_get_next_source(alert, source)) ) {

                if ( idmef_source_get_ident(source) == 0 )
                        idmef_source_set_ident(source, tmpid++);
                
                if ( insert_source(conn, ident, source) < 0 )
			return -10;
        }

        tmpid = 0;
	target = NULL;
	while ( (target = idmef_alert_get_next_target(alert, target)) ) {

		if ( idmef_target_get_ident(target) == 0 )
			idmef_target_set_ident(target, tmpid++);

                if ( insert_target(conn, ident, target) < 0 )
			return -11;
        }

	classification = NULL;
	while ( (classification = idmef_alert_get_next_classification(alert, classification)) ) {

                if ( insert_classification(conn, ident, classification) < 0 )
			return -12;
	}

	additional_data = NULL;
	while ( (additional_data = idmef_alert_get_next_additional_data(alert, additional_data)) ) {

                if ( insert_additional_data(conn, ident, 'A', additional_data) < 0 )
			return -13;
	}

        return 1;
}




static int insert_heartbeat(prelude_sql_connection_t *conn, idmef_heartbeat_t *heartbeat) 
{
	uint64_t ident;
	idmef_additional_data_t *additional_data;

	if ( ! heartbeat )
		return 0;

	ident = idmef_heartbeat_get_ident(heartbeat);

        if ( prelude_sql_insert(conn, "Prelude_Heartbeat", "ident", "%llu", ident) < 0 )
		return -1;
        
        if ( insert_analyzer(conn, ident, 'H', idmef_heartbeat_get_analyzer(heartbeat)) < 0 )
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



int idmef_db_insert(prelude_db_connection_t *conn, const idmef_message_t *message)
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

        case idmef_alert_message:
                ret = insert_alert(sql, idmef_message_get_alert(message));
                break;

        case idmef_heartbeat_message:
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

