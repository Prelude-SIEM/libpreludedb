/*****
*
* Copyright (C) 2003-2020 CS GROUP - France. All Rights Reserved.
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <libprelude/prelude.h>
#include <libprelude/prelude-ident.h>
#include <libprelude/idmef-criteria.h>


#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-path-selection.h"

#include "preludedb.h"
#include "preludedb-sql.h"
#include "preludedb-plugin-format.h"
#include "preludedb-plugin-format-prv.h"


#define PRELUDEDB_PLUGIN_SYMBOL "preludedb_plugin_init"
#define PRELUDEDB_ENOTSUP(x) preludedb_error_verbose(prelude_error_code_from_errno(ENOSYS), "Database format does not support '%s' operation", x)


struct preludedb {
        int refcount;
        char *format_version;
        char *format_uuid;
        preludedb_sql_t *sql;
        preludedb_plugin_format_t *plugin;
        void *data;
};

struct preludedb_result_idents {
        preludedb_t *db;
        void *res;
        int refcount;
};

struct preludedb_result_values {
        int refcount;
        preludedb_t *db;
        preludedb_path_selection_t *selection;
        void *res;
};

preludedb_plugin_format_t *_preludedb_get_plugin_format(preludedb_t *db);
int _preludedb_sql_transaction_start(preludedb_sql_t *sql);
int _preludedb_sql_transaction_end(preludedb_sql_t *sql);
int _preludedb_sql_transaction_abort(preludedb_sql_t *sql);
void _preludedb_sql_enable_internal_transaction(preludedb_sql_t *sql);
void _preludedb_sql_disable_internal_transaction(preludedb_sql_t *sql);



static int libpreludedb_refcount = 0;
PRELUDE_LIST(_sql_plugin_list);
static PRELUDE_LIST(plugin_format_list);



int preludedb_init(void)
{
        int ret;

        if ( libpreludedb_refcount++ > 0 )
                return 0;

        ret = prelude_init(NULL, NULL);
        if ( ret < 0 )
                return ret;

        ret = access(FORMAT_PLUGIN_DIR, F_OK);
        if ( ret < 0 )
                return preludedb_error_verbose(PRELUDEDB_ERROR_CANNOT_LOAD_FORMAT_PLUGIN,
                                               "could not access format plugin directory '%s'", FORMAT_PLUGIN_DIR);

        ret = prelude_plugin_load_from_dir(&plugin_format_list, FORMAT_PLUGIN_DIR,
                                           PRELUDEDB_PLUGIN_SYMBOL, NULL, NULL, NULL);
        if ( ret < 0 )
                return ret;

        ret = access(SQL_PLUGIN_DIR, F_OK);
        if ( ret < 0 )
                return preludedb_error_verbose(PRELUDEDB_ERROR_CANNOT_LOAD_SQL_PLUGIN,
                                               "could not access sql plugin directory '%s'", SQL_PLUGIN_DIR);

        ret = prelude_plugin_load_from_dir(&_sql_plugin_list, SQL_PLUGIN_DIR,
                                           PRELUDEDB_PLUGIN_SYMBOL, NULL, NULL, NULL);
        if ( ret < 0 )
                return ret;

        return 0;
}



void preludedb_deinit(void)
{
        prelude_list_t *iter;
        prelude_plugin_generic_t *pl;

        if ( --libpreludedb_refcount > 0 )
                return;

        iter = NULL;
        while ( (pl = prelude_plugin_get_next(&_sql_plugin_list, &iter)) ) {
                prelude_plugin_unload(pl);
                free(pl);
        }

        iter = NULL;
        while ( (pl = prelude_plugin_get_next(&plugin_format_list, &iter)) ) {
                prelude_plugin_unload(pl);
                free(pl);
        }

        prelude_deinit();
}



static int generate_uuid(preludedb_sql_t *sql, char **out)
{
        int ret;
        prelude_ident_t *ident;
        prelude_string_t *uuid;

        ret = prelude_ident_new(&ident);
        if ( ret < 0 )
                return ret;

        ret = prelude_string_new(&uuid);
        if ( ret < 0 ) {
                prelude_ident_destroy(ident);
                return ret;
        }

        ret = prelude_ident_generate(ident, uuid);
        if ( ret < 0 )
                goto error;

        ret = prelude_string_get_string_released(uuid, out);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_query_sprintf(sql, NULL, "UPDATE _format SET uuid = '%s'", *out);

 error:
        prelude_ident_destroy(ident);
        prelude_string_destroy(uuid);

        return ret;
}


static int _preludedb_autodetect_format(preludedb_t *db)
{
        int ret;
        preludedb_sql_query_t *query;
        preludedb_sql_table_t *table;
        preludedb_sql_row_t *row;
        preludedb_sql_field_t *format_name;
        preludedb_sql_field_t *format_version;
        preludedb_sql_field_t *format_uuid;

        ret = preludedb_sql_query_new(&query, "SELECT name, version, uuid FROM _format");
        if ( ret < 0 )
                return ret;

        preludedb_sql_query_set_option(query, PRELUDEDB_SQL_QUERY_OPTION_FOR_UPDATE);

        ret = preludedb_sql_query_execute(db->sql, query, &table);
        if ( ret <= 0 ) {
                preludedb_sql_query_destroy(query);
                return (ret < 0) ? ret : -1;
        }

        ret = preludedb_sql_table_fetch_row(table, &row);
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_row_get_field(row, 0, &format_name);
        if ( ret < 0 )
                goto error;

        ret = preludedb_set_format(db, preludedb_sql_field_get_value(format_name));
        if ( ret < 0 )
                goto error;

        ret = preludedb_sql_row_get_field(row, 1, &format_version);
        if ( ret < 0 )
                goto error;

        ret = db->plugin->check_schema_version(preludedb_sql_field_get_value(format_version));
        if ( ret < 0 )
                goto error;

        db->format_version = strdup(preludedb_sql_field_get_value(format_version));
        if ( ! db->format_version ) {
                ret = prelude_error_from_errno(errno);
                goto error;
        }

        ret = preludedb_sql_row_get_field(row, 2, &format_uuid);
        if ( ret < 0 )
                goto error;

        if ( format_uuid )
                db->format_uuid = strdup(preludedb_sql_field_get_value(format_uuid));
        else {
                ret = generate_uuid(db->sql, &db->format_uuid);
        }

 error:
        preludedb_sql_query_destroy(query);
        preludedb_sql_table_destroy(table);

        return ret;

}


static int preludedb_autodetect_format(preludedb_t *db)
{
        int ret, tmp;

        ret = preludedb_transaction_start(db);
        if ( ret < 0 ) {
                return ret;
        }

        ret = _preludedb_autodetect_format(db);
        if ( ret < 0 ) {
                tmp = preludedb_transaction_abort(db);
                return (tmp < 0) ? tmp : ret;
        }

        return preludedb_transaction_end(db);
}



void preludedb_set_data(preludedb_t *db, void *data)
{
        prelude_return_if_fail(db);
        db->data = data;
}



void *preludedb_get_data(preludedb_t *db)
{
        prelude_return_val_if_fail(db, NULL);
        return db->data;
}



/**
 * preludedb_new:
 * @db: Pointer to a db object to initialize.
 * @sql: Pointer to a sql object.
 * @format_name: Format name of the underlying database, if NULL the format will be automatically detected
 * @errbuf: Buffer that will be set to an error message if an error occur.
 * @size: size of @errbuf.
 *
 * This function initialize the @db object and detect the format of the underlying database if no format name
 * is given.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_new(preludedb_t **db, preludedb_sql_t *sql, const char *format_name, char *errbuf, size_t size)
{
        int ret;

        /*
         * FIXME: format_name, errbuf, are deprecated.
         */
        prelude_return_val_if_fail(sql, prelude_error(PRELUDE_ERROR_ASSERTION));

        *db = calloc(1, sizeof (**db));
        if ( ! *db ) {
                ret = preludedb_error_from_errno(errno);
                return ret;
        }

        (*db)->refcount = 1;
        (*db)->sql = preludedb_sql_ref(sql);

        ret = preludedb_autodetect_format(*db);

        if ( ret >= 0 && (*db)->plugin->init )
                ret = (*db)->plugin->init(*db);

        if ( ret < 0 ) {
                preludedb_sql_destroy(sql);

                if ( (*db)->format_version )
                        free((*db)->format_version);

                if ( (*db)->format_uuid )
                        free((*db)->format_uuid);

                free(*db);
        }

        return ret;
}




