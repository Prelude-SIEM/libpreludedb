#include <stdlib.h>
#include <string.h>

#include <libprelude/prelude.hxx>
#include <libprelude/idmef-value.hxx>

#include "preludedb-sql.hxx"
#include "preludedb-error.hxx"


using namespace PreludeDB;


SQL::Table::Row::Row(preludedb_sql_row_t *row)
{
        _row = preludedb_sql_row_ref(row);
}


SQL::Table::Row::Row(void)
{
        _row = NULL;
}


SQL::Table::Row::Row(const Row &row)
{
        _row = preludedb_sql_row_ref(row._row);
}


SQL::Table::Row::~Row()
{
        if ( _row )
                preludedb_sql_row_destroy(_row);
}


unsigned int SQL::Table::Row::getFieldCount()
{
        return preludedb_sql_row_get_field_count(_row);
}


SQL::Table::Row &SQL::Table::Row::operator = (const SQL::Table::Row &row)
{
        if ( this != &row && _row != row._row ) {
                if ( _row )
                        preludedb_sql_row_destroy(_row);

                _row = (row._row) ? preludedb_sql_row_ref(row._row) : NULL;
        }

        return *this;
}


std::string SQL::Table::Row::toString(void)
{
        size_t i;
        std::string s;

        s = "Row(";

        for ( i = 0; i < count(); i++ ) {
                if ( i > 0 )
                        s += ", ";

                const char *v = get(i);
                if ( v ) {
                        s += "'";
                        s += v;
                        s += "'";
                } else
                        s += "NULL";
        }

        s += ")";

        return s;
}


const char *SQL::Table::Row::getField(unsigned int num)
{
        int ret;
        preludedb_sql_field_t *field;

        ret = preludedb_sql_row_get_field(_row, num, &field);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        if ( ret == 0 )
                return NULL;

        return preludedb_sql_field_get_value(field);
}



const char *SQL::Table::Row::getField(const std::string &name)
{
        int ret;
        preludedb_sql_field_t *field;

        ret = preludedb_sql_row_get_field_by_name(_row, name.c_str(), &field);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        if ( ret == 0 )
                return NULL;

        return preludedb_sql_field_get_value(field);
}



/**/

SQL::Table::Table(preludedb_sql_table_t *table, unsigned int afrows)
{
        _table = table;
        affected_rows = afrows;
}


SQL::Table::Table(const Table &table)
{
        _table = (table._table) ? preludedb_sql_table_ref(table._table) : NULL;
        affected_rows = table.affected_rows;
}


SQL::Table::~Table()
{
        if ( _table )
                preludedb_sql_table_destroy(_table);
}


