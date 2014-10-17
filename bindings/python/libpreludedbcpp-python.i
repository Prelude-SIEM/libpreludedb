%feature("flatnested");

%rename (__int__) *::operator int() const;
%rename (__long__) *::operator long() const;
%rename (__float__) *::operator double() const;
%rename (__len__) *::count();

%begin %{
#define TARGET_LANGUAGE_OUTPUT_TYPE PyObject **
%}

%exception %{
        try {
                $action
        } catch (PreludeDBError &e) {
                if ( e.getCode() == PRELUDEDB_ERROR_INDEX )
                        SWIG_exception(SWIG_IndexError, e.what());
                else {
                        SWIG_Python_Raise(SWIG_NewPointerObj(new PreludeDBError(e),
                                                             SWIGTYPE_p_PreludeDB__PreludeDBError, SWIG_POINTER_OWN),
                                          "PreludeDBError", SWIGTYPE_p_PreludeDB__PreludeDBError);
                }

                SWIG_fail;
        }
%}

%insert("python") %{
import itertools
import sys

def python2_unicode_patch(cl):
    if cl.__str__ is object.__str__:
        return cl

    if sys.version_info < (3, 0):
         cl.__unicode__ = lambda self: self.__str__().decode('utf-8')

    cl.__repr__ = lambda self: self.__class__.__name__ + "(" + repr(str(self)) + ")"
    return cl
%}

%extend PreludeDB::SQL::Table {
    %insert("python") %{
        def __getitem__(self, key):
                if isinstance(key, slice):
                        return itertools.islice(self, key.start, key.stop, key.step)

                return self.get(key)

        def __repr__(self):
                return "Table(\n  " + ",\n  ".join([repr(i) for i in self]) + "\n)"
    %}
}



%extend PreludeDB::SQL::Table::Row {
    %insert("python") %{
        def __getitem__(self, key):
                if isinstance(key, slice):
                        return itertools.islice(self, key.start, key.stop, key.step)

                return self.get(key)

        def __repr__(self):
                return "Row(" + repr([i for i in self]) + ")"
    %}
}


%extend PreludeDB::DB::ResultValues::ResultValuesRow {
    %insert("python") %{
        def __getitem__(self, key):
                if isinstance(key, slice):
                        return itertools.islice(self, key.start, key.stop, key.step)

                return self.get(key)

        def __repr__(self):
                return "ResultValuesRow(" + repr([i for i in self]) + ")"
    %}
}


%extend PreludeDB::DB::ResultValues {
    %insert("python") %{
        def __getitem__(self, key):
                if isinstance(key, slice):
                        return itertools.islice(self, key.start, key.stop, key.step)

                return self.get(key)

        def __repr__(self):
                return "ResultValues(\n  " + ",\n  ".join([repr(i) for i in self]) + "\n)"
    %}
}


%extend PreludeDB::DB::ResultIdents {
    %insert("python") %{
        def __getitem__(self, key):
                if isinstance(key, slice):
                        return itertools.islice(self, key.start, key.stop, key.step)

                return self.get(key)

        def __repr__(self):
                return "ResultIdents(\n  " + ",\n  ".join([repr(i) for i in self]) + "\n)"
    %}
}


%rename(_update) PreludeDB::DB::update;
%rename(_updateFromList) PreludeDB::DB::updateFromList;


%extend PreludeDB::DB {
    %insert("python") {
        def __convertPV(self, paths, values):
                if not isinstance(paths[0], prelude.IDMEFPath):
                        paths = [ prelude.IDMEFPath(path) for path in paths ]

                if not isinstance(values[0], prelude.IDMEFValue):
                        values = [ prelude.IDMEFValue(value) for value in values ]

                return (paths, values)

        def updateFromList(self, paths, values, identlist):
                paths, values = self.__convertPV(paths, values)
                return self._updateFromList(paths, values, identlist)

        def update(self, paths, values, criteria=None, order=[], limit=-1, offset=-1):
                paths, values = self.__convertPV(paths, values)
                return self._update(paths, values, criteria, order, limit, offset)
    }
}

