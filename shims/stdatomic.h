/* Shim for stdatomic.h - C11 atomics shim for baremetal */
#ifndef SHIM_STDATOMIC_H
#define SHIM_STDATOMIC_H

/* Use GCC built-in atomics */
#define atomic_load(p)      __atomic_load_n((p), __ATOMIC_SEQ_CST)
#define atomic_store(p, v)  __atomic_store_n((p), (v), __ATOMIC_SEQ_CST)
#define atomic_fetch_add(p, v) __atomic_fetch_add((p), (v), __ATOMIC_SEQ_CST)

#endif
