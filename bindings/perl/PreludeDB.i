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
#include <pthread.h>
#include <libprelude/list.h>
#include <libprelude/idmef.h>
#include "db-type.h"
#include "db.h"
#include "sql-connection-data.h"
#include "sql.h"
#include "db-message-ident.h"
#include "db-connection.h"
#include "db-connection-data.h"
#include "db-object-selection.h"
#include "db-interface.h"
#include "db-interface-string.h"

/*
 * workaround, the function prelude_db_connection_get returns a (void *)
 * and prelude_sql_* function take a (prelude_sql_connection_t *), once 
 * perl bless those types, there are not compatible anymore, so we create
 * this function to get a (prelude_sql_connection_t *) type and not a (void *)
 */

static prelude_sql_connection_t *_prelude_db_connection_get(prelude_db_connection_t *dbconn)
{
	return prelude_db_connection_get(dbconn);
}

%}

%typemap(in) char ** {
	AV *tempav;
	I32 len;
	int i;
	SV  **tv;

	if ( ! SvROK($input))
	    croak("Argument $argnum is not a reference.");

        if ( SvTYPE(SvRV($input)) != SVt_PVAV )
	    croak("Argument $argnum is not an array.");

        tempav = (AV*) SvRV($input);
	len = av_len(tempav);
	$1 = (char **) malloc((len+2)*sizeof(char *));
	if ( ! $1 )
		croak("out of memory\n");
	for ( i = 0; i <= len; i++ ) {
	    tv = av_fetch(tempav, i, 0);	
	    $1[i] = (char *) SvPV_nolen(*tv);
        }
	$1[i] = NULL;
};

%typemap(in) uint8_t {
	$1 = (uint8_t) SvIV($input);
};

%typemap(out) prelude_sql_field_t * {

	if ( $1 ) {
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
	}
};

%typemap(in) uint64_t {
	if ( SvIOK($input) ) {
		$1 = (uint64_t) SvIV($input);

	} else {
		if ( sscanf(SvPV_nolen($input), "%llu", &($1)) < 1 ) {
			croak("Argument %s is not an unsigned 64 bits integer\n", SvPV_nolen($input));
		}
	}
};

%typemap(out) uint64_t {
	char tmp[32];
	int len;

	len = snprintf(tmp, sizeof (tmp), "%llu", $1);

	if ( len >= 0 && len < sizeof (tmp) ) {
		$result = sv_2mortal(newSVpv(tmp, len));
		argvi++;
	}
};

%include "../../src/include/db-type.h"
%include "../../src/include/db.h"
%include "../../src/include/db-object-selection.h"
%include "../../src/include/db-message-ident.h"
%include "../../src/include/db-interface.h"
%include "../../src/include/db-interface-string.h"
%include "../../src/include/db-connection.h"
%include "../../src/include/sql.h"

prelude_sql_connection_t *_prelude_db_connection_get(prelude_db_connection_t *dbconn);
