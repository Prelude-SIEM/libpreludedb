/*****
*
* Copyright (C) 2003 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_CLASSIC_IDMEF_MESSAGE_READ_H
#define _LIBPRELUDEDB_CLASSIC_IDMEF_MESSAGE_READ_H

idmef_db_values_t *idmef_db_read(prelude_db_connection_t *conn, 
       		                 idmef_object_list_t *objects, 
       		                 idmef_criterion_t *criterion);

#endif /* _LIBPRELUDEDB_CLASSIC_IDMEF_MESSAGE_READ_H */

