/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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
#include <libprelude/idmef-tree.h>
#include <libprelude/idmef-tree-func.h>
#include <libprelude/idmef-util.h>


#include "sql-connection-data.h"
#include "sql.h"
#include "db-type.h"
#include "db-connection.h"
#include "idmef-db-output.h"

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
                          char parent_type, uint64_t node_ident, idmef_address_t *addr) 
{
        const char *category;
        char *vlan_name, *address, *netmask;
        int ret;

        category = idmef_address_category_to_string(addr->category);
        if ( ! category )
                return -1;
        
        address = prelude_sql_escape(conn, idmef_string(&addr->address));
        if ( ! address )
                return -1;
        
        netmask = prelude_sql_escape(conn, idmef_string(&addr->netmask));
        if ( ! netmask ) {
                free(address);
                return -1;
        }
        
        vlan_name = prelude_sql_escape(conn, idmef_string(&addr->vlan_name));
        if ( ! vlan_name ) {
                free(address);
                free(netmask);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Address", "alert_ident, parent_type, parent_ident, "
                          "category, vlan_name, vlan_num, address, netmask",
                          "%llu, '%c', %llu, '%s', '%s', '%d', '%s', '%s'",
                         alert_ident, parent_type, parent_ident, category, vlan_name, addr->vlan_num, address, netmask);
        
        free(address);
        free(netmask);
        free(vlan_name);

        return ret;
}




static int insert_node(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                       char parent_type, idmef_node_t *node) 
{
        int ret;
        const char *category;
        idmef_address_t *addr;
        struct list_head *tmp;
        char *location, *name;

        if ( ! node )
                return 0;

        category = idmef_node_category_to_string(node->category);
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(&node->name));
        if ( ! name )
                return -1;
        
        location = prelude_sql_escape(conn, idmef_string(&node->location));
        if ( ! location ) {
                free(name);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Node", "alert_ident, parent_type, parent_ident, category, location, name",
                         "%llu, '%c', %llu, '%s', '%s', '%s'", alert_ident, parent_type, parent_ident,
                         category, location, name);
        
        free(name);
        free(location);
        
        if ( ret < 0 )
        	return ret;

        list_for_each(tmp, &node->address_list) {
                addr = list_entry(tmp, idmef_address_t, list);

                ret = insert_address(conn, alert_ident, parent_ident, parent_type, node->ident, addr);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}




static int insert_userid(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                         char parent_type, idmef_userid_t *uid) 
{
        char *name;
        const char *type;
        int ret;
        
        type = idmef_userid_type_to_string(uid->type);
        if ( ! type )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(&uid->name));
        if ( ! name )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_UserId", "alert_ident, parent_type, parent_ident, ident, type, name, number",
                         "%llu, '%c', %llu, %llu, '%s', '%s', '%u'", alert_ident, parent_type, parent_ident,
                         uid->ident, type, name, uid->number);
       
        free(name);
        
        return ret;
}



static int insert_user(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                       char parent_type, idmef_user_t *user) 
{
        int ret;
        uint64_t tmpid = 0;
        idmef_userid_t *uid;
        const char *category;
        struct list_head *tmp;
        
        if ( ! user )
                return 0;

        category = idmef_user_category_to_string(user->category);
        if ( ! category )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_User", "alert_ident, parent_type, parent_ident, category",
                         "%llu, '%c', %llu, '%s'", alert_ident, parent_type, parent_ident,
                         category);
        if ( ret < 0 )
        	return ret;
        
        list_for_each(tmp, &user->userid_list) {
                uid = list_entry(tmp, idmef_userid_t, list);

                if ( uid->ident == 0 )
                        uid->ident = tmpid++;
                
                ret = insert_userid(conn, alert_ident, user->ident, parent_type, uid);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}



