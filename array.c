#include <string.h>
 
#include "array.h"
 
int array_init(array_t *a)
{
    a->head = a->tail = a->count = a->closed = 0;
    if (pthread_mutex_init(&a->lock, NULL) != 0)
        return -1;
    if (pthread_cond_init(&a->not_full, NULL) != 0) {
        pthread_mutex_destroy(&a->lock);
        return -1;
    }
    if (pthread_cond_init(&a->not_empty, NULL) != 0) {
        pthread_cond_destroy(&a->not_full);
        pthread_mutex_destroy(&a->lock);
        return -1;
    }
    return 0;
}
 
void array_destroy(array_t *a)
{
    pthread_cond_destroy(&a->not_empty);
    pthread_cond_destroy(&a->not_full);
    pthread_mutex_destroy(&a->lock);
}
 
void array_put(array_t *a, const char *name)
{
    pthread_mutex_lock(&a->lock);
    while (a->count == ARRAY_SIZE)
        pthread_cond_wait(&a->not_full, &a->lock);
    strncpy(a->items[a->tail], name, MAX_NAME_LENGTH - 1);
    a->items[a->tail][MAX_NAME_LENGTH - 1] = '\0';
    a->tail = (a->tail + 1) % ARRAY_SIZE;
    a->count++;
    pthread_cond_signal(&a->not_empty);
    pthread_mutex_unlock(&a->lock);
}
 
int array_get(array_t *a, char *out)
{
    pthread_mutex_lock(&a->lock);
    while (a->count == 0 && !a->closed)
        pthread_cond_wait(&a->not_empty, &a->lock);
    if (a->count == 0) {            /* closed and drained */
        pthread_mutex_unlock(&a->lock);
        return -1;
    }
    memcpy(out, a->items[a->head], MAX_NAME_LENGTH);
    a->head = (a->head + 1) % ARRAY_SIZE;
    a->count--;
    pthread_cond_signal(&a->not_full);
    pthread_mutex_unlock(&a->lock);
    return 0;
}
 
void array_close(array_t *a)
{
    pthread_mutex_lock(&a->lock);
    a->closed = 1;
    pthread_cond_broadcast(&a->not_empty);
    pthread_mutex_unlock(&a->lock);
}