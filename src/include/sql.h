/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_SQL_H
#define _LIBPRELUDEDB_SQL_H


typedef struct prelude_sql_connection prelude_sql_connection_t;

int prelude_sql_insert(prelude_sql_connection_t *conn, const char *table, const char *fields, const char *fmt, ...);

prelude_sql_table_t *prelude_sql_query(prelude_sql_connection_t *conn, const char *fmt, ...);

char *prelude_sql_escape(prelude_sql_connection_t *conn, const char *string);

int prelude_sql_begin(prelude_sql_connection_t *conn);

int prelude_sql_commit(prelude_sql_connection_t *conn);

int prelude_sql_rollback(prelude_sql_connection_t *conn);

void prelude_sql_close(prelude_sql_connection_t *conn);

prelude_sql_connection_t *prelude_sql_connect(prelude_sql_connection_data_t *data);




#endif /* _LIBPRELUDEDB_SQL_H */