static int insert_process(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                          char parent_type, idmef_process_t *process) 
{
        struct list_head *tmp;
        idmef_string_item_t *str;
        char *name, *path, *env, *arg;
        int ret;
        
        if ( ! process )
                return 0;
        
        name = prelude_sql_escape(conn, idmef_string(&process->name));
        if ( ! name )
                return -1;
        
        path = prelude_sql_escape(conn, idmef_string(&process->path));
        if ( ! path ) {
                free(name);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Process", "alert_ident, parent_type, parent_ident, name, pid, path",
                         "%llu, '%c', %llu, '%s', '%u', '%s'", alert_ident, parent_type, parent_ident,
                         name, process->pid, path);
        
        free(name);
        free(path);
        
        if ( ret < 0 )
        	return ret;
        
        list_for_each(tmp, &process->arg_list) {
                str = list_entry(tmp, idmef_string_item_t, list);

                arg = prelude_sql_escape(conn, idmef_string(&str->string));
                if ( ! arg )
                        return -1;
                
                prelude_sql_insert(conn, "Prelude_ProcessArg", "alert_ident, parent_type, parent_ident, arg",
                                 "%llu, '%c', %llu, '%s'", alert_ident, parent_type, parent_ident, arg);

                free(arg);
        }

        list_for_each(tmp, &process->env_list) {
                str = list_entry(tmp, idmef_string_item_t, list);

                env = prelude_sql_escape(conn, idmef_string(&str->string));
                if ( ! env )
                        return -1;
                
                ret = prelude_sql_insert(conn, "Prelude_ProcessEnv", "alert_ident, parent_type, parent_ident, env",
                                 "%llu, '%c', %llu, '%s'", alert_ident, parent_type, parent_ident, env);
                                 
                free(env);

		if ( ret < 0 )
			return ret;

        }

        return 0;
}



static int insert_snmp_service(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t service_ident,
                               uint64_t parent_ident, char parent_type, idmef_snmpservice_t *snmp) 
{
        char *oid, *community, *command;
        int ret;

        oid = prelude_sql_escape(conn, idmef_string(&snmp->oid));
        if (! oid )
                return -1;
        
        command = prelude_sql_escape(conn, idmef_string(&snmp->command));
        if ( ! command ) {
                free(oid);
                return -1;
        }
        
        community = prelude_sql_escape(conn, idmef_string(&snmp->community));
        if ( ! community ) {
                free(oid);
                free(command);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_SnmpService", "alert_ident, parent_type, parent_ident, service_ident, oid, community, command",
                         "%llu, '%c', %llu, %llu, '%s', '%s', '%s'", alert_ident, parent_type, parent_ident, service_ident,
                         oid, community, command);

        free(oid);
        free(command);
        free(community);

        return ret;
}



static int insert_web_service(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t service_ident,
                              uint64_t parent_ident, char parent_type, idmef_webservice_t *web) 
{
        char *url, *cgi, *method;
        int ret;
        
        if ( ! web )
                return 0;

        url = prelude_sql_escape(conn, idmef_string(&web->url));
        if ( ! url )
                return -1;
        
        cgi = prelude_sql_escape(conn, idmef_string(&web->cgi));
        if ( ! cgi ) {
                free(url);
                return -1;
        }
        
        method = prelude_sql_escape(conn, idmef_string(&web->http_method));
        if ( ! method ) {
                free(url);
                free(cgi);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_WebService", "alert_ident, parent_type, parent_ident, service_ident, url, cgi, http_method",
                         "%llu, '%c', %llu, %llu, '%s', '%s', '%s'", alert_ident, parent_type, parent_ident, service_ident,
                         url, cgi, method);

        free(url);
        free(cgi);
        free(method);

        return ret;
}



static int insert_portlist(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                           char parent_type, idmef_string_t *portlist) 
{
        char *plist;
        
        plist = prelude_sql_escape(conn, idmef_string(portlist));
        if ( ! plist )
                return -1;
        
        return prelude_sql_insert(conn, "Prelude_ServicePortlist", "alert_ident, parent_type, parent_ident, portlist",
                         "%llu, '%c', %llu, '%s'", alert_ident, parent_type, parent_ident, plist);
}





