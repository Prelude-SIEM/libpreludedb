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

#include "preludedb-error.h"
#include "preludedb-path-selection.h"

struct preludedb_selected_path {
	prelude_list_t list;
	idmef_path_t *path;
	preludedb_selected_path_flags_t flags;
};

struct preludedb_path_selection {
	prelude_list_t list;
};



int preludedb_selected_path_new(preludedb_selected_path_t **selected_path,
				idmef_path_t *path, int flags)
{
	*selected_path = calloc(1, sizeof (**selected_path));
	if ( ! *selected_path )
		return preludedb_error_from_errno(errno);

	(*selected_path)->path = path;
	(*selected_path)->flags = flags;

	return 0;
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

	return preludedb_error(PRELUDEDB_ERROR_INVALID_SELECTED_OBJECT_STRING);
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
			return ret;

		flags |= ret;

		start = end + 1;
	}

	ret = parse_filter(start, strlen(start));

	return (ret < 0) ? ret : (flags | ret);
}



static int parse_function(const char *str)
{
	struct { char *name; int flag; } function_table[] = {
		{ "min(",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN	 },
		{ "max(",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX	 },
		{ "avg(",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG	 },
		{ "std(",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD	 },
		{ "count(",	PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT }
	};
	int cnt;

	for ( cnt = 0; cnt < sizeof (function_table) / sizeof (function_table[0]); cnt++ ) {
		if ( strncmp(str, function_table[cnt].name, strlen(function_table[cnt].name)) == 0 )
			return function_table[cnt].flag;
	}

	return 0;
}



static int parse_path(const char *str, size_t len, idmef_path_t **path)
{
	char *buf;
	int ret;

	buf = malloc(len + 1);
	if ( ! buf )
		return preludedb_error_from_errno(errno);

	memcpy(buf, str, len);
	buf[len] = 0;

	ret = idmef_path_new_fast(path, buf);

	free(buf);

	return ret;
}


int preludedb_selected_path_new_string(preludedb_selected_path_t **selected_path, const char *str)
{
	const char *filters;
	const char *start, *end;
	int ret;
	int flags = 0;
	idmef_path_t *path;

	filters = strchr(str, '/');
	if ( filters ) {
		ret = parse_filters(filters + 1);
		if ( ret < 0 )
			return ret;

		flags |= ret;
	}

	ret = parse_function(str);
	if ( ret < 0 )
		return ret;

	if ( ret ) {
		flags |= ret;

		start = strchr(str, '(');
		end = strrchr(str, ')');
		if ( ! start || ! end )
			return preludedb_error(PRELUDEDB_ERROR_INVALID_SELECTED_OBJECT_STRING);

		ret = parse_path(start + 1, end - start - 1, &path);

	} else {
		if ( filters )
			ret = parse_path(str, filters - str, &path);
		else
			ret = idmef_path_new_fast(&path, str);
	}

	if ( ret < 0 )
		return ret;

	ret = preludedb_selected_path_new(selected_path, path, flags);
	if ( ret < 0 )
		idmef_path_destroy(path);

	return ret;
}



void preludedb_selected_path_destroy(preludedb_selected_path_t *selected_path)
{
	idmef_path_destroy(selected_path->path);
	free(selected_path);
}



idmef_path_t *preludedb_selected_path_get_path(preludedb_selected_path_t *selected_path)
{
	return selected_path->path;
}



int preludedb_selected_path_get_flags(preludedb_selected_path_t *selected_path)
{
	return selected_path->flags;
}



int preludedb_path_selection_new(preludedb_path_selection_t **path_selection)
{
	*path_selection = calloc(1, sizeof (**path_selection));
	if ( ! *path_selection )
		return preludedb_error_from_errno(errno);

	prelude_list_init(&(*path_selection)->list);

	return 0;
}



void preludedb_path_selection_destroy(preludedb_path_selection_t *path_selection)
{
        prelude_list_t *ptr, *next;
	preludedb_selected_path_t *selected_path;

	prelude_list_for_each_safe(&path_selection->list, ptr, next) {
		selected_path = prelude_list_entry(ptr, preludedb_selected_path_t, list);
		preludedb_selected_path_destroy(selected_path);
	}

	free(path_selection);
}



void preludedb_path_selection_add(preludedb_path_selection_t *path_selection,
				  preludedb_selected_path_t *selected_path)
{
	prelude_list_add_tail(&path_selection->list, &selected_path->list);
}



preludedb_selected_path_t *preludedb_path_selection_get_next(preludedb_path_selection_t *path_selection,
							     preludedb_selected_path_t *selected_path)
{
	return prelude_list_get_next(&path_selection->list, selected_path, preludedb_selected_path_t, list);
}



size_t preludedb_path_selection_get_count(preludedb_path_selection_t *path_selection)
{
	size_t cnt;
	prelude_list_t *ptr;

	cnt = 0;

	prelude_list_for_each(&path_selection->list, ptr) {
		cnt++;
	}

	return cnt;
}
