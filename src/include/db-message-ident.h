/*****
*
* Copyright (C) 2003 Nicolas Delon <delon.nicolas@wanadoo.fr>
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

#ifndef _LIBPRELUDEDB_MESSAGE_IDENT_H
#define _LIBPRELUDEDB_MESSAGE_IDENT_H

typedef struct prelude_db_message_ident prelude_db_message_ident_t;

typedef struct prelude_db_message_ident_list prelude_db_message_ident_list_t;

prelude_db_message_ident_t *prelude_db_message_ident_new(uint64_t analyzerid, uint64_t ident);
void prelude_db_message_ident_destroy(prelude_db_message_ident_t *ident);
uint64_t prelude_db_message_ident_get_analyzerid(prelude_db_message_ident_t *ident);
uint64_t prelude_db_message_ident_get_ident(prelude_db_message_ident_t *ident);

#endif /* _LIBPRELUDEDB_MESSAGE_IDENT_H */