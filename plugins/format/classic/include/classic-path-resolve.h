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

#ifndef _LIBPRELUDEDB_CLASSIC_PATH_RESOLVE_H
#define _LIBPRELUDEDB_CLASSIC_PATH_RESOLVE_H


int classic_path_resolve_selected(preludedb_sql_t *sql,
				  preludedb_selected_path_t *selected,
				  classic_sql_join_t *join, classic_sql_select_t *select);
int classic_path_resolve_selection(preludedb_sql_t *sql,
				   preludedb_path_selection_t *selection,
				   classic_sql_join_t *join, classic_sql_select_t *select);
int classic_path_resolve_criteria(preludedb_sql_t *sql,
				  idmef_criteria_t *criteria,
				  classic_sql_join_t *join, prelude_string_t *output);


#endif /* _LIBPRELUDEDB_CLASSIC_PATH_RESOLVE_H */
