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
#include <stdarg.h>

#include <libprelude/prelude-log.h>

#include "strbuf.h"

#define CHUNK_SIZE 1024



struct strbuf {
	char *string;
	size_t textlen;
	size_t len;
};




strbuf_t *strbuf_new(void)
{
	strbuf_t *buf;
	
	buf = malloc(sizeof(*buf));
	if ( ! buf ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}
	
	buf->string = malloc(CHUNK_SIZE);
	if ( ! buf->string ) {
		log(LOG_ERR, "memory exhausted.\n");
		free(buf);
		return NULL;
	}

	buf->string[0] = '\0';
	buf->len = CHUNK_SIZE;
	buf->textlen = 0;
	
	return buf;
}



int strbuf_sprintf(strbuf_t *s, const char *fmt, ...)
{
	int ret;
	char *ptr;
	va_list ap;
	
	do {
		va_start(ap, fmt);
		ret = vsnprintf(s->string + s->textlen, s->len - s->textlen, fmt, ap);
		va_end(ap);
		
		/*
		 * From sprintf(3) on GNU/Linux:
		 *
		 * snprintf  and vsnprintf do not write more than
     	 	 * size bytes (including the trailing '\0'), and return -1 if
		 * the  output  was truncated due to this limit.  (Thus until
		 * glibc 2.0.6. Since glibc 2.1 these  functions  follow  the
		 * C99  standard and return the number of characters (exclud-
		 * ing the trailing '\0') which would have  been  written  to
		 * the final string if enough space had been available.)
		 */
		
		if ( (ret >= 0) && ret < (s->len - s->textlen) ) {
			s->textlen += ret;
			return ret;
		}
		
		ptr = realloc(s->string, s->len + CHUNK_SIZE);
		if ( ! ptr ) {
			log(LOG_ERR, "memory exhausted.\n");
			s->string[s->len - 1] = '\0';
			return -1;
		}
		
		s->string = ptr;
		s->len += CHUNK_SIZE;
		
	} while ( 1 );
}




char *strbuf_string(strbuf_t *s)
{
	return s->string;
}


int strbuf_empty(strbuf_t *s)
{
	return *(s->string) ? 0 : 1;
}


void strbuf_clear(strbuf_t *s)
{
	*(s->string) = '\0';
	s->textlen = 0;
}

void strbuf_destroy(strbuf_t *s) 
{
	free(s->string);
	free(s);
}