static int insert_service(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t parent_ident,
                          char parent_type, idmef_service_t *service) 
{
        int ret;
        char *name, *protocol;

        if ( ! service )
                return 0;
        
        name = prelude_sql_escape(conn, idmef_string(&service->name));
        if ( ! name )
                return -1;
        
        protocol = prelude_sql_escape(conn, idmef_string(&service->protocol));
        if ( ! protocol ) {
                free(name);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Service", "alert_ident, parent_type, parent_ident, name, port, protocol",
                         "%llu, '%c', %llu, '%s', '%u', '%s'", alert_ident, parent_type, parent_ident,
                         name, service->port, protocol);
	if ( ret < 0 ) {
	        free(name);
	        free(protocol);
		return ret;
	}

        if ( idmef_string(&service->portlist) )
        	ret = insert_portlist(conn, alert_ident, parent_ident, parent_type, &service->portlist);
        
        free(name);
        free(protocol);
        
        if ( ret < 0 )
        	return ret;

        switch (service->type) {
        case web_service:
                ret = insert_web_service(conn, alert_ident, service->ident, parent_ident, parent_type, service->specific.web);
                break;

        case snmp_service:
                ret = insert_snmp_service(conn, alert_ident, service->ident, parent_ident, parent_type, service->specific.snmp);
                break;

        case no_specific_service:
                ret = 0;
                break;
                
        default:
                ret = -1;
                break;
        }

        return ret;
}



static int insert_inode(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                        int file_ident, idmef_inode_t *inode) 
{
        char ctime[MAX_UTC_DATETIME_SIZE] = { '\0' };

        if ( ! inode )
                return 0;

        if ( inode->change_time )
                idmef_get_db_timestamp(inode->change_time, ctime, sizeof(ctime));
        
        return prelude_sql_insert(conn, "Prelude_Inode", "alert_ident, target_ident, file_ident, "
                         "change_time, major_device, minor_device, c_major_device, "
                         "c_minor_device", "%llu, %llu, %d, '%s', %d, %d, %d, %d",
                         alert_ident, target_ident, file_ident, ctime, inode->major_device,
                         inode->minor_device, inode->c_major_device, inode->c_minor_device);

}



static int insert_linkage(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                          int file_ident, idmef_linkage_t *linkage) 
{
        char *name, *path;
        const char *category;
        int ret;
        
        category = idmef_linkage_category_to_string(linkage->category);
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(&linkage->name));
        if (! name )
                return -1;
        
        path = prelude_sql_escape(conn, idmef_string(&linkage->path));
        if ( ! path ) {
                free(name);
                return -1;
        }
                
        ret = prelude_sql_insert(conn, "Prelude_Linkage", "alert_ident, target_ident, file_ident, category, name, path",
                         "%llu, %llu, %d, '%s', '%s', '%s'", alert_ident, target_ident, file_ident, category, name, path);
        
        free(name);
        free(path);
        
        if ( ret < 0 )
        	return ret;
        
        return insert_file(conn, alert_ident, target_ident, file_ident, 'L', linkage->file);
}




static int insert_file_access(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                              int file_ident, idmef_file_access_t *access)
{
	int ret;
	
        ret = prelude_sql_insert(conn, "Prelude_FileAccess", "alert_ident, target_ident, file_ident",
                         "%llu, %llu, %llu", alert_ident, target_ident, file_ident);
        if ( ret < 0 )
        	return ret;

        /*
         * FIXME: access permission ?
         */
        
        return insert_userid(conn, alert_ident, target_ident, 'F', &access->userid);
}




