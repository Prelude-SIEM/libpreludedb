/*****
*
* Copyright (C) 2005 PreludeIDS Technologies. All Rights Reserved.
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
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <libprelude/idmef.h>
#include <libprelude/prelude.h>
#include <libprelude/idmef-message-print.h>

#include "preludedb-sql-settings.h"
#include "preludedb-sql.h"
#include "preludedb-error.h"
#include "preludedb-path-selection.h"
#include "preludedb.h"


/*
 * FIXME: cleanup the statistics handling mess.
 */

static sig_atomic_t stop_processing = 0;
static const char *query_logging = NULL;
static unsigned int max_count = 0, cur_count = 0, start_offset = 0;

static idmef_criteria_t *alert_criteria = NULL;
static idmef_criteria_t *heartbeat_criteria = NULL;


/*
 * Global statistics
 */
static double global_elapsed1 = 0;
static double global_elapsed2 = 0;
static struct timeval start1, end1, start2, end2;
static const char *global_stats1 = NULL, *global_stats2 = NULL;


/*
 * Runtime statistics.
 */
static sig_atomic_t dump_stat = FALSE;
static unsigned int stats_processed = 0;



static void dump_generic_statistics(const char *stats1, const char *stats2)
{
        double elapsed1 = 0, elapsed2 = 0;
        
        stats_processed++;

        if ( stats1 ) {
                global_stats1 = stats1;
                elapsed1 = (end1.tv_sec * 1000000 + end1.tv_usec) - (start1.tv_sec * 1000000 + start1.tv_usec);
                global_elapsed1 += elapsed1;
        }
        
        if ( stats2 ) {
                global_stats2 = stats2;
                elapsed2 = (end2.tv_sec * 1000000 + end2.tv_usec) - (start2.tv_sec * 1000000 + start2.tv_usec);
                global_elapsed2 += elapsed2;
        }
        
        if ( ! dump_stat )
                return;
        
        fprintf(stderr, "Number of events processed: %u", stats_processed);
        if ( max_count )
                fprintf(stderr, "/%u\n", max_count);
        else
                fprintf(stderr, "\n");
        
        if ( stats1 )
                fprintf(stderr, "Average time to %s an event: %f.\n", stats1, elapsed1 / 1000000);

        if ( stats2 )
                fprintf(stderr, "Average time to %s an event: %f.\n", stats2, elapsed2 / 1000000);
        
        fprintf(stderr, "Total time to process an event: %f.\n", (elapsed1 + elapsed2) / 1000000);
        dump_stat = FALSE;
}


static void handle_stats_signal(int signo)
{
        /*
         * re-establish signal handler
         */
        signal(signo, handle_stats_signal);
        dump_stat = TRUE;
}



static void handle_signal(int signo)
{
        char buf[] = "Interrupted by signal. Will exit after current transaction.\n";

        /*
         * re-establish signal handler
         */
        signal(signo, handle_signal);
        
        write(STDERR_FILENO, buf, sizeof(buf));
        stop_processing = TRUE;
}


static int db_error(preludedb_t *db, int ret, const char *fmt, ...)
{
        va_list ap;
        char errbuf[PRELUDEDB_ERRBUF_SIZE];

        if ( ret == 0 )
                return ret;
        
        preludedb_get_error(db, ret, errbuf, sizeof(errbuf));

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        fprintf(stderr, ": %s.\n", errbuf);
        
        return ret;
}



static int set_alert_criteria(prelude_option_t *opt, const char *optarg, prelude_string_t *err, void *context)
{
        int ret;
        
        ret = idmef_criteria_new_from_string(&alert_criteria, optarg);
        if ( ret < 0 )
                prelude_perror(ret, "could not load alert criteria");

        return ret;
}



static int set_heartbeat_criteria(prelude_option_t *opt, const char *optarg, prelude_string_t *err, void *context)
{
        int ret;
        
        ret = idmef_criteria_new_from_string(&heartbeat_criteria, optarg);
        if ( ret < 0 )
                prelude_perror(ret, "could not load heartbeat criteria");

        return ret;
}



static int set_query_logging(prelude_option_t *opt, const char *optarg, prelude_string_t *err, void *context)
{
        query_logging = strdup(optarg);
        return 0;
}


static int set_count(prelude_option_t *opt, const char *optarg, prelude_string_t *err, void *context)
{
        max_count = atoi(optarg);
        return 0;
}


static int set_offset(prelude_option_t *opt, const char *optarg, prelude_string_t *err, void *context)
{
        start_offset = atoi(optarg);
        return 0;
}


