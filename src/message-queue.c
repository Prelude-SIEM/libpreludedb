static int db_message_queue(db_queue_t *queue, idmef_message_t *message)
{
	db_queued_message_t *queued_msg;
	
	queued = calloc(1, sizeof(*queued_msg));
	if (ret < 0) {
		log(LOG_ERROR, "out of memory\n");
		return -1;
	}
	
	queued_msg->message = message;

	db_queue_add(queue, queued_msg->list);
	
	return 0;
}

static void db_message_dequeue(db_queue_t *queue, db_queued_message_t *queued_msg)
{
	db_queue_delete(queue, queued_msg->list);
}

static void db_message_delete(db_queued_message_t *queued_msg)
{
	idmef_message_free(queued_msg->message);
	free(queued_msg);
}

static int db_dispatch_init(void)
{
	int i, ret;
	
	/* register db handlers */
	ret = db_dispatch_register();
	if (ret < 0)
		return -1;
	
	/* initialize input queue */
	INIT_LIST_HEAD(&db_input_queue);
	
	/* initialize output queues */
	db_output_queue = calloc(output_queues, sizeof(db_queue_t));
	if (!db_output_queue)
		return -1;
	
	for (i=0;i<output_queues;i++) {
		db_queue_init(db_output_queue[i]);
	}
	
	/* start the output threads */
	
	return 0; /* success */
}


static int db_message_dispatch(idmef_message_t *msg) 
{	
	/* XXX: force message to go into queue #0 */
	db_queue_message(db_output_queue[0], msg);
}