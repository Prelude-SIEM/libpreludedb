/*****
*
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
* Copyright (C) 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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
#include <string.h>

#include <libprelude/prelude-log.h>

#include "sql-connection-data.h"


struct prelude_sql_connection_data {
	char *type;
	char *name;
        char *user;
        char *pass;
        char *host;
        char *port;
};




prelude_sql_connection_data_t *prelude_sql_connection_data_new(void)
{
	prelude_sql_connection_data_t *data;
	
	data = calloc(1, sizeof(*data));
	if ( ! data ) {
		log(LOG_ERR, "out of memory!\n");
		return NULL;
	}
	
	return data;
}




int prelude_sql_connection_data_set_type(prelude_sql_connection_data_t *cnx, const char *type) 
{
        if ( ! type )
                return -1;
        
        return (cnx->type = strdup(type)) ? 0 : -1;
}




char *prelude_sql_connection_data_get_type(prelude_sql_connection_data_t *cnx)
{
	return cnx ? cnx->type : NULL;
}




int prelude_sql_connection_data_set_host(prelude_sql_connection_data_t *cnx, const char *host) 
{
        if ( ! host )
                return -1;
        
        return (cnx->host = strdup(host)) ? 0 : -1;
}




char *prelude_sql_connection_data_get_host(prelude_sql_connection_data_t *cnx)
{
	return cnx ? cnx->host : NULL;
}




int prelude_sql_connection_data_set_port(prelude_sql_connection_data_t *cnx, const char *port) 
{
        if ( ! port )
                return -1;
        
        return (cnx->port = strdup(port)) ? 0 : -1;
}



char *prelude_sql_connection_data_get_port(prelude_sql_connection_data_t *cnx)
{
	return cnx ? cnx->port : NULL;
}




int prelude_sql_connection_data_set_user(prelude_sql_connection_data_t *cnx, const char *user) 
{
        if ( ! user )
                return -1;
        
        return (cnx->user = strdup(user)) ? 0 : -1;
}



char *prelude_sql_connection_data_get_user(prelude_sql_connection_data_t *cnx)
{
	return cnx ? cnx->user : NULL;
}




int prelude_sql_connection_data_set_pass(prelude_sql_connection_data_t *cnx, const char *pass)
{
        if ( ! pass )
                return -1;
        
        return (cnx->pass = strdup(pass)) ? 0 : -1;
}




char *prelude_sql_connection_data_get_pass(prelude_sql_connection_data_t *cnx)
{
	return cnx ? cnx->pass : NULL;
}




void prelude_sql_connection_data_destroy(prelude_sql_connection_data_t *cnx)
{
	if ( ! cnx )
		return ;
		
	if ( cnx->name ) 
		free(cnx->name);
	
	if ( cnx->type )
		free(cnx->type);
		
	if ( cnx->user )
		free(cnx->user);
		
	if ( cnx->pass )
		free(cnx->pass);
		
	if ( cnx->host )
		free(cnx->host);
		
	if ( cnx->port )
		free(cnx->port);
	
	free(cnx);
}


