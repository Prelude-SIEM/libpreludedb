/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_CLASSIC_DB_OBJECT_H
#define _LIBPRELUDEDB_CLASSIC_DB_OBJECT_H

typedef struct db_object db_object_t;

int db_objects_init(const char *file);

db_object_t *db_object_find(idmef_object_t *object);

char *db_object_get_table(db_object_t *object);

char *db_object_get_field(db_object_t *object);

char *db_object_get_function(db_object_t *object);

char *db_object_get_top_table(db_object_t *object);

char *db_object_get_top_field(db_object_t *object);

char *db_object_get_condition(db_object_t *object);

char *db_object_get_ident_field(db_object_t *object);

void db_objects_destroy(void);

#endif /* _LIBPRELUDEDB_CLASSIC_DB_OBJECT_H */

