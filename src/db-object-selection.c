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

#include <stdio.h>
#include <stdlib.h>

#include <libprelude/idmef.h>
#include <libprelude/list.h>
#include <libprelude/prelude-log.h>

#include "db-object-selection.h"

struct prelude_db_selected_object {
	struct list_head list;
	idmef_object_t *object;
	int flags;
};

struct prelude_db_object_selection {
	struct list_head list;
};



prelude_db_selected_object_t *prelude_db_selected_object_new(idmef_object_t *object, int flags)
{
	prelude_db_selected_object_t *selected_object;

	selected_object = calloc(1, sizeof (*selected_object));
	if ( ! selected_object ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	selected_object->object = object;
	selected_object->flags = flags;

	return selected_object;
}



void prelude_db_selected_object_destroy(prelude_db_selected_object_t *selected_object)
{
	idmef_object_destroy(selected_object->object);
	free(selected_object);
}



idmef_object_t *prelude_db_selected_object_get_object(prelude_db_selected_object_t *selected_object)
{
	return selected_object->object;
}



int prelude_db_selected_object_get_flags(prelude_db_selected_object_t *selected_object)
{
	return selected_object->flags;
}



prelude_db_object_selection_t *prelude_db_object_selection_new(void)
{
	prelude_db_object_selection_t *object_selection;

	object_selection = calloc(1, sizeof (*object_selection));
	if ( ! object_selection ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	INIT_LIST_HEAD(&object_selection->list);

	return object_selection; 
}



void	prelude_db_object_selection_destroy(prelude_db_object_selection_t *object_selection)
{
	struct list_head *ptr, *next;
	prelude_db_selected_object_t *selected_object;

	list_for_each_safe(ptr, next, &object_selection->list) {
		selected_object = list_entry(ptr, prelude_db_selected_object_t, list);
		prelude_db_selected_object_destroy(selected_object);
	}

	free(object_selection);
}



void	prelude_db_object_selection_add(prelude_db_object_selection_t *object_selection,
					prelude_db_selected_object_t *selected_object)
{
	list_add_tail(&selected_object->list, &object_selection->list);
}



prelude_db_selected_object_t *prelude_db_object_selection_get_next(prelude_db_object_selection_t *object_selection,
								   prelude_db_selected_object_t *selected_object)
{
	return list_get_next(selected_object, &object_selection->list, prelude_db_selected_object_t, list);
}
