/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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
#include <sys/types.h>
#include <time.h> /* for radom numbers generator */
#include <sys/time.h>

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>
#include <libprelude/plugin-common.h>
#include <libprelude/idmef-tree.h>
#include <libprelude/idmef-tree-func.h>

#include "plugin-db.h"
#include "idmef-util.h"
#include "idmef-db-output.h"


#ifdef DEBUG

/*
 * for an unknown reason, we don't see warning about
 * invalid fmt arguments when using db_plugin_insert().
 */
 /*
#define db_plugin_insert(tbl, field, fmt, args...) \
        printf(fmt, args); db_plugin_insert(tbl, field, fmt, args)
*/
#endif

/* FIXME: this solution is probably not threads-safe */

typedef struct {
	struct list_head list;
	
	char data[64];
} number_str_rep_t; 

static LIST_HEAD(number_list);

static char *int2str(uint64_t x) 
{
	number_str_rep_t *s;
	
	s = malloc(sizeof(number_str_rep_t));
	
	if (!s) {
		log(LOG_ERR, "out of memory\n");
		return NULL;
	}
	
	list_add_tail(&s->list, &number_list);
	
	snprintf(s->data, 64, "%llu", x);
	return s->data;
}

static char *float2str(float x) 
{
	number_str_rep_t *s;
	
	s = malloc(sizeof(number_str_rep_t));
	
	if (!s) {
		log(LOG_ERR, "out of memory\n");
		return NULL;
	}
	
	list_add_tail(&s->list, &number_list);
	
	snprintf(s->data, 64, "%f", x);
	return s->data;	
}

static void numbers_cleanup(void)
{
        number_str_rep_t *entry;
	struct list_head *tmp, *n;
	
	list_for_each_safe(tmp, n, &number_list) {
		entry = list_entry(tmp, number_str_rep_t, list);
		list_del(&(entry->list));
	}
}

#define DB_STR(x)  	db_plugin_escape(x)
#define DB_IDMEF_STR(x)    db_plugin_escape(idmef_string(x))
#define DB_IDENT(x)        ((x) ? int2str(x) : "NULL")
#define DB_INT(x) 	 int2str(x)
#define DB_INT_PTR(x)      ((x) ? int2str(*x) : "NULL")
#define DB_FLOAT(x)	float2str(x)

static uint64_t get_source_types(idmef_alert_t *alert)
{
	return 0x01; /* for now... */
}

static uint64_t get_target_types(idmef_alert_t *alert)
{
	return 0x01; /* for now... */
}


static int insert_source(uint64_t alert_id, idmef_source_t *source)
{
	struct list_head *tmp;
	idmef_node_t *node;
	idmef_address_t *address;
	uint64_t node_id;
	int ret;
	
	node = source->node;
	
	if (node) {
		/* FIXME: node_id should be taken from database sequence generator */
	
		node_id = random();	
		
		/* insert node into Source_Nodes_New */
		
		list_for_each(tmp, &node->address_list) {
			address = list_entry(tmp, idmef_address_t, list);
			ret = db_plugin_insert("Source_Nodes_New",
				"node_id, node_ident, category, location, name, "
				"addr_ident, addr_category, vlan_name, vlan_num, address, "
				"netmask", 
				"int8 '%lld', %s, %ld, %s, %s, "
				"%s, %ld, %s, %ld, %s, "
				"%s",
				node_id, DB_IDENT(node->ident), node->category, 
				DB_IDMEF_STR(&node->location), DB_IDMEF_STR(&node->name),
				DB_IDENT(address->ident), address->category, 
				DB_IDMEF_STR(&address->vlan_name),
				address->vlan_num, DB_IDMEF_STR(&address->address),
				DB_IDMEF_STR(&address->netmask));
			
			if (ret < 0)
				return -1;
		}
		
		/* insert information into Source_Nodes_List */
		
		ret = db_plugin_insert("Source_Nodes_List", 
			"alert_id, node_id, spoofed, interface",
			"int8 '%lld', int8 '%lld', %ld, %s", 
			alert_id, node_id, source->spoofed, DB_IDMEF_STR(&source->interface));

		if (ret < 0)
			return -1;
		
		/* rewrite idents */
		ret = db_plugin_query("SELECT Insert_Source_Node(int8 '%lld')", node_id);
		
		if (ret < 0)
			return -1;		
	}
	
	/* other source objects here */
	
	return 0; /* success */
}

static int insert_target(uint64_t alert_id, idmef_target_t *target)
{	
	struct list_head *tmp;
	idmef_node_t *node;
	idmef_address_t *address;
	uint64_t node_id;
	
	int ret;
	
	node = target->node;
	
	if (node) {
		/* FIXME: node_id should be taken from database sequence generator */
	
		node_id = random();	
		
		/* insert node into Target_Nodes_New */
		
		list_for_each(tmp, &node->address_list) {
			address = list_entry(tmp, idmef_address_t, list);
			ret = db_plugin_insert("Target_Nodes_New",
				"node_id, node_ident, category, location, name, "
				"addr_ident, addr_category, vlan_name, vlan_num, address, "
				"netmask", 
				"int8 '%lld', %s, %ld, %s, %s, "
				"%s, %ld, %s, %ld, %s, "
				"%s",
				node_id, DB_INT(node->ident), node->category, 
				DB_IDMEF_STR(&node->location), DB_IDMEF_STR(&node->name),
				DB_INT(address->ident), address->category, 
				DB_IDMEF_STR(&address->vlan_name),
				address->vlan_num, DB_IDMEF_STR(&address->address),
				DB_IDMEF_STR(&address->netmask));
			
			if (ret < 0)
				return -1;
		}
		
		/* insert information into Target_Nodes_List */
		
		ret = db_plugin_insert("Target_Nodes_List", 
			"alert_id, node_id, decoy, interface",
			"int8 '%lld', int8 '%lld', %ld, %s", 
			alert_id, node_id, target->decoy, DB_IDMEF_STR(&target->interface));

		if (ret < 0)
			return -1;
		
		/* rewrite idents */
		ret = db_plugin_query("SELECT Insert_Target_Node(int8 '%lld')", node_id);
		
		if (ret < 0)
			return -1;		
	}
	
	/* other source objects here */
	
	return 0; /* success */
}