static int insert_file(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident,
                       int file_ident, char parent_type, idmef_file_t *file) 
{
        int ret;
        char *name, *path;
        const char *category;
        struct list_head *tmp;
        idmef_linkage_t *linkage;
        idmef_file_access_t *access;
        char ctime[MAX_UTC_DATETIME_SIZE] = { '\0' };
        char mtime[MAX_UTC_DATETIME_SIZE]= { '\0' }, atime[MAX_UTC_DATETIME_SIZE] = { '\0' };

        if ( ! file )
                return 0;

        /*
         * why no parent_ident ???
         */
        category = idmef_file_category_to_string(file->category);
        if ( ! category )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(&file->name));
        if ( ! name )
                return -1;
        
        path = prelude_sql_escape(conn, idmef_string(&file->path));
        if ( ! path ) {
                free(name);
                return -1;
        }

        if ( file->create_time )
                idmef_get_db_timestamp(file->create_time, ctime, sizeof(ctime));

        if ( file->modify_time )
                idmef_get_db_timestamp(file->modify_time, mtime, sizeof(mtime));

        if ( file->access_time )
                idmef_get_db_timestamp(file->access_time, atime, sizeof(atime));
                
        ret = prelude_sql_insert(conn, "Prelude_File", "alert_ident, target_ident, ident, category, name, path, "
                         "create_time, modify_time, access_time, data_size, disk_size", "%llu, %llu, %d, '%s', "
                         "'%s', '%s', '%s', '%s', '%s', '%d', %d", alert_ident, target_ident,
                         file_ident, category, name, path, ctime, mtime, atime, file->data_size, file->disk_size);

        free(name);
        free(path);
        
        if ( ret < 0 )
        	return ret;

        list_for_each(tmp, &file->file_access_list) {
                access = list_entry(tmp, idmef_file_access_t, list);

                ret = insert_file_access(conn, alert_ident, target_ident, file_ident, access);
                if ( ret < 0 )
                        return ret;
        }

        list_for_each(tmp, &file->file_linkage_list) {
                linkage = list_entry(tmp, idmef_linkage_t, list);
                
                ret = insert_linkage(conn, alert_ident, target_ident, file_ident, linkage);
                if ( ret < 0 )
                        return ret;
        }

        ret = insert_inode(conn, alert_ident, target_ident, file_ident, file->inode);
        if ( ret < 0 )
                return ret;
        
        return 0;
}






static int insert_source(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_source_t *source)
{
        int ret;
        char *interface;
        const char *spoofed;

        if ( ! source )
                return 0;

        spoofed = idmef_source_spoofed_to_string(source->spoofed);
        if ( ! spoofed )
                return -1;

        interface = prelude_sql_escape(conn, idmef_string(&source->interface));
        if ( ! interface )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_Source", "alert_ident, ident, spoofed, interface",
                         "%llu, %llu, '%s', '%s'", alert_ident, source->ident, spoofed, interface);
        
        free(interface);
        
        if ( ret < 0 )
        	return ret;
        
        ret = insert_node(conn, alert_ident, source->ident, 'S', source->node);
        if ( ret < 0 )
                return ret;

        ret = insert_user(conn, alert_ident, source->ident, 'S', source->user);
        if ( ret < 0 )
                return ret;
        
        ret = insert_process(conn, alert_ident, source->ident, 'S', source->process);
        if ( ret < 0 )
                return ret;
        
        ret = insert_service(conn, alert_ident, source->ident, 'S', source->service);
        if ( ret < 0 )
                return ret;
        
        return 0;
}



static int insert_file_list(prelude_sql_connection_t *conn, uint64_t alert_ident, uint64_t target_ident, struct list_head *file_list) 
{
        int ret;
        uint64_t tmpid = 0;
        idmef_file_t *file;
        struct list_head *tmp;

        if ( list_empty(file_list) )
                return 0;
        
        ret = prelude_sql_insert(conn, "Prelude_FileList", "alert_ident, target_ident",
                         "%llu, %llu", alert_ident, target_ident);
        if ( ret < 0 )
        	return ret;
        
        list_for_each(tmp, file_list) {
                file = list_entry(tmp, idmef_file_t, list);

                file->ident = tmpid++;

                ret = insert_file(conn, alert_ident, target_ident, file->ident, 'T', file);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}





static int insert_target(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_target_t *target)
{
        int ret;
        char *interface;
        const char *decoy;
        
        if ( ! target )
                return 0;

        decoy = idmef_target_decoy_to_string(target->decoy);
        if ( ! decoy )
                return -1;
        
        interface = prelude_sql_escape(conn, idmef_string(&target->interface));
        if ( ! interface )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_Target", "alert_ident, ident, decoy, interface",
                         "%llu, %llu, '%s', '%s'", alert_ident, target->ident,
                         decoy, interface);
        if ( ret < 0 )
        	goto err;
        
        ret = insert_node(conn, alert_ident, target->ident, 'T', target->node);
        if ( ret < 0 )
                goto err;
        
        ret = insert_user(conn, alert_ident, target->ident, 'T', target->user);
        if ( ret < 0 )
                goto err;
        
        ret = insert_process(conn, alert_ident, target->ident, 'T', target->process);
        if ( ret < 0 )
                goto err;
        
        ret = insert_service(conn, alert_ident, target->ident, 'T', target->service);
        if ( ret < 0 )
                goto err;

        ret = insert_file_list(conn, alert_ident, target->ident, &target->file_list);

 err:
        free(interface);
        return ret;
}




