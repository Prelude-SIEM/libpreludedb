/*****
*
* Copyright (C) 2005 PreludeIDS Technologies. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

%{
void swig_python_raise_exception(int error, const char *strerror)
{
        PyObject *module;
        PyObject *exception_class;
        PyObject *exception;

        module = PyImport_ImportModule("preludedb");
        exception_class = PyObject_GetAttrString(module, "PreludeDBError");
        exception = PyObject_CallFunction(exception_class, "is", error, strerror);

        PyErr_SetObject(exception_class, exception);

        Py_DECREF(module);
        Py_DECREF(exception_class);
        Py_DECREF(exception);
}

PyObject *swig_python_string(prelude_string_t *string)
{
        return PyString_FromStringAndSize(prelude_string_get_string(string), prelude_string_get_len(string));
}

PyObject *swig_python_data(idmef_data_t *data)
{
        switch ( idmef_data_get_type(data) ) {
        case IDMEF_DATA_TYPE_CHAR: case IDMEF_DATA_TYPE_BYTE:
                return PyString_FromStringAndSize(idmef_data_get_data(data), 1);

        case IDMEF_DATA_TYPE_CHAR_STRING:
                return PyString_FromStringAndSize(idmef_data_get_data(data), idmef_data_get_len(data) - 1);

        case IDMEF_DATA_TYPE_BYTE_STRING:
                return PyString_FromStringAndSize(idmef_data_get_data(data), idmef_data_get_len(data));

        case IDMEF_DATA_TYPE_UINT32:
                return PyLong_FromLongLong(idmef_data_get_uint32(data));

        case IDMEF_DATA_TYPE_UINT64:
                return PyLong_FromUnsignedLongLong(idmef_data_get_uint64(data));

        case IDMEF_DATA_TYPE_FLOAT:
                return PyFloat_FromDouble((double) idmef_data_get_float(data));

        default:
                return NULL;
        }
}
%}


%typemap(in) int *argc (int tmp) {
	tmp = PyInt_AsLong($input);
	$1 = &tmp;
};


%typemap(in) char **argv {
	/* Check if is a list */
	if ( PyList_Check($input) ) {
		int size = PyList_Size($input);
		int i = 0;

		$1 = (char **) malloc((size+1) * sizeof(char *));
		for ( i = 0; i < size; i++ ) {
			PyObject *o = PyList_GetItem($input,i);
			if ( PyString_Check(o) )
				$1[i] = PyString_AsString(PyList_GetItem($input, i));
			else {
				PyErr_SetString(PyExc_TypeError, "list must contain strings");
				free($1);
				return NULL;
			}
		}
		$1[i] = 0;
	} else {
		PyErr_SetString(PyExc_TypeError, "not a list");
		return NULL;
	}
};


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



%pythoncode %{
import prelude

class PreludeDBError(prelude.PreludeError):
    def __str__(self):
	if self._strerror:
	    return self._strerror

        return preludedb_strerror(self.errno) or ""
%}
