#ifndef _LIBPRELUDEDB_QUEUE_H
#define _LIBPRELUDEDB_QUEUE_H

#define MAX_THREADS 20

typedef struct {
	struct list_head list;
	pthread_mutex_t mutex;
	pthread_mutex_t input_mutex;
	pthread_cond_t input_cond;
	int reader_threads;
	pthread_t reader_thread[MAX_THREADS];
	int (*reader_callback)(void *, void *); 
	void *reader_data;
} db_queue_t;

typedef struct {
	struct list_head list;
	pthread_mutex_t mutex;
	void *data;
} db_queue_entry_t;

#define KEEP_ENTRY 0
#define FREE_ENTRY 1

void db_queue_init(db_queue_t *queue, int threads, int (*callback)(void *, void *), void *reader_data);
int db_queue_is_empty(db_queue_t *queue);
int db_queue_lock(db_queue_t *queue);
int db_queue_unlock(db_queue_t *queue);
int db_queue_trylock(db_queue_t *queue);
db_queue_entry_t *db_queue_add(db_queue_t *queue, void *data);
void db_queue_delete(db_queue_t *queue, db_queue_entry_t *entry);
int db_queue_entry_lock(db_queue_entry_t *entry);
int db_queue_entry_unlock(db_queue_entry_t *entry);
int db_queue_entry_trylock(db_queue_entry_t *entry);
void db_queue_destroy(db_queue_t *queue);


#endif /* _LIBPRELUDEDB_QUEUE_H */