#ifndef _LIBPRELUDEDB_SQL_H
#define _LIBPRELUDEDB_SQL_H


typedef struct prelude_sql_connection prelude_sql_connection_t;

int prelude_sql_insert(prelude_sql_connection_t *conn, const char *table, const char *fields, const char *fmt, ...);

sql_table_t *prelude_sql_query(prelude_sql_connection_t *conn, const char *fmt, ...);

char *prelude_sql_escape(prelude_sql_connection_t *conn, const char *string);

int prelude_sql_begin(prelude_sql_connection_t *conn);

int prelude_sql_commit(prelude_sql_connection_t *conn);

int prelude_sql_rollback(prelude_sql_connection_t *conn);

void prelude_sql_close(prelude_sql_connection_t *conn);

prelude_sql_connection_t *prelude_sql_connect(prelude_sql_connection_data_t *data);




#endif /* _LIBPRELUDEDB_SQL_H */

