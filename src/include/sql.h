#ifndef _LIBPRELUDEDB_SQL_H
#define _LIBPRELUDEDB_SQL_H


typedef struct sql_connection sql_connection_t;

int sql_insert(sql_connection_t *conn, const char *table, const char *fields, const char *fmt, ...);

sql_table_t *sql_query(sql_connection_t *conn, const char *fmt, ...);

char *sql_escape(sql_connection_t *conn, const char *string);

int sql_begin(sql_connection_t *conn);

int sql_commit(sql_connection_t *conn);

int sql_rollback(sql_connection_t *conn);

void sql_close(sql_connection_t *conn);

sql_connection_t *sql_connect(prelude_sql_connection_data_t *data);




#endif /* _LIBPRELUDEDB_SQL_H */