static int set_help(prelude_option_t *opt, const char *optarg, prelude_string_t *err, void *context)
{
        return prelude_error(PRELUDE_ERROR_EOF);
}



static void cmd_generic_help(void)
{
        fprintf(stderr, "Database arguments:\n");
        fprintf(stderr, "  type\t: Type of database (mysql/pgsql).\n");
        fprintf(stderr, "  name\t: Name of the database.\n");
        fprintf(stderr, "  user\t: User to access the database.\n");
        fprintf(stderr, "  pass\t: Password to access the database.\n\n");
        
        fprintf(stderr, "Valid options:\n");       
        fprintf(stderr, "  --offset <offset>               : Skip processing until 'offset' events.\n");
        fprintf(stderr, "  --count <count>                 : Process at most count events.\n");
        fprintf(stderr, "  --query-logging <filename>      : Log SQL query to the specified file.\n");
        fprintf(stderr, "  --alert-criteria <criteria>     : Only process alert matching criteria.\n");
        fprintf(stderr, "  --heartbeat-criteria <criteria> : Only process heartbeat matching criteria.\n");
}



static void cmd_delete_help(void)
{
        fprintf(stderr, "Usage  : delete <database> [options]\n");
        fprintf(stderr, "Example: preludedb-admin delete \"type=mysql name=dbname user=prelude\"\n\n");

        fprintf(stderr, "Delete messages from <database>.\n\n");

        cmd_generic_help();
}



static void cmd_save_help(void)
{
        fprintf(stderr, "Usage  : save <database> <filename> [options]\n");
        fprintf(stderr, "Example: preludedb-admin save \"type=mysql name=dbname user=prelude\" outputfile\n\n");
        
        fprintf(stderr, "Save messages from <database> to <filename> (- for standard output).\n\n");
        
        cmd_generic_help();
}



static void cmd_load_help(void)
{
        fprintf(stderr, "Usage  : load <database> <filename> [options]\n");
        fprintf(stderr, "Example: preludedb-admin load \"type=mysql name=dbname user=prelude\" inputfile\n\n");

        fprintf(stderr, "Load messages from <filename> (or standard input) to <database>.\n\n");
        
        cmd_generic_help();
}


static void cmd_copy_help(void)
{
        fprintf(stderr, "Usage  : copy <database1> <database2> [options]\n");
        fprintf(stderr, "Example: preludedb-admin copy \"type=mysql name=dbname user=prelude\" \"type=pgsql name=dbname user=prelude\"\n\n");
        
        fprintf(stderr, "Copy messages from <database1> to <database2>.\n\n");
        
        cmd_generic_help();
}



static void cmd_print_help(void)
{
        fprintf(stderr, "Usage  : print <database> [filename] [options]\n");
        fprintf(stderr, "Example: preludedb-admin print \"type=mysql name=dbname user=prelude\"\n\n");
        
        fprintf(stderr, "Retrieve and print message from <database>.\n\n");
        
        cmd_generic_help();
}



static void print_help(char **argv)
{
        fprintf(stderr, "Usage: %s <copy|delete|load|save> <arguments>\n\n", argv[0]);
        
        fprintf(stderr, "\tcopy   - Make a copy of a Prelude database to another database.\n");
        fprintf(stderr, "\tdelete - Delete content of a Prelude database.\n");
        fprintf(stderr, "\tload   - Load a Prelude database from a file.\n");
        fprintf(stderr, "\tsave   - Save a Prelude database to a file.\n\n");
}





static int setup_generic_options(int *argc, char **argv)
{
        int ret;
        prelude_string_t *err;
        
        prelude_option_add(NULL, NULL, PRELUDE_OPTION_TYPE_CLI, 0, "offset",
                           NULL, PRELUDE_OPTION_ARGUMENT_REQUIRED, set_offset, NULL);
         
        prelude_option_add(NULL, NULL, PRELUDE_OPTION_TYPE_CLI, 0, "count",
                           NULL, PRELUDE_OPTION_ARGUMENT_REQUIRED, set_count, NULL);
        
        prelude_option_add(NULL, NULL, PRELUDE_OPTION_TYPE_CLI, 0, "query-logging",
                           NULL, PRELUDE_OPTION_ARGUMENT_REQUIRED, set_query_logging, NULL);
        
        prelude_option_add(NULL, NULL, PRELUDE_OPTION_TYPE_CLI, 0, "heartbeat-criteria",
                           NULL, PRELUDE_OPTION_ARGUMENT_REQUIRED, set_heartbeat_criteria, NULL);
                
        prelude_option_add(NULL, NULL, PRELUDE_OPTION_TYPE_CLI, 0, "alert-criteria",
                           NULL, PRELUDE_OPTION_ARGUMENT_REQUIRED, set_alert_criteria, NULL);

        prelude_option_add(NULL, NULL, PRELUDE_OPTION_TYPE_CLI, 'h', "help",
                           NULL, PRELUDE_OPTION_ARGUMENT_NONE, set_help, NULL);
        
        ret = prelude_option_read(NULL, NULL, argc, argv, &err, NULL);
        if ( ret < 0 && prelude_error_get_code(ret) != PRELUDE_ERROR_EOF )
                fprintf(stderr, "Option error: %s.\n", prelude_string_get_string(err));
        
        return ret;
}



