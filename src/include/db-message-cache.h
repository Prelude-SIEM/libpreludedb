/*****
*
* Copyright (C) 2003 Yoann Vandoorselaere <yoann@prelude-ids.org>
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

#ifndef _LIBPRELUDEDB_DB_MESSAGE_CACHE_H
#define _LIBPRELUDEDB_DB_MESSAGE_CACHE_H

typedef struct db_message_cache db_message_cache_t;


db_message_cache_t *db_message_cache_new(const char *directory);
void db_message_cache_destroy(db_message_cache_t *cache);

int db_message_cache_write(db_message_cache_t *cache, idmef_message_t *message);
idmef_message_t *db_message_cache_read(db_message_cache_t *cache, const char *type, uint64_t ident);


#endif /* _LIBPRELUDEDB_DB_MESSAGE_CACHE_H */
