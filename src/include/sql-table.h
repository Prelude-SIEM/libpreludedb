#ifndef _LIBPRELUDEDB_SQL_TABLE_H
#define _LIBPRELUDEDB_SQL_TABLE_H

typedef struct prelude_sql_row prelude_sql_row_t;
typedef struct prelude_sql_field prelude_sql_field_t;
typedef struct prelude_sql_table prelude_sql_table_t;


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
} prelude_sql_field_type_t;




prelude_sql_field_t *prelude_sql_field_new(const char *name, const prelude_sql_field_type_t type, const char *val);
int prelude_sql_field_to_string(prelude_sql_field_t *f, char *buf, int len);
int prelude_sql_field_destroy(prelude_sql_field_t *f);

prelude_sql_row_t *prelude_sql_row_new(int cols);
int prelude_sql_row_add_field(prelude_sql_row_t *row, int column, prelude_sql_field_t *field);
int prelude_sql_row_delete_field(prelude_sql_row_t *row, int column);
int prelude_sql_row_destroy_field(prelude_sql_row_t *row, int column);
int prelude_sql_row_find_field(prelude_sql_row_t *row, const char *name);
prelude_sql_field_t *prelude_sql_row_get_field(prelude_sql_row_t *row, int field);
int prelude_sql_row_destroy(prelude_sql_row_t *row);

prelude_sql_table_t *prelude_sql_table_new(const char *name);
int prelude_sql_table_add_row(prelude_sql_table_t *table, prelude_sql_row_t *row);
void prelude_sql_table_delete_row(prelude_sql_row_t *row);
void prelude_sql_table_iterate(prelude_sql_table_t *table, void (*callback) (prelude_sql_row_t *row));
int prelude_sql_table_destroy(prelude_sql_table_t *table);

#endif /* _LIBPRELUDEDB_SQL_TABLE_H */






