/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#include <stdlib.h>

#include <libprelude/prelude-log.h>
#include <libprelude/idmef.h>

#include "db-type.h"
#include "sql-connection-data.h"
#include "sql.h"
#include "db-connection.h"


struct prelude_db_connection {
	prelude_db_type_t type;
	void *connection;
};




prelude_db_connection_t *prelude_db_connection_new(prelude_db_type_t type, void *cnx)
{
        prelude_db_connection_t *conn;

        conn = calloc(1, sizeof(*conn));
        if ( ! conn ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
        
        conn->type = type;
        conn->connection = cnx;
        
        return conn;
}





prelude_db_type_t prelude_db_connection_get_type(prelude_db_connection_t *conn)
{
	return conn ? conn->type : -1 ;
}




void *prelude_db_connection_get(prelude_db_connection_t *conn)
{
	return conn ? conn->connection : NULL;
}



prelude_sql_connection_t *prelude_db_connection_get_sql(prelude_db_connection_t *conn)
{
	if ( conn->type != prelude_db_type_sql )
		return NULL;

	return conn->connection;
}



void prelude_db_connection_destroy(prelude_db_connection_t *conn)
{
	if ( ! conn )
		return ;
	
	free(conn);
}


