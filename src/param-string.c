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

#include <string.h>
#include <stdlib.h>

#include <stdio.h>

#include <libprelude/prelude-log.h>

#include "param-string.h"

#define BUFLEN 127

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
char *parameter_value(const char *string, const char *field)
{
	char buf[BUFLEN+1]; /* +1 for trailing '\0' */
	const char *s;
	const char *sep;
	const char *data;
	char *ret;
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
			
			ret = malloc(data_len + 1);
			if ( ! ret ) {
				log(LOG_ERR, "memory exhausted.\n");
				return NULL;
			}
			
			strncpy(ret, data, data_len);
			ret[data_len] = '\0';
			
			unescape(ret);
			unquote(ret);
			
			return ret;
		}
		
		s = sep + 1;
	} while (sep && *s);
	
	return NULL;
}

