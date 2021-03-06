#ifndef _LIBPRELUDE_PRELUDEDB_SQL_HXX
#define _LIBPRELUDE_PRELUDEDB_SQL_HXX

#include <string>
#include <vector>
#include <map>

#include <libprelude/prelude.hxx>

#include "preludedb.h"

using namespace std;

typedef std::string bytestring;

namespace PreludeDB {
        class SQL {
            private:
                preludedb_sql_t *_sql = NULL;

            public:
                class Table {
                    private:
                        preludedb_sql_table_t *_table;

                    public:
                        unsigned int affected_rows;

                        class Row {
                            private:
                                preludedb_sql_row_t *_row;

                            public:
                                ~Row();
                                Row();
                                Row(preludedb_sql_row_t *row);
                                Row(const Row &row);

                                unsigned int getFieldCount();
                                unsigned int count() { return getFieldCount(); };

                                const char *getField(unsigned int num);
                                const char *getField(const std::string &name);

                                const char *get(unsigned int num) { return getField(num); };
                                const char *get(const std::string &name) { return getField(name); };

                                SQL::Table::Row &operator = (const SQL::Table::Row &row);

                                std::string toString(void);

                                const char *operator[] (unsigned int num) {
                                        return get(num);
                                };

                                const char *operator[] (const std::string &name) {
                                        return get(name);
                                };
                        };

                        ~Table();
                        Table(preludedb_sql_table_t *table, unsigned int affected_rows);
                        Table(const Table &table);
                        Table(void) { _table = NULL; affected_rows = 0; };

                        const char *getColumnName(unsigned int column_num);
                        int getColumnNum(const std::string &column_name);

                        unsigned int getColumnCount();
                        unsigned int getRowCount();
                        unsigned int count() { return getRowCount(); };

                        SQL::Table::Row *fetch(void);
                        SQL::Table::Row *get(unsigned int row_index);

                        std::string toString(void);

                        SQL::Table &operator = (const SQL::Table &table);

                };

                ~SQL();
                SQL &operator = (const SQL &sql);
                SQL(const char *settings);
                SQL(void);
                SQL(const std::map<std::string,std::string> &settings);
                SQL(preludedb_sql_t *sql);

                SQL(SQL &sql);

                long getServerVersion();

                SQL::Table query(const std::string &query);
                void transactionStart(void);
                void transactionEnd(void);
                void transactionAbort(void);
                void connect(void);
                void close(void);
                std::string escape(const char *str);
                std::string escape(const std::string &str);

#ifndef SWIG
                std::string escapeBinary(const bytestring &str);
#endif

                std::string escapeBinary(const unsigned char *bytes, size_t size);
                bytestring unescapeBinary(const std::string &str);

                uint64_t getLastInsertIdent(void);
                std::string getType(void) { return preludedb_sql_get_type(_sql); };

                operator preludedb_sql_t *() const;
        };
};

#endif