static int insert_alert(idmef_alert_t *alert) 
{
	idmef_assessment_t *assessment;
	idmef_confidence_t *confidence;
	idmef_impact_t *impact;
	
	struct list_head *tmp;
	idmef_source_t *source;
	idmef_target_t *target;
	uint64_t alert_id;
	int ret;
	
	struct timeval tv;
	struct timezone tz;
	
	if (!alert)
		return 0;
	
	/* begin transaction */
	ret = db_plugin_begin();
	if (ret < 0)
		return -1;
	
	/* get alert_id */
	
	/* FIXME: alert_id should be taken from database sequence generator */
	
	gettimeofday(&tv, &tz);
	srandom(tv.tv_usec ^ getpid());
	alert_id = random();
	
	/* insert stuff */

	/* Analyzer */
	ret = db_plugin_insert("Analyzers", 
		"ident, manufacturer, model, version, class, ostype, osversion",
		"int8 '%lld', %s, %s, %s, %s, %s, %s", 
		alert->analyzer.analyzerid, 
		DB_IDMEF_STR(&alert->analyzer.manufacturer),
		DB_IDMEF_STR(&alert->analyzer.model),
		DB_IDMEF_STR(&alert->analyzer.version),
		DB_IDMEF_STR(&alert->analyzer.class),
		DB_IDMEF_STR(&alert->analyzer.ostype),
		DB_IDMEF_STR(&alert->analyzer.osversion));
	if (ret < 0)
		goto rollback;
	
	/* add analyzer->node */

	/* Alert */
	
	assessment = alert->assessment;
	impact = (assessment) ? (assessment->impact) : NULL;
	confidence = (assessment) ? (assessment->confidence) : NULL;
	
	ret = db_plugin_insert("Alerts", 
		"id, ident, "
		"impact_severity, impact_completion, impact_type, impact_description, "
		"confidence_rating, confidence, "
		"create_time_sec, create_time_usec, detect_time_sec, detect_time_usec, "
		"analyzer_time_sec, analyzer_time_usec, "
		"analyzer_id, "
		"tool_alert, correlation_alert, " 
		"source_types, target_types",
		"int8 '%lld', int8 '%lld', "
		"%s, %s, %s, %s, " /* alert->impact */
		"%s, %s, " /* alert->confidence */
		"%ld, %ld, %s, %s, %s, %s, " /* alert->*_time */
		"int8 '%llu', " /* analyzerid */
		"%s, %s, " /* tool_alert, correlation_alert */
		"int8 '%llu', int8 '%llu'", /* source_types, target_types */
		alert_id, alert->ident,  
		/* FIXME: check handling of 0 enum values here */
		(impact) ? (DB_INT(impact->severity)) : "NULL",
		(impact) ? (DB_INT(impact->completion)) : "NULL",
		(impact) ? (DB_INT(impact->type)) : "NULL",
		(impact) ? (DB_IDMEF_STR(&(impact->description))) : "NULL",
		(confidence) ? DB_INT(confidence->rating) : "NULL",
		(confidence) ? DB_FLOAT(confidence->confidence) : "NULL",
		alert->create_time.sec, alert->create_time.usec,
		(alert->detect_time) ? DB_INT(alert->detect_time->sec) : "NULL",
		(alert->detect_time) ? DB_INT(alert->detect_time->usec) : "NULL",
		(alert->analyzer_time) ? DB_INT(alert->analyzer_time->sec) : "NULL",
		(alert->analyzer_time) ? DB_INT(alert->analyzer_time->usec) : "NULL",		
		alert->analyzer.analyzerid, 
		"NULL", "NULL", /* tool_alert, correlation_alert */
		get_source_types(alert), get_target_types(alert));

	if (ret < 0)
		goto rollback;
	
	
	/* Source(s) */

	list_for_each(tmp, &alert->source_list) {
		source = list_entry(tmp, idmef_source_t, list);
		ret = insert_source(alert_id, source);
		if (ret < 0)
			goto rollback;
	}	

	/* Target(s) */

	list_for_each(tmp, &alert->target_list) {
		target = list_entry(tmp, idmef_target_t, list);
		ret = insert_target(alert_id, target);
		if (ret < 0)
			goto rollback;
	}
	
	/* commit */
	ret = db_plugin_commit();
	if (ret < 0)
		return -1;

	
	return 0;  /* success */

rollback:
	db_plugin_rollback();

	return -1;
}

static int insert_heartbeat(idmef_heartbeat_t *heartbeat) 
{
	return 0;  /* unimplemented */
}

int idmef_db_output(idmef_message_t *msg) 
{
        int ret = -1;
        
        switch (msg->type) {

        case idmef_alert_message:
                ret = insert_alert(msg->message.alert);
                break;

        case idmef_heartbeat_message:
                ret = insert_heartbeat(msg->message.heartbeat);
                break;

        default:
                log(LOG_ERR, "unknown message type: %d.\n", msg->type);
                break;
        }

        if ( ret < 0 ) 
                log(LOG_ERR, "error processing IDMEF message.\n");

	numbers_cleanup();

        return ret;
}
