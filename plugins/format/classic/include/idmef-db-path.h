/*****
*
* Copyright (C) 2005 Nicolas Delon <nicolas@prelude-ids.org>
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

#ifndef _LIBPRELUDEDB_CLASSIC_DB_PATH_H
#define _LIBPRELUDEDB_CLASSIC_DB_PATH_H


typedef struct classic_join classic_join_t;
typedef struct classic_select classic_select_t;


int classic_join_new(classic_join_t **join);
void classic_join_destroy(classic_join_t *join);
void classic_join_set_top_class(classic_join_t *join, idmef_class_id_t top_class);
int classic_join_to_string(classic_join_t *join, prelude_string_t *output);

int classic_select_new(classic_select_t **select);
void classic_select_destroy(classic_select_t *select);
int classic_select_add_field(classic_select_t *select, const char *field_name, preludedb_selected_path_flags_t flags);
int classic_select_fields_to_string(classic_select_t *select, prelude_string_t *output);
int classic_select_modifiers_to_string(classic_select_t *select, prelude_string_t *output);

int classic_resolve_selected(preludedb_sql_t *sql,
			     const preludedb_selected_path_t *selected, classic_join_t *join, classic_select_t *select);

int classic_resolve_selection(preludedb_sql_t *sql,
			      const preludedb_path_selection_t *selection, classic_join_t *join, classic_select_t *select);

int classic_resolve_criteria(preludedb_sql_t *sql,
			     const idmef_criteria_t *criteria, classic_join_t *join, prelude_string_t *output);


#endif /* _LIBPRELUDEDB_CLASSIC_DB_PATH_H */
