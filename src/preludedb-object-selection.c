/*****
*
* Copyright (C) 2003-2005 Nicolas Delon <nicolas@prelude-ids.org>
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
#include <string.h>

#include <libprelude/idmef.h>
#include <libprelude/prelude-list.h>
#include <libprelude/prelude-log.h>

#include "preludedb-object-selection.h"

struct preludedb_selected_object {
	prelude_list_t list;
	idmef_object_t *object;
	preludedb_selected_object_flags_t flags;
};

struct preludedb_object_selection {
	prelude_list_t list;
};



preludedb_selected_object_t *preludedb_selected_object_new(idmef_object_t *object, int flags)
{
	preludedb_selected_object_t *selected_object;

	selected_object = calloc(1, sizeof (*selected_object));
	if ( ! selected_object ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	selected_object->object = object;
	selected_object->flags = flags;

	return selected_object;
}



static int parse_filter(const char *str, size_t len)
{
	struct { char *name; int flag; } filter_table[] = {
		{ "group_by",	PRELUDEDB_SELECTED_OBJECT_GROUP_BY	},
		{ "order_desc",	PRELUDEDB_SELECTED_OBJECT_ORDER_DESC	},
		{ "order_asc",	PRELUDEDB_SELECTED_OBJECT_ORDER_ASC	}
	};
	int cnt;

	for ( cnt = 0; cnt < sizeof (filter_table) / sizeof (filter_table[0]); cnt++ ) {
		if ( len == strlen(filter_table[cnt].name) &&
		     strncmp(str, filter_table[cnt].name, len) == 0 )
			return filter_table[cnt].flag;
	}

	return -1;
}



static int parse_filters(const char *str)
{
	const char *start, *end;
	int flags = 0;
	int ret;

	start = str;

	while ( (end = strchr(start, ',')) ) {
		ret = parse_filter(start, end - start);
		if ( ret < 0 )
			return -1;

		flags |= ret;

		start = end + 1;
	}

	ret = parse_filter(start, strlen(start));

	return (ret < 0) ? -1 : (flags | ret);
}



static int parse_function(const char *str, size_t len)
{
	struct { char *name; int flag; } function_table[] = {
		{ "min",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN	 },
		{ "max",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX	 },
		{ "avg",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG	 },
		{ "std",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD	 },
		{ "count",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT }
	};
	int cnt;

	for ( cnt = 0; cnt < sizeof (function_table) / sizeof (function_table[0]); cnt++ ) {
		if ( len == strlen(function_table[cnt].name) &&
		     strncmp(str, function_table[cnt].name, len) == 0 )
			return function_table[cnt].flag;
	}

	return -1;
}



static idmef_object_t *parse_object(const char *str, size_t len)
{
	char *buf;
	idmef_object_t *object;

	buf = malloc(len + 1);
	if ( ! buf ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	memcpy(buf, str, len);
	buf[len] = 0;

	object = idmef_object_new_fast(buf);

	free(buf);

	return object;	
}


preludedb_selected_object_t *preludedb_selected_object_new_string(const char *str)
{
	const char *filters;
	const char *start, *end;
	int ret;
	int flags = 0;
	idmef_object_t *object;
	preludedb_selected_object_t *selected;

	filters = strchr(str, '/');
	if ( filters ) {
		ret = parse_filters(filters + 1);
		if ( ret < 0 )
			return NULL;

		flags |= ret;
	}

	start = strchr(str, '(');
	if ( start ) {
		end = strchr(start, ')');
		if ( ! end )
			return NULL;

		ret = parse_function(str, start - str);
		if ( ret < 0 )
			return NULL;

		flags |= ret;

		object = parse_object(start + 1, end - start - 1);

	} else {
		if ( filters )
			object = parse_object(str, filters - str);
		else
			object = idmef_object_new_fast(str);
	}

	if ( ! object )
		return NULL;

	selected = preludedb_selected_object_new(object, flags);
	if ( ! selected )
		idmef_object_destroy(object);

	return selected;
}



void preludedb_selected_object_destroy(preludedb_selected_object_t *selected_object)
{
	idmef_object_destroy(selected_object->object);
	free(selected_object);
}



idmef_object_t *preludedb_selected_object_get_object(preludedb_selected_object_t *selected_object)
{
	return selected_object->object;
}



int preludedb_selected_object_get_flags(preludedb_selected_object_t *selected_object)
{
	return selected_object->flags;
}



preludedb_object_selection_t *preludedb_object_selection_new(void)
{
	preludedb_object_selection_t *object_selection;

	object_selection = calloc(1, sizeof (*object_selection));
	if ( ! object_selection ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	PRELUDE_INIT_LIST_HEAD(&object_selection->list);

	return object_selection; 
}



void preludedb_object_selection_destroy(preludedb_object_selection_t *object_selection)
{
        prelude_list_t *ptr, *next;
	preludedb_selected_object_t *selected_object;

	prelude_list_for_each_safe(ptr, next, &object_selection->list) {
		selected_object = prelude_list_entry(ptr, preludedb_selected_object_t, list);
		preludedb_selected_object_destroy(selected_object);
	}

	free(object_selection);
}



void preludedb_object_selection_add(preludedb_object_selection_t *object_selection,
				    preludedb_selected_object_t *selected_object)
{
	prelude_list_add_tail(&selected_object->list, &object_selection->list);
}



preludedb_selected_object_t *preludedb_object_selection_get_next(preludedb_object_selection_t *object_selection,
								 preludedb_selected_object_t *selected_object)
{
	return prelude_list_get_next(selected_object, &object_selection->list, preludedb_selected_object_t, list);
}



size_t preludedb_object_selection_get_count(preludedb_object_selection_t *object_selection)
{
	size_t cnt;
	prelude_list_t *ptr;

	cnt = 0;

	prelude_list_for_each(ptr, &object_selection->list) {
		cnt++;
	}

	return cnt;
}
