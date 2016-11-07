#ifndef _LIBPRELUDE_PRELUDEDB_HXX
#define _LIBPRELUDE_PRELUDEDB_HXX

#include "preludedb.h"
#include "preludedb-sql.hxx"
#include "preludedb-error.hxx"


namespace PreludeDB {
        const char *checkVersion(const char *wanted = NULL);

        class DB {
            private:
                preludedb_t *_db;

            public:
                enum ResultIdentsOrderByEnum {
                        NONE     = PRELUDEDB_RESULT_IDENTS_ORDER_BY_NONE,
                        ORDER_BY_CREATE_TIME_ASC   = PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_ASC,
                        ORDER_BY_CREATE_TIME_DESC  = PRELUDEDB_RESULT_IDENTS_ORDER_BY_CREATE_TIME_DESC,
                };

                class ResultIdents {
                   public:
                        preludedb_result_idents_t *_result;

                        ResultIdents(const ResultIdents &result);
                        ResultIdents(DB *db, preludedb_result_idents_t *result);
                        ResultIdents(void);
                        ~ResultIdents(void);
                        unsigned int getCount(void);
                        unsigned int count() { return getCount(); };
                        uint64_t *get(unsigned int row_index=(unsigned int) -1);

                        ResultIdents &operator = (const ResultIdents &result);
                };


                class ResultValues {

                    public:
                        class ResultValuesRow {
                                private:
                                        void *_row;
                                        preludedb_result_values_t *_result;

                                public:
                                        ResultValuesRow() { _row = NULL; _result = NULL; };
                                        ResultValuesRow(const ResultValuesRow &row);
                                        ResultValuesRow(preludedb_result_values_t *result, void *row);
                                        ~ResultValuesRow(void);
                                        void *get(int col, preludedb_result_values_get_field_cb_func_t cb);
                                        Prelude::IDMEFValue *get(int col);
                                        unsigned int getFieldCount(void);
                                        unsigned int count() { return getFieldCount(); };
                                        std::string toString(void);
                                        ResultValuesRow &operator = (const ResultValuesRow &row);
                        };

                        preludedb_result_values_t *_result;


                        ResultValues(const ResultValues &result);
                        ResultValues(preludedb_result_values_t *result);
                        ResultValues(void);
                        ~ResultValues(void);

                        std::string toString(void);
                        unsigned int getCount(void);
                        unsigned int getFieldCount(void);
                        unsigned int count(void) { return getCount(); };
                        ResultValuesRow *get(unsigned int row=(unsigned int)-1);
                        ResultValuesRow *getRow(unsigned int row) { return get(row); };
                        ResultValues &operator = (const ResultValues &result);
                };

                ~DB();
                DB &operator = (const DB &db);
                DB(PreludeDB::SQL &sql);

                ResultIdents getAlertIdents(Prelude::IDMEFCriteria *criteria=NULL, int limit=-1, int offset=-1, ResultIdentsOrderByEnum order=ORDER_BY_CREATE_TIME_DESC);
                ResultIdents getHeartbeatIdents(Prelude::IDMEFCriteria *criteria=NULL, int limit=-1, int offset=-1, ResultIdentsOrderByEnum order=ORDER_BY_CREATE_TIME_DESC);
                ResultValues getValues(const std::vector<std::string> &selection, const Prelude::IDMEFCriteria *criteria=NULL, bool distinct=0, int limit=-1, int offset=-1);

                std::string getFormatName(void);
                std::string getFormatVersion(void);

                void insert(Prelude::IDMEF &idmef);

                Prelude::IDMEF getAlert(uint64_t ident);
                Prelude::IDMEF getHeartbeat(uint64_t ident);

/*
 * FIXME: on 64 bits machine, uint64_t is defined as long unsigned int.
 *
 * However, for SWIG to generate code that work for both 32/64 bits architecture,
 * we have to manually map uint64_t to unsigned long long in the SWIG generated code.
 *
 * The result is a compilation problem because G++ doesn't know how to convert
 * a vector of unsigned long long, to a vector of unsigned long.
 */
#define _VECTOR_UINT64_TYPE unsigned long long int
                /*
                 * delete is a reserved keyword
                 */
                void remove(Prelude::IDMEFCriteria *criteria);

                void deleteAlert(uint64_t ident);
                void deleteAlert(ResultIdents &idents);
                void deleteAlert(std::vector<_VECTOR_UINT64_TYPE> idents);

                void deleteHeartbeat(uint64_t ident);
                void deleteHeartbeat(ResultIdents &idents);
                void deleteHeartbeat(std::vector<_VECTOR_UINT64_TYPE> idents);

                void updateFromList(const std::vector<Prelude::IDMEFPath> &paths, const std::vector<Prelude::IDMEFValue> &values, DB::ResultIdents &idents);

                void updateFromList(const std::vector<Prelude::IDMEFPath> &paths, const std::vector<Prelude::IDMEFValue> &values,
                                    const std::vector<_VECTOR_UINT64_TYPE> idents);

                void update(const std::vector<Prelude::IDMEFPath> &paths, const std::vector<Prelude::IDMEFValue> &values,
                            Prelude::IDMEFCriteria *criteria=NULL, const std::vector<std::string> &order=std::vector<std::string>(),
                            int limit=-1, int offset=-1);

                void optimize(void);
        };
};

#endif