preludedb_t *preludedb_ref(preludedb_t *db)
{
        db->refcount++;
        return db;
}




/**
 * preludedb_destroy:
 * @db: Pointer to a db object.
 *
 * Destroy @db object and the underlying @sql object given as argument to preludedb_new.
 */
void preludedb_destroy(preludedb_t *db)
{
        if ( --db->refcount != 0 )
                return;

        if ( db->plugin && db->plugin->destroy_func )
                db->plugin->destroy_func(db);

        preludedb_sql_destroy(db->sql);
        free(db->format_version);
        free(db->format_uuid);
        free(db);
}



/**
 * preludedb_get_format_name:
 * @db: Pointer to a db object.
 *
 * Returns: the format name currently used by the @db object.
 */
const char *preludedb_get_format_name(preludedb_t *db)
{
        prelude_return_val_if_fail(db, NULL);
        return prelude_plugin_get_name(db->plugin);
}



/**
 * preludedb_get_format_version:
 * @db: Pointer to a db object.
 *
 * Returns: the format version currently used by the @db object.
 */
const char *preludedb_get_format_version(preludedb_t *db)
{
        prelude_return_val_if_fail(db, NULL);
        return db->format_version;
}



/**
 * preludedb_get_format_uuid:
 * @db: Pointer to a db object.
 *
 * Returns: the UUID for this database.
 */
