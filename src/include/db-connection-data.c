#include <stdlib.h>

#include "db-connection.h"
#include "db-connection-data.h"
#include "sql-connection-data.h"

struct prelude_db_connection_data {
	prelude_db_type type;
	void *data;
}


prelude_db_connection_data_t *prelude_db_connection_data_new(prelude_db_type_t type, void *data)
{
	prelude_db_connection_data_t *d;

        d = calloc(1, sizeof(*d));
        if ( ! d ) {
                log(LOG_ERR, "memory exhausted.\n");
                return NULL;
        }
        
        d->type = type;
        d->data = data;
        
        return d;
}



prelude_db_type prelude_db_connection_data_get_type(prelude_db_connection_data_t *data)
{
	return data ? data->type;
}




void *prelude_db_connection_data_get(prelude_db_connection_data_t *data)
{
	return data ? data->data;
}




void prelude_db_connection_data_destroy(prelude_db_connection_data_t *data)
{
	if ( ! data )
		return ;
	
	switch ( data->type ) {
		case prelude_db_type_sql:
			data->data ? prelude_sql_connection_data_destroy(data->data);
			break;
		default:
			log(LOG_ERR, "unknown database type %d\n", data->type);
			return ;
	}
}


