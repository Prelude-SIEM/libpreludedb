#ifndef _LIBPRELUDEDB_FORMAT_H
#define _LIBPRELUDEDB_FORMAT_H

typedef enum {
	sql = 1
} db_type_t;

typedef struct {
	db_type_t type;
	union {
		prelude_sql_connection_t *connection;
	} connection;
} db_connection_t;

#endif /* _LIBPRELUDEDB_FORMAT_H */