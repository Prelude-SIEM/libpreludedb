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


#ifndef _LIBPRELUDEDB_SQL_CONNECTION_DATA_H
#define _LIBPRELUDEDB_SQL_CONNECTION_DATA_H

typedef struct prelude_sql_connection_data prelude_sql_connection_data_t;

prelude_sql_connection_data_t *prelude_sql_connection_data_new(void);

int prelude_sql_connection_data_set_type(prelude_sql_connection_data_t *cnx, const char *type);

char *prelude_sql_connection_data_get_type(prelude_sql_connection_data_t *cnx);

int prelude_sql_connection_data_set_host(prelude_sql_connection_data_t *cnx, const char *host) ;

char *prelude_sql_connection_data_get_host(prelude_sql_connection_data_t *cnx);

int prelude_sql_connection_data_set_port(prelude_sql_connection_data_t *db, const char *port);

char *prelude_sql_connection_data_get_port(prelude_sql_connection_data_t *cnx);

int prelude_sql_connection_data_set_user(prelude_sql_connection_data_t *cnx, const char *user);

char *prelude_sql_connection_data_get_user(prelude_sql_connection_data_t *cnx);

int prelude_sql_connection_data_set_pass(prelude_sql_connection_data_t *cnx, const char *pass);

char *prelude_sql_connection_data_get_pass(prelude_sql_connection_data_t *cnx);

void prelude_sql_connection_data_destroy(prelude_sql_connection_data_t *cnx);

#endif /* _LIBPRELUDEDB_SQL_CONNECTION_DATA_H */

