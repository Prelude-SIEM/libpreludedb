#include <stdlib.h>

#include "preludedb.hxx"
#include "preludedb-error.hxx"
#include "preludedb-version.h"

using namespace PreludeDB;



const char *PreludeDB::checkVersion(const char *wanted)
{
        const char *ret;

        ret = preludedb_check_version(wanted);
        if ( wanted && ! ret ) {
                std::string s = "libpreludedb ";
                s += wanted;
                s += " or higher is required (";
                s += preludedb_check_version(NULL);
                s += " found).";
                throw PreludeDBError(s);
        }

        return ret;
}


static preludedb_path_selection_t *_getSelection(preludedb_t *db, const std::vector<std::string> &selection)
{
        int ret;
        preludedb_selected_path_t *selected;
        preludedb_path_selection_t *cselection;

        if ( selection.size() == 0 )
                return NULL;

        ret = preludedb_path_selection_new(db, &cselection);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        for ( std::vector<std::string>::const_iterator iter = selection.begin(); iter != selection.end(); iter++) {
                ret = preludedb_selected_path_new_string(&selected, (*iter).c_str());
                if ( ret < 0 ) {
                        preludedb_path_selection_destroy(cselection);
                        throw PreludeDBError(ret);
                }

                ret = preludedb_path_selection_add(cselection, selected);
                if ( ret < 0 ) {
                        preludedb_path_selection_destroy(cselection);
                        throw PreludeDBError(ret);
                }
        }

        return cselection;
}


/**/

DB::ResultIdents::ResultIdents()
{
        _result = NULL;
}


DB::ResultIdents::ResultIdents(DB *db, preludedb_result_idents_t *result)
{
        _result = result;
}


DB::ResultIdents::~ResultIdents()
{
        if ( _result )
                preludedb_result_idents_destroy(_result);
}


DB::ResultIdents::ResultIdents(const DB::ResultIdents &result)
{
        _result = (result._result) ? preludedb_result_idents_ref(result._result) : NULL;
}


unsigned int DB::ResultIdents::getCount()
{
        return (_result) ? preludedb_result_idents_get_count(_result) : 0;
}


uint64_t *DB::ResultIdents::get(unsigned int row_index)
{
        int ret;
        uint64_t ident;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        ret = preludedb_result_idents_get(_result, row_index, &ident);
        if ( ret <= 0 )
                throw PreludeDBError(ret ? ret : preludedb_error(PRELUDEDB_ERROR_INDEX));

        return new uint64_t(ident);
}



DB::ResultIdents &DB::ResultIdents::operator = (const DB::ResultIdents &result)
{
        if ( this != &result && _result != result._result ) {
                if ( _result )
                        preludedb_result_idents_destroy(_result);

                _result = (result._result) ? preludedb_result_idents_ref(result._result) : NULL;
        }

        return *this;
}


/* */
DB::ResultValues::ResultValuesRow::ResultValuesRow(preludedb_result_values_t *rv, void *row)
{
        _row = row;
        _result = preludedb_result_values_ref(rv);
}


DB::ResultValues::ResultValuesRow::~ResultValuesRow()
{
        if ( _result )
                preludedb_result_values_destroy(_result);
}


DB::ResultValues::ResultValuesRow::ResultValuesRow(const ResultValuesRow &row)
{
        _row = row._row;
        _result = (row._result) ? preludedb_result_values_ref(row._result) : NULL;
}


DB::ResultValues::ResultValuesRow &DB::ResultValues::ResultValuesRow::operator = (const DB::ResultValues::ResultValuesRow &row)
{
        if ( this != &row && _row != row._row && _result != row._result ) {
                if ( _result )
                        preludedb_result_values_destroy(_result);

                _row = row._row;
                _result = (row._result) ? preludedb_result_values_ref(row._result) : NULL;
        }

        return *this;
}


unsigned int DB::ResultValues::ResultValuesRow::getFieldCount(void)
{
        return preludedb_result_values_get_field_count(_result);
}


std::string DB::ResultValues::ResultValuesRow::toString(void)
{
        size_t i;
        std::string s;

        s = "ResultValuesRow(";

        for ( i = 0; i < count(); i++ ) {
                if ( i > 0 )
                        s += ", ";

                Prelude::IDMEFValue *v = get(i);
                if ( v && ! v->isNull() ) {
                        if ( v->getType() == Prelude::IDMEFValue::TYPE_STRING )
                                s += "'";

                        s += v->toString();

                        if ( v->getType() == Prelude::IDMEFValue::TYPE_STRING )
                                s += "'";
                } else
                        s += "NULL";

                delete(v);
        }

        s += ")";

        return s;
}



Prelude::IDMEFValue *DB::ResultValues::ResultValuesRow::get(int col)
{
        int ret, i;
        idmef_value_t *out = NULL;
        preludedb_selected_path_t *selected;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        if ( col < 0 )
                col = getFieldCount() - (-col);

        ret = preludedb_path_selection_get_selected(preludedb_result_values_get_selection(_result), &selected, col);
        if ( ret <= 0 )
                throw PreludeDBError(ret);

        ret = preludedb_result_values_get_field(_result, _row, selected, (idmef_value_t **) &out);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return new Prelude::IDMEFValue(out);
}



