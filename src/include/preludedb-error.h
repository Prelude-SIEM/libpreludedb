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

#ifndef _LIBPRELUDEDB_ERROR_H
#define _LIBPRELUDEDB_ERROR_H

#include <libprelude/prelude-inttypes.h>
#include <libprelude/prelude-error.h>

typedef enum {
        PRELUDEDB_ERROR_NO_ERROR = 0,
        PRELUDEDB_ERROR_GENERIC = 1,
        PRELUDEDB_ERROR_INVALID_SETTINGS_STRING = 2,
        PRELUDEDB_ERROR_CONNECTION = 3,
        PRELUDEDB_ERROR_QUERY = 4,
        PRELUDEDB_ERROR_INVALID_COLUMN_NUM = 5,
        PRELUDEDB_ERROR_INVALID_COLUMN_NAME = 6,
        PRELUDEDB_ERROR_INVALID_VALUE = 7,
        PRELUDEDB_ERROR_INVALID_VALUE_TYPE = 8,
        PRELUDEDB_ERROR_UNKNOWN_SQL_PLUGIN = 9,
        PRELUDEDB_ERROR_UNKNOWN_FORMAT_PLUGIN = 10,
        PRELUDEDB_ERROR_ALREADY_IN_TRANSACTION = 11,
        PRELUDEDB_ERROR_NOT_IN_TRANSACTION = 12,
	PRELUDEDB_ERROR_INVALID_MESSAGE_IDENT = 13
} preludedb_error_code_t;

typedef prelude_error_t preludedb_error_t;


static inline preludedb_error_t preludedb_error(preludedb_error_code_t error)
{
        return prelude_error_make(PRELUDE_ERROR_SOURCE_PRELUDEDB, error);
}


static inline prelude_bool_t preludedb_error_check(preludedb_error_t error,
						   preludedb_error_code_t code)
{
	return (prelude_error_get_source(error) == PRELUDE_ERROR_SOURCE_PRELUDEDB &&
		prelude_error_get_code(error) == code);
}


static inline preludedb_error_t preludedb_error_from_errno(int err)
{
	return prelude_error_from_errno(err);
}


const char *preludedb_strerror(preludedb_error_t error);

#endif /* _LIBPRELUDEDB_ERROR_H */
