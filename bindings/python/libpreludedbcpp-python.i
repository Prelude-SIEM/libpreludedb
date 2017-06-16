%include pystrings.swg


%begin %{
#pragma GCC diagnostic ignored "-Wunused-variable"
# define SWIG_PYTHON_2_UNICODE
# define TARGET_LANGUAGE_SELF PyObject *
# define TARGET_LANGUAGE_OUTPUT_TYPE PyObject **
%}

%exception %{
        try {
                $action
        } catch (PreludeDBError &e) {
                        SWIG_Python_Raise(SWIG_NewPointerObj(new PreludeDBError(e),
                                                             SWIGTYPE_p_PreludeDB__PreludeDBError, SWIG_POINTER_OWN),
                                          "PreludeDBError", SWIGTYPE_p_PreludeDB__PreludeDBError);
                SWIG_fail;
        }
%}

%feature("flatnested");

%feature("python:slot", "tp_repr", functype="reprfunc") *::toString;
%feature("python:slot", "mp_length", functype="lenfunc") *::count;
%feature("python:slot", "mp_subscript") *::get;
%feature("python:slot", "tp_iter", functype="getiterfunc") *::__iter__;

/*
 * We cannot use iternextfunc, since the default SWIG wrapper incorrectly interpret Py_None
 * return as NULL. Unaryfunc let us do the processing ourself.
 */
%feature("python:slot", "tp_iternext", functype="unaryfunc") GenericIterator::next;
%feature("python:slot", "tp_iternext", functype="unaryfunc") GenericDirectIterator::next;


%typemap(out) _VECTOR_UINT64_TYPE *next {
        if ( ! $1 )
                return NULL;

        $result = SWIG_From_long_SS_long(*$1);
        delete($1);
}


%typemap(out) uint64_t *get {
        if ( ! $1 )
                return NULL;

        $result = SWIG_From_long_SS_long(*$1);
        delete($1);
}


%typemap(out) const char *next {
        if ( ! $1 ) {
                if ( arg1->_done )
                        return NULL;

                Py_INCREF(Py_None);
                return Py_None;
        }

        $result = SWIG_FromCharPtr($1);
}

%typemap(out) PreludeDB::SQL::Table::Row *next {
        if ( ! $1 )
                return NULL;

        $result = SWIG_NewPointerObj(SWIG_as_voidptr(result), $descriptor(PreludeDB::SQL::Table::Row *), 1);
};

%typemap(out) PreludeDB::DB::ResultValues::ResultValuesRow *next {
        if ( ! $1 )
                return NULL;

        $result = SWIG_NewPointerObj(SWIG_as_voidptr(result), $descriptor(PreludeDB::DB::ResultValues::ResultValuesRow *), 1);
};


%typemap(in) const std::vector<Prelude::IDMEFPath> &paths {
        int ret, alloc = 1, error = FALSE;
        char *str = NULL;
        Prelude::IDMEFPath *path;
        $1 = new std::vector<Prelude::IDMEFPath>();
        PyObject *item, *iterator = PyObject_GetIter($input);

        while ( (! error) && (item = PyIter_Next(iterator)) ) {
                ret = SWIG_ConvertPtr(item, (void **) &path, $descriptor(Prelude::IDMEFPath), 0);
                if ( SWIG_IsOK(ret) )
                        $1->push_back(*path);
                else {
                        ret = SWIG_AsCharPtrAndSize(item, &str, NULL, &alloc);
                        if ( ! SWIG_IsOK(ret) ) {
                                SWIG_exception_fail(SWIG_ArgError(res1), "Input should be a list of Prelude::IDMEFPath or string");
                                error = TRUE;
                        }
                        try {
                                $1->push_back(Prelude::IDMEFPath(str));
                        } catch ( Prelude::PreludeError &e ) {
                                SWIG_Python_Raise(SWIG_NewPointerObj(new PreludeDBError(e),
                                                  SWIGTYPE_p_PreludeDB__PreludeDBError, SWIG_POINTER_OWN),
                                                  "PreludeDBError", SWIGTYPE_p_PreludeDB__PreludeDBError);
                                error = TRUE;
                        }
                }

                Py_DECREF(item);
        }

        Py_DECREF(iterator);
        if ( error )
                SWIG_fail;
}


%typemap(freearg) const std::vector<Prelude::IDMEFPath> &paths {
        delete($1);
}


/*
 * We have to specify get function individually in order to avoid
 * freeing the const char * string returned by Row::get()
 */
%newobject *::__iter__;

%newobject PreludeDB::SQL::Table::get;
%newobject PreludeDB::SQL::Table::Row::get(PyObject *);
%newobject PreludeDB::DB::ResultValues::get;
%newobject PreludeDB::DB::ResultValues::ResultValuesRow::get;
%newobject PreludeDB::DB::ResultIdents::get(PyObject *);