static int insert_analyzer(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_analyzer_t *analyzer) 
{
        int ret;
        char *manufacturer, *model, *version, *class, *ostype, *osversion;

        class = prelude_sql_escape(conn, idmef_string(&analyzer->class));
        if ( ! class )
                return -1;
        
        model = prelude_sql_escape(conn, idmef_string(&analyzer->model));
        if ( ! model ) {
                free(class);
                return -1;
        }
        
        version = prelude_sql_escape(conn, idmef_string(&analyzer->version));
        if ( ! version ) {
                free(class);
                free(model);
                return -1;
        }
        
        manufacturer = prelude_sql_escape(conn, idmef_string(&analyzer->manufacturer));
        if ( ! manufacturer ) {
                free(class);
                free(model);
                free(version);
                return -1;
        }

        ostype = prelude_sql_escape(conn, idmef_string(&analyzer->ostype));
        if ( ! ostype ) {
                free(class);
                free(model);
                free(version);
                free(manufacturer);
                return -1;
        }

        osversion = prelude_sql_escape(conn, idmef_string(&analyzer->osversion));
        if ( ! ostype ) {
                free(class);
                free(model);
                free(version);
                free(manufacturer);
                free(ostype);
                return -1;
        }

        
        ret = prelude_sql_insert(conn, "Prelude_Analyzer", "parent_ident, parent_type, analyzerid, manufacturer, model, version, class, "
                         "ostype, osversion", "%llu, '%c', %llu, '%s', '%s', '%s', '%s', '%s', '%s'", parent_ident,
                         parent_type, analyzer->analyzerid, manufacturer, model, version, class, ostype, osversion);
        
        free(class);
        free(model);
        free(version);
        free(manufacturer);
        free(ostype);
        free(osversion);
        
        if ( ret < 0 )
        	return ret;
        
        ret = insert_node(conn, parent_ident, analyzer->analyzerid, parent_type, analyzer->node);
        if ( ret < 0 )
                return ret;
        
        ret = insert_process(conn, parent_ident, analyzer->analyzerid, parent_type, analyzer->process);
        if ( ret < 0 )
                return ret;

        return 0;
}




static int insert_classification(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_classification_t *class) 
{
        char *name, *url;
        const char *origin;
        int ret;

        origin = idmef_classification_origin_to_string(class->origin);
        if ( ! origin )
                return -1;

        url = prelude_sql_escape(conn, idmef_string(&class->url));
        if ( ! url )
                return -1;
        
        name = prelude_sql_escape(conn, idmef_string(&class->name));
        if ( ! name ) {
                free(url);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_Classification", "alert_ident, origin, name, url",
                         "%llu, '%s', '%s', '%s'", alert_ident, origin, name, url);

        free(url);
        free(name);

        return ret;
}



