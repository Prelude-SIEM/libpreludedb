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

#define NO_DISTINCT 0
#define DISTINCT 1

#define NO_LIMIT -1

#define NO_OFFSET -1

#define AS_MESSAGES 0
#define AS_VALUES 1

int idmef_db_select(preludedb_sql_t *sql,
		    preludedb_path_selection_t *selection, 
		    idmef_criteria_t *criteria,
		    int distinct, int limit, int offset, int as_values,
		    preludedb_sql_table_t **table);

int idmef_db_select_idents(preludedb_sql_t *sql,
			   char type,
			   idmef_criteria_t *criteria,
			   int limit, int offset, preludedb_result_idents_order_t order,
			   preludedb_sql_table_t **table);

#endif /* _LIBPRELUDEDB_CLASSIC_IDMEF_DB_SELECT_H */