static int db_new_from_string(preludedb_t **db, const char *str)
{
        int ret;
        preludedb_sql_t *sql;
	preludedb_sql_settings_t *sql_settings;
	char errbuf[PRELUDEDB_ERRBUF_SIZE];
        
	ret = preludedb_sql_settings_new_from_string(&sql_settings, str);
	if ( ret < 0 ) {
                fprintf(stderr, "Error loading database settings: %s.\n", preludedb_strerror(ret));
		return ret;
        }
        
	ret = preludedb_sql_new(&sql, NULL, sql_settings);
	if ( ret < 0 ) {
                fprintf(stderr, "Error creating database interface: %s.\n", preludedb_strerror(ret));
                preludedb_sql_settings_destroy(sql_settings);
                return ret;
        }
        
	ret = preludedb_new(db, sql, NULL, errbuf, sizeof(errbuf));
	if ( ret < 0 ) {
                fprintf(stderr, "could not initialize database: %s\n", errbuf);
		preludedb_sql_destroy(sql);
		return ret;
	}

        if ( query_logging )
                preludedb_sql_enable_query_logging(sql, query_logging);
        
	return 0;
}



static int copy_iterate(preludedb_t *src, preludedb_t *dst,
                        preludedb_result_idents_t *idents,
                        int (*get_message)(preludedb_t *db, uint64_t ident, idmef_message_t **msg))
{
        int ret = 0;
        uint64_t ident;
        idmef_message_t *msg;
        
        while ( ! stop_processing && (ret = preludedb_result_idents_get_next(idents, &ident)) > 0 ) {
                
                if ( cur_count++ >= max_count && max_count )
                        return 0;
                
                if ( start_offset ) {
                        start_offset--;
                        continue;
                }

                gettimeofday(&start1, NULL);
                
                ret = get_message(src, ident, &msg);
                if ( ret < 0 ) {
                        fprintf(stderr, "Error retrieving message %" PRELUDE_PRIu64 ": %s.\n",
                                ident, preludedb_strerror(ret));
                        continue;
                }

                gettimeofday(&end1, NULL);

                gettimeofday(&start2, NULL);
                ret = preludedb_insert_message(dst, msg);
                gettimeofday(&end2, NULL);
                
                idmef_message_destroy(msg);
                
                if ( ret < 0 ) {
                        fprintf(stderr, "Error inserting message %" PRELUDE_PRIu64 ": %s.\n",
                                ident, preludedb_strerror(ret));
                        continue;
                }

                dump_generic_statistics("retrieve", "insert");
        }
        
        return ret;
}



static int cmd_copy(int argc, char **argv)
{
        int ret;
        preludedb_t *src, *dst;
        preludedb_result_idents_t *alert_idents, *heartbeat_idents;
        
        ret = setup_generic_options(&argc, argv);
        if ( ret < 0 || argc < 4 ) {
                cmd_copy_help();
                exit(1);
        }
        
	ret = db_new_from_string(&src, argv[2]);
	if ( ret < 0 )
		return ret;
        
	ret = db_new_from_string(&dst, argv[3]);
	if ( ret < 0 )
		return ret;
        
        ret = preludedb_get_alert_idents(src, alert_criteria, -1, -1, 0, &alert_idents);        
        if ( ret < 0 )
                return db_error(src, ret, "retrieving alert idents list failed");

        if ( ret > 0 )
                ret = copy_iterate(src, dst, alert_idents, preludedb_get_alert);

        ret = preludedb_get_heartbeat_idents(src, heartbeat_criteria, -1, -1, 0, &heartbeat_idents);
	if ( ret < 0 )
                return db_error(src, ret, "retrieving heartbeat idents list failed");

        if ( ret > 0 )
                ret = copy_iterate(src, dst, heartbeat_idents, preludedb_get_heartbeat);
        
	return ret;
}



