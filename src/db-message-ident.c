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

#include <stdio.h>
#include <stdlib.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "db-message-ident.h"

struct prelude_db_message_ident {
	uint64_t analyzerid;
	uint64_t ident;
};

prelude_db_message_ident_t *prelude_db_message_ident_new(uint64_t analyzerid, uint64_t ident)
{
	prelude_db_message_ident_t *message_ident;

	message_ident = malloc(sizeof (*message_ident));
	if ( ! message_ident ) {
		log(LOG_ERR, "memory exhausted.\n");
		return NULL;
	}

	message_ident->analyzerid = analyzerid;
	message_ident->ident = ident;

	return message_ident;
}



void prelude_db_message_ident_destroy(prelude_db_message_ident_t *message_ident)
{
	free(message_ident);
}



uint64_t prelude_db_message_ident_get_analyzerid(prelude_db_message_ident_t *message_ident)
{
	return message_ident->analyzerid;
}



uint64_t prelude_db_message_ident_get_ident(prelude_db_message_ident_t *message_ident)
{
	return message_ident->ident;
}