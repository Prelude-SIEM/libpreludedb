/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_CLASSIC_IDMEF_DB_SELECT_H
#define _LIBPRELUDEDB_CLASSIC_IDMEF_DB_SELECT_H

prelude_sql_table_t * idmef_db_select(prelude_db_connection_t *conn,
				      int distinct,
				      idmef_selection_t *selection, 
				      idmef_criterion_t *criterion);

#endif /* _LIBPRELUDEDB_CLASSIC_IDMEF_DB_SELECT_H */

