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

/* 
 * Find char 'c' in first 'n' bytes of string 's'
 */

static const char *strnchr(const char *s, char c, int n)
{
	int i;
	
	for ( i = 0; i < n; i++ )
		if ( s[i] == c )
			return s + i;
		
	return NULL;
}

/*
 * Find the first occurence of any of separator characters from string 
 * 'sep' in string 's'.
 *
 * If a `'' or `"' is found, the search is resumed at the next occurence
 * or `'' or `"' respectively. 
 * If a `\' is found, the following character is skipped. 
 */

static const char *find_separator(const char *s, char *sep)
{
	if ( !s || ! sep )
		return NULL;
	
	while ( s && *s ) {
		
		if ( ( *s == '\'' ) && ( *(s-1) != '\\' ) ) {
			/* allow sigle quote to be escaped with a backslash */
			do {
				s = strchr(s+1, '\'');
			} while ( s && ( *(s-1) == '\\' ) ); 
			s++;
			continue;
		}
		
		if ( ( *s == '"' ) && ( *(s-1) != '\\' ) ) {
			/* allow double quote to be escaped with a backslash */
			do {
				s = strchr(s+1, '"');
			} while ( s && ( *(s-1) == '\\') ); 
			s++;
			continue;
			
		}
		
		if ( *s == '\\' ) {
			/* don't step over trailing \0 */
			if (*++s)
				s++;
				
			continue;
		}
		
		if ( strchr(sep, *s) )
			return s;
			
		s++;
	}
	
	return NULL;
}

/* strcpy(3) says that strings may not overlap, so we reimplement */
static void my_strcpy(char *dst, char *src)
{
	if ( !dst || !src )
		return ;
	
	while (*src) 
		*dst++ = *src++;
		
	*dst = '\0';
}

/* unescape backslash-espaced sequences */
static void unescape(char *s)
{
	if ( !s ) 
		return ;
	
	while ( *s ) {
		if ( *s == '\\' ) 
			my_strcpy(s, s+1);
		
		s++;
	}
}

/* remove quotes surrounding data */
static void unquote(char *s)
{
	int len;
	
	len = strlen(s);

	if ( ( s[0] == '"' ) && ( s[len-1] == '"' ) ) {
		s[len-1] = '\0';
		my_strcpy(s, s+1);
	}


	if ( ( s[0] == '\'' ) && ( s[len-1] == '\'' ) ) {
		s[len-1] = '\0';
		my_strcpy(s, s+1);
	}
			
}

/* 
 * Get value of the parameter 'field' from the configuration string 'string'. 
 *
 * The string must have structure similar to the example below:
 * field1=value1 field2="par11=val11 par12=val12" field3='par22=val22 par23' field4\' 
 *
 * Field name is case insensitive. 
 * If the field is not found, NULL is returned. 
 * If the field value is empty, a pointer to empty string is returned. 
 */
static int parameter_value(const char *string, const char *field, char **value)
{
	char buf[BUFLEN+1]; /* +1 for trailing '\0' */
	const char *s;
	const char *sep;
	const char *data;
	int len;
	int name_len;
	int data_len;
	
	s = string;
	
	do {
		/*sep = strpbrk(s, " ");*/
		sep = find_separator(s, " \t");
		if ( sep ) {
			if ( sep == s ) {
				s++;
				continue;
			}
			
			len = sep - s;
		} else
			len = strlen(s);
		
		data = strnchr(s, '=', len);
		if ( data ) 
			data++;
		else
			data = s + len;

		data_len = s + len - data;
		name_len = len - data_len - 1;
				
		if ( name_len > BUFLEN )
			name_len = BUFLEN;

		strncpy(buf, s, name_len);
		buf[name_len] = '\0';
			
		if ( strncasecmp(buf, field, name_len) == 0 ) {
			
			*value = malloc(data_len + 1);
			if ( ! *value )
				return preludedb_error_from_errno(errno);
			
			strncpy(*value, data, data_len);
			(*value)[data_len] = '\0';
			
			unescape(*value);
			unquote(*value);
			
			return 1;
		}
		
		s = sep + 1;
	} while (sep && *s);
	
	return 0;
}




static int db_path_new(char *descr)
{
	db_path_t *obj;
	char *val;
	prelude_list_t *tmp;
	db_path_t *entry, *prev;
	int ret;
	
	obj = calloc(1, sizeof(*obj));
	if ( ! obj )
		return preludedb_error_from_errno(errno);
	
	ret = parameter_value(descr, "object", &val);
	if ( ret <= 0 )
		goto parse_error;
	
	ret = idmef_path_new_fast(&obj->path, val);
	free(val);
	if ( ret < 0 )
		goto error;

	ret = parameter_value(descr, "table", &obj->table);
	if ( ret <= 0 )
		goto parse_error;

	ret = parameter_value(descr, "field", &obj->field);
	if ( ret < 0 )
		goto error;

	ret = parameter_value(descr, "function", &obj->function);
	if ( ret < 0 )
		goto error;
	
	if ( ! obj->field && ! obj->function )
		goto parse_error;

	/* Following fields are optional */
	ret = parameter_value(descr, "top_table", &obj->top_table);
	if ( ret < 0 )
		goto error;

	ret = parameter_value(descr, "top_field", &obj->top_field);
	if ( ret < 0 )
		return ret;

	ret = parameter_value(descr, "condition", &obj->condition);
	if ( ret < 0 )
		return ret;

	ret = parameter_value(descr, "ident_field", &obj->ident_field);
	if ( ret < 0 )
		return ret;

	ret = parameter_value(descr, "usec_field", &obj->usec_field);
	if ( ret < 0 )
		return ret;

	ret = parameter_value(descr, "gmtoff_field", &obj->gmtoff_field);
	if ( ret < 0 )
		return ret;
	

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
		
		if (ret == 0)
			goto error_duplicated; /* path found */
			
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

	return 0;  /* Success */

error_duplicated:
	db_path_destroy(obj);

	return -OBJECT_DUPLICATED_ERROR;
			
parse_error:
	db_path_destroy(obj);
		
	return -PARSE_ERROR;
	
error:
	db_path_destroy(obj);
	
	return -OTHER_ERROR;
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