const char *preludedb_get_format_uuid(preludedb_t *db)
{
        prelude_return_val_if_fail(db, NULL);
        return db->format_uuid;
}


/**
 * preludedb_set_format:
 * @db: Pointer to a db object.
 * @format_name: New format to use.
 *
 * Change the current format plugin.
 *
 * Returns: 0 on success or negative value if an error occur.
 */
int preludedb_set_format(preludedb_t *db, const char *format_name)
{
        prelude_return_val_if_fail(db && format_name, prelude_error(PRELUDE_ERROR_ASSERTION));

        db->plugin = (preludedb_plugin_format_t *) prelude_plugin_search_by_name(&plugin_format_list, format_name);
        if ( ! db->plugin )
                return preludedb_error_verbose(PRELUDEDB_ERROR_CANNOT_LOAD_FORMAT_PLUGIN,
                                               "could not find format plugin '%s'", format_name);

        return 0;
}



/**
 * preludedb_get_sql:
 * @db: Pointer to a db object.
 *
 * Returns: a pointer to the underlying sql object.
 */
preludedb_sql_t *preludedb_get_sql(preludedb_t *db)
{
        prelude_return_val_if_fail(db, NULL);
        return db->sql;
}



/**
 * preludedb_get_error:
 * @db: Pointer to a db object.
 * @error: Error code to build the error string from.
 * @errbuf: Buffer where the error message will be stored,
 * @size: size of this buffer must be PRELUDEDB_ERRBUF_SIZE.
 *
 * Build an error message from the error code given as argument and from
 * the sql plugin error string (if any) if the error code is db related.
 *
 * FIXME: deprecated.
 *
 * Returns: a pointer to the error string or NULL if an error occured.
 */
char *preludedb_get_error(preludedb_t *db, preludedb_error_t error, char *errbuf, size_t size)
{
        int ret;
        preludedb_error_t tmp;

        tmp = preludedb_error(prelude_error_get_code(error));

        ret = snprintf(errbuf, size, "%s: %s", preludedb_strerror(tmp), preludedb_strerror(error));
        if ( ret < 0 || (size_t) ret >= size )
                return NULL;

        return errbuf;
}



/**
 * preludedb_insert_message:
 * @db: Pointer to a db object.
 * @message: Pointer to an IDMEF message.
 *
 * Insert an IDMEF message into the database.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_insert_message(preludedb_t *db, idmef_message_t *message)
{
        prelude_return_val_if_fail(db && message, prelude_error(PRELUDE_ERROR_ASSERTION));

        return db->plugin->insert_message(db, message);
}



preludedb_result_idents_t *preludedb_result_idents_ref(preludedb_result_idents_t *results)
{
        prelude_return_val_if_fail(results, NULL);

        results->refcount++;
        return results;
}


/**
 * preludedb_result_idents_destroy:
 * @result: Pointer to an idents result object.
 *
 * Destroy the @result object.
 */
