#ifndef _LIBPRELUDEDB_DB_CONNECTION_H
#define _LIBPRELUDEDB_DB_CONNECTION_H

typedef enum {
	sql = 1
} db_type_t;

typedef struct {
	db_type_t type;
	union {
		sql_connection_t *sql;
	} connection;
} db_connection_t;

int db_interfaces_init(void);
int db_write_idmef_message(idmef_message_t *msg);

#endif /* _LIBPRELUDEDB_DB_CONNECTION_H */

