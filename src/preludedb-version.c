/*****
*
* Copyright (C) 2005 Yoann Vandoorselaere <yoann@prelude-ids.org>
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

#include <stdio.h>
#include <stdlib.h>

#include "preludedb-version.h"
#include "config.h"


const char *preludedb_check_version(const char *req_version)
{
        int ret;
        int major, minor, micro;
        int rq_major, rq_minor, rq_micro;

        if ( ! req_version )
                return VERSION;
        
        ret = sscanf(VERSION, "%d.%d.%d", &major, &minor, &micro);
        if ( ret != 3 )
                return NULL;
        
        ret = sscanf(req_version, "%d.%d.%d", &rq_major, &rq_minor, &rq_micro);
        if ( ret != 3 )
                return NULL;
        
        if ( major > rq_major
             || (major == rq_major && minor > rq_minor)
             || (major == rq_major && minor == rq_minor && micro > rq_micro)
             || (major == rq_major && minor == rq_minor && micro == rq_micro) ) {
                return VERSION;
        }

        return NULL;
}