void preludedb_result_idents_destroy(preludedb_result_idents_t *result)
{
        prelude_return_if_fail(result);

        if ( --result->refcount != 0 )
                return;

        result->db->plugin->destroy_message_idents_resource(result->res);
        preludedb_destroy(result->db);

        free(result);
}



/**
 * preludedb_result_idents_get:
 * @result: Pointer to an idents result object.
 * @row_index: Row index to retrieve the ident from.
 * @ident: Pointer to an ident where the next ident will be stored.
 *
 * Retrieve the ident located at @row_index from the idents result object.
 *
 * Returns: 1 if an ident is available, 0 if there is no more idents available or
 * a negative value if an error occur.
 */
int preludedb_result_idents_get(preludedb_result_idents_t *result, unsigned int row_index, uint64_t *ident)
{
        prelude_return_val_if_fail(result, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! result->db->plugin->get_message_ident )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "format plugin doesn't implement ident retrieval by index");

        return result->db->plugin->get_message_ident(result->res, row_index, ident);
}


unsigned int preludedb_result_idents_get_count(preludedb_result_idents_t *result)
{
        prelude_return_val_if_fail(result, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! result->db->plugin->get_message_ident_count )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "format plugin doesn't implement ident count retrieval");

        return result->db->plugin->get_message_ident_count(result->res);
}



/**
 * preludedb_result_values_destroy:
 * @result: Pointer to a result values object.
 *
 * Destroy the @result object.
 */
void preludedb_result_values_destroy(preludedb_result_values_t *result)
{
        prelude_return_if_fail(result);

        if ( --result->refcount != 0 )
                return;

        result->db->plugin->destroy_values_resource(result->res);
        preludedb_path_selection_destroy(result->selection);
        preludedb_destroy(result->db);

        free(result);
}



preludedb_path_selection_t *preludedb_result_values_get_selection(preludedb_result_values_t *result)
{
        prelude_return_val_if_fail(result, NULL);
        return result->selection;
}


int preludedb_result_values_get_row(preludedb_result_values_t *result, unsigned int rownum, void **row)
{
        prelude_return_val_if_fail(result && row, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! result->db->plugin->get_result_values_row )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "format plugin doesn't implement value selection");

        return result->db->plugin->get_result_values_row(result, rownum, row);
}



static int internal_value_cb(void **out, void *data, size_t size, idmef_value_type_id_t type)
{
        if ( ! type && ! data ) {
                *out = NULL;
                return 0;
        }

        else if ( type == IDMEF_VALUE_TYPE_STRING ) {
                int ret;
                prelude_string_t *str;

                ret = prelude_string_new_dup_fast(&str, data, size);
                if ( ret < 0 )
                        return ret;

                return idmef_value_new_string((idmef_value_t **) out, str);
        }

        else if ( type == IDMEF_VALUE_TYPE_TIME )
                return idmef_value_new_time((idmef_value_t **) out, idmef_time_ref(data));

        return idmef_value_new_from_string((idmef_value_t **) out, type, data);
}


int preludedb_result_values_get_field(preludedb_result_values_t *result, void *row, preludedb_selected_path_t *selected, idmef_value_t **field)
{
        prelude_return_val_if_fail(result && row && field, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! result->db->plugin->get_result_values_field )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "format plugin doesn't implement value selection");

        return result->db->plugin->get_result_values_field(result, row, selected, internal_value_cb, (void **) field);
}


int preludedb_result_values_get_field_direct(preludedb_result_values_t *result, void *row, preludedb_selected_path_t *selected,
                                             preludedb_result_values_get_field_cb_func_t cb, void **out)
{
        prelude_return_val_if_fail(result && row && cb && out, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! result->db->plugin->get_result_values_field )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "format plugin doesn't implement value selection");

        return result->db->plugin->get_result_values_field(result, row, selected, cb, out);
}


