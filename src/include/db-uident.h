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

#ifndef _LIBPRELUDEDB_DB_UIDENT_H
#define _LIBPRELUDEDB_DB_UIDENT_H

typedef struct {
	uint64_t alert_ident;
	uint64_t analyzerid;
}	prelude_db_alert_uident_t;

typedef struct {
	uint64_t heartbeat_ident;
	uint64_t analyzerid;
}	prelude_db_heartbeat_uident_t;

#endif /* _LIBPRELUDEDB_DB_UIDENT_H */