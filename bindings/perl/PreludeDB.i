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
#include "db-uident.h"
#include "db-connection.h"
#include "db-connection-data.h"
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
	sscanf(SvPV_nolen($input), "%llu", &($1));
};

%typemap(out) uint64_t {
	char tmp[32];
	int len;

	len = sprintf(tmp, "%llu", $1);
	$result = sv_2mortal(newSVpv(tmp, len));
	argvi++;
};

/* this piece of code is not thread safe... */

/*
 * FIXME: do data checking
 */

%typemap(in) prelude_db_alert_uident_t * {
	static prelude_db_alert_uident_t uident;
	HV *hv;
	SV **alert_ident;
	SV **analyzerid;

	hv = (HV *) SvRV($input);

	alert_ident = hv_fetch(hv, "alert_ident", sizeof ("alert_ident") - 1, 0);
	sscanf(SvPV_nolen(*alert_ident), "%llu", &uident.alert_ident);

	analyzerid = hv_fetch(hv, "analyzerid", sizeof ("analyzerid") - 1, 0);
	sscanf(SvPV_nolen(*analyzerid), "%llu", &uident.analyzerid);

	$1 = &uident;
};

%typemap(out) prelude_db_alert_uident_t * {
	prelude_db_alert_uident_t *uident = $1;
	HV *hv;
	SV *sv;
	char tmp[32];
	int len;

	if ( uident ) {

		hv = newHV();

		len = sprintf(tmp, "%llu", uident->alert_ident);
		sv = newSVpv(tmp, len);
		hv_store(hv, "alert_ident", sizeof ("alert_ident") - 1, sv, 0);

		len = sprintf(tmp, "%llu", uident->analyzerid);
		sv = newSVpv(tmp, len);
		hv_store(hv, "analyzerid", sizeof ("analyzerid") - 1, sv, 0);

		$result = sv_2mortal(newRV_noinc((SV *) hv));

	} else {
		$result = &PL_sv_undef;
	}

	argvi++;
};

%typemap(in) prelude_db_heartbeat_uident_t * {
	static prelude_db_heartbeat_uident_t uident;
	HV *hv;
	SV **heartbeat_ident;
	SV **analyzerid;

	hv = (HV *) SvRV($input);

	heartbeat_ident = hv_fetch(hv, "heartbeat_ident", sizeof ("heartbeat_ident") - 1, 0);
	sscanf(SvPV_nolen(*heartbeat_ident), "%llu", &uident.heartbeat_ident);

	analyzerid = hv_fetch(hv, "analyzerid", sizeof ("analyzerid") - 1, 0);
	sscanf(SvPV_nolen(*analyzerid), "%llu", &uident.analyzerid);

	$1 = &uident;
};

%typemap(out) prelude_db_heartbeat_uident_t * {
	prelude_db_heartbeat_uident_t *uident = $1;
	HV *hv;
	SV *sv;
	char tmp[32];
	int len;

	if ( uident ) {

		hv = newHV();

		len = sprintf(tmp, "%llu", uident->heartbeat_ident);
		sv = newSVpv(tmp, len);
		hv_store(hv, "heartbeat_ident", sizeof ("heartbeat_ident") - 1, sv, 0);

		len = sprintf(tmp, "%llu", uident->analyzerid);
		sv = newSVpv(tmp, len);
		hv_store(hv, "analyzerid", sizeof ("analyzerid") - 1, sv, 0);

		$result = sv_2mortal(newRV_noinc((SV *) hv));

	} else {
		$result = &PL_sv_undef;
	}

	argvi++;
};

%include "../../src/include/db-type.h"
%include "../../src/include/db.h"
%include "../../src/include/db-interface.h"
%include "../../src/include/db-interface-string.h"
%include "../../src/include/db-connection.h"
%include "../../src/include/sql.h"

prelude_sql_connection_t *_prelude_db_connection_get(prelude_db_connection_t *dbconn);
