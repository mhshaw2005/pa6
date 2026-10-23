#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
 
#include "multi-lookup.h"
 
static double elapsed(const struct timeval *start)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)(now.tv_sec - start->tv_sec) +
           (double)(now.tv_usec - start->tv_usec) / 1e6;
}
 
static void log_err(shared_t *s, const char *fmt, const char *arg)
{
    pthread_mutex_lock(&s->stderr_lock);
    fprintf(stderr, fmt, arg);
    pthread_mutex_unlock(&s->stderr_lock);
}
 
void *requester(void *arg)
{
    shared_t *s = arg;
    struct timeval start;
    int serviced = 0;
    char line[MAX_NAME_LENGTH + 2];
 
    gettimeofday(&start, NULL);
 
    for (;;) {
        /* claim the next unserviced file */
        pthread_mutex_lock(&s->file_lock);
        int idx = (s->next_file < s->nfiles) ? s->next_file++ : -1;
        pthread_mutex_unlock(&s->file_lock);
        if (idx < 0)
            break;
 
        FILE *fp = fopen(s->files[idx], "r");
        if (!fp) {
            log_err(s, "invalid file %s\n", s->files[idx]);
            continue;
        }
 
        while (fgets(line, sizeof(line), fp)) {
            size_t len = strlen(line);
            int has_nl = (len > 0 && line[len - 1] == '\n');
 
            if (has_nl) {
                line[--len] = '\0';
                if (len > 0 && line[len - 1] == '\r')
                    line[--len] = '\0';
            } else if (len >= MAX_NAME_LENGTH && !feof(fp)) {
                /* name too long: report, discard rest of the line */
                int c;
                log_err(s, "hostname too long in file %s, skipping\n",
                        s->files[idx]);
                while ((c = fgetc(fp)) != '\n' && c != EOF)
                    ;
                continue;
            }
            if (len == 0)
                continue;
 
            array_put(&s->array, line);
 
            pthread_mutex_lock(&s->req_log_lock);
            fprintf(s->req_log, "%s\n", line);
            pthread_mutex_unlock(&s->req_log_lock);
        }
        if (ferror(fp))
            log_err(s, "error reading file %s\n", s->files[idx]);
        fclose(fp);
        serviced++;
    }
 
    pthread_mutex_lock(&s->stdout_lock);
    printf("thread %lx serviced %d files in %f seconds\n",
           (unsigned long)pthread_self(), serviced, elapsed(&start));
    pthread_mutex_unlock(&s->stdout_lock);
    return NULL;
}
 
void *resolver(void *arg)
{
    shared_t *s = arg;
    struct timeval start;
    int resolved = 0;
    char name[MAX_NAME_LENGTH];
    char ip[MAX_IP_LENGTH];
 
    gettimeofday(&start, NULL);
 
    while (array_get(&s->array, name) == 0) {
        const char *result;
        if (dnslookup(name, ip, sizeof(ip)) == UTIL_SUCCESS) {
            result = ip;
            resolved++;
        } else {
            result = NOT_RESOLVED;
        }
        pthread_mutex_lock(&s->res_log_lock);
        fprintf(s->res_log, "%s, %s\n", name, result);
        pthread_mutex_unlock(&s->res_log_lock);
    }
 
    pthread_mutex_lock(&s->stdout_lock);
    printf("thread %lx resolved %d hosts in %f seconds\n",
           (unsigned long)pthread_self(), resolved, elapsed(&start));
    pthread_mutex_unlock(&s->stdout_lock);
    return NULL;
}
 
static int parse_count(const char *str, int max, const char *what)
{
    char *end;
    errno = 0;
    long v = strtol(str, &end, 10);
    if (errno != 0 || *end != '\0' || end == str || v < 1) {
        fprintf(stderr, "Error: invalid number of %s: %s\n", what, str);
        return -1;
    }
    if (v > max) {
        fprintf(stderr, "Error: number of %s (%ld) exceeds the maximum of %d\n",
                what, v, max);
        return -1;
    }
    return (int)v;
}
 
