/* Minimal pthread function declarations for baremetal - no TLS */
/* Types are already provided by <sys/types.h> (_pthreadtypes.h) */
#ifndef SHIM_PTHREAD_H
#define SHIM_PTHREAD_H

#include <sys/types.h>  /* provides pthread_t, pthread_mutex_t, etc. */

int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a);
int pthread_mutex_lock(pthread_mutex_t* m);
int pthread_mutex_unlock(pthread_mutex_t* m);
int pthread_mutex_destroy(pthread_mutex_t* m);
int pthread_create(pthread_t* t, const pthread_attr_t* a, void*(*fn)(void*), void* arg);
int pthread_join(pthread_t t, void** ret);
pthread_t pthread_self(void);

/* nanosleep - also needed by Sage runtime */
struct timespec;
int nanosleep(const struct timespec* req, struct timespec* rem);

#endif
