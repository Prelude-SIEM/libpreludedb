/*****
*
* Copyright (C) 2005-2020 CS GROUP - France. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_ERROR_H
#define _LIBPRELUDEDB_ERROR_H

#include <libprelude/prelude-inttypes.h>
#include <libprelude/prelude-error.h>

#ifdef __cplusplus
 extern "C" {
#endif

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
	PRELUDEDB_ERROR_INVALID_MESSAGE_IDENT = 13,
	PRELUDEDB_ERROR_INVALID_SELECTED_OBJECT_STRING = 14,
	PRELUDEDB_ERROR_INVALID_OBJECT_SELECTION = 15,
	PRELUDEDB_ERROR_SCHEMA_VERSION_TOO_OLD = 16,
	PRELUDEDB_ERROR_SCHEMA_VERSION_TOO_RECENT = 17,
	PRELUDEDB_ERROR_SCHEMA_VERSION_INVALID = 18,
	PRELUDEDB_ERROR_CANNOT_LOAD_SQL_PLUGIN = 19,
	PRELUDEDB_ERROR_CANNOT_LOAD_FORMAT_PLUGIN = 20,
	PRELUDEDB_ERROR_INDEX = 21,
} preludedb_error_code_t;

typedef prelude_error_t preludedb_error_t;


static inline preludedb_error_t preludedb_error(preludedb_error_code_t error)
{
        return (preludedb_error_t) prelude_error_make(PRELUDE_ERROR_SOURCE_PRELUDEDB, (prelude_error_code_t) error);
}



static inline preludedb_error_t preludedb_error_verbose(preludedb_error_code_t error, const char *fmt, ...)
{
        int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = prelude_error_verbose_make_v(PRELUDE_ERROR_SOURCE_PRELUDEDB, (prelude_error_code_t) error, fmt, ap);
	va_end(ap);

	return ret;
}



static inline prelude_bool_t preludedb_error_check(preludedb_error_t error,
						   preludedb_error_code_t code)
{
	return (prelude_bool_t) (prelude_error_get_source(error) == PRELUDE_ERROR_SOURCE_PRELUDEDB &&
		                 prelude_error_get_code(error) == (prelude_error_code_t) code);
}


static inline preludedb_error_t preludedb_error_from_errno(int err)
{
	return prelude_error_from_errno(err);
}


const char *preludedb_strerror(preludedb_error_t error);

#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_ERROR_H */
