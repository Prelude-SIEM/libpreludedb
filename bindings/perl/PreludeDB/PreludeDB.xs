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


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <libprelude/list.h>
#include <libprelude/idmef.h>
#include <libpreludedb/idmef-db-values.h>
#include <libpreludedb/idmef-object-list.h>
#include <libpreludedb/db-type.h>
#include <libpreludedb/sql-connection-data.h>
#include <libpreludedb/sql.h>
#include <libpreludedb/db-connection.h>
#include <libpreludedb/db-connection-data.h>
#include <libpreludedb/db-interface.h>
#include <libpreludedb/db-interface-string.h>

static prelude_db_interface_t *p_iface = NULL;
static prelude_sql_connection_t *p_sqlconn = NULL;
static prelude_sql_table_t *p_table = NULL;
static prelude_sql_row_t *p_row = NULL;

static SV *field2scalar(prelude_sql_field_t *field)
{
	SV *sv;
	prelude_sql_field_type_t type;

	type = prelude_sql_field_info_type(field);
	switch ( type ) {
		
	case dbtype_int32:
		sv = newSViv(prelude_sql_field_value_int32(field));
		break;

	case dbtype_uint32:
		sv = newSVuv(prelude_sql_field_value_uint32(field));
		break;

	case dbtype_int64: case dbtype_uint64:
		sv = newSVpv(prelude_sql_field_value(field), 0);
		break;

	case dbtype_float:
		sv = newSVnv((double) prelude_sql_field_value_float(field));
		break;

	case dbtype_double:
		sv = newSVnv(prelude_sql_field_value_double(field));
		break;

	case dbtype_string:
		sv = newSVpv(prelude_sql_field_value(field), 0);
		break;

	default:
		sv = NULL;
	}

	return sv;
}

MODULE = PreludeDB

void
INIT()
CODE:
prelude_db_init();

void
END()
CODE:
prelude_db_shutdown();

MODULE = PreludeDB		PACKAGE = PreludeDB		

prelude_db_interface_t *
new(classname, conn_string)
const char *classname
const char *conn_string
CODE:
if ( p_iface ) {
	RETVAL = NULL;
} else {
	p_iface = prelude_db_interface_new_string(conn_string);
	RETVAL = p_iface;
}
OUTPUT:
RETVAL


MODULE = PreludeDB		PACKAGE = prelude_db_interface_tPtr

int
connect(iface)
prelude_db_interface_t *iface
CODE:
RETVAL = prelude_db_interface_connect(iface);
OUTPUT:
RETVAL

int
get_connection_type(iface)
prelude_db_interface_t *iface
PREINIT:
prelude_db_connection_t *dbconn;
CODE:
dbconn = prelude_db_interface_get_connection(iface);
RETVAL = dbconn ? prelude_db_connection_get_type(dbconn) : -1;
OUTPUT:
RETVAL

prelude_sql_connection_t *
get_sql_connection(iface)
prelude_db_interface_t *iface
PREINIT:
prelude_db_connection_t *dbconn;
CODE:
dbconn = prelude_db_interface_get_connection(iface);
if ( dbconn && prelude_db_connection_get_type(dbconn) == prelude_db_type_sql )
     p_sqlconn = prelude_db_connection_get(dbconn);
RETVAL = p_sqlconn;
OUTPUT:
RETVAL

void
DESTROY(iface)
prelude_db_interface_t *iface
CODE:
if ( p_table )
     prelude_sql_table_free(p_table);
prelude_db_interface_destroy(iface);
p_iface = NULL;
p_sqlconn = NULL;
p_table = NULL;
p_row = NULL;

MODULE = PreludeDB		PACKAGE = prelude_sql_connection_tPtr

int
errno(sqlconn)
prelude_sql_connection_t *sqlconn
CODE:
RETVAL = p_iface ? prelude_sql_errno(sqlconn) : 0;
OUTPUT:
RETVAL

char *
error(sqlconn)
prelude_sql_connection_t *sqlconn
CODE:
RETVAL = p_iface ? (char *) prelude_sql_error(sqlconn) : NULL;
OUTPUT:
RETVAL

