#ifndef _LIBPRELUDEDB_DISPATCH_H
#define _LIBPRELUDEDB_DISPATCH_H

typedef struct {
	idmef_message_t *message;
	int active_queues;
} idmef_message_container_t;

typedef struct {
	struct list_head list;
	db_queue_t queue;
	db_connection_t *connection;
	struct list_head *filter_list;
} output_queue_t;

#endif _LIBPRELUDEDB_DISPATCH_H