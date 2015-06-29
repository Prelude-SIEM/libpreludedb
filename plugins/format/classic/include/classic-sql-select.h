/*****
*
* Copyright (C) 2005-2015 CS-SI. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_CLASSIC_SQL_SELECT_H
#define _LIBPRELUDEDB_CLASSIC_SQL_SELECT_H


typedef struct classic_sql_select classic_sql_select_t;


int classic_sql_select_new(classic_sql_select_t **select);
void classic_sql_select_destroy(classic_sql_select_t *select);
int classic_sql_select_add_field(classic_sql_select_t *select, const char *field_name,
			     preludedb_selected_path_flags_t flags, unsigned int num_field);
int classic_sql_select_fields_to_string(classic_sql_select_t *select, prelude_string_t *output);
int classic_sql_select_modifiers_to_string(classic_sql_select_t *select, prelude_string_t *output);


#endif /* _LIBPRELUDEDB_CLASSIC_SQL_SELECT_H */
