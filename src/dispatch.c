#include <stdio.h>
#include <pthread.h>

#include <libprelude/log.h>
#include <libprelude/list.h>
#include <libprelude/idmef-tree.h>

#include "queue.h"
#include "dispatch.h"

static db_queue_t input_queue;

static list_head output_queues_list;
static int output_queues;

void output_reader(void *arg, void *extra)
{
	idmef_message_container_t *cont = (idmef_message_container_t *) arg;
	
	/* write message */
	printf("writing message\n");
	
	/* delete if necessary */
	if (--cont->active_queues == 0) {
		printf("freeing %p\n", cont);
		idmef_message_free(cont->message);
		free(cont);
	}
	
	return FREE_ENTRY; /* delete list entry */
}

int input_reader(void *arg, void *extra)
{
	struct list_head *tmp;
	output_queue_t *queue;
	idmef_message_t *msg = (idmef_message_t *) arg;
	idmef_message_container_t *container;
	
	/* create container object */
	container = calloc(1, sizeof(idmef_message_container_t));
	container->message = msg;
	container->active_queues = output_queues;
	
	/* add the container to output queues */
	list_for_each(tmp, output_queues_list) {
		queue = list_entry(tmp, output_queue_t, list);
		
		db_queue_add(&queue->queue, container);
	}
	
	return FREE_ENTRY; /* remove message from input queue */
}

int db_dispatch_message(const idmef_message_t *msg)
{
	void *ret;
	
	ret = db_queue_add(&input_queue, (void *) msg);
	
	return (ret == NULL) ? -1 : 0;
}


int db_dispatch_init(void)
{
	/* initialize output queues */
	INIT_LIST_HEAD(output_queues);
	
	output_queues = 0;
	
	/* initialize input queue */
	/*db_queue_init(&init_queue, output_queues, input_reader, NULL);*/
}

int db_dispatch_register_output(const db_connection_t *connection)
{
	output_queue_t *output;
	
	output = calloc(1, sizeof(output_queue_t));
	if (!output) {
		log(LOG_ERR, "out of memory!\n");
		return -1;
	}
	
	output->connection = connection;
	INIT_LIST_HEAD(&output->filter_list);
	db_queue_init(&queue->queue, 1, output_reader, queue);
}