prelude_sql_table_t *
query(sqlconn, query_string)
prelude_sql_connection_t *sqlconn
const char *query_string
CODE:
p_table = p_iface ? prelude_sql_query(sqlconn, query_string) : NULL;
RETVAL = p_table;
OUTPUT:
RETVAL

int
begin(sqlconn)
prelude_sql_connection_t *sqlconn
CODE:
RETVAL = p_iface ? prelude_sql_begin(sqlconn) : 0;
OUTPUT:
RETVAL

int
commit(sqlconn)
prelude_sql_connection_t *sqlconn
CODE:
RETVAL = p_iface ? prelude_sql_commit(sqlconn) : 0;
OUTPUT:
RETVAL

int
rollback(sqlconn)
prelude_sql_connection_t *sqlconn
CODE:
RETVAL = p_iface ? prelude_sql_rollback(sqlconn) : 0;
OUTPUT:
RETVAL

char *
escape(sqlconn, str)
prelude_sql_connection_t *sqlconn
char *str
CODE:
RETVAL = p_iface ? prelude_sql_escape(sqlconn, str) : NULL;
OUTPUT:
RETVAL

void
close(sqlconn)
prelude_sql_connection_t *sqlconn
CODE:
prelude_sql_close(sqlconn);

MODULE = PreludeDB		PACKAGE = prelude_sql_table_tPtr

char *
field_name(table, i)
prelude_sql_table_t *table
unsigned int i
CODE:
RETVAL = p_sqlconn ? (char *) prelude_sql_field_name(table, i) : NULL;
OUTPUT:
RETVAL

unsigned int
fields_num(table)
prelude_sql_table_t *table
CODE:
RETVAL = p_sqlconn ? prelude_sql_fields_num(table) : 0;
OUTPUT:
RETVAL

unsigned int
rows_num(table)
prelude_sql_table_t *table
CODE:
RETVAL = p_sqlconn ? prelude_sql_rows_num(table) : 0;
OUTPUT:
RETVAL

prelude_sql_row_t *
row_fetch(table)
prelude_sql_table_t *table
CODE:
p_row = p_sqlconn ? prelude_sql_row_fetch(table) : NULL;
RETVAL = p_row;
OUTPUT:
RETVAL

void
row_fetch_array(table)
prelude_sql_table_t *table
PREINIT:
prelude_sql_row_t *row;
int fields;
PPCODE:
if ( p_sqlconn ) {
	row = prelude_sql_row_fetch(table);
	fields = prelude_sql_fields_num(table);
	if ( row ) {
		SV *sv;
		int cnt;
		prelude_sql_field_t *field;
		for (cnt = 0; cnt < fields; cnt++) {
			field = prelude_sql_field_fetch(row, cnt);
			sv = field2scalar(field);
			XPUSHs(sv_2mortal(sv));
		}
	}
}

void
row_fetch_hash(table)
prelude_sql_table_t *table
PREINIT:
prelude_sql_row_t *row;
int fields;
PPCODE:
if ( p_sqlconn ) {
	row = prelude_sql_row_fetch(table);
	fields = prelude_sql_fields_num(table);
	if ( row ) {
		SV *sv;
		int cnt;
		prelude_sql_field_t *field;
		for (cnt = 0; cnt < fields; cnt++) {
			field = prelude_sql_field_fetch(row, cnt);
			sv = field2scalar(field);
			XPUSHs(sv_2mortal(newSVpv(prelude_sql_field_name(table, cnt), 0)));
			XPUSHs(sv_2mortal(sv));
		}
	}
}

void
DESTROY(table)
prelude_sql_table_t *table
CODE:
if ( p_sqlconn )
     prelude_sql_table_free(table);
p_table = NULL;
p_row = NULL;


MODULE = PreludeDB		PACKAGE = prelude_sql_row_tPtr

SV *
field_fetch(row, sv)
prelude_sql_row_t *row
SV *sv
PREINIT:
prelude_sql_field_t *field;
CODE:
RETVAL = NULL;
if ( p_table ) {
	if ( SvIOK(sv) )
		field = prelude_sql_field_fetch(row, SvUV(sv));
	else if ( SvPOK(sv) )
		field = prelude_sql_field_fetch_by_name(row, SvPV_nolen(sv));
}
RETVAL = field ? field2scalar(field) : NULL;
OUTPUT:
RETVAL
