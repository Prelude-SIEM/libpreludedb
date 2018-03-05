/*****
*
* Copyright (C) 2003-2018 CS-SI. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_H
#define _LIBPRELUDEDB_H

#include <libprelude/prelude-inttypes.h>
#include <libprelude/idmef.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"

#ifdef __cplusplus
 extern "C" {
#endif


typedef struct preludedb preludedb_t;

#include "preludedb-path-selection.h"
#include "preludedb-sql-select.h"

typedef struct preludedb_result_idents preludedb_result_idents_t;
typedef struct preludedb_result_values preludedb_result_values_t;

typedef enum {
        PRELUDEDB_RESULT_IDENTS_ORDER_BY_NONE = 0,
        PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_DESC = 1,
        PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_ASC = 2
} preludedb_result_idents_order_t;


#define PRELUDEDB_ERRBUF_SIZE 512

int preludedb_result_values_get_count(preludedb_result_values_t *results);

preludedb_t *preludedb_result_values_get_db(preludedb_result_values_t *results);

void *preludedb_result_values_get_data(preludedb_result_values_t *results);

int preludedb_result_values_get_row(preludedb_result_values_t *result, unsigned int rownum, void **row);

typedef int (*preludedb_result_values_get_field_cb_func_t)(void **out, void *data, size_t size, idmef_value_type_id_t type);

int preludedb_result_values_get_field_direct(preludedb_result_values_t *result, void *row, preludedb_selected_path_t *selected,
                                             preludedb_result_values_get_field_cb_func_t cb, void **out);

int preludedb_result_values_get_field(preludedb_result_values_t *result, void *row, preludedb_selected_path_t *selected, idmef_value_t **field);

unsigned int preludedb_result_values_get_field_count(preludedb_result_values_t *results);

preludedb_path_selection_t *preludedb_result_values_get_selection(preludedb_result_values_t *result);

int preludedb_init(void);
void preludedb_deinit(void);

int preludedb_new(preludedb_t **db, preludedb_sql_t *sql, const char *format_name, char *errbuf, size_t size);

preludedb_t *preludedb_ref(preludedb_t *db);

void preludedb_destroy(preludedb_t *db);

const char *preludedb_get_format_name(preludedb_t *db);
const char *preludedb_get_format_version(preludedb_t *db);

int preludedb_set_format(preludedb_t *db, const char *format_name);

preludedb_sql_t *preludedb_get_sql(preludedb_t *db);

void preludedb_result_idents_destroy(preludedb_result_idents_t *result);
int preludedb_result_idents_get(preludedb_result_idents_t *result, unsigned int row_index, uint64_t *ident);
unsigned int preludedb_result_idents_get_count(preludedb_result_idents_t *result);
preludedb_result_idents_t *preludedb_result_idents_ref(preludedb_result_idents_t *results);

void preludedb_result_values_destroy(preludedb_result_values_t *result);

char *preludedb_get_error(preludedb_t *db, preludedb_error_t error, char *errbuf, size_t size) PRELUDE_DEPRECATED;

int preludedb_insert_message(preludedb_t *db, idmef_message_t *message);

void preludedb_set_data(preludedb_t *db, void *data);

void *preludedb_get_data(preludedb_t *db);

int preludedb_get_alert_idents(preludedb_t *db, idmef_criteria_t *criteria,
                               int limit, int offset,
                               preludedb_result_idents_order_t order,
                               preludedb_result_idents_t **result) PRELUDE_DEPRECATED;
int preludedb_get_alert_idents2(preludedb_t *db, idmef_criteria_t *criteria,
                               int limit, int offset,
                               const preludedb_path_selection_t *order,
                               preludedb_result_idents_t **result);
int preludedb_get_heartbeat_idents(preludedb_t *db, idmef_criteria_t *criteria,
                                   int limit, int offset,
                                   preludedb_result_idents_order_t order,
                                   preludedb_result_idents_t **result) PRELUDE_DEPRECATED;
int preludedb_get_heartbeat_idents2(preludedb_t *db, idmef_criteria_t *criteria,
                                   int limit, int offset,
                                   const preludedb_path_selection_t *order,
                                   preludedb_result_idents_t **result);

int preludedb_get_alert(preludedb_t *db, uint64_t ident, idmef_message_t **message);
int preludedb_get_heartbeat(preludedb_t *db, uint64_t ident, idmef_message_t **message);

int preludedb_delete(preludedb_t *db, idmef_criteria_t *criteria);

int preludedb_delete_alert(preludedb_t *db, uint64_t ident);

ssize_t preludedb_delete_alert_from_list(preludedb_t *db, uint64_t *idents, size_t isize);

ssize_t preludedb_delete_alert_from_result_idents(preludedb_t *db, preludedb_result_idents_t *result);

int preludedb_delete_heartbeat(preludedb_t *db, uint64_t ident);

ssize_t preludedb_delete_heartbeat_from_list(preludedb_t *db, uint64_t *idents, size_t isize);

ssize_t preludedb_delete_heartbeat_from_result_idents(preludedb_t *db, preludedb_result_idents_t *result);

preludedb_result_values_t *preludedb_result_values_ref(preludedb_result_values_t *results);

int preludedb_get_values(preludedb_t *db, preludedb_path_selection_t *path_selection,
                         idmef_criteria_t *criteria, prelude_bool_t distinct, int limit, int offset,
                         preludedb_result_values_t **result);

ssize_t preludedb_update_from_list(preludedb_t *db,
                                   const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                   uint64_t *idents, size_t isize);

ssize_t preludedb_update_from_result_idents(preludedb_t *db,
                                            const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                            preludedb_result_idents_t *results);

int preludedb_update(preludedb_t *db,
                     const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                     idmef_criteria_t *criteria, preludedb_path_selection_t *order, int limit, int offset);

int preludedb_optimize(preludedb_t *db);

int preludedb_transaction_start(preludedb_t *db);


int preludedb_transaction_end(preludedb_t *db);


int preludedb_transaction_abort(preludedb_t *db);


#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_H */
