/*****
*
* Copyright (C) 2005 PreludeIDS Technologies. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#ifndef _LIBPRELUDEDB_SQL_SETTINGS_H
#define _LIBPRELUDEDB_SQL_SETTINGS_H

#ifdef __cplusplus
 extern "C" {
#endif
         
#define PRELUDEDB_SQL_SETTING_HOST "host"
#define PRELUDEDB_SQL_SETTING_PORT "port"
#define PRELUDEDB_SQL_SETTING_NAME "name"
#define PRELUDEDB_SQL_SETTING_USER "user"
#define PRELUDEDB_SQL_SETTING_PASS "pass"
#define PRELUDEDB_SQL_SETTING_TYPE "type"
#define PRELUDEDB_SQL_SETTING_FILE "file"
#define PRELUDEDB_SQL_SETTING_LOG "log"

typedef struct preludedb_sql_settings preludedb_sql_settings_t;


int preludedb_sql_settings_new(preludedb_sql_settings_t **settings);
int preludedb_sql_settings_new_from_string(preludedb_sql_settings_t **settings, const char *str);

void preludedb_sql_settings_destroy(preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set(preludedb_sql_settings_t *settings,
			       const char *name, const char *value);
int preludedb_sql_settings_set_from_string(preludedb_sql_settings_t *settings, const char *str);

const char *preludedb_sql_settings_get(const preludedb_sql_settings_t *settings, const char *name);

int preludedb_sql_settings_set_host(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_host(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_port(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_port(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_name(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_name(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_user(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_user(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_pass(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_pass(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_log(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_log(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_type(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_type(const preludedb_sql_settings_t *settings);

int preludedb_sql_settings_set_file(preludedb_sql_settings_t *settings, const char *value);
const char *preludedb_sql_settings_get_file(const preludedb_sql_settings_t *settings);

         
#ifdef __cplusplus
  }
#endif

#endif /* _LIBPRELUDEDB_SQL_SETTINGS_H */
