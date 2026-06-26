/* Stub implementations for baremetal (pthread, nanosleep) */
#define _POSIX_THREADS 1
#define _POSIX_TIMERS 1
#include <pthread.h>
#include <time.h>

int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) {
    (void)a; if (m) *m = 0; return 0;
}
int pthread_mutex_lock(pthread_mutex_t* m)       { (void)m; return 0; }
int pthread_mutex_unlock(pthread_mutex_t* m)     { (void)m; return 0; }
int pthread_mutex_destroy(pthread_mutex_t* m)    { (void)m; return 0; }
int pthread_create(pthread_t* t, const pthread_attr_t* a,
                   void*(*fn)(void*), void* arg) {
    (void)t; (void)a; (void)fn; (void)arg; return -1;
}
int pthread_join(pthread_t t, void** ret)        { (void)t; (void)ret; return -1; }
pthread_t pthread_self(void)                     { return 0; }

int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)req; (void)rem; return 0;
}
