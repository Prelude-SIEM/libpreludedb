/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* Copyright (C) 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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


#ifndef _LIBPRELUDEDB_DB_CONNECTION_DATA_H
#define _LIBPRELUDEDB_DB_CONNECTION_DATA_H

typedef struct prelude_db_connection_data prelude_db_connection_data_t;

prelude_db_connection_data_t *prelude_db_connection_data_new(prelude_db_type_t type, void *data);

prelude_db_type_t prelude_db_connection_data_get_type(prelude_db_connection_data_t *data);

void *prelude_db_connection_data_get(prelude_db_connection_data_t *data);

void prelude_db_connection_data_destroy(prelude_db_connection_data_t *data);

#endif /* _LIBPRELUDEDB_DB_CONNECTION_DATA_H */