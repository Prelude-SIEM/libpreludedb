/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
* Copyright (C) 2002 Krzysztof Zaraska <kzaraska@student.uci.agh.edu.pl>
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

#ifndef _LIBPRELUDEDB_PLUGIN_FORMAT_H
#define _LIBPRELUDEDB_PLUGIN_FORMAT_H

typedef struct {
	PLUGIN_GENERIC;

	void * (*format_get_message_list)(prelude_db_connection_t * connection,
					  idmef_cache_t * cache,
					  idmef_criterion_t * criterion);

	void (*format_free_message_list)(prelude_db_connection_t * connection,
					 idmef_cache_t * cache,
					 void * message_list);

	void * (*format_get_message)(prelude_db_connection_t * connection,
				     idmef_cache_t * cache,
				     void * message_list);

	void (*format_free_message)(prelude_db_connection_t * connection,
				    idmef_cache_t * cache,
				    void * message_list,
				    void * message);

	idmef_value_t * (*format_get_message_field_value)(prelude_db_connection_t * connection,
							  idmef_cache_t * cache,
							  void * message_list,
							  void * message);

	int (*format_insert_idmef_message)(prelude_db_connection_t * connection,
					   const idmef_message_t * message);
} plugin_format_t;

#define	plugin_get_message_list_func(p) (p)->format_get_message_list
#define plugin_free_message_list_func(p) (p)->format_free_message_list
#define plugin_get_message_func(p) (p)->format_get_message
#define plugin_free_message_func(p) (p)->format_free_message
#define plugin_get_message_field_value_func(p) (p)->format_get_message_field_value
#define	plugin_insert_idmef_message_func(p) (p)->format_insert_idmef_message

#define	plugin_set_get_message_list_func(p, f) plugin_get_message_list_func(p) = (f)
#define plugin_set_free_message_list_func(p, f) plugin_free_message_list_func(p) = (f)
#define	plugin_set_get_message_func(p, f) plugin_get_message_func(p) = (f)
#define plugin_set_free_message_func(p, f) plugin_free_message_func(p) = (f)
#define plugin_set_get_message_field_value_func(p, f) plugin_get_message_field_value_func(p) = (f)
#define	plugin_set_insert_idmef_message_func(p, f) plugin_insert_idmef_message_func(p) = (f)

int format_plugins_init(const char *dirname);

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */
