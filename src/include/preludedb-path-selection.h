/*****
*
* Copyright (C) 2003-2019 CS-SI. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_OBJECT_SELECTION_H
#define _LIBPRELUDEDB_OBJECT_SELECTION_H

#include "preludedb-sql.h"

#ifdef __cplusplus
 extern "C" {
#endif


typedef enum {
        PRELUDEDB_SELECTED_OBJECT_TYPE_MIN = 1,
        PRELUDEDB_SELECTED_OBJECT_TYPE_MAX = 2,
        PRELUDEDB_SELECTED_OBJECT_TYPE_AVG = 3,
        PRELUDEDB_SELECTED_OBJECT_TYPE_COUNT = 4,
        PRELUDEDB_SELECTED_OBJECT_TYPE_INTERVAL = 5,
        PRELUDEDB_SELECTED_OBJECT_TYPE_EXTRACT = 6,
        PRELUDEDB_SELECTED_OBJECT_TYPE_STRING = 7,
        PRELUDEDB_SELECTED_OBJECT_TYPE_IDMEFPATH = 8,
        PRELUDEDB_SELECTED_OBJECT_TYPE_INT = 9,
        PRELUDEDB_SELECTED_OBJECT_TYPE_TIMEZONE = 10,
        PRELUDEDB_SELECTED_OBJECT_TYPE_DISTINCT = 11,
        PRELUDEDB_SELECTED_OBJECT_TYPE_SUM = 12,
} preludedb_selected_object_type_t;



typedef enum {
        PRELUDEDB_SELECTED_PATH_FLAGS_GROUP_BY = 0x20,
        PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_ASC = 0x40,
        PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_DESC = 0x80
} preludedb_selected_path_flags_t;

typedef struct preludedb_selected_object preludedb_selected_object_t;
typedef struct preludedb_path_selection preludedb_path_selection_t;
typedef struct preludedb_selected_path preludedb_selected_path_t;

#include "preludedb.h"

prelude_bool_t preludedb_selected_object_is_function(preludedb_selected_object_t *object);
idmef_value_type_id_t preludedb_selected_object_get_value_type(preludedb_selected_object_t *object, const void **data, preludedb_selected_object_type_t *dtype);
void preludedb_selected_object_destroy(preludedb_selected_object_t *object);
preludedb_selected_object_t *preludedb_selected_object_get_arg(preludedb_selected_object_t *object, size_t i);
const void *preludedb_selected_object_get_data(preludedb_selected_object_t *object);
preludedb_selected_object_type_t preludedb_selected_object_get_type(preludedb_selected_object_t *object);
int preludedb_selected_object_push_arg(preludedb_selected_object_t *object, preludedb_selected_object_t *arg);
int preludedb_selected_object_new_string(preludedb_selected_object_t **object, const char *str, size_t size);
int preludedb_selected_object_new(preludedb_selected_object_t **object, preludedb_selected_object_type_t type, const void *data);
preludedb_selected_object_t *preludedb_selected_object_ref(preludedb_selected_object_t *object);

int preludedb_path_selection_get_selected(const preludedb_path_selection_t *selection, preludedb_selected_path_t **selected, unsigned int index);
int preludedb_selected_path_get_column_index(preludedb_selected_path_t *selected);

int preludedb_path_selection_parse(preludedb_selected_path_t *selected, const char *str);
int preludedb_selected_path_new(preludedb_selected_path_t **selected_path, preludedb_selected_object_t *object, preludedb_selected_path_flags_t flags);
int preludedb_selected_path_new_string(preludedb_selected_path_t **selected_path, const char *str);

void preludedb_selected_path_destroy(preludedb_selected_path_t *selected_path);

preludedb_path_selection_t *preludedb_path_selection_ref(preludedb_path_selection_t *path_selection);

void preludedb_selected_path_set_object(preludedb_selected_path_t *path, preludedb_selected_object_t *object);
void preludedb_selected_path_set_flags(preludedb_selected_path_t *path, preludedb_selected_path_flags_t flags);

preludedb_selected_object_t *preludedb_selected_path_get_object(preludedb_selected_path_t *selected_path);
preludedb_selected_path_flags_t preludedb_selected_path_get_flags(preludedb_selected_path_t *selected_path);
preludedb_sql_time_constraint_type_t preludedb_selected_path_get_time_constraint(preludedb_selected_path_t *selected_path);
void preludedb_selected_path_set_column_count(preludedb_selected_path_t *selected_path, unsigned int count);
unsigned int preludedb_selected_path_get_column_count(preludedb_selected_path_t *selected_path);

int preludedb_path_selection_new(preludedb_t *db, preludedb_path_selection_t **path_selection);
void preludedb_path_selection_destroy(preludedb_path_selection_t *path_selection);
int preludedb_path_selection_add(preludedb_path_selection_t *path_selection,
                                 preludedb_selected_path_t *selected_path);

preludedb_selected_path_t *preludedb_path_selection_get_next(const preludedb_path_selection_t *path_selection,
                                                                 preludedb_selected_path_t *selected_path);
unsigned int preludedb_path_selection_get_count(preludedb_path_selection_t *path_selection);
unsigned int preludedb_path_selection_get_column_count(preludedb_path_selection_t *path_selection);

#ifdef __cplusplus
  }
#endif

#endif /* ! _LIBPRELUDEDB_OBJECT_SELECTION_H */
