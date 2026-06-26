/* Shim for clock/time functions on RP2350 baremetal */
#ifndef SHIM_BAREMETAL_TIME_H
#define SHIM_BAREMETAL_TIME_H

#include <time.h>

/* nanosleep stub - pico provides sleep_ms instead */
struct timespec;
static inline int nanosleep(const struct timespec* req, struct timespec* rem) {
    (void)req; (void)rem;
    return 0;
}

/* clock() stub - use pico time functions instead */
#include <sys/time.h>

#endif
