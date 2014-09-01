#include "preludedb.hxx"
#include "preludedb-error.hxx"
#include "preludedb-path-selection.hxx"


using namespace PreludeDB;


PathSelection::PathSelection()
{
        _ps = NULL;
}


PathSelection::PathSelection(const std::vector<std::string> &selection)
{
        int ret;
        preludedb_selected_path_t *selected;

        ret = preludedb_path_selection_new(&_ps);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        for ( std::vector<std::string>::const_iterator iter = selection.begin(); iter != selection.end(); iter++) {
                ret = preludedb_selected_path_new_string(&selected, (*iter).c_str());
                if ( ret < 0 ) {
                        preludedb_path_selection_destroy(_ps);
                        throw PreludeDBError(ret);
                }

                preludedb_path_selection_add(_ps, selected);
        }
}



PathSelection::~PathSelection()
{
        if ( _ps )
                preludedb_path_selection_destroy(_ps);
}



PathSelection::PathSelection(const PathSelection &selection)
{
        _ps = (selection._ps) ? preludedb_path_selection_ref(selection._ps) : NULL;
}



PathSelection::operator preludedb_path_selection_t *() const
{
        return _ps;
}



PathSelection &PathSelection::operator=(const PathSelection &selection)
{
        if ( this != &selection && _ps != selection._ps ) {
                if ( _ps )
                        preludedb_path_selection_destroy(_ps);

                _ps = (selection._ps) ? preludedb_path_selection_ref(selection._ps) : NULL;
        }

        return *this;
}
