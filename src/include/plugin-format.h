/*****
*
* Copyright (C) 2001, 2002 Yoann Vandoorselaere <yoann@mandrakesoft.com>
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

#ifndef _LIBPRELUDEDB_PLUGIN_FORMAT_H
#define _LIBPRELUDEDB_PLUGIN_FORMAT_H

typedef struct {
        PLUGIN_GENERIC;
        int (*format_write)(prelude_db_connection_t *connection, idmef_message_t *message);
} plugin_format_t;

#define plugin_write_func(p) (p)->format_write

#define plugin_set_write_func(p, f) plugin_write_func(p) = (f)

int format_plugins_init(const char *dirname);

plugin_generic_t *plugin_init(int argc, char **argv);

#endif /* _LIBPRELUDEDB_PLUGIN_FORMAT_H */




