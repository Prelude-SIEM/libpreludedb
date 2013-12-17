/*****
*
* Copyright (C) 2003-2012 CS-SI. All Rights Reserved.
* Author: Nicolas Delon <nicolas.delon@prelude-ids.com>
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#define inline 

%{
#include <libprelude/prelude.h>
#include <libprelude/idmef.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"
#include "preludedb-version.h"
#include <libprelude/prelude-inttypes.h>
%}

typedef struct idmef_value idmef_value_t;
typedef struct idmef_criteria idmef_criteria_t;
typedef struct idmef_message idmef_message_t;
typedef struct idmef_time idmef_time_t;
typedef struct idmef_path idmef_path_t;
typedef struct idmef_criterion_value idmef_criterion_value_t;
typedef struct prelude_string prelude_string_t;

typedef int int32_t;
typedef unsigned int uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

%ignore preludedb_error_t;
typedef signed int preludedb_error_t;

typedef enum { 
	PRELUDE_BOOL_TRUE = TRUE, 
	PRELUDE_BOOL_FALSE = FALSE 
} prelude_bool_t;


#ifdef SWIGPYTHON
%include libpreludedb-python.i
#endif /* ! SWIGPYTHON */

#ifdef SWIGPERL
%include perl/libpreludedb-perl.i
#endif /* ! SWIGPERL */

%apply SWIGTYPE **OUTPARAM {
        idmef_message_t **,
        preludedb_t **,
        preludedb_path_selection_t **,
        preludedb_result_idents_t **,
        preludedb_result_values_t **,
        preludedb_selected_path_t **,
        preludedb_sql_t **,
        preludedb_sql_settings_t **
};


%apply SWIGTYPE *INPARAM {
        idmef_message_t *,
        preludedb_t *,
        preludedb_path_selection_t *,
        preludedb_result_idents_t *,
        preludedb_result_values_t *,
        preludedb_selected_path_t *,
        preludedb_sql_t *,
        preludedb_sql_field_t *,
        preludedb_sql_row_t *,
        preludedb_sql_settings_t *,
        preludedb_sql_table_t *
};

%apply SWIGTYPE **OUTRESULT {
        preludedb_result_idents_t **,
        preludedb_result_values_t **,
        preludedb_sql_field_t **,
        preludedb_sql_row_t **,
        preludedb_sql_table_t **
};


%include "preludedb-sql-settings.h"
%include "preludedb-sql.h"
%include "preludedb.h"
%include "preludedb-error.h"
%include "preludedb-version.h"
%include "preludedb-path-selection.h"