static int insert_data(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_additional_data_t *ad) 
{
        int size;
        const char *type, *ptr;
        char buf[1024], *meaning, *data;
        int ret;

        type = idmef_additional_data_type_to_string(ad->type);
        if ( ! type )
                return -1;

        size = sizeof(buf);
        
        ptr = idmef_additional_data_to_string(ad, buf, &size);
        if ( ! ptr )
                return -1;

        meaning = prelude_sql_escape(conn, idmef_string(&ad->meaning));
        if ( ! meaning ) 
                return -1;
        
        data = prelude_sql_escape(conn, ptr);
        if ( ! data ) {
                free(meaning);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_AdditionalData", "parent_ident, parent_type, type, meaning, data",
                         "%llu, '%c', '%s', '%s', '%s'", parent_ident, parent_type, type, meaning, data);

        free(data);
        free(meaning);

        return ret;
}




static int insert_createtime(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_time_t *time) 
{
        char utc_time[MAX_UTC_DATETIME_SIZE], ntpstamp[MAX_NTP_TIMESTAMP_SIZE], *u, *n;
        int ret;
        
        idmef_get_db_timestamp(time, utc_time, sizeof(utc_time));
        idmef_get_ntp_timestamp(time, ntpstamp, sizeof(ntpstamp));
        
        u = prelude_sql_escape(conn, utc_time);
        if ( ! u )
                return -1;
        
        n = prelude_sql_escape(conn, ntpstamp);
        if ( ! n ) {
                free(u);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_CreateTime", "parent_ident, parent_type, time, ntpstamp", 
                         "%llu, '%c', '%s', '%s'", parent_ident, parent_type, u, n);

        free(u);
        free(n);

        return ret;
}



static int insert_detecttime(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_time_t *time) 
{        
        char utc_time[MAX_UTC_DATETIME_SIZE], ntpstamp[MAX_NTP_TIMESTAMP_SIZE], *u, *n;
        int ret;
        
        if ( ! time )
                return 0;
        
        idmef_get_db_timestamp(time, utc_time, sizeof(utc_time));
        idmef_get_ntp_timestamp(time, ntpstamp, sizeof(ntpstamp));
        
        u = prelude_sql_escape(conn, utc_time);
        if ( ! u )
                return -1;
        
        n = prelude_sql_escape(conn, ntpstamp);
        if ( ! n ) {
                free(u);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_DetectTime", "alert_ident, time, ntpstamp",
                          "%llu, '%s', '%s'", alert_ident, u, n);
        
        free(u);
        free(n);

        return ret;
}



static int insert_analyzertime(prelude_sql_connection_t *conn, uint64_t parent_ident, char parent_type, idmef_time_t *time) 
{
        char utc_time[MAX_UTC_DATETIME_SIZE], ntpstamp[MAX_NTP_TIMESTAMP_SIZE], *u, *n;
        int ret;
        
        if ( ! time )
                return 0;
        
        idmef_get_db_timestamp(time, utc_time, sizeof(utc_time));
        idmef_get_ntp_timestamp(time, ntpstamp, sizeof(ntpstamp));
        
        u = prelude_sql_escape(conn, utc_time);
        if ( ! u )
                return -1;
        
        n = prelude_sql_escape(conn, ntpstamp);
        if ( ! n ) {
                free(u);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_AnalyzerTime", "parent_ident, parent_type, time, ntpstamp",
                          "%llu, '%c', '%s', '%s'", parent_ident, parent_type, u, n);
        
        free(u);
        free(n);

        return ret;
}




static int insert_impact(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_impact_t *impact) 
{
        char *desc;
        const char *completion, *type, *severity;
        int ret;
        
        if ( ! impact )
                return 0;

        completion = idmef_impact_completion_to_string(impact->completion);
        if ( ! completion )
                return -1;

        type = idmef_impact_type_to_string(impact->type);
        if ( ! type )
                return -1;

        severity = idmef_impact_severity_to_string(impact->severity);
        if ( ! severity )
                return -1;
        
        desc = prelude_sql_escape(conn, idmef_string(&impact->description));
        if ( ! desc )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_Impact", "alert_ident, severity, completion, type, description",
                         "%llu, '%s', '%s', '%s', '%s'", alert_ident, severity, completion, type, desc);

        free(desc);
        
        return ret;
}