std::string SQL::Table::toString(void)
{
        std::string s;
        size_t i;
        Row *row;

        s = "Table(\n";

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


const char *SQL::Table::getColumnName(unsigned int column_num)
{
        if ( ! _table )
                throw PreludeDBError("Table is not initialized");

        return preludedb_sql_table_get_column_name(_table, column_num);
}


int SQL::Table::getColumnNum(const std::string &column_name)
{
        int ret;

        if ( ! _table )
                throw PreludeDBError("Table is not initialized");

        ret = preludedb_sql_table_get_column_num(_table, column_name.c_str());
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ret;
}


unsigned int SQL::Table::getColumnCount()
{
        if ( ! _table )
                throw PreludeDBError("Table is not initialized");

        return preludedb_sql_table_get_column_count(_table);
}


unsigned int SQL::Table::getRowCount()
{
        if ( ! _table )
                return 0;

        return preludedb_sql_table_get_row_count(_table);
}



SQL::Table::Row *SQL::Table::get(unsigned int row_index)
{
        int ret;
        SQL::Table::Row r;
        preludedb_sql_row_t *row;

        if ( ! _table )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        ret = preludedb_sql_table_get_row(_table, row_index, &row);
        if ( ret <= 0 )
                throw PreludeDBError(ret ? ret : preludedb_error(PRELUDEDB_ERROR_INDEX));

        return new SQL::Table::Row(row);
}



SQL::Table::Row *SQL::Table::fetch(void)
{
        int ret;
        SQL::Table::Row r;
        preludedb_sql_row_t *row;

        if ( ! _table )
                throw PreludeDBError(preludedb_error(PRELUDEDB_ERROR_INDEX));

        ret = preludedb_sql_table_fetch_row(_table, &row);
        if ( ret <= 0 )
                throw PreludeDBError(ret ? ret : preludedb_error(PRELUDEDB_ERROR_INDEX));

        return new SQL::Table::Row(row);
}


SQL::Table &SQL::Table::operator = (const SQL::Table &table)
{
        if ( this != &table && _table != table._table ) {
                if ( _table )
                        preludedb_sql_table_destroy(_table);

                _table = (table._table) ? preludedb_sql_table_ref(table._table) : NULL;
        }

        affected_rows = table.affected_rows;
        return *this;
}



SQL::SQL(const std::map<std::string,std::string> &settings)
{
        int ret;
        preludedb_sql_settings_t *dbconfig;

        ret = preludedb_sql_settings_new(&dbconfig);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        for ( std::map<std::string,std::string>::const_iterator it = settings.begin(); it != settings.end(); it++ ) {
                ret = preludedb_sql_settings_set(dbconfig, it->first.c_str(), it->second.c_str());
                if ( ret < 0 )
                        goto error;
        }

        ret = preludedb_sql_new(&_sql, NULL, dbconfig);

error:
        if ( ret < 0 ) {
                preludedb_sql_settings_destroy(dbconfig);
                throw PreludeDBError(ret);
        }
}


SQL::SQL(void)
{
        _sql = NULL;
}


SQL::SQL(SQL &sql)
{
        _sql = preludedb_sql_ref(sql);
}


SQL::SQL(const char *settings)
{
        int ret;
        preludedb_sql_settings_t *dbconfig;

        ret = preludedb_sql_settings_new_from_string(&dbconfig, settings);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        ret = preludedb_sql_new(&_sql, NULL, dbconfig);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


SQL::SQL(preludedb_sql_t *sql)
{
        _sql = preludedb_sql_ref(sql);
}


SQL &SQL::operator = (const SQL &sql)
{
        if ( this != &sql && _sql != sql._sql ) {
                if ( _sql )
                        preludedb_sql_destroy(_sql);

                _sql = (sql._sql) ? preludedb_sql_ref(sql._sql) : NULL;
        }

        return *this;
}


SQL::~SQL()
{
        if (_sql)
                preludedb_sql_destroy(_sql);
}



long SQL::getServerVersion()
{
        long ret;

        ret = preludedb_sql_get_server_version(_sql);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return ret;
}


SQL::Table SQL::query(const std::string &query)
{
        int ret;
        preludedb_sql_table_t *table = NULL;

        ret = preludedb_sql_query(_sql, query.c_str(), &table);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        return SQL::Table(table, ret);
}


void SQL::transactionStart()
{
        int ret;

        ret = preludedb_sql_transaction_start(_sql);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void SQL::transactionEnd()
{
        int ret;

        ret = preludedb_sql_transaction_end(_sql);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void SQL::transactionAbort()
{
        int ret;

        ret = preludedb_sql_transaction_abort(_sql);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}



std::string SQL::escapeBinary(const unsigned char *bytes, size_t size)
{
        int ret;
        char *out;
        std::string retstr;

        ret = preludedb_sql_escape_binary(_sql, bytes, size, &out);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        retstr = std::string(out);
        free(out);

        return retstr;
}


std::string SQL::escapeBinary(const std::string &str)
{
        return escapeBinary((const unsigned char *) str.c_str(), str.size());
}


std::string SQL::unescapeBinary(const std::string &str)
{
        int ret;
        unsigned char *out;
        size_t out_size;
        std::string retstr;

        ret = preludedb_sql_unescape_binary(_sql, (const char *) str.c_str(), str.size(), &out, &out_size);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        retstr = std::string((char *) out, out_size);
        free(out);

        return retstr;
}


std::string SQL::escape(const std::string &str)
{
        int ret;
        char *out;
        std::string retstr;

        ret = preludedb_sql_escape_fast(_sql, str.c_str(), str.length(), &out);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        retstr = std::string(out);
        free(out);

        return retstr;
}


std::string SQL::escape(const char *str)
{
        int ret;
        char *out;
        std::string retstr;

        ret = preludedb_sql_escape(_sql, str, &out);
        if ( ret < 0 )
                throw PreludeDBError(ret);

        retstr = std::string(out);
        free(out);

        return retstr;
}


void SQL::connect(void)
{
        int ret;

        ret = preludedb_sql_connect(_sql);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


void SQL::close(void)
{
        int ret;

        ret = preludedb_sql_close(_sql);
        if ( ret < 0 )
                throw PreludeDBError(ret);
}


uint64_t SQL::getLastInsertIdent()
{
    int ret;
    uint64_t ident;

    ret = preludedb_sql_get_last_insert_ident(_sql, &ident);
    if ( ret < 0 )
            throw PreludeDBError(ret);

    return ident;
}


SQL::operator preludedb_sql_t *() const
{
        return _sql;
}
