/* Stub implementations for baremetal RP2350 (no OS) */

#include <stddef.h>

/* pthread stubs - single-threaded baremetal has no pthread */
typedef int pthread_t;
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef int pthread_cond_t;
typedef int pthread_condattr_t;
typedef int pthread_rwlock_t;
typedef int pthread_rwlockattr_t;
typedef int pthread_attr_t;

#define PTHREAD_MUTEX_INITIALIZER 0

static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) {
    (void)a; if (m) *m = 0; return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t* m) {
    (void)m; return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
    (void)m; return 0;
}
static inline int pthread_mutex_destroy(pthread_mutex_t* m) {
    (void)m; return 0;
}
static inline int pthread_mutex_trylock(pthread_mutex_t* m) {
    (void)m; return 0;
}
static inline int pthread_create(pthread_t* t, const pthread_attr_t* a,
                                  void* (*fn)(void*), void* arg) {
    (void)t; (void)a; (void)fn; (void)arg; return -1;
}
static inline int pthread_join(pthread_t t, void** ret) {
    (void)t; (void)ret; return -1;
}
static inline pthread_t pthread_self(void) { return 0; }
static inline int pthread_cond_init(pthread_cond_t* c, const pthread_condattr_t* a) {
    (void)c; (void)a; return 0;
}
static inline int pthread_cond_destroy(pthread_cond_t* c) {
    (void)c; return 0;
}
static inline int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m) {
    (void)c; (void)m; return 0;
}
static inline int pthread_cond_signal(pthread_cond_t* c) {
    (void)c; return 0;
}
static inline int pthread_cond_broadcast(pthread_cond_t* c) {
    (void)c; return 0;
}
static inline int pthread_rwlock_init(pthread_rwlock_t* r, const pthread_rwlockattr_t* a) {
    (void)r; (void)a; return 0;
}
static inline int pthread_rwlock_destroy(pthread_rwlock_t* r) {
    (void)r; return 0;
}
static inline int pthread_rwlock_rdlock(pthread_rwlock_t* r) {
    (void)r; return 0;
}
static inline int pthread_rwlock_wrlock(pthread_rwlock_t* r) {
    (void)r; return 0;
}
static inline int pthread_rwlock_unlock(pthread_rwlock_t* r) {
    (void)r; return 0;
}
static inline int pthread_rwlock_tryrdlock(pthread_rwlock_t* r) {
    (void)r; return 0;
}
static inline int pthread_rwlock_trywrlock(pthread_rwlock_t* r) {
    (void)r; return 0;
}

/* nanosleep stub */
struct timespec;
static inline int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)req; (void)rem; return 0;
}

/* dlopen/dlsym stubs (already in shims/dlfcn.h but re-provided for safety) */
#define RTLD_NOW 0
static inline void* dlopen(const char* p, int m) { (void)p; (void)m; return NULL; }
static inline void  dlclose(void* h) { (void)h; }
static inline void* dlsym(void* h, const char* n) { (void)h; (void)n; return NULL; }