static int insert_action(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_action_t *action)
{
        char *desc;
        const char *category;
        int ret;

        category = idmef_action_category_to_string(action->category);
        if ( ! category )
                return -1;
        
        desc = prelude_sql_escape(conn, idmef_string(&action->description));
        if ( ! desc )
                return -1;
        
        ret = prelude_sql_insert(conn, "Prelude_Action", "alert_ident, category, description",
                         "%llu, '%s', '%s'", alert_ident, category, desc);

        free(desc);

        return ret;
}




static int insert_confidence(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_confidence_t *confidence) 
{
        if ( ! confidence )
                return 0;
        
        return prelude_sql_insert(conn, "Prelude_Confidence", "alert_ident, rating, confidence", "%llu, '%s', %f",
                         alert_ident, idmef_confidence_rating_to_string(confidence->rating),
                         confidence->confidence);

}




static int insert_assessment(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_assessment_t *assessment) 
{
        int ret;
        struct list_head *tmp;
        idmef_action_t *action;

        if ( ! assessment )
                return 0;

        ret = prelude_sql_insert(conn, "Prelude_Assessment", "alert_ident", "%llu", alert_ident);
        if ( ret < 0 )
        	return ret;
        
        ret = insert_impact(conn, alert_ident, assessment->impact);
        if ( ret < 0 )
                return ret;
        
        ret = insert_confidence(conn, alert_ident, assessment->confidence);
        if ( ret < 0 )
                return ret;
        
        list_for_each(tmp, &assessment->action_list) {
                action = list_entry(tmp, idmef_action_t, list);

                ret = insert_action(conn, alert_ident, action);
                if ( ret < 0 )
                        return ret;
        }
        
        return 0;
}




static int insert_overflow_alert(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_overflow_alert_t *overflow) 
{
        char *program, *buffer;
        int ret;
        
        program = prelude_sql_escape(conn, idmef_string(&overflow->program));
        if ( ! program )
                return -1;

        buffer = prelude_sql_escape(conn, overflow->buffer);
        if ( ! buffer ) {
                free(program);
                return -1;
        }
        
        ret = prelude_sql_insert(conn, "Prelude_OverflowAlert", "alert_ident, program, size, buffer",
                         "%llu, '%s', %d, '%s'", alert_ident, program, overflow->size, buffer);

        free(buffer);
        free(program);
        
        return ret;
}




static int insert_tool_alert(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_tool_alert_t *tool) 
{
        char *name, *command;
        int ret;
                
        /*
         * FIXME use alert_ident ?
         */
        name = prelude_sql_escape(conn, idmef_string(&tool->name));
        if ( ! name )
                return -1;

        command = prelude_sql_escape(conn, idmef_string(&tool->command));
        if ( ! command ) {
                free(name);
                return -1;
        }

        ret = prelude_sql_insert(conn, "Prelude_ToolAlert", "alert_ident, name, command", "%llu, '%s', '%s'",
                         alert_ident, name, command);

        free(name);
        free(command);

        return ret;
}



static int insert_correlation_alert(prelude_sql_connection_t *conn, uint64_t alert_ident, idmef_correlation_alert_t *correlation) 
{
        char *name;
        struct list_head *tmp;
        idmef_alertident_t *ai;
        int ret;
                
        /*
         * FIXME: use alert_ident ?
         */
        name = prelude_sql_escape(conn, idmef_string(&correlation->name));
        if ( ! name )
                return -1;

        ret = prelude_sql_insert(conn, "Prelude_CorrelationAlert", "ident, name", "%llu, '%s'",
                         alert_ident, name);
        free(name);

        list_for_each(tmp, &correlation->alertident_list){
                ai = list_entry(tmp, idmef_alertident_t, list);
                prelude_sql_insert(conn, "Prelude_CorrelationAlert_Alerts", "ident, alert_ident", "%llu", alert_ident);
        }

        return ret;
}





