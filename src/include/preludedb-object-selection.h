/*****
*
* Copyright (C) 2003-2005 Nicolas Delon <nicolas@prelude-ids.org>
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

typedef enum {
	PRELUDEDB_SELECTED_OBJECT_FUNCTION_MIN = 0x01,
	PRELUDEDB_SELECTED_OBJECT_FUNCTION_MAX = 0x02,
	PRELUDEDB_SELECTED_OBJECT_FUNCTION_AVG = 0x04,
	PRELUDEDB_SELECTED_OBJECT_FUNCTION_STD = 0x08,
	PRELUDEDB_SELECTED_OBJECT_FUNCTION_COUNT = 0x10,

	PRELUDEDB_SELECTED_OBJECT_GROUP_BY = 0x20,

	PRELUDEDB_SELECTED_OBJECT_ORDER_ASC = 0x40,
	PRELUDEDB_SELECTED_OBJECT_ORDER_DESC = 0x80
} preludedb_selected_object_flags_t;


typedef struct preludedb_object_selection preludedb_object_selection_t;
typedef struct preludedb_selected_object preludedb_selected_object_t;

preludedb_selected_object_t *preludedb_selected_object_new(idmef_object_t *object, int flags);
preludedb_selected_object_t *preludedb_selected_object_new_string(const char *str);
void preludedb_selected_object_destroy(preludedb_selected_object_t *selected_object);
idmef_object_t *preludedb_selected_object_get_object(preludedb_selected_object_t *selected_object);
int preludedb_selected_object_get_flags(preludedb_selected_object_t *selected_object);

preludedb_object_selection_t *preludedb_object_selection_new(void);
void preludedb_object_selection_destroy(preludedb_object_selection_t *object_selection);
void preludedb_object_selection_add(preludedb_object_selection_t *object_selection,
				    preludedb_selected_object_t *selected_object);
preludedb_selected_object_t *preludedb_object_selection_get_next(preludedb_object_selection_t *object_selection,
								 preludedb_selected_object_t *selected_object);
size_t preludedb_object_selection_get_count(preludedb_object_selection_t *object_selection);

#endif /* ! _LIBPRELUDEDB_OBJECT_SELECTION_H */
