/*****
*
* Copyright (C) 2003 Yoann Vandoorselaere <yoann@prelude-ids.org>
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <libprelude/idmef.h>
#include <libprelude/prelude-io.h>
#include <libprelude/prelude-log.h>
#include <libprelude/prelude-msg.h>
#include <libprelude/prelude-msgbuf.h>
#include <libprelude/prelude-error.h>

#include <libprelude/idmef-message-id.h>
#include <libprelude/idmef-message-write.h>
#include <libprelude/idmef-message-read.h>

#include "db-message-cache.h"


struct db_message_cache {
        char *directory;
};



static prelude_io_t *pio_open(const char *filename, const char *mode) 
{
        int ret;
        FILE *fd;
        prelude_io_t *pfd;

        ret = prelude_io_new(&pfd);
        if ( ret < 0 )
                return NULL;

        fd = fopen(filename, mode);
        if ( ! fd ) {
                prelude_io_destroy(pfd);
                return NULL;
        }

        prelude_io_set_file_io(pfd, fd);

        return pfd;
}



static void get_message_cache_filename(db_message_cache_t *cache,
				       char *out, size_t size, const char *type, uint64_t ident)
{
        int key;

	/*
	 * right now, ident are incremented of 1 for each new alert/heartbeat.
	 */
        key = ident % 10;

        snprintf(out, size, "%s/%d/%s.%" PRIu64, cache->directory, key, type, ident);
}



static int get_message_infos(idmef_message_t *message, const char **out, uint64_t *ident)
{
        idmef_message_type_t type;

        type = idmef_message_get_type(message);

        if ( type == IDMEF_MESSAGE_TYPE_ALERT ) {
                *out = "alert";
                *ident = idmef_alert_get_messageid(idmef_message_get_alert(message));
        }

	else if ( type == IDMEF_MESSAGE_TYPE_HEARTBEAT ) {
                *out = "heartbeat";
                *ident = idmef_heartbeat_get_messageid(idmef_message_get_heartbeat(message));

        }

	else
		return -1;

        return 0;
}



static idmef_message_t *read_message_from_cache(prelude_io_t *fd) 
{
        int ret;
        idmef_message_t *message;
        prelude_msg_t *pmsg = NULL;
        
        if ( prelude_msg_read(&pmsg, fd) < 0 )
                return NULL;

        message = idmef_message_new();
        if ( ! message ) {
                prelude_msg_destroy(pmsg);
                return NULL;
        }

        ret = idmef_message_read(message, pmsg);
        if ( ret < 0 && prelude_error_get_code(ret) != PRELUDE_ERROR_EOF ) {
                prelude_msg_destroy(pmsg);
                idmef_message_destroy(message);
                return NULL;
        }
        
        idmef_message_set_pmsg(message, pmsg);

        return message;
}




static int cache_write_cb(prelude_msgbuf_t *msgbuf, prelude_msg_t *msg)
{
        prelude_io_t *fd;
        
        fd = prelude_msgbuf_get_data(msgbuf);
        assert(fd);
        
        return prelude_msg_write(msg, fd);
}




static int write_message_to_cache(idmef_message_t *idmef, prelude_io_t *fd) 
{
        int ret;
        prelude_msgbuf_t *msgbuf;

        ret = prelude_msgbuf_new(&msgbuf);
        if ( ret < 0 ) 
                return -1;

        prelude_msgbuf_set_data(msgbuf, fd);
        prelude_msgbuf_set_callback(msgbuf, cache_write_cb);
        idmef_message_write(idmef, msgbuf);
        
        prelude_msgbuf_mark_end(msgbuf);
        prelude_msgbuf_destroy(msgbuf);
        
        return 0;
}



static int create_directory_if_needed(const char *directory) 
{
        int ret;
        struct stat st;
        
        ret = stat(directory, &st);

        if ( ret == 0 )
                return 0;
        
        if ( ret < 0 && errno != ENOENT ) {
                log(LOG_ERR, "error stating %s.\n", directory);
                return -1;
        }

        if ( mkdir(directory, S_IRWXU) < 0 ) {
                log(LOG_ERR, "couldn't create directory %s.\n", directory);
                return -1;
        }

	return 0;
}




static db_message_cache_t *create_message_cache(const char *directory)
{
        db_message_cache_t *cache;

        cache = malloc(sizeof(*cache));
        if ( ! cache ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }

        cache->directory = strdup(directory);
        if ( ! cache->directory ) {
                log(LOG_ERR, "memory exhausted.\n");
                free(cache);
                return NULL;
        }

        return cache;
}




int db_message_cache_write(db_message_cache_t *cache, idmef_message_t *message)
{
        int ret;
        uint64_t ident;
        const char *type;
        prelude_io_t *fd;
        char filename[256];

        assert(get_message_infos(message, &type, &ident) == 0);

        get_message_cache_filename(cache, filename, sizeof(filename), type, ident);
        
        fd = pio_open(filename, "w");
        if ( ! fd )
                return -1;
        
        ret = write_message_to_cache(message, fd);

        prelude_io_close(fd);
        prelude_io_destroy(fd);

        return ret;
}



idmef_message_t *db_message_cache_read(db_message_cache_t *cache, const char *type, uint64_t ident)
{
        struct stat st;
        prelude_io_t *fd;
        char filename[256];
        idmef_message_t *message;

        get_message_cache_filename(cache, filename, sizeof(filename), type, ident);

        if ( stat(filename, &st) < 0 ) {
                if ( errno != ENOENT )
                        log(LOG_ERR, "error stating %s.\n", filename);

                return NULL;
        }

        fd = pio_open(filename, "r");
        if ( ! fd )
                return NULL;

        message = read_message_from_cache(fd);

        prelude_io_close(fd);
        prelude_io_destroy(fd);

        return message;
}



db_message_cache_t *db_message_cache_new(const char *directory)
{
        int i;
        char dname[256];
        
        if ( create_directory_if_needed(directory) < 0 )
                return NULL;
        
        for ( i = 0; i < 10; i++ ) {
                snprintf(dname, sizeof(dname), "%s/%d", directory, i);

                if ( create_directory_if_needed(dname) < 0 )
                        return NULL;
        }

        return create_message_cache(directory);
}



void db_message_cache_destroy(db_message_cache_t *cache)
{
        free(cache->directory);
        free(cache);
}
