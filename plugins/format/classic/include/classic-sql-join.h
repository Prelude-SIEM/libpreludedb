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

#ifndef _LIBPRELUDEDB_CLASSIC_SQL_JOIN_H
#define _LIBPRELUDEDB_CLASSIC_SQL_JOIN_H


typedef struct classic_sql_joined_table classic_sql_joined_table_t;
typedef struct classic_sql_join classic_sql_join_t;


int classic_sql_join_new(classic_sql_join_t **join);
void classic_sql_join_destroy(classic_sql_join_t *join);
void classic_sql_join_set_top_class(classic_sql_join_t *join, idmef_class_id_t top_class);
classic_sql_joined_table_t *classic_sql_join_lookup_table(const classic_sql_join_t *join, const idmef_path_t *path);
int classic_sql_join_to_string(classic_sql_join_t *join, prelude_string_t *output);

int classic_sql_join_new_table(classic_sql_join_t *join, classic_sql_joined_table_t **table,
			       const idmef_path_t *path, char *table_name);
const char *classic_sql_joined_table_get_name(classic_sql_joined_table_t *table);


#endif /* _LIBPRELUDEDB_CLASSIC_SQL_JOIN_H */