static int get_message_idents_prepare_order(preludedb_t *db, idmef_class_id_t message_type,
                                            preludedb_result_idents_order_t order,
                                            preludedb_path_selection_t **path_selection)
{
        int ret;
        preludedb_selected_path_t *selected_path;
        const char *path = (message_type == IDMEF_CLASS_ID_ALERT) ? "alert.create_time": "heartbeat.create_time";

        ret = preludedb_path_selection_new(db, path_selection);
        if ( ret < 0 )
                return ret;

        ret = preludedb_selected_path_new_string(&selected_path, path);
        if ( ret < 0 ) {
                preludedb_path_selection_destroy(*path_selection);
                return ret;
        }

        preludedb_selected_path_set_flags(selected_path,
                                          (order == PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_DESC) ?
                                           PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_DESC : PRELUDEDB_SELECTED_PATH_FLAGS_ORDER_ASC);

        ret = preludedb_path_selection_add(*path_selection, selected_path);
        if ( ret < 0 ) {
                preludedb_path_selection_destroy(*path_selection);
                preludedb_selected_path_destroy(selected_path);
        }

        return ret;
}


static int
preludedb_get_message_idents(preludedb_t *db,
                             idmef_criteria_t *criteria,
                             int (*get_idents)(preludedb_t *db, idmef_criteria_t *criteria,
                                               int limit, int offset,
                                               const preludedb_path_selection_t *order,
                                               void **res),
                             int limit, int offset,
                             const preludedb_path_selection_t *order,
                             preludedb_result_idents_t **result)
{
        int ret;

        *result = calloc(1, sizeof(**result));
        if ( ! *result )
                return preludedb_error_from_errno(errno);

        ret = get_idents(db, criteria, limit, offset, order, &(*result)->res);
        if ( ret <= 0 ) {
                free(*result);
                return ret;
        }

        (*result)->refcount++;
        (*result)->db = preludedb_ref(db);

        return ret;
}



/**
 * preludedb_get_alert_idents:
 * @db: Pointer to a db object.
 * @criteria: Pointer to an idmef criteria.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @order: Result order.
 * @result: Idents result.
 *
 * Returns: the number of result or a negative value if an error occured.
 */
int preludedb_get_alert_idents(preludedb_t *db,
                               idmef_criteria_t *criteria, int limit, int offset,
                               preludedb_result_idents_order_t order,
                               preludedb_result_idents_t **result)
{
        int ret;
        preludedb_path_selection_t *order_selection;

        prelude_return_val_if_fail(db && result, prelude_error(PRELUDE_ERROR_ASSERTION));

        ret = get_message_idents_prepare_order(db, IDMEF_CLASS_ID_ALERT, order, &order_selection);
        if ( ret < 0 )
                return ret;

        ret = preludedb_get_message_idents(db, criteria, db->plugin->get_alert_idents, limit, offset, order_selection, result);
        preludedb_path_selection_destroy(order_selection);

        return ret;
}



/**
 * preludedb_get_alert_idents2:
 * @db: Pointer to a db object.
 * @criteria: Pointer to an idmef criteria.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @order: Result order.
 * @result: Idents result.
 *
 * Returns: the number of result or a negative value if an error occured.
 */
int preludedb_get_alert_idents2(preludedb_t *db,
                                idmef_criteria_t *criteria, int limit, int offset,
                                const preludedb_path_selection_t *order,
                                preludedb_result_idents_t **result)
{
        prelude_return_val_if_fail(db && result, prelude_error(PRELUDE_ERROR_ASSERTION));
        return preludedb_get_message_idents(db, criteria, db->plugin->get_alert_idents, limit, offset, order, result);
}



/**
 * preludedb_get_heartbeat_idents:
 * @db: Pointer to a db object.
 * @criteria: Pointer to an idmef criteria.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @order: Result order.
 * @result: Idents result.
 *
 * Returns: the number of result or a negative value if an error occured.
 */
int preludedb_get_heartbeat_idents(preludedb_t *db,
                                   idmef_criteria_t *criteria, int limit, int offset,
                                   preludedb_result_idents_order_t order,
                                   preludedb_result_idents_t **result)
{
        int ret;
        preludedb_path_selection_t *order_selection;

        prelude_return_val_if_fail(db && result, prelude_error(PRELUDE_ERROR_ASSERTION));

        ret = get_message_idents_prepare_order(db, IDMEF_CLASS_ID_HEARTBEAT, order, &order_selection);
        if ( ret < 0 )
                return ret;

        ret = preludedb_get_message_idents(db, criteria, db->plugin->get_heartbeat_idents, limit, offset, order_selection, result);
        preludedb_path_selection_destroy(order_selection);

        return ret;
}



