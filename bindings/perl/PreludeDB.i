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

%module PreludeDB
%{
#include <libprelude/list.h>
#include <libprelude/idmef.h>
#include "db-type.h"
#include "sql-connection-data.h"
#include "sql.h"
#include "db-connection.h"
#include "db-connection-data.h"
#include "db-interface.h"
#include "db-interface-string.h"
%}

%typemap(in) char ** {
	AV *tempav;
	I32 len;
	int i;
	SV  **tv;
	if (!SvROK($input))
	    croak("Argument $argnum is not a reference.");
        if (SvTYPE(SvRV($input)) != SVt_PVAV)
	    croak("Argument $argnum is not an array.");
        tempav = (AV*)SvRV($input);
	len = av_len(tempav);
	$1 = (char **) malloc((len+2)*sizeof(char *));
	for (i = 0; i <= len; i++) {
	    tv = av_fetch(tempav, i, 0);	
	    $1[i] = (char *) SvPV_nolen(*tv);
        }
	$1[i] = NULL;
};

%typemap(in) uint8_t {
	$1 = (uint8_t) SvIV($input);
};

%typemap(out) prelude_sql_field_t * {

	switch ( prelude_sql_field_info_type($1) ) {
	case dbtype_int32:
                $result = newSViv(prelude_sql_field_value_int32($1));
		argvi++;
                break;

        case dbtype_uint32:
                $result = newSVuv(prelude_sql_field_value_uint32($1));
		argvi++;
                break;

        case dbtype_int64: case dbtype_uint64:
                $result = newSVpv(prelude_sql_field_value($1), 0);
		argvi++;
                break;

        case dbtype_float:
                $result = newSVnv((double) prelude_sql_field_value_float($1));
		argvi++;
                break;

        case dbtype_double:
                $result = newSVnv(prelude_sql_field_value_double($1));
		argvi++;
                break;

        case dbtype_string:
                $result = newSVpv(prelude_sql_field_value($1), 0);
		argvi++;
                break;

        default:
		/* nop */;
	}
};

%typemap(in) uint64_t {
/* 	$1 = (uint64_t) SvUV($input); */
/* 	if ( SvIOK($input) ) { */
/* 		printf("debug1.1: %u\n", SvUV($input)); */
/* 		$1 = (uint64_t) SvUV($input); */

/* 	} else { */
/* 		printf("debug1.2: %s\n", SvPV_nolen($input)); */
	sscanf(SvPV_nolen($input), "%llu", &($1));
/* 	} */
};

%typemap(out) uint64_t {
/* 	$result = newSViv($1); */
	char tmp[32];
	int len;

	len = sprintf(tmp, "%llu", $1);
	$result = sv_2mortal(newSVpv(tmp, len));
	argvi++;
};

typedef enum {
        prelude_db_type_invalid = 0,
        prelude_db_type_sql = 1
} prelude_db_type_t;

int prelude_db_init(void);
int prelude_db_shutdown(void);

prelude_db_interface_t *prelude_db_interface_new_string(const char *conn_string);
int prelude_db_interface_connect(prelude_db_interface_t *iface);
prelude_db_connection_t *prelude_db_interface_get_connection(prelude_db_interface_t *interface);
void prelude_db_interface_destroy(prelude_db_interface_t *iface);

prelude_db_ident_list_t * prelude_db_interface_get_ident_list(prelude_db_interface_t *interface,
                                                              idmef_criterion_t *criterion);
void prelude_db_interface_free_ident_list(prelude_db_ident_list_t *message_list);
uint64_t prelude_db_interface_get_next_ident(prelude_db_ident_list_t *ident_list);
idmef_message_t *prelude_db_interface_get_alert(prelude_db_interface_t *interface, 
                                                uint64_t ident,
                                                idmef_selection_t *selection);
idmef_message_t *prelude_db_interface_get_heartbeat(prelude_db_interface_t *interface,
                                                    uint64_t ident,
                                                    idmef_selection_t *selection);
int prelude_db_interface_delete_alert(prelude_db_interface_t *interface, int ident);
int prelude_db_interface_delete_heartbeat(prelude_db_interface_t *interface, int ident);
int prelude_db_interface_insert_idmef_message(prelude_db_interface_t * interface, const idmef_message_t * msg);

int prelude_db_connection_get_type(prelude_db_connection_t *dbconn);
prelude_sql_connection_t *prelude_db_connection_get(prelude_db_connection_t *dbconn);

int prelude_sql_errno(prelude_sql_connection_t *sqlconn);
const char *prelude_sql_error(prelude_sql_connection_t *sqlconn);
prelude_sql_table_t *prelude_sql_query(prelude_sql_connection_t *sqlconn, const char *query_string);
int prelude_sql_begin(prelude_sql_connection_t *sqlconn);
int prelude_sql_commit(prelude_sql_connection_t *sqlconn);
int prelude_sql_rollback(prelude_sql_connection_t *sqlconn);
char *prelude_sql_escape(prelude_sql_connection_t *sqlconn, const char *string);
void prelude_sql_close(prelude_sql_connection_t *sqlconn);

const char *prelude_sql_field_name(prelude_sql_table_t *table, int i);
int prelude_sql_field_num(prelude_sql_table_t *table, const char *name);
unsigned int prelude_sql_fields_num(prelude_sql_table_t *table);
unsigned int prelude_sql_rows_num(prelude_sql_table_t *table);
prelude_sql_row_t *prelude_sql_row_fetch(prelude_sql_table_t *table);
void prelude_sql_table_free(prelude_sql_table_t *table);

prelude_sql_field_t *prelude_sql_field_fetch(prelude_sql_row_t *row, unsigned int i);
prelude_sql_field_t *prelude_sql_field_fetch_by_name(prelude_sql_row_t *row, const char *name);
