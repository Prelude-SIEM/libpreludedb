/*****
*
* Copyright (C) 2005-2016 CS-SI. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_CLASSIC_PATH_RESOLVE_H
#define _LIBPRELUDEDB_CLASSIC_PATH_RESOLVE_H


int classic_path_resolve(preludedb_selected_path_t *selpath, preludedb_selected_object_t *object, void *data, prelude_string_t *output);

int classic_path_resolve_criteria(preludedb_sql_t *sql,
				  idmef_criteria_t *criteria,
				  classic_sql_join_t *join, prelude_string_t *output);


#endif /* _LIBPRELUDEDB_CLASSIC_PATH_RESOLVE_H */