int main(int argc, char *argv[])
{
    struct timeval start;
    gettimeofday(&start, NULL);
 
    if (argc < 5) {
        printf("Usage: %s <# requester> <# resolver> <requester log> "
               "<resolver log> [ <data file> ... ]\n", argv[0]);
        return EXIT_FAILURE;
    }
 
    int nreq = parse_count(argv[1], MAX_REQUESTER_THREADS, "requester threads");
    int nres = parse_count(argv[2], MAX_RESOLVER_THREADS, "resolver threads");
    if (nreq < 0 || nres < 0)
        return EXIT_FAILURE;
 
    int nfiles = argc - 5;
    if (nfiles > MAX_INPUT_FILES) {
        fprintf(stderr, "Error: too many input files (%d); maximum is %d\n",
                nfiles, MAX_INPUT_FILES);
        return EXIT_FAILURE;
    }
 
    shared_t s;
    memset(&s, 0, sizeof(s));
    s.files = &argv[5];
    s.nfiles = nfiles;
 
    s.req_log = fopen(argv[3], "w");
    if (!s.req_log) {
        fprintf(stderr, "Error: cannot open requester log %s: %s\n",
                argv[3], strerror(errno));
        return EXIT_FAILURE;
    }
    s.res_log = fopen(argv[4], "w");
    if (!s.res_log) {
        fprintf(stderr, "Error: cannot open resolver log %s: %s\n",
                argv[4], strerror(errno));
        fclose(s.req_log);
        return EXIT_FAILURE;
    }
 
    int rc = EXIT_SUCCESS;
    int mutexes_ok = 0;
    if (array_init(&s.array) != 0) {
        fprintf(stderr, "Error: cannot initialize shared array\n");
        rc = EXIT_FAILURE;
        goto close_logs;
    }
    if (pthread_mutex_init(&s.file_lock, NULL) ||
        pthread_mutex_init(&s.req_log_lock, NULL) ||
        pthread_mutex_init(&s.res_log_lock, NULL) ||
        pthread_mutex_init(&s.stdout_lock, NULL) ||
        pthread_mutex_init(&s.stderr_lock, NULL)) {
        fprintf(stderr, "Error: cannot initialize mutexes\n");
        rc = EXIT_FAILURE;
        goto destroy_array;
    }
    mutexes_ok = 1;
 
    pthread_t req_tid[MAX_REQUESTER_THREADS];
    pthread_t res_tid[MAX_RESOLVER_THREADS];
    int req_started = 0, res_started = 0;
 
    /* Resolvers first so they are ready to drain the array. */
    for (int i = 0; i < nres; i++) {
        int err = pthread_create(&res_tid[res_started], NULL, resolver, &s);
        if (err) {
            fprintf(stderr, "Error: cannot create resolver thread: %s\n",
                    strerror(err));
            break;
        }
        res_started++;
    }
    for (int i = 0; i < nreq && res_started > 0; i++) {
        int err = pthread_create(&req_tid[req_started], NULL, requester, &s);
        if (err) {
            fprintf(stderr, "Error: cannot create requester thread: %s\n",
                    strerror(err));
            break;
        }
        req_started++;
    }
    if (req_started < nreq || res_started < nres)
        rc = EXIT_FAILURE;  /* ran with fewer threads (or none) than asked */
 
    for (int i = 0; i < req_started; i++)
        pthread_join(req_tid[i], NULL);
 
    /* All names are now on the array (or consumed); let resolvers finish. */
    array_close(&s.array);
 
    for (int i = 0; i < res_started; i++)
        pthread_join(res_tid[i], NULL);
 
    printf("%s: total time is %f seconds\n", argv[0], elapsed(&start));
 
    if (mutexes_ok) {
        pthread_mutex_destroy(&s.file_lock);
        pthread_mutex_destroy(&s.req_log_lock);
        pthread_mutex_destroy(&s.res_log_lock);
        pthread_mutex_destroy(&s.stdout_lock);
        pthread_mutex_destroy(&s.stderr_lock);
    }
destroy_array:
    array_destroy(&s.array);
close_logs:
    if (fclose(s.req_log) != 0) {
        fprintf(stderr, "Error closing requester log: %s\n", strerror(errno));
        rc = EXIT_FAILURE;
    }
    if (fclose(s.res_log) != 0) {
        fprintf(stderr, "Error closing resolver log: %s\n", strerror(errno));
        rc = EXIT_FAILURE;
    }
    return rc;
}