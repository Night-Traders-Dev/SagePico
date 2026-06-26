/* Shim for dlfcn.h - not available on baremetal RP2350 */
#ifndef SHIM_DLFCN_H
#define SHIM_DLFCN_H

#define RTLD_NOW 0

static inline void* dlopen(const char* path, int mode) { (void)path; (void)mode; return NULL; }
static inline void  dlclose(void* handle) { (void)handle; }
static inline void* dlsym(void* handle, const char* name) { (void)handle; (void)name; return NULL; }

#endif
