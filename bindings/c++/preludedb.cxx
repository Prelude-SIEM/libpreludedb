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


uint64_t DB::ResultIdents::get(unsigned int row_index)
{
        int ret;
        uint64_t ident;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        ret = preludedb_result_idents_get(_result, row_index, &ident);
        if ( ret <= 0 )
                throw PreludeDBError(ret ? ret : preludedb_error(PRELUDEDB_ERROR_INDEX));

        return ident;
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
        preludedb_result_values_destroy(_result);
}


DB::ResultValues::ResultValuesRow::ResultValuesRow(const ResultValuesRow &row)
{
        _row = row._row;
        _result = (row._result) ? preludedb_result_values_ref(row._result) : NULL;
}


DB::ResultValues::ResultValuesRow &DB::ResultValues::ResultValuesRow::operator = (const DB::ResultValues::ResultValuesRow &row)
{
        if ( this != &row && _row != row._row ) {
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



Prelude::IDMEFValue DB::ResultValues::ResultValuesRow::get(int col)
{
        int ret, i;
        idmef_value_t *value;
        preludedb_selected_path_t *selected;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        if ( col < 0 )
                col = getFieldCount() - (-col);

        ret = preludedb_path_selection_get_selected(preludedb_result_values_get_selection(_result), &selected, col);
        if ( ret <= 0 )
                throw PreludeDBError(ret);

        ret = preludedb_result_values_get_field(_result, _row, selected, &value);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return Prelude::IDMEFValue(value);
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


DB::ResultValues::ResultValuesRow DB::ResultValues::get(unsigned int rownum)
{
        int ret;
        void *row;

        if ( ! _result )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        ret = preludedb_result_values_get_row(_result, rownum, &row);
        if ( ret <= 0 )
                throw PreludeDBError(ret ? ret : preludedb_error(PRELUDEDB_ERROR_INDEX));

        return DB::ResultValues::ResultValuesRow(_result, row);
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



DB::ResultValues DB::getValues(PathSelection &selection, const Prelude::IDMEFCriteria *criteria, bool distinct, int limit, int offset)
{
        int ret;
        preludedb_result_values_t *res;
        idmef_criteria_t *crit = NULL;

        if ( criteria )
                crit = *criteria;

        ret = preludedb_get_values(_db, selection, crit, (prelude_bool_t) distinct, limit, offset, &res);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ResultValues((ret == 0) ? NULL : res);
}



DB::ResultIdents DB::getAlertIdents(Prelude::IDMEFCriteria *criteria, int limit, int offset, ResultIdentsOrderByEnum order)
{
        int ret;
        idmef_criteria_t *ccriteria = NULL;
        preludedb_result_idents_t *result;

        if ( criteria )
                ccriteria = *criteria;

        ret = preludedb_get_alert_idents(_db, ccriteria, limit, offset, (preludedb_result_idents_order_t) order, &result);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ResultIdents(this, (ret == 0) ? NULL : result);
}


DB::ResultIdents DB::getHeartbeatIdents(Prelude::IDMEFCriteria *criteria, int limit, int offset, ResultIdentsOrderByEnum order)
{
        int ret;
        idmef_criteria_t *ccriteria = NULL;
        preludedb_result_idents_t *result;

        if ( criteria )
                ccriteria = *criteria;

        ret = preludedb_get_heartbeat_idents(_db, ccriteria, limit, offset, (preludedb_result_idents_order_t) order, &result);
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
                Prelude::IDMEFCriteria *criteria, const PreludeDB::PathSelection *order, int limit, int offset)
{
        int ret;
        preludedb_path_selection_t *corder = NULL;
        idmef_criteria_t *ccriteria = NULL;
        size_t i;
        const idmef_path_t *cpath[paths.size()];
        const idmef_value_t *cvals[values.size()];

        if ( criteria )
                ccriteria = *criteria;

        if ( order )
                corder = *order;

        if ( paths.size() != values.size() )
                throw PreludeDBError("Paths size does not match value size");

        for ( i = 0; i < paths.size(); i++ ) {
                cpath[i] = paths[i];
                cvals[i] = values[i];
        }

        ret = preludedb_update(_db, cpath, cvals, paths.size(), ccriteria, corder, limit, offset);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}