void *DB::ResultValues::ResultValuesRow::get(int col, preludedb_result_values_get_field_cb_func_t cb)
{
        int ret, i;
        void *out = NULL;
        preludedb_selected_path_t *selected;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        if ( col < 0 )
                col = getFieldCount() - (-col);

        ret = preludedb_path_selection_get_selected(preludedb_result_values_get_selection(_result), &selected, col);
        if ( ret <= 0 )
                throw PreludeDBError(ret);

        ret = preludedb_result_values_get_field_direct(_result, _row, selected, cb, &out);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return out;
}



/* */

DB::ResultValues::ResultValues()
{
        _result = NULL;
}


DB::ResultValues::ResultValues(preludedb_result_values_t *result)
{
        _result = result;
}


DB::ResultValues::~ResultValues()
{
        if ( _result )
                preludedb_result_values_destroy(_result);
}


DB::ResultValues::ResultValues(const DB::ResultValues &result)
{
        _result = (result._result) ? preludedb_result_values_ref(result._result) : NULL;
}


std::string DB::ResultValues::toString(void)
{
        std::string s;
        unsigned int i;
        ResultValuesRow *row;

        s = "ResultValues(\n";

        for ( i = 0; i < count(); i++ ) {
                if ( i > 0 )
                        s += ",\n";

                s += " ";

                row = get(i);
                s += row->toString();
                delete(row);
        }

        s += "\n)";

        return s;
}


DB::ResultValues::ResultValuesRow *DB::ResultValues::get(unsigned int rownum)
{
        int ret;
        void *row;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        ret = preludedb_result_values_get_row(_result, rownum, &row);
        if ( ret <= 0 )
                throw PreludeDBError(ret ? ret : preludedb_error(PRELUDEDB_ERROR_INDEX));

        return new DB::ResultValues::ResultValuesRow(_result, row);
}


unsigned int DB::ResultValues::getCount()
{
        return (_result) ? preludedb_result_values_get_count(_result) : 0;
}


unsigned int DB::ResultValues::getFieldCount(void)
{
        return preludedb_result_values_get_field_count(_result);
}


DB::ResultValues &DB::ResultValues::operator = (const DB::ResultValues &result)
{
        if ( this != &result && _result != result._result ) {
                if ( _result )
                        preludedb_result_values_destroy(_result);

                _result = (result._result) ? preludedb_result_values_ref(result._result) : NULL;
        }

        return *this;
}

/**/