static int drop_iterate(preludedb_t *db, preludedb_result_idents_t *idents,
                        int (*delete_message)(preludedb_t *db, uint64_t ident))
{
        int ret = 0;
        uint64_t ident;
        
        while ( ! stop_processing && (ret = preludedb_result_idents_get_next(idents, &ident)) > 0 ) {

                if ( cur_count++ >= max_count && max_count )
                        return 0;
                
                if ( start_offset ) {
                        start_offset--;
                        continue;
                }

                gettimeofday(&start1, NULL);
                ret = delete_message(db, ident);
                gettimeofday(&end1, NULL);
                
                if ( ret < 0 ) 
                        fprintf(stderr, "error deleting message %" PRELUDE_PRIu64 ": %s.\n", ident, preludedb_strerror(ret));

                dump_generic_statistics("delete", NULL);
        }

        return ret;
}


static int cmd_delete(int argc, char **argv)
{
        int ret;
        preludedb_t *db;
        preludedb_result_idents_t *idents;

        ret = setup_generic_options(&argc, argv);
        if ( ret < 0 || argc != 3 ) {
                cmd_delete_help();
                exit(1);
        }
        
	ret = db_new_from_string(&db, argv[2]);
	if ( ret < 0 )
		return ret;
        
        ret = preludedb_get_alert_idents(db, alert_criteria, -1, -1, 0, &idents);
	if ( ret < 0 )
                return db_error(db, ret, "retrieving alert ident failed");
        
        if ( ret > 0 )
                drop_iterate(db, idents, preludedb_delete_alert);
                
        ret = preludedb_get_heartbeat_idents(db, heartbeat_criteria, -1, -1, 0, &idents);
        if ( ret < 0 ) 
                return db_error(db, ret, "retrieving heartbeat ident failed");

        if ( ret > 0 )
                drop_iterate(db, idents, preludedb_delete_heartbeat);
        
        return ret;
}



static int save_msg(prelude_msgbuf_t *msgbuf, prelude_msg_t *msg)
{
        int ret;
        prelude_io_t *fd = prelude_msgbuf_get_data(msgbuf);

        ret = prelude_msg_write(msg, fd);
        prelude_msg_recycle(msg);

        return ret;
}



static int save_iterate_message(preludedb_t *db, preludedb_result_idents_t *idents, prelude_msgbuf_t *msgbuf,
                                int (*get_message)(preludedb_t *db, uint64_t ident, idmef_message_t **out))
{
        int ret = 0;
        uint64_t ident;
        idmef_message_t *message;
        
	while ( ! stop_processing && (ret = preludedb_result_idents_get_next(idents, &ident)) > 0 ) {

                if ( cur_count++ >= max_count && max_count )
                        return 0;
                
                if ( start_offset ) {
                        start_offset--;
                        continue;
                }
                
                gettimeofday(&start1, NULL);
                
                ret = get_message(db, ident, &message);
                if ( ret < 0 ) {
                        fprintf(stderr, "Error retrieving message %" PRELUDE_PRIu64 ": %s.\n", ident, preludedb_strerror(ret));
                        continue;
                }

                gettimeofday(&end1, NULL);

                gettimeofday(&start2, NULL);
                ret = idmef_message_write(message, msgbuf);
                prelude_msgbuf_mark_end(msgbuf);
                gettimeofday(&end2, NULL);
                
                idmef_message_destroy(message);
                
                if ( ret < 0 ) {
                        prelude_perror(ret, "Error writing message %" PRELUDE_PRIu64 " to disk", ident);
                        continue;
                }

                dump_generic_statistics("retrieve", "save");
        }

        return 0;
}