static int insert_alert(prelude_sql_connection_t *conn, idmef_alert_t *alert) 
{
        int ret;
        uint64_t tmpid;
        struct list_head *tmp;
        idmef_source_t *source;
        idmef_target_t *target;
        idmef_classification_t *class;
        idmef_additional_data_t *data;
        
        ret = prelude_sql_insert(conn, "Prelude_Alert", "ident", "%llu", alert->ident);
	if ( ret < 0 )
		return ret;

        ret = insert_assessment(conn, alert->ident, alert->assessment);
        if ( ret < 0 )
                return ret;
        
        ret = insert_analyzer(conn, alert->ident, 'A', &alert->analyzer);
        if ( ret < 0 )
                return ret;
        
        ret = insert_createtime(conn, alert->ident, 'A', &alert->create_time);
        if ( ret < 0 )
                return ret;
        
        ret = insert_detecttime(conn, alert->ident, alert->detect_time);
        if ( ret < 0 )
                return ret;
        
        ret = insert_analyzertime(conn, alert->ident, 'A', alert->analyzer_time);
        if ( ret < 0 )
                return ret;

        switch (alert->type) {
        case idmef_default:
                ret = 0;
                break;

        case idmef_tool_alert:
                ret = insert_tool_alert(conn, alert->ident, alert->detail.tool_alert);
                break;

        case idmef_overflow_alert:
                ret = insert_overflow_alert(conn, alert->ident, alert->detail.overflow_alert);
                break;

        case idmef_correlation_alert:
                ret = insert_correlation_alert(conn, alert->ident, alert->detail.correlation_alert);
                break;
        }

        if ( ret < 0 )
                return ret;

        tmpid = 0;
        list_for_each(tmp, &alert->source_list) {
                source = list_entry(tmp, idmef_source_t, list);

                if ( source->ident == 0 )
                        source->ident = tmpid++;
                
                ret = insert_source(conn, alert->ident,source);
                if ( ret < 0 )
                        return ret;
        }

        tmpid = 0;
        list_for_each(tmp, &alert->target_list) {
                target = list_entry(tmp, idmef_target_t, list);

                if ( target->ident == 0 )
                        target->ident = tmpid++;
                
                ret = insert_target(conn, alert->ident, target);
                if ( ret < 0 )
                        return ret;
        }
        
        list_for_each(tmp, &alert->classification_list) {
                class = list_entry(tmp, idmef_classification_t, list);
                ret = insert_classification(conn, alert->ident, class);
                if ( ret < 0 )
                        return ret;
        }

        list_for_each(tmp, &alert->additional_data_list) {
                data = list_entry(tmp, idmef_additional_data_t, list);
                ret = insert_data(conn, alert->ident, 'A', data);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}




static int insert_heartbeat(prelude_sql_connection_t *conn, idmef_heartbeat_t *heartbeat) 
{
        int ret;
        struct list_head *tmp;
        idmef_additional_data_t *data;

        ret = prelude_sql_insert(conn, "Prelude_Heartbeat", "ident", "%llu", heartbeat->ident);
        if ( ret < 0 )
        	return ret;
        
        ret = insert_analyzer(conn, heartbeat->ident, 'H', &heartbeat->analyzer);
        if ( ret < 0 )
                return ret;
        
        ret = insert_createtime(conn, heartbeat->ident, 'H', &heartbeat->create_time);
        if ( ret < 0 )
                return ret;
        
        ret = insert_analyzertime(conn, heartbeat->ident, 'H', heartbeat->analyzer_time);
        if ( ret < 0 )
                return ret;
        
        list_for_each(tmp, &heartbeat->additional_data_list) {
                data = list_entry(tmp, idmef_additional_data_t, list);

                ret = insert_data(conn, heartbeat->ident, 'H', data);
                if ( ret < 0 )
                        return ret;
        }

        return 0;
}
        




int idmef_db_output(prelude_db_connection_t *conn, idmef_message_t *msg) 
{
	prelude_sql_connection_t *sql;
        int ret, ret2;

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
        
        switch (msg->type) {

        case idmef_alert_message:
                ret = insert_alert(sql, msg->message.alert);
                break;

        case idmef_heartbeat_message:
                ret = insert_heartbeat(sql, msg->message.heartbeat);
                break;

        default:
                log(LOG_ERR, "unknown message type: %d.\n", msg->type);
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

