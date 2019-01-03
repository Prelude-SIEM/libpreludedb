/*****
*
* Copyright (C) 2003-2019 CS-SI. All Rights Reserved.
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

#ifndef _LIBPRELUDEDB_CLASSIC_DELETE_H
#define _LIBPRELUDEDB_CLASSIC_DELETE_H

int classic_delete_alert(preludedb_t *db, uint64_t ident);

ssize_t classic_delete_alert_from_list(preludedb_t *db, uint64_t *ident, size_t size);

ssize_t classic_delete_alert_from_result_idents(preludedb_t *db, preludedb_result_idents_t *results);

int classic_delete_heartbeat(preludedb_t *db, uint64_t ident);

ssize_t classic_delete_heartbeat_from_list(preludedb_t *db, uint64_t *ident, size_t size);

ssize_t classic_delete_heartbeat_from_result_idents(preludedb_t *db, preludedb_result_idents_t *results);

#endif /* _LIBPRELUDEDB_CLASSIC_DELETE_H */
