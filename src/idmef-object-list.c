/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>
#include <libprelude/idmef-string.h>
#include <libprelude/idmef-type.h>
#include <libprelude/idmef-value.h>
#include <libprelude/idmef-tree-wrap.h>
#include <libprelude/idmef-object.h>

#include "idmef-object-list.h"

struct listed_object {
	struct list_head list;
	idmef_object_t *object;
};

struct idmef_object_list {
	struct list_head *pos;
	struct list_head object_list;
};


idmef_object_list_t *idmef_object_list_new(void)
{
	idmef_object_list_t *list;
	
	list = malloc(sizeof(*list));
	if ( ! list ) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}
	
	INIT_LIST_HEAD(&list->object_list);
	
	list->pos = NULL;
	
	return list;
}


int idmef_object_list_add(idmef_object_list_t *list, idmef_object_t *object)
{
	struct listed_object *listed;
	
	if ( ! list || ! object )
		return -1;
	
	listed = malloc(sizeof(*listed));
	if ( ! listed ) {
		log(LOG_ERR, "out of memory!\n");
		return -1;
	}
	
	listed->object = idmef_object_clone(object);
	if ( ! listed->object ) {
		log(LOG_ERR, "out of memory!\n");
		free(listed);
		return -1;
	}
	
	list_add_tail(&listed->list, &list->object_list);
	
	return 0;
}



idmef_object_t *idmef_object_list_next(idmef_object_list_t *list)
{
	if ( ! list->pos )
		list->pos = &list->object_list;

	list->pos = list->pos->next;
	
	if ( list->pos == &list->object_list ) {
		list->pos = NULL;
		return NULL;
	}

	return list_entry(list->pos, struct listed_object, list)->object;
}


void idmef_object_list_destroy(idmef_object_list_t *list)
{
	struct list_head *n, *pos;
	struct listed_object *entry;
	
	list_for_each_safe(pos, n, &list->object_list) {
		entry = list_entry(pos, struct listed_object, list);
		idmef_object_destroy(entry->object);
		free(entry);
	}
}

