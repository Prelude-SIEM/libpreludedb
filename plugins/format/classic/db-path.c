/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>
#include <libprelude/prelude-list.h>
#include <libprelude/prelude-hash.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"

#include "db-path.h"


struct db_path {
	int listed;
	prelude_list_t list;
	idmef_path_t *path;
	char *table;
	char *field;
	char *function;
	char *top_table;
	char *top_field;
	char *condition;
	char *ident_field;
	/* special fields for idmef_time */
	char *usec_field;
	char *gmtoff_field;
};


PRELUDE_LIST(db_paths);

static int db_path_count = 0;

db_path_t **db_path_index = NULL;


#define OBJECT_DUPLICATED_ERROR 1
#define PARSE_ERROR 2
#define OTHER_ERROR 3
#define BUFLEN 127

static void db_path_destroy(db_path_t *obj);


#define skip_whitespaces(str)				\
	while ( *(str) == ' ' || *(str) == '\t' )	\
		(str)++;



static int get_next_key_value_pair(const char **string, char **key, char **value)
{
	const char *ptr;

	skip_whitespaces(*string);

	ptr = strchr(*string, '=');
	if ( ! ptr )
		return 0;

	*key = strndup(*string, ptr - *string);
	if ( ! *key )
		return prelude_error_from_errno(errno);

	*string = ptr + 1;

	if ( **string == '"' ) {
		(*string)++;

		ptr = strchr(*string, '"');
		if ( ! ptr )
			return prelude_error(PRELUDE_ERROR_GENERIC);

		*value = strndup(*string, ptr - *string);
		if ( ! *value )
			return prelude_error_from_errno(errno);

		*string = ptr + 1;		

	} else {
		size_t size;

		size = strcspn(*string, " \t");

		*value = strndup(*string, size);
		if ( ! *value )
			return prelude_error_from_errno(errno);

		*string += size;
	}

	return 1;
}


static int parse_path_descr(const char *string, prelude_hash_t **hash)
{
	int ret;
	char *name, *value;

	ret = prelude_hash_new(hash, NULL, NULL, free, NULL);
	if ( ret < 0 )
		return ret;

	while ( (ret = get_next_key_value_pair(&string, &name, &value)) > 0 ) {
		ret = prelude_hash_set(*hash, name, value);
		if ( ret < 0 )
			goto error;
	}

	if ( ret < 0 )
		goto error;

	return 0;

 error:
	prelude_hash_destroy(*hash);
	return ret;
}



static int db_path_new(char *descr)
{
	prelude_hash_t *hash;
	db_path_t *obj;
	char *value;
	prelude_list_t *tmp;
	db_path_t *entry, *prev;
	int ret;

	obj = calloc(1, sizeof(*obj));
	if ( ! obj )
		return preludedb_error_from_errno(errno);

	ret = parse_path_descr(descr, &hash);
	if ( ret < 0 ) {
		free(obj);
		return ret;
	}

	value = prelude_hash_get(hash, "object");
	if ( ! value ) {
		ret = prelude_error(PRELUDE_ERROR_GENERIC);
		goto error;
	}

	ret = idmef_path_new_fast(&obj->path, value);
	free(value);
	if ( ret < 0 )
		goto error;

	obj->table = prelude_hash_get(hash, "table");
	if ( ! obj->table ) {
		ret = prelude_error(PRELUDE_ERROR_GENERIC);
		goto error;
	}

	obj->field = prelude_hash_get(hash, "field");
	obj->function = prelude_hash_get(hash, "function");
	if ( ! obj->field && ! obj->function ) {
		ret = prelude_error(PRELUDE_ERROR_GENERIC);
		goto error;
	}

	/* Following fields are optional */
	obj->top_table = prelude_hash_get(hash, "top_table");
	obj->top_field = prelude_hash_get(hash, "top_field");
	obj->condition = prelude_hash_get(hash, "condition");
	obj->ident_field = prelude_hash_get(hash, "ident_field");
	obj->usec_field = prelude_hash_get(hash, "usec_field");
	obj->gmtoff_field = prelude_hash_get(hash, "gmtoff_field");

	/* From idmef-cache.c -- adapted (shamelessly copying my own code :) */
	
	/*
	 * determine if path is in the list, or where it should be 
	 * added to the list, so the list is sorted by idmef_path_t->id
	 */
	entry = NULL;
	prev = NULL;
	prelude_list_for_each(&db_paths, tmp) {
		entry = prelude_list_entry(tmp, db_path_t, list);

		ret = idmef_path_compare(entry->path, obj->path);
		if ( ret == 0 ) {
			ret = prelude_error(PRELUDE_ERROR_GENERIC);
			goto error;
		}

		if (ret > 0) {
			entry = prev;
			break;        /* add after previous path */
		}
		
		prev = entry;
	}	
	
	/* 
	 * @entry@ holds the pointer to path we should add our path 
	 * after, or NULL if we should add it before the first path
	 */
	 
	if ( entry )
		prelude_list_add(&entry->list, &obj->list);
	else 
	 	prelude_list_add(&db_paths, &obj->list);
	 	
	obj->listed = 1;

	prelude_hash_destroy(hash);

	return 0;

 error:
	prelude_hash_destroy(hash);
	db_path_destroy(obj);

	return ret;
}



