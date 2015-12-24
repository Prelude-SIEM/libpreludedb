/*****
*
* Copyright (C) 2003-2015 CS-SI. All Rights Reserved.
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


preludedb_plugin_format_t *_preludedb_get_plugin_format(preludedb_t *db);


struct preludedb_selected_object {
        unsigned int refcount;
        preludedb_selected_object_type_t type;
        union {
                int ival;
                char *sval;
                idmef_path_t *path;
                preludedb_selected_object_t **args;
        } data;

        size_t argno;
};


struct preludedb_selected_path {
        preludedb_sql_time_constraint_type_t time_filter;
        preludedb_selected_path_flags_t flags;

        unsigned int idx;
        unsigned int position;
        unsigned int column_count;

        preludedb_selected_object_t *object;
};

struct preludedb_path_selection {
        preludedb_t *db;
        preludedb_selected_path_t **selecteds;
        unsigned int count;
        unsigned int numpos;
        int refcount;
};



/**
 * preludedb_selected_object_push_arg:
 * @object: Pointer to the object
 * @arg: Argument to push
 *
 * Push @arg as an argument of @object (which needs be a function).
 *
 * Returns: 0 on success, a negative value on error.
 */
int preludedb_selected_object_push_arg(preludedb_selected_object_t *object, preludedb_selected_object_t *arg)
{
        object->data.args = realloc(object->data.args, sizeof(object->data.args) * (object->argno + 1));
        if ( ! object->data.args )
                return prelude_error_from_errno(errno);

        object->data.args[object->argno++] = arg;
        return 0;
}



/**
 * preludedb_selected_object_ref:
 * @object: Pointer to an object to reference
 *
 * Increase preludedb_selected_object_t object reference count
 *
 * Returns: @object.
 */
preludedb_selected_object_t *preludedb_selected_object_ref(preludedb_selected_object_t *object)
{
        object->refcount++;
        return object;
}



/**
 * preludedb_selected_object_new:
 * @object: Pointer to an object to create
 * @type: The type of data the object carry
 * @data: Pointer to the data
 *
 * Create a new preludedb_selected_object_t object, abstracting the selected datatype
 *
 * Returns: 0 on success, a negative value on error.
 */
int preludedb_selected_object_new(preludedb_selected_object_t **object, preludedb_selected_object_type_t type, const void *data)
{
        int ret;

        *object = malloc(sizeof(**object));
        if ( ! *object )
                return preludedb_error_from_errno(errno);

        (*object)->argno = 0;
        (*object)->type = type;
        (*object)->refcount = 1;

        switch(type) {
                case PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH:
                        ret = idmef_path_new_fast(&(*object)->data.path, data);
                        if ( ret < 0 )
                                return ret;

                        break;

                case PRELUDEDB_SELECTED_OBJECT_TYPE_STRING:
                        (*object)->data.sval = strdup(data);
                        break;

                case PRELUDEDB_SELECTED_OBJECT_TYPE_INT:
                        (*object)->data.ival = *(const int *) data;
                        break;

                case PRELUDEDB_SELECTED_OBJECT_TYPE_MIN:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_MAX:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_AVG:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_COUNT:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_EXTRACT:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_INTERVAL:
                        (*object)->data.args = NULL;
                        break;

                default:
                        return -1;
        }

        return 0;
}




/**
 * preludedb_selected_object_new_string:
 * @object: Pointer to the object
 * @str: String that the object should hold
 * @size: size of string.
 *
 * Create a new preludedb_selected_object_t object, holding a string value.
 *
 * Returns: 0 on success, a negative value on error.
 */
int preludedb_selected_object_new_string(preludedb_selected_object_t **object, const char *str, size_t size)
{
        *object = malloc(sizeof(**object));
        if ( ! *object )
                return preludedb_error_from_errno(errno);

        (*object)->argno = 0;
        (*object)->refcount = 1;
        (*object)->data.sval = strndup(str, size);
        (*object)->type = PRELUDEDB_SELECTED_OBJECT_TYPE_STRING;

        return 0;
}



int preludedb_selected_path_new(preludedb_selected_path_t **selected_path, preludedb_selected_object_t *object, preludedb_selected_path_flags_t flags)
{
        *selected_path = calloc(1, sizeof (**selected_path));
        if ( ! *selected_path )
                return preludedb_error_from_errno(errno);

        (*selected_path)->flags = flags;
        (*selected_path)->column_count = 1;
        (*selected_path)->object = object;

        return 0;
}


/**
 * preludedb_selected_object_destroy:
 * @object: Pointer to the object
 *
 * Destroy @object and any data/subobject it might hold.
 */
void preludedb_selected_object_destroy(preludedb_selected_object_t *object)
{
        size_t i;

        if ( ! object )
                return;

        if ( --object->refcount > 0 )
                return;

        if ( object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_STRING )
                free(object->data.sval);

        else if ( object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH )
                idmef_path_destroy(object->data.path);

        else if ( preludedb_selected_object_is_function(object) ) {
                for ( i = 0; i < object->argno; i++ )
                        preludedb_selected_object_destroy(object->data.args[i]);

                free(object->data.args);
        }

        free(object);
}


/**
 * preludedb_selected_object_is_function:
 * @object: Pointer to the object
 *
 * Check whether @object carry a function, or something else.
 *
 * Returns: TRUE if @object carry a function, FALSE otherwise.
 */
prelude_bool_t preludedb_selected_object_is_function(preludedb_selected_object_t *object)
{
        if ( object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_STRING ||
             object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH ||
             object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_INT )
                return FALSE;

        return TRUE;
}



