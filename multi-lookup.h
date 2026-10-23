 
#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H
 
#include <pthread.h>
#include <stdio.h>
 
#include "array.h"
#include "util.h"
 
/* Limits (ARRAY_SIZE and MAX_NAME_LENGTH are defined in array.h) */
#define MAX_INPUT_FILES       100
#define MAX_REQUESTER_THREADS 10
#define MAX_RESOLVER_THREADS  10
#define MAX_IP_LENGTH         INET6_ADDRSTRLEN
 
#define NOT_RESOLVED "NOT_RESOLVED"
 
/* Everything the worker threads share. Lives on main's stack and is passed
 * to each thread through pthread_create's arg, so no globals are needed. */
typedef struct {
    array_t array;
 
    char **files;            /* input file names (points into argv) */
    int nfiles;
    int next_file;           /* index of next unclaimed file */
    pthread_mutex_t file_lock;
 
    FILE *req_log;
    FILE *res_log;
    pthread_mutex_t req_log_lock;
    pthread_mutex_t res_log_lock;
    pthread_mutex_t stdout_lock;
    pthread_mutex_t stderr_lock;
} shared_t;
 
/* Requester thread: services input files, feeding names to the array. */
void *requester(void *arg);
 
/* Resolver thread: takes names off the array and resolves them. */
void *resolver(void *arg);
 
#endif