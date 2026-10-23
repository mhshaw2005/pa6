#ifndef ARRAY_H
#define ARRAY_H
 
#include <pthread.h>
 
#define ARRAY_SIZE      8
#define MAX_NAME_LENGTH 255   /* hostname size including the null terminator */
 
typedef struct {
    char items[ARRAY_SIZE][MAX_NAME_LENGTH];  /* single contiguous block */
    int head;                /* next slot to remove from */
    int tail;                /* next slot to insert into */
    int count;               /* number of items currently stored */
    int closed;              /* set once no more items will ever be added */
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} array_t;
 
/* Returns 0 on success, -1 on failure. */
int array_init(array_t *a);
 
/* Releases the array's synchronization primitives. */
void array_destroy(array_t *a);
 
/* Copies name onto the array; blocks while the array is full. */
void array_put(array_t *a, const char *name);
 
int array_get(array_t *a, char *out);

void array_close(array_t *a);

#endif