%typemap(out, fragment="IDMEFValue_to_SWIG") Prelude::IDMEFValue * {
        int ret;

        if ( ! $1 )
                return NULL;

        if ( $1->isNull() ) {
                Py_INCREF(Py_None);
                $result = Py_None;
        } else {
                ret = IDMEFValue_to_SWIG(self, *$1, NULL, &$result);
                if ( ret < 0 ) {
                        std::stringstream s;
                        s << "IDMEFValue typemap does not handle value of type '" << idmef_value_type_to_string((idmef_value_type_id_t) $1->getType()) << "'";
                        SWIG_exception_fail(SWIG_ValueError, s.str().c_str());
                }
        }

        delete($1);
};



%wrapper %{
        int data_to_python(void **ret, void *data, size_t size, idmef_value_type_id_t type)
        {
                switch(type) {

                case 0:
                        *ret = Py_None;
                        Py_INCREF(Py_None);
                        break;

                case IDMEF_VALUE_TYPE_STRING:
                case IDMEF_VALUE_TYPE_DATA:
                        *ret = SWIG_FromCharPtrAndSize((const char *) data, size);
                        break;

                case IDMEF_VALUE_TYPE_INT8:
                case IDMEF_VALUE_TYPE_UINT8:
                case IDMEF_VALUE_TYPE_INT16:
                case IDMEF_VALUE_TYPE_UINT16:
                case IDMEF_VALUE_TYPE_INT32:
                case IDMEF_VALUE_TYPE_UINT32:
                case IDMEF_VALUE_TYPE_INT64:
                case IDMEF_VALUE_TYPE_UINT64:
#if PY_MAJOR_VERSION < 3
                        *ret = PyInt_FromString((char *) data, NULL, 10);
#else
                        *ret = PyLong_FromString((char *) data, NULL, 10);
#endif
                        break;

                case IDMEF_VALUE_TYPE_FLOAT:
                        *ret = SWIG_From_float(strtof((const char *)data, NULL));
                        break;

                case IDMEF_VALUE_TYPE_DOUBLE:
                        *ret = SWIG_From_double(strtod((const char *)data, NULL));
                        break;

                case IDMEF_VALUE_TYPE_ENUM:
                        *ret = SWIG_FromCharPtr((const char *) data);
                        break;

                case IDMEF_VALUE_TYPE_TIME:
                        *ret = SWIG_InternalNewPointerObj(new Prelude::IDMEFTime(idmef_time_ref((idmef_time_t *) data)), SWIGTYPE_p_Prelude__IDMEFTime, 1);
                        break;

                default:
                        return prelude_error_verbose(PRELUDE_ERROR_GENERIC, "Unknown data type '%d'", type);
        }

                return 0;
        }
%}

%inline %{
#if PY_VERSION_HEX >= 0x03020000
# define _SWIG_PY_SLICE_OBJECT PyObject
#else
# define _SWIG_PY_SLICE_OBJECT PySliceObject
#endif

void python2_return_unicode(int enabled)
{
    _PYTHON2_RETURN_UNICODE = enabled;
}

        extern "C" {
                int data_to_python(void **out, void *data, size_t size, idmef_value_type_id_t type);
        }

        template <class Parent, class Child>
        class GenericIterator {
                private:
                        ssize_t _start, _step, _i, _len;
                        Parent _rval;

                public:
                        bool _done;

                        GenericIterator(Parent &rval, ssize_t start, ssize_t step, ssize_t slicelength)
                        {
                                _i = 0;
                                _start = start;
                                _step = step;
                                _len = slicelength;
                                _rval = rval;
                                _done = FALSE;
                        };

                        GenericIterator __iter__(void) {
                                return *this;
                        };

                        Child *next(void) {
                                if ( _i >= _len ) {
                                        _done = TRUE;
                                        return NULL;
                                }

                                return (Child *) _rval.get(_i++ * _step + _start);
                        };
        };

        template <class Parent, class Child>
        class GenericDirectIterator {
                private:
                        ssize_t _start, _step, _len, _i;
                        Parent _rval;
                        preludedb_result_values_get_field_cb_func_t _cb;

                public:
                        GenericDirectIterator(Parent &rval, ssize_t start, ssize_t step, ssize_t slicelength,
                                preludedb_result_values_get_field_cb_func_t cb)
                        {
                                _i = 0;
                                _start = start;
                                _step = step;
                                _len = slicelength;
                                _rval = rval;
                                _cb = cb;
                        };

                        GenericDirectIterator __iter__(void) {
                                return *this;
                        };

                        Child *next(void) {
                                if ( _i >= _len )
                                        return NULL;

                                return (Child *) _rval.get(_i++ * _step + _start, _cb);
                        };
        };
%}

%extend PreludeDB::SQL::Table {
        GenericIterator<PreludeDB::SQL::Table, PreludeDB::SQL::Table::Row> *get(PyObject *item) {
                if ( ! PySlice_Check(item) )
                        throw PreludeDB::PreludeDBError("Object is not a slice");

                Py_ssize_t start = 0, stop = 0, step = 0, slicelength = 0;
                PySlice_GetIndicesEx((_SWIG_PY_SLICE_OBJECT *) item, self->count(), &start, &stop, &step, &slicelength);

                return new GenericIterator<PreludeDB::SQL::Table, PreludeDB::SQL::Table::Row>(*self, start, step, slicelength);
        };

        GenericIterator<PreludeDB::SQL::Table, PreludeDB::SQL::Table::Row> *__iter__(void) {
                return new GenericIterator<PreludeDB::SQL::Table, PreludeDB::SQL::Table::Row>(*self, 0, 1, self->count());
        };
}