static void db_path_destroy(db_path_t *obj)
{
	if ( obj->listed )
		prelude_list_del(&obj->list);
	
	if ( obj->path )
		idmef_path_destroy(obj->path);
		
	if ( obj->table )
		free(obj->table);
		
	if ( obj->field )
		free(obj->field);
		
	if ( obj->function)
		free(obj->function);
		
	if ( obj->top_table )
		free(obj->top_table);
		
	if ( obj->top_field )
		free(obj->top_field);
		
	if ( obj->condition )
		free(obj->condition);
		
	if ( obj->ident_field )
		free(obj->ident_field);

	if ( obj->usec_field )
		free(obj->usec_field);

	if ( obj->gmtoff_field )
		free(obj->ident_field);

	free(obj);
}




int db_paths_init(const char *file)
{
	FILE *f;
	char buf[1024];
	int ret;
	int i,line;
	prelude_list_t *tmp;
	db_path_t *entry;
	char *ptr;

	f = fopen(file, "r");
	if ( ! f ) {
		prelude_log(PRELUDE_LOG_ERR, "could not open file %s\n", file);
		return -1;
	}

#ifdef DEBUG
	prelude_log(PRELUDE_LOG_INFO, "- Loading database specification from file %s\n", file);
#endif /* DEBUG */
	
	line = 0;
	while ( fgets(buf, sizeof(buf), f) ) {
		line++;

		/* skip comments */
		if ( buf[0] == '#' )
			continue;
			
		/* skip empty line */
		ptr = buf;
		while ( ( *ptr == ' ' ) || ( *ptr == '\t' ) ) ptr++;
		if ( ( *ptr == '\n' ) || ( *ptr == '\0' ) )
			continue;
		
		/* remove trailing \n */
		buf[strlen(buf)-1] = '\0';
		
		ret = db_path_new(buf);
		switch ( ret ) {
		case -OBJECT_DUPLICATED_ERROR:
			prelude_log(PRELUDE_LOG_ERR, "error: duplicated path on line %d\n", line);
			fclose(f);
			return -1;
		
		case -PARSE_ERROR:
			prelude_log(PRELUDE_LOG_ERR, "parse error: on line %d\n", line);
			fclose(f);
			return -1;
		
		case -OTHER_ERROR:
			prelude_log(PRELUDE_LOG_ERR, "error on line %d\n", line);
			fclose(f);
			return -1;
		
		default:
			break;
		}
		
		db_path_count++;
	}
	
	if ( ferror(f) ) {
		prelude_log(PRELUDE_LOG_ERR, "file read error.\n");
		return -1;
	}
		
	fclose(f);

	db_path_index = calloc(db_path_count, sizeof(*db_path_index));
	if ( ! db_path_index ) {
		prelude_log(PRELUDE_LOG_ERR, "out of memory\n");
		return -1;
	}
	
	i = 0;
	prelude_list_for_each(&db_paths, tmp) {
		entry = prelude_list_entry(tmp, db_path_t, list);
		db_path_index[i++] = entry;
	}
#ifdef DEBUG
	prelude_log(PRELUDE_LOG_INFO, "- %d paths loaded and indexed\n", db_path_count);
#endif /* DEBUG */
	
	return 0;
}




/* From idmef-cache.c -- adapted (shamelessly copying my own code :) */
db_path_t *db_path_find(idmef_path_t *path)
{
	int low, high, mid, cmp;
		
	/* Mostly from A.Drozdek, D.L.Simon "Data structures in C" */
	low = 0;
	high = db_path_count-1;
	
	while ( low <= high ) {
		mid = (low + high) / 2;
		cmp = idmef_path_compare(path, db_path_index[mid]->path);
		if ( cmp > 0 )
			low = mid+1;
		else if ( cmp < 0 )
			high = mid-1;
		else
			return db_path_index[mid];
	}
	
	return NULL;
}


char *db_path_get_table(db_path_t *path)
{
	return path->table;
}


char *db_path_get_field(db_path_t *path)
{
	return path->field;
}



char *db_path_get_function(db_path_t *path)
{
	return path->function;
}




char *db_path_get_top_table(db_path_t *path)
{
	return path->top_table;
}



char *db_path_get_top_field(db_path_t *path)
{
	return path->top_field;
}



char *db_path_get_condition(db_path_t *path)
{
	return path->condition;
}



char *db_path_get_ident_field(db_path_t *path)
{
	return path->ident_field;
}



char *db_path_get_usec_field(db_path_t *path)
{
	return path->usec_field;
}



char *db_path_get_gmtoff_field(db_path_t *path)
{
	return path->gmtoff_field;
}




void db_paths_destroy(void)
{
	prelude_list_t *n, *tmp;
	db_path_t *entry;
	
	prelude_list_for_each_safe(&db_paths, tmp, n) {
		entry = prelude_list_entry(tmp, db_path_t, list);
		db_path_destroy(entry);
	}
	
	free(db_path_index);
}