static int cmd_save(int argc, char **argv)
{
        int ret;
        preludedb_t *db;
        prelude_io_t *io;
        FILE *fd = stdout;
        prelude_msgbuf_t *msgbuf;
        preludedb_result_idents_t *idents;

        ret = setup_generic_options(&argc, argv);
        if ( ret < 0 || argc != 4 ) {
                cmd_save_help();
                exit(1);
        }
        
        ret = db_new_from_string(&db, argv[2]);
	if ( ret < 0 )
		return ret;

        if ( *argv[3] != '-' ) {
                fd = fopen(argv[3], "w");
                if ( ! fd ) {
                        fprintf(stderr, "could not open '%s' for writing: %s.\n", argv[3], strerror(errno));
                        return -1;
                }
        }
        
        ret = prelude_io_new(&io);
        if ( ret < 0 )
                return ret;

        prelude_io_set_file_io(io, fd);

        ret = prelude_msgbuf_new(&msgbuf);
        if ( ret < 0 )
                return ret;

        prelude_msgbuf_set_data(msgbuf, io);
        prelude_msgbuf_set_callback(msgbuf, save_msg);
                
        ret = preludedb_get_alert_idents(db, alert_criteria, -1, -1, 0, &idents);
	if ( ret < 0 )
                return ret;

        if ( ret > 0 )
                save_iterate_message(db, idents, msgbuf, preludedb_get_alert);
                
        ret = preludedb_get_heartbeat_idents(db, heartbeat_criteria, -1, -1, 0, &idents);
	if ( ret < 0 )
		return ret;

        if ( ret > 0 )
                save_iterate_message(db, idents, msgbuf, preludedb_get_heartbeat);

        prelude_msgbuf_destroy(msgbuf);
        prelude_io_close(io);
        
        return ret;
}



static int cmd_load(int argc, char **argv)
{
        int ret;
        FILE *fd = stdin;
        preludedb_t *db;
        prelude_io_t *io;
        prelude_msg_t *msg;
        idmef_message_t *idmef;
 
        ret = setup_generic_options(&argc, argv);
        if ( ret < 0 || argc < 3 ) {
                cmd_load_help();
                exit(1);
        }

        ret = db_new_from_string(&db, argv[2]);
	if ( ret < 0 )
		return ret;
        
        if ( argc >= 4 && *argv[3] != '-' ) {
                
                fd = fopen(argv[3], "r");
                if ( ! fd ) {
                        fprintf(stderr, "could not open '%s' for reading: %s.\n", argv[3], strerror(errno));
                        return -1;
                }
        }
        
        ret = prelude_io_new(&io);
        if ( ret < 0 )
                return ret;

        prelude_io_set_file_io(io, fd);
        
        while ( ! stop_processing ) {                
                msg = NULL;

                ret = prelude_msg_read(&msg, io);                
                if ( ret < 0 ) {
                        if ( prelude_error_get_code(ret) == PRELUDE_ERROR_EOF ) {
                                ret = 0;
                                break;
                        }
                        
                        fprintf(stderr, "error reading message: %s.\n", prelude_strerror(ret));
                        return ret;
                }
                
                if ( cur_count++ >= max_count && max_count ) {
                        prelude_msg_destroy(msg);
                        break;
                }
                
                if ( start_offset ) {
                        start_offset--;
                        continue;
                }

                ret = idmef_message_new(&idmef);
                if ( ret < 0 ) {
                        fprintf(stderr, "error creating new IDMEF message: %s.\n", prelude_strerror(ret));
                        return ret;
                }

                gettimeofday(&start1, NULL);
                
                ret = idmef_message_read(idmef, msg);
                if ( ret < 0 ) {
                        fprintf(stderr, "error decoding IDMEF message: %s.\n", prelude_strerror(ret));
                        return ret;
                }

                gettimeofday(&end1, NULL);

                gettimeofday(&start2, NULL);
                ret = preludedb_insert_message(db, idmef);
                if ( ret < 0 ) {
                        fprintf(stderr, "error inserting IDMEF message: %s.\n", preludedb_strerror(ret));
                        return ret;
                }
                gettimeofday(&end2, NULL);
                
                idmef_message_destroy(idmef);
                prelude_msg_destroy(msg);

                dump_generic_statistics("read", "insert");
        }

        prelude_io_close(io);
        prelude_io_destroy(io);
        
        return ret;
}



static int print_iterate_message(preludedb_t *db, preludedb_result_idents_t *idents, prelude_io_t *io,
                                 int (*get_message)(preludedb_t *db, uint64_t ident, idmef_message_t **idmef))
{
        int ret = 0;
        uint64_t ident;
        idmef_message_t *idmef;
        
	while ( ! stop_processing && (ret = preludedb_result_idents_get_next(idents, &ident)) > 0 ) {
                                
                if ( cur_count++ >= max_count && max_count )
                        return 0;
                
                if ( start_offset ) {
                        start_offset--;
                        continue;
                }


                gettimeofday(&start1, NULL);

                ret = get_message(db, ident, &idmef);
                if ( ret < 0 ) {
                        fprintf(stderr, "Error retrieving message %" PRELUDE_PRIu64 ": %s.\n", ident, preludedb_strerror(ret));
                        continue;
                }
                
                gettimeofday(&end1, NULL);

                gettimeofday(&start2, NULL);
                idmef_message_print(idmef, io);
                gettimeofday(&end2, NULL);
                
                idmef_message_destroy(idmef);
                
                if ( ret < 0 ) {
                        prelude_perror(ret, "Error writing message %" PRELUDE_PRIu64 " to disk", ident);
                        continue;
                }

                dump_generic_statistics("retrieve", "print");
        }

        return ret;
}



