/*****
*
* Copyright (C) 2005-2015 CS-SI. All Rights Reserved.
* Author: Yoann Vandoorselaere <yoann.v@prelude-ids.com>
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "preludedb-version.h"



static int levelstr_to_int(const char *wanted)
{
        int i;
        struct {
                int level;
                const char *level_str;
        } tbl[] = {
                { LIBPRELUDEDB_RELEASE_LEVEL_ALPHA, "alpha" },
                { LIBPRELUDEDB_RELEASE_LEVEL_BETA, "beta"   },
                { LIBPRELUDEDB_RELEASE_LEVEL_RC, "rc"       }
        };

        for ( i = 0; i < sizeof(tbl) / sizeof(*tbl); i++ ) {
                if ( strcmp(wanted, tbl[i].level_str) == 0 )
                        return tbl[i].level;
        }

        return -1;
}



/**
 * preludedb_check_version:
 * @req_version: The minimum acceptable version number.
 *
 * If @req_version is NULL this function will return the version of the library.
 * Otherwise, @req_version will be compared to the current library version. If
 * the library version is higher or equal, this function will return the current
 * library version. Otherwise, NULL is returned.
 *
 * Returns: The current library version, or NULL if @req_version doesn't match.
 */
const char *preludedb_check_version(const char *req_version)
{
        int ret;
        unsigned int hexreq;
        char rq_levels[6] = { 0 };
        int rq_major, rq_minor, rq_micro, rq_level = 0, rq_patch = 0;

        if ( ! req_version )
                return VERSION;

        ret = sscanf(req_version, "%d.%d.%d%5[^0-9]%d", &rq_major, &rq_minor, &rq_micro, rq_levels, &rq_patch);
        if ( ret < 3 )
                return NULL;

        if ( *rq_levels == 0 || *rq_levels == '.' )
                rq_level = LIBPRELUDEDB_RELEASE_LEVEL_FINAL;
        else {
                rq_level = levelstr_to_int(rq_levels);
                if ( rq_level < 0 )
                        return NULL;
        }

        hexreq = (rq_major << 24) | (rq_minor << 16) | (rq_micro << 8) | (rq_level << 4) | (rq_patch << 0);
        return (hexreq <= LIBPRELUDEDB_HEXVERSION) ? VERSION : NULL;
}
