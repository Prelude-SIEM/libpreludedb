/*****
*
* Copyright (C) 2005-2019 CS-SI. All Rights Reserved.
* Author: Nicolas Delon <nicolas.delon@prelude-ids.com>
*
* This file is part of the PreludeDB library.
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
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*****/

#include "config.h"
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
                "Invalid path selection",
                "Database schema version too old",
                "Database schema version too recent",
                "Database schema version invalid",
                "Cannot load sql plugin",
                "Cannot load format plugin",
                "Invalid index"
        };

        if ( prelude_error_is_verbose(error) )
                return _prelude_thread_get_error();

        if ( prelude_error_get_source(error) == PRELUDE_ERROR_SOURCE_PRELUDEDB ) {
                preludedb_error_code_t code;

                code = prelude_error_get_code(error);
                if ( code >= sizeof(error_strings) / sizeof(*error_strings) )
                        return "Unknown error code";

                return error_strings[code];
        }

        return prelude_strerror(error);
}
