/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* All Rights Reserved
*
* This file is part of the Prelude program.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

#include <libprelude/list.h>
#include <libprelude/prelude-log.h>

#include "sql-table.h"


#define MAX_NAME_LEN 255


#define allocate(var, retval) do {                   \
	var = calloc(1, sizeof(*var));               \
	if ( ! var ) {                               \
		log(LOG_ERR, "memory exhausted.\n"); \
		return retval;                       \
	}                                            \
} while(0)


struct prelude_sql_table {
	char name[MAX_NAME_LEN + 1];
	struct list_head row_list;
};


struct prelude_sql_row {
	struct list_head list;

	int fields;
	prelude_sql_field_t **field;
};



struct prelude_sql_field {
	struct list_head list;
	prelude_sql_field_type_t type;
	char name[MAX_NAME_LEN + 1];
        
	union {
		int32_t int32_val;
		uint32_t uint32_val;
		int64_t int64_val;
		uint64_t uint64_val;
		float float_val;
		double double_val;
		char *string_val;
		void *void_val;
	} value;
};




prelude_sql_field_t *prelude_sql_field_new(const char *name, prelude_sql_field_type_t type, const char *val)
{
	prelude_sql_field_t *f;
	
	allocate(f, NULL);
	strncpy(f->name, name, sizeof(f->name));
	f->type = type;
        
	switch(type) {

        case type_int32:
                sscanf(val, "%d", &f->value.int32_val);
                break;

        case type_uint32:
                sscanf(val, "%u", &f->value.uint32_val);
                break;

        case type_int64:
                sscanf(val, "%lld", &f->value.int64_val);
                break;
                
        case type_uint64:
                sscanf(val, "%llu", &f->value.uint64_val);
                break;

        case type_float:
                sscanf(val, "%f", &f->value.float_val);
                break;				  

        case type_double:
                sscanf(val, "%lf", &f->value.double_val);
                break;

        case type_string:
                f->value.string_val = strdup(val);
                break;

        case type_void:
                f->value.void_val = val;
                break;

        default:
                f->type = type_unknown;
                return NULL;	
	}
	return f;
}




int prelude_sql_field_to_string(prelude_sql_field_t *f, char *buf, int len)
{
	if ( ! f || ! buf )
		return -1;
		
	switch(f->type) {
                
        case type_int32:
                snprintf(buf, len, "%d", f->value.int32_val);
                break;

        case type_uint32:
                snprintf(buf, len, "%u", f->value.uint32_val);
                break;

        case type_int64:
                snprintf(buf, len, "%lld", f->value.int64_val);
                break;

        case type_uint64:
                snprintf(buf, len, "%llu", f->value.uint64_val);
                break;

        case type_double:
                snprintf(buf, len, "%f", f->value.double_val);
                break;

        case type_string:
                snprintf(buf, len, "%s", f->value.string_val);
                break;

        default:
                return -1;
	}
	
	return 0;
}



int prelude_sql_field_destroy(prelude_sql_field_t *f)
{
	if ( ! f )
		return -1;
	
	if ( f->type == type_string && f->value.string_val)
		free(f->value.string_val);
	
	free(f);
        
	return 0;
}



prelude_sql_row_t *prelude_sql_row_new(int cols)
{
	prelude_sql_row_t *row;
	
	if ( ! cols )
		return NULL;
	
	allocate(row, NULL);
	
	/*
         * don't use allocate here to clean up in case of failure
         */
	row->field = calloc(cols, sizeof(prelude_sql_field_t *));
	if ( ! row->field ) {
		log(LOG_ERR, "memory exhausted.\n");
		free(row);
		return NULL;
	}
        
	row->fields = cols;
	
	return row;
}




int prelude_sql_row_set_field(prelude_sql_row_t *row, int column, prelude_sql_field_t *field)
{
	if ( ! row || column < 0 || column > row->fields || row->field[column] )
		return -1;
	
	row->field[column] = field;
	
	return 0;
}




int prelude_sql_row_delete_field(prelude_sql_row_t *row, int column)
{
	if ( ! row || column > row->fields || column < 0 )
		return -1;
		
	row->field[column] = NULL;
	
	return 0;
}




int prelude_sql_row_destroy_field(prelude_sql_row_t *row, int column)
{
	prelude_sql_field_t *f;
	
	if ( ! row || column > row->fields || column < 0 )
		return -1;
	
	f = row->field[column];
	row->field[column] = NULL;
	
	return prelude_sql_field_destroy(f);
}




int prelude_sql_row_find_field(prelude_sql_row_t *row, const char *name)
{
	int i = 0;

        for ( i = 0; i < row->fields; i++ ) {

                if ( ! row->field[i] )
                        continue;
                
		if ( strncmp(row->field[i]->name, name, MAX_NAME_LEN) == 0 )
			return i;
	}
	
	return -1;
}



prelude_sql_field_t *prelude_sql_row_get_field(prelude_sql_row_t *row, int field)
{
	if ( ! row )
		return NULL;
	
	return (field < row->fields) ? row->field[field] : NULL;
}



int prelude_sql_row_get_width(prelude_sql_row_t *row)
{
	return row ? row->fields : -1;
}



int prelude_sql_row_destroy(prelude_sql_row_t *row)
{
	int i;
	
	for ( i = 0; i < row->fields; i++ )
		prelude_sql_row_destroy_field(row, i);
	
	free(row->field);	
	free(row);
        
	return 0;
}



prelude_sql_table_t *prelude_sql_table_new(const char *name)
{
	prelude_sql_table_t *t;

	allocate(t, NULL);

        strncpy(t->name, name, MAX_NAME_LEN);
	INIT_LIST_HEAD(&t->row_list);
	
	return t;
}



int prelude_sql_table_add_row(prelude_sql_table_t *table, prelude_sql_row_t *row)
{
	if ( ! table || ! row )
		return -1;
		
	list_add_tail(&row->list, &table->row_list);

        return 0;
}



void prelude_sql_table_delete_row(prelude_sql_row_t *row)
{
	list_del(&row->list);
}



void prelude_sql_table_iterate(prelude_sql_table_t *table, void (*callback)(prelude_sql_row_t *row))
{
	prelude_sql_row_t *row;
	struct list_head *tmp;
	
	list_for_each(tmp, &table->row_list) {
		row = list_entry(tmp, prelude_sql_row_t, list);
		callback(row);
	}
}



int prelude_sql_table_destroy(prelude_sql_table_t *table)
{
        prelude_sql_row_t *row;
	struct list_head *tmp, *n;
	
	list_for_each_safe(tmp, n, &table->row_list) {
		row = list_entry(tmp, prelude_sql_row_t, list);
		prelude_sql_table_delete_row(row);
		prelude_sql_row_destroy(row);
	}
        
	free(table);
	
	return 0;
}


