/*****
*
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

%module PreludeDB
%{
#include <pthread.h>
#include <libprelude/prelude-list.h>
#include <libprelude/idmef.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-object-selection.h"
#include "preludedb.h"

%}

typedef struct idmef_value idmef_value_t;

%typemap(perl5, in) char ** {
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


%typemap(python, in) const char * {
        if ( $input == Py_None )
                $1 = NULL;
        else if ( PyString_Check($input) )
                $1 = PyString_AsString($input);
        else {
                PyErr_Format(PyExc_TypeError,
                             "expected None or string, %s found", $input->ob_type->tp_name);
                return NULL;
        }
};


%typemap(perl5, in) uint8_t {
	$1 = (uint8_t) SvIV($input);
};


%typemap(perl5, in) uint64_t {
	if ( SvIOK($input) ) {
		$1 = (uint64_t) SvIV($input);

	} else {
		if ( sscanf(SvPV_nolen($input), "%" PRIu64, &($1)) < 1 ) {
			croak("Argument %s is not an unsigned 64 bits integer\n", SvPV_nolen($input));
		}
	}
};


%typemap(perl5, out) uint64_t {
	char tmp[32];
	int len;

	len = snprintf(tmp, sizeof (tmp), "%" PRIu64, $1);

	if ( len >= 0 && len < sizeof (tmp) ) {
		$result = sv_2mortal(newSVpv(tmp, len));
		argvi++;
	}
};


%typemap(python, in, numinputs=0) idmef_value_t ***values (idmef_value_t **tmp) {
	$1 = &tmp;
};

%typemap(python, argout) idmef_value_t ***values {
	long size;

	size = PyInt_AsLong($result);
	if ( size <= 0 ) {
		$result = Py_None;

	} else {
		idmef_value_t **values = *($1);
		int cnt;

		$result = PyList_New(0);

		for ( cnt = 0; cnt < size; cnt++ ) {
			PyList_Append($result,
				      SWIG_NewPointerObj((void *) values[cnt], $descriptor(idmef_value_t *), 0));
		}

		free(values);
	}
};


%typemap(python, in, numinputs=0) preludedb_sql_t **new (preludedb_sql_t *tmp) {
	$1 = &tmp;
};

%typemap(python, argout) preludedb_sql_t **new {
	if ( PyInt_AsLong($result) < 0 )
		$result = Py_None;
	else
		$result = SWIG_NewPointerObj((void *) * $1, SWIGTYPE_p_preludedb_sql_t, 0);
};


%typemap(python, in, numinputs=0) uint64_t *ident (uint64_t tmp) {
	$1 = &tmp;
};

%typemap(python, argout) uint64_t *ident {
	if ( PyInt_AsLong($result) <= 0 )
		$result = Py_None;
	else
		$result = PyLong_FromUnsignedLongLong(*($1));
};


typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

%include "preludedb-sql-settings.h"
%include "preludedb-sql.h"
%include "preludedb.h"
%include "preludedb-object-selection.h"
