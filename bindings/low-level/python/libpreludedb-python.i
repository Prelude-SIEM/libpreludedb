/*****
*
* Copyright (C) 2005-2012 CS-SI. All Rights Reserved.
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

%{
void swig_python_raise_exception(int error, const char *strerror)
{
        PyObject *module;
        PyObject *exception_class;
        PyObject *exception;

        module = PyImport_ImportModule("preludedbold");
        exception_class = PyObject_GetAttrString(module, "PreludeDBError");
        exception = PyObject_CallFunction(exception_class, "is", error, strerror);

        if ( exception ) {
                PyErr_SetObject(exception_class, exception);
                Py_DECREF(exception);
        }

        Py_DECREF(module);
        Py_DECREF(exception_class);
}
%}

%exception {
   Py_BEGIN_ALLOW_THREADS
   $function
   Py_END_ALLOW_THREADS
}


%typemap(in, numinputs=0) preludedb_result_values_t **result2 ($*1_type tmp) {
        $1 = ($1_ltype) &tmp;
};


%typemap(in) preludedb_result_values_t ** {
        if ( $input == Py_None )
                return NULL;

        if ( SWIG_ConvertPtr($input, (void **)&arg$argnum, $1_descriptor, SWIG_POINTER_EXCEPTION|0) )
                return NULL;
}

%typemap(argout) preludedb_result_values_t **result2 {
        long size;

        size = PyInt_AsLong($result);
        if ( size < 0 ) {
                swig_python_raise_exception(size, NULL);
                $result = NULL;

        } else if ( size == 0 ) {
                $result = Py_None;
                Py_INCREF(Py_None);

        } else {
                PyObject *s = SWIG_NewPointerObj((void *) * $1, $*1_descriptor, 0);

                $result = PyTuple_New(2);
                PyTuple_SetItem($result, 0, s);
                PyTuple_SetItem($result, 1, PyInt_FromLong(size));
        }
};


%inline %{
int preludedb_get_values2(preludedb_t *db, preludedb_path_selection_t *path_selection,
                         idmef_criteria_t *criteria, prelude_bool_t distinct, int limit, int offset,
                         preludedb_result_values_t **result2)
{
        return preludedb_get_values(db, path_selection, criteria, distinct, limit, offset, result2);
}
%}


%typemap(in, numinputs=0) preludedb_result_idents_t **result2 ($*1_type tmp) {
        $1 = ($1_ltype) &tmp;
};


%typemap(in) preludedb_result_idents_t ** {
        if ( $input == Py_None )
                return NULL;

        if ( SWIG_ConvertPtr($input, (void **)&arg$argnum, $1_descriptor, SWIG_POINTER_EXCEPTION|0) )
                return NULL;
}

%typemap(argout) preludedb_result_idents_t **result2 {
        long size;

        size = PyInt_AsLong($result);
        if ( size < 0 ) {
                swig_python_raise_exception(size, NULL);
                $result = NULL;

        } else if ( size == 0 ) {
                $result = Py_None;
                Py_INCREF(Py_None);

        } else {
                PyObject *s = SWIG_NewPointerObj((void *) * $1, $*1_descriptor, 0);

                $result = PyTuple_New(2);
                PyTuple_SetItem($result, 0, s);
                PyTuple_SetItem($result, 1, PyInt_FromLong(size));
        }
};


%inline %{
int preludedb_get_alert_idents2(preludedb_t *db, idmef_criteria_t *criteria,
                               int limit, int offset,
                               preludedb_result_idents_order_t order,
                               preludedb_result_idents_t **result2)
{
        return preludedb_get_alert_idents(db, criteria, limit, offset, order, result2);
}

int preludedb_get_heartbeat_idents2(preludedb_t *db, idmef_criteria_t *criteria,
                                   int limit, int offset,
                                   preludedb_result_idents_order_t order,
                                   preludedb_result_idents_t **result2)
{
        return preludedb_get_heartbeat_idents(db, criteria, limit, offset, order, result2);
}
%}


%typemap(in) const char * {
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


%typemap(freearg) const char * "";


%typemap(in, numinputs=0) idmef_value_t ***values (idmef_value_t **tmp) {
        $1 = &tmp;
};


%typemap(argout) idmef_value_t ***values {
        long size;

        size = PyInt_AsLong($result);
        if ( size < 0 ) {
                swig_python_raise_exception(size, NULL);
                $result = NULL;

        } else if ( size == 0 ) {
                $result = Py_None;
                Py_INCREF(Py_None);

        } else {
                idmef_value_t **values = *($1);
                int cnt;

                $result = PyList_New(0);

                for ( cnt = 0; cnt < size; cnt++ ) {
                        PyObject *s = SWIG_NewPointerObj((void *) values[cnt], $descriptor(idmef_value_t *), 0);
                        PyList_Append($result, s);
                        Py_XDECREF(s);
                }

                free(values);
        }
};



%typemap(in, numinputs=0) SWIGTYPE **OUTPARAM ($*1_type tmp) {
        $1 = ($1_ltype) &tmp;
};


%typemap(argout) SWIGTYPE **OUTPARAM {
        $result = SWIG_NewPointerObj((void *) * $1, $*1_descriptor, 0);
};


%typemap(in) SWIGTYPE *INPARAM {
        if ( $input == Py_None )
                return NULL;

        if ( SWIG_ConvertPtr($input, (void **)&arg$argnum, $1_descriptor, SWIG_POINTER_EXCEPTION|0) )
                return NULL;
}


%typemap(in, numinputs=0) uint64_t *ident (uint64_t tmp) {
        $1 = &tmp;
};


%typemap(in, numinputs=0) (char *errbuf, size_t size) (char tmp[PRELUDEDB_ERRBUF_SIZE]) {
        $1 = tmp;
        $2 = sizeof(tmp);
};

%typemap(out) int {
        if ( $1 < 0 ) {
                swig_python_raise_exception($1, NULL);
                return NULL;
        }

        /*
         * swig workaround: we want to access ret from some argout typemap when ret can
         * contain the number of results for example, but the C result is not reachable
         * from argout
         */
        $result = PyInt_FromLong($1);
};


