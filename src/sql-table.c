#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/list.h>

#include "sql-table.h"

#define allocate(var, type, retval) do {        \
	var = (type *) calloc(1, sizeof(type)); \
	if (!var) {                             \
		printf("out of memory\n");      \
		return retval;                  \
	}                                       \
} while(0)


sql_field_t *sql_field_create(const char *name, const sql_field_type_t type, const char *val)
{
	sql_field_t *f;
	
	allocate(f, sql_field_t, NULL);
	strncpy(f->name, name, MAX_NAME_LEN);
	f->type = type;
	switch(type) {
		case type_int32:  sscanf(val, "%ld", &f->value.int32_val);
				  break;
		case type_uint32: sscanf(val, "%lu", &f->value.uint32_val);
				  break;
		case type_int64:  sscanf(val, "%lld", &f->value.int64_val);
				  break;
		case type_uint64: sscanf(val, "%llu", &f->value.uint64_val);
				  break;
		case type_float:  sscanf(val, "%f", &f->value.float_val);
				  break;				  
		case type_double: sscanf(val, "%lf", &f->value.double_val);
				  break;
		case type_string: f->value.string_val = strdup(val);
				  break;
		case type_void:   f->value.void_val = (void *) val;
				  break;
		default:	  f->type = type_unknown;
				  return NULL;	
	}
	return f;
}

int sql_field_to_string(sql_field_t *f, char *buf, int len)
{
	if ((!f) || (!buf))
		return -1;
		
	switch(f->type)
	{
		case type_int32:  snprintf(buf, len, "%ld", f->value.int32_val);
				  break;
		case type_uint32: snprintf(buf, len, "%lu", f->value.uint32_val);
				  break;
		case type_int64:  snprintf(buf, len, "%lld", f->value.int64_val);
				  break;
		case type_uint64: snprintf(buf, len, "%llu", f->value.uint64_val);
				  break;
		case type_double: snprintf(buf, len, "%lf", f->value.double_val);
				  break;
		case type_string: snprintf(buf, len, "%s", f->value.string_val);
				  break;
		default: return -1;
	}
	
	return 0;
}

int sql_field_destroy(sql_field_t *f)
{
	if (!f)
		return -1;
	
	if ((f->type == type_string) && f->value.string_val)
		free(f->value.string_val);
	
	free(f);
	return 0;
}

sql_row_t *sql_row_create(int cols)
{
	sql_row_t *row;
	
	if (!cols)
		return NULL;
	
	allocate(row, sql_row_t, NULL);
	
	/* don't use allocate here to clean up in case of failure */
	row->field = (sql_field_t **) calloc(cols, sizeof(sql_field_t *));
	if (!row->field) {
		printf("out of memory!\n");
		free(row);
		return NULL;
	}
	row->fields = cols;
	
	return row;
}

int sql_row_add_field(sql_row_t *row, int column, sql_field_t *field)
{
	if ((!row) || (column < 0) || (column > row->fields) || (row->field[column]))
		return -1;
	
	row->field[column] = field;
	
	return 0;
}

int sql_row_delete_field(sql_row_t *row, int column)
{
	if (!row || (column > row->fields) || (column < 0))
		return -1;
		
	row->field[column] = NULL;
	
	return 0;
}

int sql_row_destroy_field(sql_row_t *row, int column)
{
	sql_field_t *f;
	
	if (!row || (column > row->fields) || (column < 0))
		return -1;
	
	f = row->field[column];
		
	row->field[column] = NULL;
	
	return sql_field_destroy(f);
}

int sql_row_find_field(sql_row_t *row, char *name)
{
	int i = 0;

	while (i<row->fields) {
		if ((row->field[i]) && 
		    (strncmp(row->field[i]->name, name, MAX_NAME_LEN) == 0))
			return i;
		i++;
	}
	
	return -1;
}

sql_field_t *sql_row_get_field(sql_row_t *row, int field)
{
	if (!row || !field)
		return NULL;
	
	return (field < row->fields) ? row->field[field] : NULL ;
}

int sql_row_destroy(sql_row_t *row)
{
	int i;
	
	for (i=0;i<row->fields;i++)
		sql_row_destroy_field(row, i);
	
	free(row->field);	
	free(row);
	return 0;
}

sql_table_t *sql_table_create(const char *name)
{
	sql_table_t *t;

	allocate(t, sql_table_t, NULL);
	strncpy(t->name, name, MAX_NAME_LEN);
	INIT_LIST_HEAD(&t->row_list);
	
	return t;
}

int sql_table_add_row(sql_table_t *table, sql_row_t *row)
{
	if (!table || !row)
		return -1;
		
	list_add_tail(&row->list, &table->row_list);
	return 0;
}

void sql_table_delete_row(sql_row_t *row)
{
	list_del(&row->list);
}

void sql_table_iterate(sql_table_t *table, void (*callback) (sql_row_t *))
{
	struct list_head *tmp;
	sql_row_t *row;
	
	list_for_each(tmp, &table->row_list) {
		row = list_entry(tmp, sql_row_t, list);
		callback(row);
	}
}

int sql_table_destroy(sql_table_t *table)
{
	struct list_head *tmp,*n;
	sql_row_t *row;
	
	list_for_each_safe(tmp, n, &table->row_list) {
		row = list_entry(tmp, sql_row_t, list);
		sql_table_delete_row(row);
		sql_row_destroy(row);
	}
	free(table);
	
	return 0;
}