static int cmd_print(int argc, char **argv)
{
        int ret;
        FILE *fd = stdout;
        preludedb_t *db;
        prelude_io_t *io;
        preludedb_result_idents_t *idents;

        ret = setup_generic_options(&argc, argv);
        if ( ret < 0 || argc < 3 ) {
                cmd_print_help();
                exit(1);
        }

        ret = db_new_from_string(&db, argv[2]);
	if ( ret < 0 )
		return ret;

        if ( argc >= 4 && *argv[3] != '-' ) {
                                
                fd = fopen(argv[3], "w");
                if ( ! fd ) {
                        fprintf(stderr, "could not open '%s' for reading: %s.\n", argv[3], strerror(errno));
                        return -1;
                }
        }
        
        ret = prelude_io_new(&io);
        if ( ret < 0 )
                return ret;

        prelude_io_set_file_io(io, fd);

        ret = preludedb_get_alert_idents(db, alert_criteria, -1, -1, 0, &idents);
	if ( ret < 0 )
                return db_error(db, ret, "retrieving alert ident failed");
        
        if ( ret > 0 )
                print_iterate_message(db, idents, io, preludedb_get_alert);

        ret = preludedb_get_heartbeat_idents(db, heartbeat_criteria, -1, -1, 0, &idents);
	if ( ret < 0 )
                return db_error(db, ret, "retrieving heartbeat ident failed");

        if ( ret > 0 )
                print_iterate_message(db, idents, io, preludedb_get_heartbeat);
        
        prelude_io_close(io);
        prelude_io_destroy(io);

        return ret;
}



static void dump_stats(void)
{
        double global_elapsed = global_elapsed1 + global_elapsed2;

        if ( global_stats1 ) {
                fprintf(stderr, "%u '%s' in %f seconds (%f seconds/events - %f %s/sec average).\n",
                        stats_processed, global_stats1, global_elapsed1 / 1000000, 
                        (global_elapsed1 / stats_processed) / 1000000,
                        stats_processed / (global_elapsed1 / 1000000), global_stats1);
        }
        
        if ( global_stats2 ) {
                fprintf(stderr, "%u '%s' in %f seconds (%f seconds/events - %f %s/sec average).\n",
                        stats_processed, global_stats2, global_elapsed2 / 1000000, 
                        (global_elapsed2 / stats_processed) / 1000000,
                        stats_processed / (global_elapsed2 / 1000000), global_stats2);
        }
        
        fprintf(stderr, "%u events processed in %f seconds (%f seconds/events - %f events/sec average).\n",
                stats_processed, global_elapsed / 1000000, 
                (global_elapsed / stats_processed) / 1000000,
                stats_processed / (global_elapsed / 1000000)
                );
}



int main(int argc, char **argv)
{
	int ret;
        size_t i;
        const struct {
                char *name;
                int (*run)(int argc, char **argv);
        } commands[] = {
		{ "copy", cmd_copy     },
                { "delete", cmd_delete },
                { "load", cmd_load     },
                { "print", cmd_print   },
                { "save", cmd_save     },
        };

        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);
        signal(SIGUSR1, handle_stats_signal);
        signal(SIGQUIT, handle_stats_signal);
        
	ret = preludedb_init();
	if ( ret < 0 )
		return ret;

	if ( argc < 2 ) {
                print_help(argv);
                return -1;
	}

	for ( i = 0; i < sizeof(commands) / sizeof(*commands); i++ ) {
		if ( strcmp(argv[1], commands[i].name) != 0 )
                        continue;
                
                ret = commands[i].run(argc, argv);
                if ( ret < 0 )                        
                        return ret;
                
                if ( stop_processing )
                        fprintf(stderr, "Interrupted by signal at transaction %u. Use --offset %u to resume operation.\n",
                                cur_count, cur_count);
                
                dump_stats();
                
                return ret;
        }

	fprintf(stderr, "Unknown command: %s.\n", argv[1]);
        print_help(argv);
        
        return -1;
}
