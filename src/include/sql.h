/*****
*
* Copyright (C) 2002, 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_SQL_H
#define _LIBPRELUDEDB_SQL_H

#include <libprelude/idmef-string.h>
#include <libprelude/idmef-type.h>
#include <libprelude/idmef-value.h>

typedef struct prelude_sql_connection prelude_sql_connection_t;

typedef struct prelude_sql_table prelude_sql_table_t;
typedef struct prelude_sql_row prelude_sql_row_t;
typedef struct prelude_sql_field prelude_sql_field_t;

typedef enum {
	dbtype_unknown = 0,
	dbtype_int32 = 1,
	dbtype_uint32 = 2,
	dbtype_int64 = 3,
	dbtype_uint64 = 4,
	dbtype_float = 5,
	dbtype_double = 6,
	dbtype_string = 7,
	dbtype_void = 8,
        dbtype_end  = 9,
} prelude_sql_field_type_t;

prelude_sql_table_t *prelude_sql_query(prelude_sql_connection_t *conn, const char *fmt, ...);
int prelude_sql_errno(prelude_sql_connection_t *conn);
const char *prelude_sql_error(prelude_sql_connection_t *conn);
char *prelude_sql_escape(prelude_sql_connection_t *conn, const char *string);
int prelude_sql_begin(prelude_sql_connection_t *conn);
int prelude_sql_commit(prelude_sql_connection_t *conn);
int prelude_sql_rollback(prelude_sql_connection_t *conn);
void prelude_sql_close(prelude_sql_connection_t *conn);
prelude_sql_connection_t *prelude_sql_connect(prelude_sql_connection_data_t *data);
int prelude_sql_insert(prelude_sql_connection_t *conn, const char *table, const char *fields,
		       const char *fmt, ...);

void prelude_sql_table_free(prelude_sql_table_t *table);
const char *prelude_sql_field_name(prelude_sql_table_t *table, unsigned int i);
int prelude_sql_field_num(prelude_sql_table_t *table, const char *name);
prelude_sql_field_type_t prelude_sql_field_type(prelude_sql_table_t *table, unsigned int i);
prelude_sql_field_type_t prelude_sql_field_type_by_name(prelude_sql_table_t *table, const char *name);
unsigned int prelude_sql_fields_num(prelude_sql_table_t *table);
unsigned int prelude_sql_rows_num(prelude_sql_table_t *table);
prelude_sql_row_t *prelude_sql_row_fetch(prelude_sql_table_t *table);

void prelude_sql_row_free(prelude_sql_row_t *row);
prelude_sql_field_t *prelude_sql_field_fetch(prelude_sql_row_t *row, unsigned int i);
prelude_sql_field_t *prelude_sql_field_fetch_by_name(prelude_sql_row_t *row, const char *name);

prelude_sql_field_type_t prelude_sql_field_info_type(prelude_sql_field_t *field);
const char *prelude_sql_field_value(prelude_sql_field_t *field);
int32_t prelude_sql_field_value_int32(prelude_sql_field_t *field);
uint32_t prelude_sql_field_value_uint32(prelude_sql_field_t *field);
int64_t prelude_sql_field_value_int64(prelude_sql_field_t *field);
uint64_t prelude_sql_field_value_uint64(prelude_sql_field_t *field);
float prelude_sql_field_value_float(prelude_sql_field_t *field);
double prelude_sql_field_value_double(prelude_sql_field_t *field);
idmef_string_t *prelude_sql_field_value_string(prelude_sql_field_t *field);
idmef_value_t *prelude_sql_field_value_idmef(prelude_sql_field_t *field);

/*
 * Memory management:
 * 
 * memory allocated by the prelude_sql_query, prelude_sql_row_fetch, prelude_sql_field_fetch
 * functions is automatically freed when you call the prelude_sql_table_free function
 * however, if you need to free rows as soon as you don't use them anymore, you can call
 * the prelude_sql_row_free function (as prelude_sql_table_free, this function will also free
 * all the fields instantiated from the row you want to free)
 */

/*
 * NB for functions prelude_sql_field_fetch*:
 * the function returns NULL if an error occured (invalid field number / name) or if the field
 * has a NULL value
 * you can do the difference between both cases using prelude_sql_errno function, as the first case
 * is an error but not the second one, the prelude_sql_errno will return a non-zero value in the
 * first case, and a zero value in the second case
 */

#endif /* _LIBPRELUDEDB_SQL_H */