/**
 * preludedb_get_heartbeat_idents2:
 * @db: Pointer to a db object.
 * @criteria: Pointer to an idmef criteria.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @order: Result order.
 * @result: Idents result.
 *
 * Returns: the number of result or a negative value if an error occured.
 */
int preludedb_get_heartbeat_idents2(preludedb_t *db,
                                    idmef_criteria_t *criteria, int limit, int offset,
                                    const preludedb_path_selection_t *order,
                                    preludedb_result_idents_t **result)
{
        prelude_return_val_if_fail(db && result, prelude_error(PRELUDE_ERROR_ASSERTION));
        return preludedb_get_message_idents(db, criteria, db->plugin->get_heartbeat_idents, limit, offset, order, result);
}



/**
 * preludedb_get_alert:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the alert.
 * @message: Pointer to an idmef message object where the retrieved message will be stored.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_get_alert(preludedb_t *db, uint64_t ident, idmef_message_t **message)
{
        prelude_return_val_if_fail(db && message, prelude_error(PRELUDE_ERROR_ASSERTION));
        return db->plugin->get_alert(db, ident, message);
}



/**
 * preludedb_get_heartbeat:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the heartbeat.
 * @message: Pointer to an idmef message object where the retrieved message will be stored.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_get_heartbeat(preludedb_t *db, uint64_t ident, idmef_message_t **message)
{
        prelude_return_val_if_fail(db && message, prelude_error(PRELUDE_ERROR_ASSERTION));
        return db->plugin->get_heartbeat(db, ident, message);
}


/**
 * preludedb_delete_alert:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the alert.
 *
 * Delete an alert.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_delete_alert(preludedb_t *db, uint64_t ident)
{
        prelude_return_val_if_fail(db, prelude_error(PRELUDE_ERROR_ASSERTION));
        return db->plugin->delete_alert(db, ident);
}


/**
 * preludedb_delete_alert_from_list:
 * @db: Pointer to a db object.
 * @idents: Pointer to an array of idents.
 * @size: Size of @idents.
 *
 * Delete all alerts from ident stored within @idents.
 *
 * Returns: the number of alert deleted on success, or a negative value if an error occur.
 */