%extend PreludeDB::SQL::Table::Row {
        GenericIterator<PreludeDB::SQL::Table::Row, const char> *get(PyObject *item) {
                if ( ! PySlice_Check(item) )
                        throw PreludeDB::PreludeDBError("Object is not a slice");

                Py_ssize_t start = 0, stop = 0, step = 0, slicelength = 0;
                PySlice_GetIndicesEx((_SWIG_PY_SLICE_OBJECT *) item, self->count(), &start, &stop, &step, &slicelength);

                return new GenericIterator<PreludeDB::SQL::Table::Row, const char>(*self, start, step, slicelength);
        };

        GenericIterator<PreludeDB::SQL::Table::Row, const char> *__iter__(void) {
                return new GenericIterator<PreludeDB::SQL::Table::Row, const char>(*self, 0, 1, self->count());
        };
}


%extend PreludeDB::DB::ResultValues {
        GenericIterator<PreludeDB::DB::ResultValues, PreludeDB::DB::ResultValues::ResultValuesRow> *get(PyObject *item) {
                if ( ! PySlice_Check(item) )
                        throw PreludeDB::PreludeDBError("Object is not a slice");

                Py_ssize_t start = 0, stop = 0, step = 0, slicelength = 0;
                PySlice_GetIndicesEx((_SWIG_PY_SLICE_OBJECT *) item, self->count(), &start, &stop, &step, &slicelength);

                return new GenericIterator<PreludeDB::DB::ResultValues, PreludeDB::DB::ResultValues::ResultValuesRow>(*self, start, step, slicelength);
        };

        GenericIterator<PreludeDB::DB::ResultValues, PreludeDB::DB::ResultValues::ResultValuesRow> *__iter__(void) {
                return new GenericIterator<PreludeDB::DB::ResultValues, PreludeDB::DB::ResultValues::ResultValuesRow>(*self, 0, 1, self->count());
        };
}

%extend PreludeDB::DB::ResultValues::ResultValuesRow {
        GenericDirectIterator<PreludeDB::DB::ResultValues::ResultValuesRow, PyObject> *get(PyObject *item) {
                if ( ! PySlice_Check(item) )
                        throw PreludeDB::PreludeDBError("Object is not a slice");

                Py_ssize_t start = 0, stop = 0, step = 0, slicelength = 0;
                PySlice_GetIndicesEx((_SWIG_PY_SLICE_OBJECT *) item, self->count(), &start, &stop, &step, &slicelength);

                return new GenericDirectIterator<PreludeDB::DB::ResultValues::ResultValuesRow, PyObject>(*self, start, step, slicelength, data_to_python);
        };

        GenericDirectIterator<PreludeDB::DB::ResultValues::ResultValuesRow, PyObject> *__iter__(void) {
                return new GenericDirectIterator<PreludeDB::DB::ResultValues::ResultValuesRow, PyObject>(*self, 0, 1, self->count(), data_to_python);
        };
}

%extend PreludeDB::DB::ResultIdents {
        GenericIterator<PreludeDB::DB::ResultIdents, _VECTOR_UINT64_TYPE> *get(PyObject *item) {
                if ( ! PySlice_Check(item) )
                        throw PreludeDB::PreludeDBError("Object is not a slice");

                Py_ssize_t start = 0, stop = 0, step = 0, slicelength = 0;
                PySlice_GetIndicesEx((_SWIG_PY_SLICE_OBJECT *) item, self->count(), &start, &stop, &step, &slicelength);

                return new GenericIterator<PreludeDB::DB::ResultIdents, _VECTOR_UINT64_TYPE>(*self, start, step, slicelength);
        };

        GenericIterator<PreludeDB::DB::ResultIdents, _VECTOR_UINT64_TYPE> *__iter__(void) {
                return new GenericIterator<PreludeDB::DB::ResultIdents, _VECTOR_UINT64_TYPE>(*self, 0, 1, self->count());
        };
}

%template(TableIterator) GenericIterator<PreludeDB::SQL::Table, PreludeDB::SQL::Table::Row>;
%template(TableRowIterator) GenericIterator<PreludeDB::SQL::Table::Row, const char>;
%template(ResultIdentsIterator) GenericIterator<PreludeDB::DB::ResultIdents, _VECTOR_UINT64_TYPE>;
%template(ResultValuesIterator) GenericIterator<PreludeDB::DB::ResultValues, PreludeDB::DB::ResultValues::ResultValuesRow>;
%template(ResultValuesDRowIterator) GenericDirectIterator<PreludeDB::DB::ResultValues::ResultValuesRow, PyObject>;
%template(ResultValuesRowIterator) GenericIterator<PreludeDB::DB::ResultValues::ResultValuesRow, Prelude::IDMEFValue>;
