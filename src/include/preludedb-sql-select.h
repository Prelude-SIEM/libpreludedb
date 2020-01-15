/*****
*
* Copyright (C) 2005-2020 CS-SI. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoannv@gmail.com>
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

#ifndef _LIBPRELUDEDB_SQL_SELECT_H
#define _LIBPRELUDEDB_SQL_SELECT_H

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum {
        PRELUDEDB_SQL_SELECT_FLAGS_ALIAS_FUNCTION = 0x01,
} preludedb_sql_select_flags_t;

typedef struct preludedb_sql_select preludedb_sql_select_t;

int preludedb_sql_select_new(preludedb_t *db, preludedb_sql_select_t **select);
void preludedb_sql_select_destroy(preludedb_sql_select_t *select);
void preludedb_sql_select_set_flags(preludedb_sql_select_t *select, preludedb_sql_select_flags_t flags);

int preludedb_sql_select_add_field(preludedb_sql_select_t *select, const char *selection);
int preludedb_sql_select_add_selected(preludedb_sql_select_t *select, preludedb_selected_path_t *selpath, void *data);
int preludedb_sql_select_add_selection(preludedb_sql_select_t *select, preludedb_path_selection_t *selection, void *data);

int preludedb_sql_select_fields_to_string(preludedb_sql_select_t *select, prelude_string_t *output);
int preludedb_sql_select_modifiers_to_string(preludedb_sql_select_t *select, prelude_string_t *output);


#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_SQL_SELECT_H */
