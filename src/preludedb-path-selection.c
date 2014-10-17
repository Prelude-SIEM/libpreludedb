/*****
*
* Copyright (C) 2003-2012 CS-SI. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libprelude/idmef.h>
#include <libprelude/prelude-list.h>
#include <libprelude/prelude-log.h>

#include "preludedb.h"
#include "preludedb-error.h"
#include "preludedb-path-selection.h"
#include "preludedb-plugin-format.h"
#include "preludedb-plugin-format-prv.h"

struct preludedb_selected_path {
        idmef_path_t *path;
        preludedb_selected_path_flags_t flags;
        unsigned int idx;
        unsigned int position;
};

struct preludedb_path_selection {
        preludedb_t *db;
        preludedb_selected_path_t **selecteds;
        unsigned int count;
        unsigned int numpos;
        int refcount;
};

preludedb_plugin_format_t *_preludedb_get_plugin_format(preludedb_t *db);

int preludedb_selected_path_new(preludedb_selected_path_t **selected_path, idmef_path_t *path, int flags)
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
        unsigned int i;
        struct {
                const char *name;
                int flag;
        } filter_table[] = {
                { "group_by",        PRELUDEDB_SELECTED_OBJECT_GROUP_BY        },
                { "order_desc",        PRELUDEDB_SELECTED_OBJECT_ORDER_DESC        },
                { "order_asc",        PRELUDEDB_SELECTED_OBJECT_ORDER_ASC        }
        };

        for ( i = 0; i < sizeof(filter_table) / sizeof(*filter_table); i++ ) {
                if ( strncmp(str, filter_table[i].name, len) == 0 )
                        return filter_table[i].flag;
        }

        return preludedb_error_verbose(PRELUDEDB_ERROR_INVALID_SELECTED_OBJECT_STRING, "Invalid path filter string '%s'", str);
}



static int parse_filters(const char *str)
{
        int flags = 0, ret;
        const char *start, *end;

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
        unsigned int i;
        struct {
                const char *name;
                size_t len;
                int flag;
        } function_table[] = {
                { "min(",   4, PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN        },
                { "max(",   4, PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX        },
                { "avg(",   4, PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG        },
                { "std(",   4, PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD        },
                { "count(", 6, PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT }
        };

        for ( i = 0; i < sizeof(function_table) / sizeof(*function_table); i++ ) {
                if ( strncmp(str, function_table[i].name, function_table[i].len) == 0 )
                        return function_table[i].flag;
        }

        return 0;
}


static int parse_path(const char *str, size_t len, idmef_path_t **path)
{
        char buf[len + 1];

        if ( len + 1 < len )
                return -1;

        memcpy(buf, str, len);
        buf[len] = 0;

        return idmef_path_new_fast(path, buf);
}


int preludedb_selected_path_new_string(preludedb_selected_path_t **selected_path, const char *str)
{
        int ret, flags = 0;
        idmef_path_t *path;
        const char *filters, *start, *end;

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

                if ( ! (start = strchr(str, '(')) || ! (end = strrchr(str, ')')) )
                        return preludedb_error(PRELUDEDB_ERROR_INVALID_SELECTED_OBJECT_STRING);

                ret = parse_path(start + 1, end - (start + 1), &path);
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



preludedb_selected_path_flags_t preludedb_selected_path_get_flags(preludedb_selected_path_t *selected_path)
{
        return selected_path->flags;
}


int preludedb_selected_path_get_column_index(preludedb_selected_path_t *selected)
{
        return selected->position;
}


int preludedb_path_selection_get_selected(preludedb_path_selection_t *selection, preludedb_selected_path_t **selected, unsigned int index)
{
        if ( index >= selection->count )
                return preludedb_error_verbose(PRELUDEDB_ERROR_INDEX, "Invalid index '%u' for path selection", index);

        *selected = selection->selecteds[index];
        return 1;
}



int preludedb_path_selection_new(preludedb_t *db, preludedb_path_selection_t **path_selection)
{
        *path_selection = calloc(1, sizeof(**path_selection));
        if ( ! *path_selection )
                return preludedb_error_from_errno(errno);

        (*path_selection)->selecteds = NULL;
        (*path_selection)->refcount = 1;
        (*path_selection)->db = preludedb_ref(db);

        return 0;
}



void preludedb_path_selection_destroy(preludedb_path_selection_t *path_selection)
{
        unsigned int i;

        if ( --path_selection->refcount != 0 )
                return;

        for ( i = 0; i < path_selection->count; i++ )
                preludedb_selected_path_destroy(path_selection->selecteds[i]);

        preludedb_destroy(path_selection->db);
        free(path_selection->selecteds);
        free(path_selection);
}



preludedb_path_selection_t *preludedb_path_selection_ref(preludedb_path_selection_t *path_selection)
{
        path_selection->refcount++;
        return path_selection;
}



int preludedb_path_selection_add(preludedb_path_selection_t *path_selection,
                                 preludedb_selected_path_t *selected_path)
{
        preludedb_plugin_format_t *format = _preludedb_get_plugin_format(path_selection->db);

        selected_path->idx = path_selection->count++;
        selected_path->position = path_selection->numpos;

        path_selection->numpos += format->get_path_column_count(selected_path);
        if ( selected_path->position < 0 )
                return selected_path->position;

        path_selection->selecteds = realloc(path_selection->selecteds, sizeof(*path_selection->selecteds) * path_selection->count);
        if ( ! path_selection->selecteds )
                return preludedb_error(PRELUDEDB_ERROR_GENERIC);

        path_selection->selecteds[path_selection->count - 1] = selected_path;

        return 0;
}



preludedb_selected_path_t *preludedb_path_selection_get_next(preludedb_path_selection_t *path_selection,
                                                             preludedb_selected_path_t *selected_path)
{
        int ret;
        unsigned int pos = 0;
        preludedb_selected_path_t *selected;

        if ( selected_path )
                pos = selected_path->idx + 1;

        ret = preludedb_path_selection_get_selected(path_selection, &selected, pos);
        if ( ret <= 0 )
                return NULL;

        return selected;
}



unsigned int preludedb_path_selection_get_count(preludedb_path_selection_t *path_selection)
{
        return path_selection->count;
}
