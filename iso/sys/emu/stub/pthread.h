/* Rudolf OS: single-threaded stand-ins for the few pthread calls DeSmuME's helpers make (WebAssembly has
   no threads here). Thread creation fails, so DeSmuME runs everything on one thread, as in its wasm port. */
#ifndef RUDOLF_PTHREAD_STUB_H
#define RUDOLF_PTHREAD_STUB_H
#include <errno.h>
#include <time.h>
#include <sched.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef SCHED_RR
struct sched_param { int sched_priority; };
#define SCHED_RR 2
#define SCHED_FIFO 1
#define SCHED_OTHER 0
#endif
typedef unsigned long pthread_t;
typedef struct { int x; } pthread_attr_t;
typedef struct { int x; } pthread_mutex_t;
typedef struct { int x; } pthread_mutexattr_t;
typedef struct { int x; } pthread_cond_t;
typedef struct { int x; } pthread_condattr_t;
typedef int pthread_once_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}
#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1
static inline int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *p) { (void)t; (void)a; (void)f; (void)p; return EAGAIN; }
static inline int pthread_join(pthread_t t, void **r) { (void)t; (void)r; return 0; }
static inline int pthread_detach(pthread_t t) { (void)t; return 0; }
static inline pthread_t pthread_self(void) { return 1; }
static inline int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
static inline void pthread_exit(void *r) { (void)r; }
static inline int pthread_attr_init(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_attr_setdetachstate(pthread_attr_t *a, int s) { (void)a; (void)s; return 0; }
static inline int pthread_attr_setstacksize(pthread_attr_t *a, size_t s) { (void)a; (void)s; return 0; }
static inline int pthread_attr_setschedpolicy(pthread_attr_t *a, int p) { (void)a; (void)p; return 0; }
static inline int pthread_attr_setschedparam(pthread_attr_t *a, const struct sched_param *p) { (void)a; (void)p; return 0; }
static inline int pthread_attr_setinheritsched(pthread_attr_t *a, int p) { (void)a; (void)p; return 0; }
static inline int pthread_setname_np(pthread_t t, const char *n) { (void)t; (void)n; return 0; }
static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)m; (void)a; return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_trylock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { (void)m; return 0; }
static inline int pthread_mutexattr_init(pthread_mutexattr_t *a) { (void)a; return 0; }
static inline int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t) { (void)a; (void)t; return 0; }
static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }
static inline int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) { (void)c; (void)a; return 0; }
static inline int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { (void)c; (void)m; return 0; }
static inline int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t) { (void)c; (void)m; (void)t; return ETIMEDOUT; }
static inline int pthread_cond_signal(pthread_cond_t *c) { (void)c; return 0; }
static inline int pthread_cond_broadcast(pthread_cond_t *c) { (void)c; return 0; }
static inline int pthread_once(pthread_once_t *o, void (*f)(void)) { if (!*o) { *o = 1; f(); } return 0; }
#ifdef __cplusplus
}
#endif
#endif