ssize_t preludedb_delete_alert_from_list(preludedb_t *db, uint64_t *idents, size_t isize)
{
        prelude_return_val_if_fail(db, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( isize == 0 )
                return 0;

        return _preludedb_plugin_format_delete_alert_from_list(db->plugin, db, idents, isize);
}


/**
 * preludedb_delete_alert_from_result_idents:
 * @db: Pointer to a db object.
 * @result: Pointer to an idents result object.
 *
 * Delete all alert from ident stored within @result.
 *
 * Returns: the number of alert deleted on success, or a negative value if an error occur.
 */
ssize_t preludedb_delete_alert_from_result_idents(preludedb_t *db, preludedb_result_idents_t *result)
{
        prelude_return_val_if_fail(db && result, prelude_error(PRELUDE_ERROR_ASSERTION));
        return _preludedb_plugin_format_delete_alert_from_result_idents(db->plugin, db, result);
}



/**
 * preludedb_delete_heartbeat:
 * @db: Pointer to a db object.
 * @ident: Internal database ident of the heartbeat.
 *
 * Delete an heartbeat.
 *
 * Returns: 0 on success, or a negative value if an error occur.
 */
int preludedb_delete_heartbeat(preludedb_t *db, uint64_t ident)
{
        prelude_return_val_if_fail(db, prelude_error(PRELUDE_ERROR_ASSERTION));
        return db->plugin->delete_heartbeat(db, ident);
}


/**
 * preludedb_delete_heartbeat_from_list:
 * @db: Pointer to a db object.
 * @idents: Pointer to an array of idents.
 * @size: Size of @idents.
 *
 * Delete all heartbeat from ident stored within @idents.
 *
 * Returns: the number of heartbeat deleted on success, or a negative value if an error occur.
 */
ssize_t preludedb_delete_heartbeat_from_list(preludedb_t *db, uint64_t *idents, size_t isize)
{
        prelude_return_val_if_fail(db, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( isize == 0 )
                return 0;

        return _preludedb_plugin_format_delete_heartbeat_from_list(db->plugin, db, idents, isize);
}



/**
 * preludedb_delete_heartbeat_from_result_idents:
 * @db: Pointer to a db object.
 * @result: Pointer to an idents result object.
 *
 * Delete all heartbeat from ident stored within @result.
 *
 * Returns: the number of heartbeat deleted on success, or a negative value if an error occur.
 */
ssize_t preludedb_delete_heartbeat_from_result_idents(preludedb_t *db, preludedb_result_idents_t *result)
{
        prelude_return_val_if_fail(db && result, prelude_error(PRELUDE_ERROR_ASSERTION));
        return _preludedb_plugin_format_delete_heartbeat_from_result_idents(db->plugin, db, result);
}



/**
 * preludedb_delete:
 * @db: Pointer to a db object.
 * @criteria: Pointer to a criteria object.
 *
 * Delete all database object matching @criteria.
 *
 * Returns: a negative value if an error occured.
 */
int preludedb_delete(preludedb_t *db, idmef_criteria_t *criteria)
{
        int ret;
        preludedb_result_idents_t *result;

        prelude_return_val_if_fail(db, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( db->plugin->delete )
                return db->plugin->delete(db, criteria);

        ret = idmef_criteria_get_class(criteria);
        if ( ret < 0 )
                return ret;

        if ( ret == IDMEF_CLASS_ID_ALERT ) {
                ret = preludedb_get_alert_idents2(db, criteria, -1, 0, NULL, &result);
                if ( ret <= 0 )
                        return ret;

                ret = preludedb_delete_alert_from_result_idents(db, result);
        }

        else if ( IDMEF_CLASS_ID_HEARTBEAT ) {
                ret = preludedb_get_heartbeat_idents2(db, criteria, -1, 0, NULL, &result);
                if ( ret <= 0 )
                        return ret;

                ret = preludedb_delete_heartbeat_from_result_idents(db, result);
        }

        preludedb_result_idents_destroy(result);
        return ret;
}



/**
 * preludedb_get_values:
 * @db: Pointer to a db object.
 * @path_selection: Pointer to a path selection.
 * @criteria: Pointer to a criteria object.
 * @distinct: Get distinct or not distinct result rows.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 * @result: Values result.
 *
 * Returns: 1 if there are result, 0 if there are none, or a negative value if an error occured.
 */
int preludedb_get_values(preludedb_t *db,
                         preludedb_path_selection_t *path_selection,
                         idmef_criteria_t *criteria,
                         prelude_bool_t distinct,
                         int limit, int offset,
                         preludedb_result_values_t **result)
{
        int ret;

        prelude_return_val_if_fail(db && path_selection && result, prelude_error(PRELUDE_ERROR_ASSERTION));

        *result = calloc(1, sizeof (**result));
        if ( ! *result )
                return preludedb_error_from_errno(errno);

        ret = db->plugin->get_values(db , path_selection, criteria, distinct, limit, offset, &(*result)->res);
        if ( ret <= 0 ) {
                free(*result);
                *result = NULL;
                return ret;
        }

        (*result)->refcount = 1;
        (*result)->db = preludedb_ref(db);
        (*result)->selection = preludedb_path_selection_ref(path_selection);

        return ret;
}



void *preludedb_result_values_get_data(preludedb_result_values_t *results)
{
        prelude_return_val_if_fail(results, NULL);
        return results->res;
}



preludedb_t *preludedb_result_values_get_db(preludedb_result_values_t *results)
{
        prelude_return_val_if_fail(results, NULL);
        return results->db;
}


int preludedb_result_values_get_count(preludedb_result_values_t *results)
{
        prelude_return_val_if_fail(results, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! results->db->plugin->get_result_values_count )
                return preludedb_error_verbose(PRELUDEDB_ERROR_GENERIC, "format plugin doesn't implement value count retrieval");

        return results->db->plugin->get_result_values_count(results);
}



preludedb_result_values_t *preludedb_result_values_ref(preludedb_result_values_t *results)
{
        prelude_return_val_if_fail(results, NULL);

        results->refcount++;
        return results;
}


unsigned int preludedb_result_values_get_field_count(preludedb_result_values_t *results)
{
        prelude_return_val_if_fail(results, prelude_error(PRELUDE_ERROR_ASSERTION));
        return preludedb_path_selection_get_count(results->selection);
}


ssize_t preludedb_update_from_list(preludedb_t *db,
                                   const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                   uint64_t *idents, size_t isize)
{
        prelude_return_val_if_fail(db && paths && values, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! db->plugin->update_from_list )
                return preludedb_error_from_errno(ENOSYS);

        return db->plugin->update_from_list(db, paths, values, pvsize, idents, isize);
}


ssize_t preludedb_update_from_result_idents(preludedb_t *db,
                                            const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                                            preludedb_result_idents_t *result)
{
        prelude_return_val_if_fail(db && paths && values && result, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! db->plugin->update_from_result_idents )
                return PRELUDEDB_ENOTSUP("update_from_result_ident");

        return db->plugin->update_from_result_idents(db, paths, values, pvsize, result);
}



/**
 * preludedb_update:
 * @db: Pointer to a db object.
 * @paths: Array of path to update
 * @values: Array of value for their respective @paths.
 * @pvsize: Number of element in the @paths and @values array.
 * @criteria: Criteria updated event should match.
 * @order: Optional list of path used to order the update command.
 * @limit: Limit of results or -1 if no limit.
 * @offset: Offset in results or -1 if no offset.
 *
 *
 * Returns: the number of result or a negative value if an error occured.
 */

int preludedb_update(preludedb_t *db,
                     const idmef_path_t * const *paths, const idmef_value_t * const *values, size_t pvsize,
                     idmef_criteria_t *criteria,
                     preludedb_path_selection_t *order,
                     int limit, int offset)
{
        prelude_return_val_if_fail(db && paths && values, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! db->plugin->update )
                return PRELUDEDB_ENOTSUP("update");

        return db->plugin->update(db, paths, values, pvsize, criteria, order, limit, offset);
}



/**
 * preludedb_optimize:
 * @db: Pointer to a db object.
 *
 * Call a database optimization function, if supported.
 *
 * Returns: 0 on success or a negative value if an error occurred.
 */
int preludedb_optimize(preludedb_t *db)
{
        prelude_return_val_if_fail(db, prelude_error(PRELUDE_ERROR_ASSERTION));

        if ( ! db->plugin->optimize )
                return PRELUDEDB_ENOTSUP("optimize");

        return db->plugin->optimize(db);
}




/**
 * preludedb_transaction_start:
 * @db: Pointer to a #preludedb_t object.
 *
 * Begin a transaction using @db object. Internal transaction
 * handling will be disabled until preludedb_transaction_end()
 * or preludedb_transaction_abort() is called.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_transaction_start(preludedb_t *db)
{
        int ret;

        prelude_return_val_if_fail(db && db->sql, prelude_error(PRELUDE_ERROR_ASSERTION));

        ret = _preludedb_sql_transaction_start(db->sql);
        if ( ret < 0 )
                return ret;

        _preludedb_sql_disable_internal_transaction(db->sql);

        return ret;
}



/**
 * preludedb_transaction_end:
 * @db: Pointer to a #preludedb_t object.
 *
 * Terminate a sql transaction (SQL COMMIT command) initiated
 * with preludedb_transaction_start(). Internal transaction
 * handling will be enabled again once this function return.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_transaction_end(preludedb_t *db)
{
        int ret;

        prelude_return_val_if_fail(db && db->sql, prelude_error(PRELUDE_ERROR_ASSERTION));

        ret = _preludedb_sql_transaction_end(db->sql);
        _preludedb_sql_enable_internal_transaction(db->sql);

        if ( ret < 0 )
                return ret;

        return ret;
}



/**
 * preludedb_transaction_abort:
 * @db: Pointer to a #preludedb_t object.
 *
 * Abort a sql transaction (SQL ROLLBACK command) initiated
 * with preludedb_transaction_start(). Internal transaction
 * handling will be enabled again once this function return.
 *
 * Returns: 0 on success or a negative value if an error occur.
 */
int preludedb_transaction_abort(preludedb_t *db)
{
        int ret;

        prelude_return_val_if_fail(db && db->sql, prelude_error(PRELUDE_ERROR_ASSERTION));

        ret = _preludedb_sql_transaction_abort(db->sql);
        _preludedb_sql_enable_internal_transaction(db->sql);

        if ( ret < 0 )
                return ret;

        return ret;
}


preludedb_plugin_format_t *_preludedb_get_plugin_format(preludedb_t *db)
{
        return db->plugin;
}