DB::DB(PreludeDB::SQL &sql)
{
        int ret;

        ret = preludedb_new(&_db, sql, NULL, NULL, 0);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


DB::~DB()
{
        preludedb_destroy(_db);
}


DB &DB::operator = (const DB &db)
{
        if ( this != &db && _db != db._db ) {
                if ( _db )
                        preludedb_destroy(_db);

                _db = (db._db) ? preludedb_ref(db._db) : NULL;
        }

        return *this;
}


DB::ResultValues DB::getValues(const std::vector<std::string> &selection, const Prelude::IDMEFCriteria *criteria, bool distinct, int limit, int offset)
{
        int ret;
        preludedb_selected_path_t *selected;
        preludedb_path_selection_t *c_selection;
        preludedb_result_values_t *res;
        idmef_criteria_t *crit = NULL;

        if ( criteria )
                crit = *criteria;

        c_selection = _getSelection(_db, selection);

        ret = preludedb_get_values(_db, c_selection, crit, (prelude_bool_t) distinct, limit, offset, &res);

        if ( c_selection )
                preludedb_path_selection_destroy(c_selection);

        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ResultValues((ret == 0) ? NULL : res);
}



DB::ResultIdents DB::getAlertIdents(Prelude::IDMEFCriteria *criteria, int limit, int offset, const std::vector<std::string> &order)
{
        int ret;
        preludedb_path_selection_t *corder = NULL;
        idmef_criteria_t *ccriteria = NULL;
        preludedb_result_idents_t *result;

        if ( criteria )
                ccriteria = *criteria;

        corder = _getSelection(_db, order);

        ret = preludedb_get_alert_idents2(_db, ccriteria, limit, offset, corder, &result);

        if ( corder )
                preludedb_path_selection_destroy(corder);

        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ResultIdents(this, (ret == 0) ? NULL : result);
}


DB::ResultIdents DB::getHeartbeatIdents(Prelude::IDMEFCriteria *criteria, int limit, int offset, const std::vector<std::string> &order)
{
        int ret;
        preludedb_path_selection_t *corder = NULL;
        idmef_criteria_t *ccriteria = NULL;
        preludedb_result_idents_t *result;

        if ( criteria )
                ccriteria = *criteria;

        corder = _getSelection(_db, order);

        ret = preludedb_get_heartbeat_idents2(_db, ccriteria, limit, offset, corder, &result);

        if ( corder )
                preludedb_path_selection_destroy(corder);

        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ResultIdents(this, (ret == 0) ? NULL : result);
}


/*
 * Main DB module functions
 */
std::string DB::getFormatName(void)
{
        return preludedb_get_format_name(_db);
}


std::string DB::getFormatVersion(void)
{
        return preludedb_get_format_version(_db);
}


void DB::insert(Prelude::IDMEF &idmef)
{
        int ret;

        ret = preludedb_insert_message(_db, (idmef_message_t *) (idmef_object_t *) idmef);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


Prelude::IDMEF DB::getAlert(uint64_t ident)
{
        int ret;
        idmef_message_t *idmef;

        ret = preludedb_get_alert(_db, ident, &idmef);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return Prelude::IDMEF((idmef_object_t *) idmef);
}


Prelude::IDMEF DB::getHeartbeat(uint64_t ident)
{
        int ret;
        idmef_message_t *idmef;

        ret = preludedb_get_heartbeat(_db, ident, &idmef);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return Prelude::IDMEF((idmef_object_t *) idmef);
}


void DB::remove(Prelude::IDMEFCriteria *criteria)
{
        int ret;
        idmef_criteria_t *ccriteria = NULL;

        if ( criteria )
                ccriteria = *criteria;

        ret = preludedb_delete(_db, ccriteria);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void DB::deleteAlert(uint64_t ident)
{
        int ret;

        ret = preludedb_delete_alert(_db, ident);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}



void DB::deleteAlert(DB::ResultIdents &idents)
{
        int ret;

        if ( ! idents._result )
                return;

        ret = preludedb_delete_alert_from_result_idents(_db, idents._result);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void DB::deleteAlert(std::vector<_VECTOR_UINT64_TYPE> idents)
{
        int ret;

        ret = preludedb_delete_alert_from_list(_db, (uint64_t *) &idents[0], idents.size());
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void DB::deleteHeartbeat(uint64_t ident)
{
        int ret;

        ret = preludedb_delete_heartbeat(_db, ident);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void DB::deleteHeartbeat(DB::ResultIdents &idents)
{
        int ret;

        if ( ! idents._result )
                return;

        ret = preludedb_delete_heartbeat_from_result_idents(_db, idents._result);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void DB::deleteHeartbeat(std::vector<_VECTOR_UINT64_TYPE> idents)
{
        int ret;

        ret = preludedb_delete_heartbeat_from_list(_db, (uint64_t *) &idents[0], idents.size());
        if ( ret < 0 )
                throw PreludeDBError(ret);
}



void DB::updateFromList(const std::vector<Prelude::IDMEFPath> &paths, const std::vector<Prelude::IDMEFValue> &values, DB::ResultIdents &idents)
{
        int ret;
        size_t i;
        const idmef_path_t *cpath[paths.size()];
        const idmef_value_t *cvals[values.size()];

        if ( ! idents._result )
                return;

        if ( paths.size() != values.size() )
                throw PreludeDBError("Paths size does not match value size");

        for ( i = 0; i < paths.size(); i++ ) {
                cpath[i] = paths[i];
                cvals[i] = values[i];
        }

        ret = preludedb_update_from_result_idents(_db, cpath, cvals, paths.size(), idents._result);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}



void DB::updateFromList(const std::vector<Prelude::IDMEFPath> &paths, const std::vector<Prelude::IDMEFValue> &values, const std::vector<_VECTOR_UINT64_TYPE> idents)
{
        int ret;
        size_t i;
        const idmef_path_t *cpath[paths.size()];
        const idmef_value_t *cvals[values.size()];

        if ( paths.size() != values.size() )
                throw PreludeDBError("Paths size does not match value size");

        for ( i = 0; i < paths.size(); i++ ) {
                cpath[i] = paths[i];
                cvals[i] = values[i];
        }

        ret = preludedb_update_from_list(_db, cpath, cvals, paths.size(), (uint64_t *) &idents[0], idents.size());
        if ( ret < 0 )
                throw PreludeDBError(ret);
}



void DB::update(const std::vector<Prelude::IDMEFPath> &paths, const std::vector<Prelude::IDMEFValue> &values,
                Prelude::IDMEFCriteria *criteria, const std::vector<std::string> &order, int limit, int offset)
{
        int ret;
        preludedb_selected_path_t *selected;
        preludedb_path_selection_t *corder = NULL;
        idmef_criteria_t *ccriteria = NULL;
        size_t i;
        const idmef_path_t *cpath[paths.size()];
        const idmef_value_t *cvals[values.size()];

        if ( criteria )
                ccriteria = *criteria;

        if ( paths.size() != values.size() )
                throw PreludeDBError("Paths size does not match value size");

        for ( i = 0; i < paths.size(); i++ ) {
                cpath[i] = paths[i];
                cvals[i] = values[i];
        }

        corder = _getSelection(_db, order);

        ret = preludedb_update(_db, cpath, cvals, paths.size(), ccriteria, corder, limit, offset);

        if ( corder )
                preludedb_path_selection_destroy(corder);

        if ( ret < 0 )
                throw PreludeDBError(ret);
}



void DB::optimize(void)
{
        int ret;

        ret = preludedb_optimize(_db);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}
