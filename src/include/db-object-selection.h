/*****
*
* Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
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

#ifndef _LIBPRELUDEDB_OBJECT_SELECTION_H
#define _LIBPRELUDEDB_OBJECT_SELECTION_H

#define PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN		0x01
#define PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX		0x02
#define PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG		0x04
#define PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD		0x08
#define PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT	0x10

#define PRELUDEDB_SELECTED_OBJECT_GROUP_BY		0x20

#define PRELUDEDB_SELECTED_OBJECT_ORDER_ASC		0x40
#define PRELUDEDB_SELECTED_OBJECT_ORDER_DESC		0x80

typedef struct prelude_db_object_selection prelude_db_object_selection_t;
typedef struct prelude_db_selected_object prelude_db_selected_object_t;

prelude_db_selected_object_t *prelude_db_selected_object_new(idmef_object_t *object, int flags);
prelude_db_selected_object_t *prelude_db_selected_object_new_string(const char *str);
void prelude_db_selected_object_destroy(prelude_db_selected_object_t *selected_object);
idmef_object_t *prelude_db_selected_object_get_object(prelude_db_selected_object_t *selected_object);
int prelude_db_selected_object_get_flags(prelude_db_selected_object_t *selected_object);

prelude_db_object_selection_t *prelude_db_object_selection_new(void);
void prelude_db_object_selection_destroy(prelude_db_object_selection_t *object_selection);
void prelude_db_object_selection_add(prelude_db_object_selection_t *object_selection,
				     prelude_db_selected_object_t *selected_object);
prelude_db_selected_object_t *prelude_db_object_selection_get_next(prelude_db_object_selection_t *object_selection,
								   prelude_db_selected_object_t *selected_object);

#endif /* ! _LIBPRELUDEDB_OBJECT_SELECTION_H */
