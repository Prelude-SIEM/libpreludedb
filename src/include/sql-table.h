#ifndef _LIBPRELUDEDB_SQL_TABLE_H
#define _LIBPRELUDEDB_SQL_TABLE_H

typedef struct sql_row sql_row_t;
typedef struct sql_field sql_field_t;
typedef struct sql_table sql_table_t;


typedef enum {
	type_unknown = 0,
	type_int32 = 1,
	type_uint32 = 2,
	type_int64 = 3,
	type_uint64 = 4,
	type_float = 5,
	type_double = 6,
	type_string = 7,
	type_void = 8,
        type_end  = 9,
} sql_field_type_t;




sql_field_t *sql_field_new(const char *name, const sql_field_type_t type, const char *val);
int sql_field_to_string(sql_field_t *f, char *buf, int len);
int sql_field_destroy(sql_field_t *f);

sql_row_t *sql_row_new(int cols);
int sql_row_add_field(sql_row_t *row, int column, sql_field_t *field);
int sql_row_delete_field(sql_row_t *row, int column);
int sql_row_destroy_field(sql_row_t *row, int column);
int sql_row_find_field(sql_row_t *row, const char *name);
sql_field_t *sql_row_get_field(sql_row_t *row, int field);
int sql_row_destroy(sql_row_t *row);

sql_table_t *sql_table_new(const char *name);
int sql_table_add_row(sql_table_t *table, sql_row_t *row);
void sql_table_delete_row(sql_row_t *row);
void sql_table_iterate(sql_table_t *table, void (*callback) (sql_row_t *row));
int sql_table_destroy(sql_table_t *table);

#endif /* _LIBPRELUDEDB_SQL_TABLE_H */