/**
 * preludedb_selected_object_get_data:
 * @object: Pointer to the object
 *
 * Return the data that @object hold
 *
 * Returns: @object data.
 */
const void *preludedb_selected_object_get_data(preludedb_selected_object_t *object)
{
        if ( object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_STRING )
                return object->data.sval;

        else if ( object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH )
                return object->data.path;

        else if ( object->type == PRELUDEDB_SELECTED_OBJECT_TYPE_INT )
                return &object->data.ival;

        else
                return NULL;
}



/**
 * preludedb_selected_object_get_arg:
 * @object: Pointer to the object
 * @i: Index of argument to retrieve
 *
 * Retrieve argument @i from @object (which should carry a function).
 *
 * Returns: Pointer to the @object argument.
 */
preludedb_selected_object_t *preludedb_selected_object_get_arg(preludedb_selected_object_t *object, size_t i)
{
        if ( i >= object->argno )
                return NULL;

        return object->data.args[i];
}


/**
 * preludedb_selected_object_get_type:
 * @object: Pointer to the object
 *
 * Returns: Type of @object.
 */
preludedb_selected_object_type_t preludedb_selected_object_get_type(preludedb_selected_object_t *object)
{
        return object->type;
}



/**
 * preludedb_selected_object_get_value_type:
 * @object: Pointer to the object
 * @data: Pointer where to store data
 * @dtype: Pointer to store @object datatype
 *
 * This function provide information about the data carried by @object.
 * Pointer to the real data is stored within @data, and @object type information is
 * stored within @dtype.
 *
 * Additionally, the real #idmef_value_type_id_t of the data is returned.
 *
 * Returns: #idmef_value_type_id_t of the object, or a negative value if an error occured.
 */
idmef_value_type_id_t preludedb_selected_object_get_value_type(preludedb_selected_object_t *object, const void **data, preludedb_selected_object_type_t *dtype)
{
        switch(object->type) {
                case PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH:
                        *dtype = object->type;
                        *data = object->data.path;
                        return idmef_path_get_value_type(object->data.path, -1);

                case PRELUDEDB_SELECTED_OBJECT_TYPE_INT:
                        *data = &object->data.ival;

                case PRELUDEDB_SELECTED_OBJECT_TYPE_COUNT:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_EXTRACT:
                        *dtype = object->type;
                        return IDMEF_VALUE_TYPE_UINT32;

                case PRELUDEDB_SELECTED_OBJECT_TYPE_STRING:
                        *dtype = object->type;
                        *data = object->data.sval;
                        return IDMEF_VALUE_TYPE_STRING;

                case PRELUDEDB_SELECTED_OBJECT_TYPE_AVG:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_MAX:
                case PRELUDEDB_SELECTED_OBJECT_TYPE_MIN:
                        return preludedb_selected_object_get_value_type(object->data.args[0], data, dtype);

                case PRELUDEDB_SELECTED_OBJECT_TYPE_INTERVAL:
                        *dtype = object->type;
                        *data = NULL;
                        return IDMEF_VALUE_TYPE_TIME;
        }

        return -1;
}


void preludedb_selected_path_set_object(preludedb_selected_path_t *path, preludedb_selected_object_t *object)
{
        path->object = object;
}


void preludedb_selected_path_set_flags(preludedb_selected_path_t *path, preludedb_selected_path_flags_t flags)
{
        path->flags = flags;
}



int preludedb_selected_path_new_string(preludedb_selected_path_t **selected_path, const char *str)
{
        int ret;

        ret = preludedb_selected_path_new(selected_path, NULL, 0);
        if ( ret < 0 )
                return ret;

        ret = preludedb_path_selection_parse(*selected_path, str);
        if ( ret < 0 )
                preludedb_selected_path_destroy(*selected_path);

        return ret;
}



void preludedb_selected_path_destroy(preludedb_selected_path_t *selected_path)
{
        preludedb_selected_object_destroy(selected_path->object);
        free(selected_path);
}



preludedb_selected_object_t *preludedb_selected_path_get_object(preludedb_selected_path_t *selected_path)
{
        return selected_path->object;
}



preludedb_selected_path_flags_t preludedb_selected_path_get_flags(preludedb_selected_path_t *selected_path)
{
        return selected_path->flags;
}



preludedb_sql_time_constraint_type_t preludedb_selected_path_get_time_constraint(preludedb_selected_path_t *selected_path)
{
        return selected_path->time_filter;
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

        if ( selected_path->column_count == 1 ) {
                selected_path->column_count = format->get_path_column_count(selected_path);
                if ( selected_path->column_count < 0 )
                        return selected_path->column_count;
        }

        path_selection->numpos += selected_path->column_count;
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


/**
 * preludedb_path_selection_get_column_count:
 * @path_selection: Pointer to the #preludedb_path_selection_t object
 *
 * Returns: the number of column the given selection will return.
 */
unsigned int preludedb_path_selection_get_column_count(preludedb_path_selection_t *path_selection)
{
        return path_selection->numpos;
}


/**
 * preludedb_selected_path_get_column_count:
 * @selected_path: Pointer to the #preludedb_selected_path_t object
 *
 * Returns: the number of column the provided selected object will return.
 */
unsigned int preludedb_selected_path_get_column_count(preludedb_selected_path_t *selected_path)
{
        return selected_path->column_count;
}



/**
 * preludedb_selected_path_set_column_count:
 * @selected_path: Pointer to the #preludedb_selected_path_t object
 * @count: The number of columns this @selected_path holds
 */
void preludedb_selected_path_set_column_count(preludedb_selected_path_t *selected_path, unsigned int count)
{
        selected_path->column_count = count;
}