%typemap(out) ssize_t {
        if ( $1 < 0 ) {
                swig_python_raise_exception($1, NULL);
                return NULL;
        }

        /*
         * swig workaround: we want to access ret from some argout typemap when ret can
         * contain the number of results for example, but the C result is not reachable
         * from argout
         */
        $result = PyLong_FromLongLong($1);
};


%typemap(argout) uint64_t *ident {
        if ( PyInt_AsLong($result) == 0 ) {
                $result = Py_None;
                Py_INCREF(Py_None);
        }
        else
                $result = PyLong_FromUnsignedLongLong(*($1));
};


%typemap(in, numinputs=0) prelude_string_t *output {
        int ret;

        ret = prelude_string_new(&($1));
        if ( ret < 0 ) {
                swig_python_raise_exception(ret, NULL);
                return NULL;
        }
};


%typemap(argout) prelude_string_t *output {
        $result = PyString_FromStringAndSize(prelude_string_get_string($1), prelude_string_get_len($1));
        prelude_string_destroy($1);
};


%typemap(in, numinputs=0) SWIGTYPE **OUTRESULT ($*1_type tmp) {
        $1 = ($1_ltype) &tmp;
};


%typemap(argout) SWIGTYPE **OUTRESULT {
        if ( PyInt_AsLong($result) == 0 ) {
                $result = Py_None;
                Py_INCREF(Py_None);
        }
        else
                $result = SWIG_NewPointerObj((void *) * $1, $*1_descriptor, 0);
};


%typemap(in, numinputs=0) char **output (char *tmp) {
        $1 = &tmp;
};


%typemap(argout) char **output {
        if ( PyInt_AsLong($result) < 0 ) {
                swig_python_raise_exception(PyInt_AsLong($result), NULL);
                return NULL;
        }

        $result = PyString_FromString(*$1);
        free(*$1);
};


%typemap(in) (preludedb_path_selection_t *order) {
        if ( SWIG_ConvertPtr($input, (void **)&arg$argnum, $1_descriptor, SWIG_POINTER_EXCEPTION|0) )
                return NULL;
};

%typemap(in) (const idmef_path_t **paths, const idmef_value_t **values, size_t pvsize) {
        size_t i = 0;

        if ( ! PyList_Check($input) ) {
                PyErr_SetString(PyExc_TypeError, "not a list");
                return NULL;
        }

        $3 = PyList_Size($input);

        $1 = malloc($3 * sizeof(const idmef_path_t *));
        if ( !$1 )
                return NULL;

        $2 = malloc($3 * sizeof(const idmef_value_t *));
        if ( !$2 ) {
                free($1);
                return NULL;
        }

        for ( i = 0; i < $3; i++ ) {
                PyObject *l = PyList_GetItem($input, i);

                if ( ! PyTuple_Check(l) ) {
                        PyErr_SetString(PyExc_TypeError, "not a tuple");
                        return NULL;
                }

                idmef_path_new_fast(&$1[i], PyString_AsString(PyTuple_GetItem(l, 0)));
                const char *val = PyString_AsString(PyTuple_GetItem(l, 1));
                if ( val )
                        idmef_value_new_from_string(&$2[i], idmef_path_get_value_type($1[i], -1), val);
                else
                        $2[i] = NULL;
        }
}

%typemap(freearg) (const idmef_path_t **paths, const idmef_value_t **values, size_t pvsize) {
   size_t i;

   for ( i = 0; i < $3; i++ )
        idmef_path_destroy($1[i]);

   free($1);

   for ( i = 0; i < $3; i++ ) {
        if ( $2[i] )
                idmef_value_destroy($2[i]);
   }

   free($2);
}

%typemap(in) (uint64_t *idents, size_t isize) {
        int i = 0;

        if ( ! PyList_Check($input) ) {
                PyErr_SetString(PyExc_TypeError,"not a list");
                return NULL;
        }

        $2 = PyList_Size($input);

        $1 = malloc($2 * sizeof(uint64_t));
        if ( !$1 )
                return NULL;

        for ( i = 0; i < $2; i++ )
                $1[i] = (uint64_t) PyLong_AsUnsignedLongLong(PyList_GetItem($input, i));
}

%typemap(freearg) (uint64_t *idents, size_t size) {
        free($1);
}



%pythoncode %{
import prelude

class PreludeDBError(prelude.PreludeError):
    def __str__(self):
        if self._strerror:
            return self._strerror

        return preludedb_strerror(self.errno) or ""
%}
