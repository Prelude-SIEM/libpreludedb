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
#include <libprelude/prelude-message.h>
#include <libprelude/prelude-message-buffered.h>

#include <libprelude/idmef-message-id.h>
#include <libprelude/idmef-message-send.h>
#include <libprelude/idmef-message-recv.h>

#include "db-cache.h"


struct db_cache {
        char *directory;
};



/*
 * right now, ident are incremented of 1 for each new alert/heartbeat.
 */
inline static int get_cache_entry(uint64_t ident)
{
        return ident % 10;
}



static prelude_io_t *pio_open(const char *filename, const char *mode) 
{
        FILE *fd;
        prelude_io_t *pfd;
        
        pfd = prelude_io_new();
        if ( ! pfd ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }        

        fd = fopen(filename, mode);
        if ( ! fd ) {
                log(LOG_ERR, "couldn't open %s (%s).\n", filename, mode);
                prelude_io_close(pfd);
                prelude_io_destroy(pfd);
                return NULL;
        }

        prelude_io_set_file_io(pfd, fd);

        return pfd;
}



static void get_cache_filename(db_cache_t *cache, char *out, size_t size, const char *type, uint64_t ident)
{
        int key;

        key = get_cache_entry(ident);
        snprintf(out, size, "%s/%d/%s.%llu", cache->directory, key, type, ident);
}



static int get_message_infos(idmef_message_t *msg, const char **out, uint64_t *ident)
{
        idmef_message_type_t type;
        
        type = idmef_message_get_type(msg);

        if ( type == idmef_alert_message ) {
                *out = "alert";
                *ident = idmef_alert_get_ident(idmef_message_get_alert(msg));
        }

        else if ( type == idmef_heartbeat_message ) {
                *out = "heartbeat";
                *ident = idmef_heartbeat_get_ident(idmef_message_get_heartbeat(msg));
        }

        else return -1;

        return 0;
}




static int msg_to_alert(idmef_message_t *idmef, prelude_msg_t *msg)
{
        idmef_alert_t *alert;

        alert = idmef_message_new_alert(idmef);
        if ( ! alert )
                return -1;
        
        if ( ! idmef_recv_alert(msg, alert) )
                return -1;

        return 0;
}




static int msg_to_heartbeat(idmef_message_t *idmef, prelude_msg_t *msg)
{
        idmef_heartbeat_t *heartbeat;
        
        heartbeat = idmef_message_new_heartbeat(idmef);
        if ( ! heartbeat )
                return -1;

        if ( ! idmef_recv_heartbeat(msg, heartbeat) )
                return -1;

        return 0;
}




static idmef_message_t *read_message_from_cache(prelude_io_t *fd) 
{
        int ret;
        uint8_t tag;
        prelude_msg_t *msg;
        idmef_message_t *idmef;
        
        ret = prelude_msg_read(&msg, fd);
        if ( ret != prelude_msg_finished )
                return NULL;

        idmef = idmef_message_new();
        if ( ! idmef ) {
                prelude_msg_destroy(msg);
                return NULL;
        }
        
        tag = prelude_msg_get_tag(msg);

        if ( tag == MSG_ALERT_TAG ) 
                ret = msg_to_alert(idmef, msg);
        
        else if ( tag == MSG_HEARTBEAT_TAG )
                ret = msg_to_heartbeat(idmef, msg);

        if ( ret == 0 )
                return idmef;
        
        prelude_msg_destroy(msg);
        idmef_message_destroy(idmef);
                
        return NULL;
}




static prelude_msg_t *cache_write_cb(prelude_msgbuf_t *msgbuf)
{
        prelude_io_t *fd;
        prelude_msg_t *msg;
        
        fd = prelude_msgbuf_get_data(msgbuf);
        assert(fd);

        msg = prelude_msgbuf_get_msg(msgbuf);
        assert(msg);
        
        prelude_msg_write(msg, fd);
        prelude_msg_recycle(msg);
        
        return 0;
}




static int write_message_to_cache(idmef_message_t *idmef, prelude_io_t *fd) 
{
        prelude_msgbuf_t *msgbuf;
        
        msgbuf = prelude_msgbuf_new(0);
        if ( ! msgbuf ) 
                return -1;

        prelude_msgbuf_set_data(msgbuf, fd);
        prelude_msgbuf_set_callback(msgbuf, cache_write_cb);
        idmef_send_message(msgbuf, idmef);
        
        prelude_msgbuf_mark_end(msgbuf);
        prelude_msgbuf_close(msgbuf);
        
        return 0;
}



static int init_cache_directory(const char *directory)
{
        int ret, i;
        struct stat st;
        char dname[256];

        ret = stat(directory, &st);
        if ( ret < 0 && errno != ENOENT ) {
                log(LOG_ERR, "error stating %s.\n", directory);
                return -1;
        }
        
        ret = mkdir(directory, S_IRWXU);
        if ( ret < 0 ) {
                log(LOG_ERR, "couldn't create directory %s.\n", directory);
                return -1;
        }

        for ( i = 0; i < 10; i++ ) {
                snprintf(dname, sizeof(dname), "%s/%d", directory, i);

                ret = stat(dname, &st);
                if ( ret < 0 && errno != ENOENT ) {
                        log(LOG_ERR, "error stating %s.\n", dname);
                        return -1;
                }
                
                ret = mkdir(dname, S_IRWXU);
                if ( ret < 0 ) {
                        log(LOG_ERR, "couldn't create directory %s.\n", directory);
                        return -1;
                }
        }

        return 0;
}




static db_cache_t *create_cache_object(const char *directory)
{
        db_cache_t *new;

        new = malloc(sizeof(*new));
        if ( ! new ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }

        new->directory = strdup(directory);
        if ( ! new->directory ) {
                log(LOG_ERR, "memory exhausted.\n");
                free(new);
                return NULL;
        }

        return new;
}




int db_cache_write(db_cache_t *cache, idmef_message_t *message)
{
        int ret;
        uint64_t ident;
        const char *type;
        prelude_io_t *fd;
        char filename[256];
        
        ret = get_message_infos(message, &type, &ident);
        assert(ret == 0);

        get_cache_filename(cache, filename, sizeof(filename), type, ident);
        
        fd = pio_open(filename, "w");
        if ( ! fd )
                return -1;
        
        ret = write_message_to_cache(message, fd);

        prelude_io_close(fd);
        prelude_io_destroy(fd);

        return ret;
}



idmef_message_t *db_cache_read(db_cache_t *cache, const char *type, uint64_t ident)
{
        int ret;
        struct stat st;
        prelude_io_t *fd;
        char filename[256];
        idmef_message_t *idmef;
        
        get_cache_filename(cache, filename, sizeof(filename), type, ident);
        
        ret = stat(filename, &st);
        if ( ret < 0 ) {
                if ( errno != ENOENT )
                        log(LOG_ERR, "error stating %s.\n", filename);
                
                return NULL;
        }
        
        fd = pio_open(filename, "r");
        if ( ! fd )
                return NULL;
        
        idmef = read_message_from_cache(fd);

        prelude_io_close(fd);
        prelude_io_destroy(fd);

        return idmef;
}



db_cache_t *db_cache_new(const char *directory)
{
        int ret;
        
        ret = init_cache_directory(directory);
        if ( ret < 0 )
                return NULL;
        
        return create_cache_object(directory);
}



void db_cache_destroy(db_cache_t *cache)
{
        free(cache->directory);
        free(cache);
}


