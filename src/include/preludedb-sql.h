/*****
*
* Copyright (C) 2002, 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_SQL_H
#define _LIBPRELUDEDB_SQL_H

#include <libprelude/prelude-string.h>

typedef enum {
	PRELUDEDB_SQL_STATUS_DISCONNECTED = 0,
	PRELUDEDB_SQL_STATUS_CONNECTED =    1,
	PRELUDEDB_SQL_STATUS_TRANSACTION =  2
} preludedb_sql_status_t;

typedef enum {
	PRELUDEDB_SQL_TIME_CONSTRAINT_YEAR,
	PRELUDEDB_SQL_TIME_CONSTRAINT_MONTH,
	PRELUDEDB_SQL_TIME_CONSTRAINT_YDAY,
	PRELUDEDB_SQL_TIME_CONSTRAINT_MDAY,
	PRELUDEDB_SQL_TIME_CONSTRAINT_WDAY,
	PRELUDEDB_SQL_TIME_CONSTRAINT_HOUR,
	PRELUDEDB_SQL_TIME_CONSTRAINT_MIN,
	PRELUDEDB_SQL_TIME_CONSTRAINT_SEC
}	preludedb_sql_time_constraint_type_t;

typedef struct preludedb_sql preludedb_sql_t;

typedef struct preludedb_sql_table preludedb_sql_table_t;
typedef struct preludedb_sql_row preludedb_sql_row_t;
typedef struct preludedb_sql_field preludedb_sql_field_t;

int preludedb_sql_new(preludedb_sql_t **new, const char *type, preludedb_sql_settings_t *settings);

void preludedb_sql_destroy(preludedb_sql_t *sql);

int preludedb_sql_enable_query_logging(preludedb_sql_t *sql, const char *filename);
void preludedb_sql_disable_query_logging(preludedb_sql_t *sql);

int preludedb_sql_connect(preludedb_sql_t *sql);
void preludedb_sql_disconnect(preludedb_sql_t *sql);

const char *preludedb_sql_get_plugin_error(preludedb_sql_t *sql);

int preludedb_sql_query(preludedb_sql_t *sql, const char *query, preludedb_sql_table_t **table);
int preludedb_sql_query_sprintf(preludedb_sql_t *sql, preludedb_sql_table_t **table, const char *format, ...);

int preludedb_sql_insert(preludedb_sql_t *sql, const char *table, const char *fields, const char *format, ...);

int preludedb_sql_build_limit_offset_string(preludedb_sql_t *sql, int limit, int offset, prelude_string_t *output);

int preludedb_sql_transaction_start(preludedb_sql_t *sql);
int preludedb_sql_transaction_end(preludedb_sql_t *sql);
int preludedb_sql_transaction_abort(preludedb_sql_t *sql);

int preludedb_sql_escape_fast(preludedb_sql_t *sql, const char *input, size_t input_size, char **output);
int preludedb_sql_escape(preludedb_sql_t *sql, const char *input, char **output);
int preludedb_sql_escape_binary(preludedb_sql_t *sql, const unsigned char *input, size_t input_size, char **output);
int preludedb_sql_unescape_binary(preludedb_sql_t *sql, const char *input, size_t input_size,
				  unsigned char **output, size_t *output_size);

void preludedb_sql_table_destroy(preludedb_sql_table_t *table);
const char *preludedb_sql_table_get_column_name(preludedb_sql_table_t *table, unsigned int column_num);
int preludedb_sql_table_get_column_num(preludedb_sql_table_t *table, const char *column_name);
unsigned int preludedb_sql_table_get_column_count(preludedb_sql_table_t *table);
unsigned int preludedb_sql_table_get_row_count(preludedb_sql_table_t *table);
int preludedb_sql_table_fetch_row(preludedb_sql_table_t *table, preludedb_sql_row_t **row);

int preludedb_sql_row_fetch_field(preludedb_sql_row_t *row, unsigned int column_num, preludedb_sql_field_t **field);
int preludedb_sql_row_fetch_field_by_name(preludedb_sql_row_t *row, const char *column_name, preludedb_sql_field_t **field);

const char *preludedb_sql_field_get_value(preludedb_sql_field_t *field);
size_t preludedb_sql_field_get_len(preludedb_sql_field_t *field);
int preludedb_sql_field_to_int8(preludedb_sql_field_t *field, int8_t *value);
int preludedb_sql_field_to_uint8(preludedb_sql_field_t *field, uint8_t *value);
int preludedb_sql_field_to_int16(preludedb_sql_field_t *field, int16_t *value);
int preludedb_sql_field_to_uint16(preludedb_sql_field_t *field, uint16_t *value);
int preludedb_sql_field_to_int32(preludedb_sql_field_t *field, int32_t *value);
int preludedb_sql_field_to_uint32(preludedb_sql_field_t *field, uint32_t *value);
int preludedb_sql_field_to_int64(preludedb_sql_field_t *field, int64_t *value);
int preludedb_sql_field_to_uint64(preludedb_sql_field_t *field, uint64_t *value);
int preludedb_sql_field_to_float(preludedb_sql_field_t *field, float *value);
int preludedb_sql_field_to_double(preludedb_sql_field_t *field, double *value);
int preludedb_sql_field_to_string(preludedb_sql_field_t *field, prelude_string_t *output);

const char *preludedb_sql_get_relation_string(idmef_value_relation_t relation);

int preludedb_sql_build_criterion_string(preludedb_sql_t *sql,
					 prelude_string_t *output,
					 const char *field,
					 idmef_value_relation_t relation, idmef_criterion_value_t *value);

int preludedb_sql_time_from_timestamp(idmef_time_t *time, const char *time_buf, uint32_t gmtoff, uint32_t usec);
int preludedb_sql_time_to_timestamp(const idmef_time_t *time,
				    char *time_buf, size_t time_buf_size,
				    char *gmtoff_buf, size_t gmtoff_buf_size,
				    char *usec_buf, size_t usec_buf_size);
     

#endif /* _LIBPRELUDEDB_SQL_H */
