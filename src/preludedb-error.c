/*****
*
* Copyright (C) 2005 Nicolas Delon <nicolas@prelude-ids.org>
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

#include <stdlib.h>

#include "preludedb-error.h"


const char *preludedb_strerror(preludedb_error_t error)
{
	static const char *error_strings[] = {
                "Successful",
                "Unknown generic error",
                "Invalid SQL settings string",
                "Connection error",
                "Query error",
                "Invalid column num",
                "Invalid column name",
                "Invalid value",
                "Invalid value type",
                "Unknown sql plugin",
                "Unknown format plugin",
                "Already in transaction",
                "Not in transaction",
		"Invalid message ident",
		"Invalid selected path string",
		"Invalid path selection"
        };

	if ( prelude_error_get_source(error) == PRELUDE_ERROR_SOURCE_PRELUDEDB ) {
		preludedb_error_code_t code;

		code = prelude_error_get_code(error);
		if ( code < 0 || code >= sizeof (error_strings) / sizeof (error_strings[0]) )
			return NULL;

		return error_strings[code];
	}

	return prelude_strerror(error);
}
