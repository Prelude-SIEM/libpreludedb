/*****
*
* Copyright (C) 2014-2019 CS-SI. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoann.v@prelude-ids.com>
*
* This file is part of the Prelude library.
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

%module preludedb

%feature("nothread");
%include "std_string.i"
%include "exception.i"
%include "std_map.i"
%include "std_vector.i"

%import "libpreludecpp.i"
//%feature("nothread", 0);


%template() std::vector<uint64_t>;
%template() std::vector<unsigned long long>;
%template() std::vector<std::string>;
%template() std::vector<char*>;
%template() std::map<std::string,std::string>;
%template() std::vector<Prelude::IDMEFPath>;
%template() std::vector<Prelude::IDMEFValue>;


%{
#define SWIG /* for _VECTOR_UINT64_TYPE */

#include <list>
#include <sstream>

#include "preludedb.hxx"

using namespace std;
using namespace PreludeDB;

%}

#define SWIG_Prelude_NewPointerObj SWIG_NewPointerObj

%init {
        int ret;

        ret = preludedb_init();
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


#ifdef SWIGPYTHON
%include python/libpreludedbcpp-python.i
#endif

%feature("exceptionclass") PreludeDB::PreludeDBError;
%feature("kwargs") PreludeDB::DB::ResultIdents::get;
%feature("kwargs") PreludeDB::DB::ResultValues::get;
%feature("kwargs") PreludeDB::DB::getAlertIdents;
%feature("kwargs") PreludeDB::DB::getHeartbeatIdents;
%feature("kwargs") PreludeDB::DB::getValues;
%feature("kwargs") PreludeDB::DB::update;
%feature("kwargs") PreludeDB::checkVersion;

%feature("nothread", "0") PreludeDB::SQL::query;
%feature("nothread", "0") PreludeDB::DB::getAlert;
%feature("nothread", "0") PreludeDB::DB::getAlertIdents;
%feature("nothread", "0") PreludeDB::DB::deleteAlert;
%feature("nothread", "0") PreludeDB::DB::getHeartbeat;
%feature("nothread", "0") PreludeDB::DB::getHeartbeatIdents;
%feature("nothread", "0") PreludeDB::DB::deleteHeartbeat;
%feature("nothread", "0") PreludeDB::DB::getValues;
%feature("nothread", "0") PreludeDB::DB::update;
%feature("nothread", "0") PreludeDB::DB::updateFromList;



%ignore preludedb_t;
%ignore preludedb_sql_t;
%ignore *::operator [];
%ignore *::operator preludedb_sql_t *;
%ignore *::operator preludedb_path_selection_t *;
%ignore PreludeDB::SQL::escape(const std::string &str);


%typemap(in) Prelude::IDMEFCriteria * {
        int ret, alloc = 0;
        void *crit = NULL;
        char *strin = NULL;

        ret = SWIG_AsCharPtrAndSize($input, &strin, NULL, &alloc);
        if ( SWIG_IsOK(ret) ) {
                if ( strin ) {
                        try {
                                $1 = new Prelude::IDMEFCriteria(strin);
                        } catch (Prelude::PreludeError &err) {
                                SWIG_exception_fail(SWIG_ArgError(res1), err);
                        }
                }
        } else {
                ret = SWIG_ConvertPtr($input, &crit, $descriptor(Prelude::IDMEFCriteria *), 0);
                if ( SWIG_IsOK(ret) )
                        $1 = new Prelude::IDMEFCriteria(*(Prelude::IDMEFCriteria *) crit);
        }

        if ( ! SWIG_IsOK(ret) )
                SWIG_exception_fail(SWIG_ArgError(res1), "Input should be a Prelude::IDMEFCriteria or string");
}


%typemap(freearg) Prelude::IDMEFCriteria * {
        if ( $1 )
                delete($1);
}


%import(module="prelude") <libprelude/prelude-error.hxx>
%include preludedb.hxx
%include preludedb-error.hxx
%include preludedb-sql.hxx
