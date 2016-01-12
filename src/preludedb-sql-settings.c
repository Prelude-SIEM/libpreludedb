/*****
*
* Copyright (C) 2005-2016 CS-SI. All Rights Reserved.
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
#include <string.h>
#include <ctype.h>

#include <libprelude/prelude-hash.h>

#include "preludedb-error.h"
#include "preludedb-sql-settings.h"


struct preludedb_sql_settings {
        prelude_hash_t *hash;
};



static inline void skip_spaces(const char **str)
{
        while ( isspace((int) **str) )
                (*str)++;
}



int preludedb_sql_settings_new(preludedb_sql_settings_t **settings)
{
        int ret;

        *settings = malloc(sizeof(**settings));
        if ( ! *settings )
                return prelude_error_from_errno(errno);

        ret = prelude_hash_new(&(*settings)->hash, NULL, NULL, free, free);
        if ( ret < 0 )
                free(*settings);

        return ret;
}



int preludedb_sql_settings_new_from_string(preludedb_sql_settings_t **settings, const char *str)
{
        int ret;

        ret = preludedb_sql_settings_new(settings);
        if ( ret < 0 )
                return ret;

        ret = preludedb_sql_settings_set_from_string(*settings, str);
        if ( ret < 0 )
                preludedb_sql_settings_destroy(*settings);

        return ret;
}



void preludedb_sql_settings_destroy(preludedb_sql_settings_t *settings)
{
        prelude_hash_destroy(settings->hash);
        free(settings);
}



int preludedb_sql_settings_set(preludedb_sql_settings_t *settings,
                               const char *name, const char *value)
{
        char *n, *v;

        n = strdup(name);
        if ( ! n )
                return preludedb_error_from_errno(errno);

        v = strdup(value);
        if ( ! v ) {
                free(n);
                return preludedb_error_from_errno(errno);
        }

        return prelude_hash_set(settings->hash, n, v);
}



static int get_name(const char **str, char **name)
{
        const char *start;

        skip_spaces(str);

        if ( **str == '\0' )
                return 0;

        start = *str;

        while ( isalnum((int) **str) || **str == '_' )
                (*str)++;

        if ( **str != '=' || *str == start )
                return preludedb_error(PRELUDEDB_ERROR_INVALID_SETTINGS_STRING);

        *name = strndup(start, *str - start);

        (*str)++;

        return *name ? (*str - start - 1) : preludedb_error_from_errno(errno);
}



static int get_value(const char **str, char **value)
{
        int escaped = 0, cnt;
        char end_of_string = ' ';

        *value = calloc(strlen(*str) + 1, 1);
        if ( ! *value )
                return preludedb_error_from_errno(errno);

        if ( **str == '\'' || **str == '"' ) {
                end_of_string = **str;
                (*str)++;
        }

        cnt = 0;
        while ( **str ) {
                if ( escaped ) {
                        (*value)[cnt++] = **str;
                        escaped = 0;

                } else if ( **str == '\\' ) {
                        escaped = 1;

                } else if ( **str == end_of_string ) {
                        (*str)++;
                        break;

                } else {
                        (*value)[cnt++] = **str;
                }

                (*str)++;
        }

        if ( **str == '\0' && (end_of_string == '\'' || end_of_string == '"') ) {
                free(*value);
                return preludedb_error(PRELUDEDB_ERROR_INVALID_SETTINGS_STRING);
        }

        return cnt;
}



int preludedb_sql_settings_set_from_string(preludedb_sql_settings_t *settings, const char *str)
{
        int ret;
        char *name = NULL, *value;

        while ( (ret = get_name(&str, &name)) > 0 ) {
                ret = get_value(&str, &value);
                if ( ret < 0 )
                        return ret;

                ret = prelude_hash_set(settings->hash, name, value);
                if ( ret < 0 )
                        return ret;
        }

        return ret;
}



const char *preludedb_sql_settings_get(const preludedb_sql_settings_t *settings, const char *name)
{
        return prelude_hash_get(settings->hash, name);
}



/*
 * Convenient functions for client/server databases.
 */


#define convenient_functions(name, key, default)                                                \
int preludedb_sql_settings_set_ ## name(preludedb_sql_settings_t *settings, const char *value)        \
{                                                                                                \
        return preludedb_sql_settings_set(settings, key, value);                                \
}                                                                                                \
                                                                                                \
                                                                                                \
                                                                                                \
const char *preludedb_sql_settings_get_ ## name(const preludedb_sql_settings_t *settings)        \
{                                                                                                \
        const char *value;                                                                        \
                                                                                                \
        value = preludedb_sql_settings_get(settings, key);                                        \
                                                                                                \
        return value ? value : default;                                                                \
}



convenient_functions(host, PRELUDEDB_SQL_SETTING_HOST, NULL)
convenient_functions(port, PRELUDEDB_SQL_SETTING_PORT, NULL)
convenient_functions(name, PRELUDEDB_SQL_SETTING_NAME, "prelude")
convenient_functions(user, PRELUDEDB_SQL_SETTING_USER, NULL)
convenient_functions(pass, PRELUDEDB_SQL_SETTING_PASS, NULL)
convenient_functions(type, PRELUDEDB_SQL_SETTING_TYPE, NULL)
convenient_functions(file, PRELUDEDB_SQL_SETTING_FILE, NULL)
convenient_functions(log, PRELUDEDB_SQL_SETTING_LOG, NULL)
