#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include <libprelude/list.h>

#include "queue.h"


static void wait_for_entry(db_queue_t *queue)
{			
	pthread_mutex_lock(&queue->input_mutex);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			
	while (list_empty(&queue->list)) 
		pthread_cond_wait(&queue->input_cond, &queue->input_mutex);
		
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_mutex_unlock(&queue->input_mutex);
}

static void *db_queue_reader(void *arg)
{
	struct list_head *tmp, *n;
	db_queue_entry_t *entry;
	db_queue_t *queue = (db_queue_t *) arg;	
	sigset_t sigset;
	int ret;
	
	sigfillset(&sigset);
	ret = pthread_sigmask(SIG_SETMASK, &sigset, NULL);
	if (ret < 0) {
		printf("can't set thread signal mask\n");
		
		return NULL;
	}
	
	
	while (1) {
		
		if (list_empty(&queue->list)) 
			wait_for_entry(queue);
		else {
			/* process all objects on the list */
			
			list_for_each_safe(tmp, n, &queue->list) {
				entry = list_entry(tmp, db_queue_entry_t, list);
				
				if (entry->data) {
					/*pthread_mutex_lock(&entry->mutex);*/
					
					ret = pthread_mutex_trylock(&entry->mutex);
					if (ret < 0) 
						/* mutex already locked or not initialized */
						continue ;
						
					/* FIXME: this will lead to 100% CPU utilization
					   when all entries in the queue are locked */
					
					ret = queue->reader_callback(entry->data, queue->reader_data);
					pthread_mutex_unlock(&entry->mutex);
				
					switch (ret) {
						case FREE_ENTRY: entry->data = NULL;
								 break;
						case KEEP_ENTRY: break;
						default:	 printf("incorrect callback return value!\n");
							 	break;
					}
				} else {
					/* data has been freed, destroy the entry */
					db_queue_delete(queue, entry);
					pthread_mutex_destroy(&entry->mutex);
					free(entry);
					printf("%p freed!\n", entry);
				}		
			}
		}		
	}
}

void db_queue_init(db_queue_t *queue, int threads, int (*callback)(void *, void *), void *reader_data)
{
	int i;
	
	INIT_LIST_HEAD(&queue->list);
	pthread_mutex_init(&queue->mutex, NULL);
	pthread_mutex_init(&queue->input_mutex, NULL);

	pthread_cond_init(&queue->input_cond, NULL);
	
	queue->reader_data = reader_data;
	queue->reader_callback = callback;

	if (threads > MAX_THREADS) {
		printf("too many reader threads requested, reverting to maximum number (%d)\n", MAX_THREADS);
		threads = MAX_THREADS;
	}

	queue->reader_threads = threads;

	for (i=0;i<threads;i++)	
		pthread_create(&queue->reader_thread[i], NULL, db_queue_reader, queue);
}

int db_queue_is_empty(db_queue_t *queue)
{
	return list_empty(&queue->list);
}

int db_queue_lock(db_queue_t *queue)
{
	return pthread_mutex_lock(&queue->mutex);
}

int db_queue_unlock(db_queue_t *queue)
{
	return pthread_mutex_unlock(&queue->mutex);
}

int db_queue_trylock(db_queue_t *queue)
{
	return pthread_mutex_trylock(&queue->mutex);
}

db_queue_entry_t *db_queue_add(db_queue_t *queue, void *data)
{
	db_queue_entry_t *entry;
	
	entry = (db_queue_entry_t *) calloc(1, sizeof(db_queue_entry_t));
	if (!entry) {
		printf("out of memory\n");
		return NULL;
	}
	
	pthread_mutex_init(&entry->mutex, NULL);
	entry->data = data;
	
	db_queue_lock(queue);
	list_add_tail(&entry->list, &queue->list);
	db_queue_unlock(queue);
	
	pthread_mutex_lock(&queue->input_mutex);
	pthread_cond_signal(&queue->input_cond);
	pthread_mutex_unlock(&queue->input_mutex);
	
	return entry;	
}

void db_queue_delete(db_queue_t *queue, db_queue_entry_t *entry)
{
	db_queue_lock(queue);
	list_del(&entry->list);
	db_queue_unlock(queue);
}

void db_queue_destroy(db_queue_t *queue)
{
	int i;
	
	for (i=0;i<queue->reader_threads;i++)
		pthread_cancel(queue->reader_thread[i]);
	
	pthread_mutex_destroy(&queue->mutex);
	pthread_mutex_destroy(&queue->input_mutex);
}

int db_queue_entry_lock(db_queue_entry_t *entry)
{
	return pthread_mutex_lock(&entry->mutex);
}

int db_queue_entry_unlock(db_queue_entry_t *entry)
{
	return pthread_mutex_unlock(&entry->mutex);
}

int db_queue_entry_trylock(db_queue_entry_t *entry)
{
	return pthread_mutex_trylock(&entry->mutex);
}
