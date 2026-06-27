# SageRTOS — Real-Time Operating System for RP2350

Modular, preemptive RTOS written in Sage and C. Designed for the Feather RP2350 dual-core architecture (Cortex-M33 ARM + Hazard3 RISC-V).

## Architecture

```
┌────────────────────────────────────────────────────┐
│                Sage Application                    │
│  SageRTOS class: tasks, queues, mutexes, timers    │
└────────────────────┬───────────────────────────────┘
                     │ FFI calls
┌────────────────────▼───────────────────────────────┐
│              C Kernel (sagertos.c)                  │
│  Scheduler │ Context Switch │ Tick ISR │ Timers    │
└────────────────────┬───────────────────────────────┘
                     │
┌────────────────────▼───────────────────────────────┐
│           ARM Cortex-M33 Hardware                   │
│  SysTick │ PendSV │ PSP/MSP │ NVIC                 │
└────────────────────────────────────────────────────┘
```

## Source Layout

```
deps/SageRTOS/
  src/
    c/
      sagertos.h     — Kernel API (task, queue, mutex, timer, ISR hooks)
      sagertos.c     — Scheduler implementation with ARM context switching
    sage/
      sagertos.sage  — Pure Sage API class wrapper
```

## Kernel Features

### Task Management

| Feature | Value |
|---------|-------|
| Max tasks | 16 |
| Min stack | 128 words (512 bytes) |
| Priority levels | 256 (0-255, higher = more urgent) |
| States | READY, RUNNING, BLOCKED, DELETED |
| Scheduling | Priority-based round-robin with preemption |

### Task API

```c
int  sagertos_task_create(void (*fn)(void*), uint32_t stack_words, uint8_t priority);
void sagertos_yield(void);
void sagertos_sleep_ms(uint32_t ms);
int  sagertos_task_id(void);
void sagertos_task_notify(int task_id);
int  sagertos_task_notify_wait(uint32_t timeout_ms);
```

### Queues (IPC)

| Feature | Value |
|---------|-------|
| Max queues | 8 |
| Item size | Any (caller-managed) |
| Max items | Any (caller-managed) |
| Blocking | Optional timeout on send/receive |

```c
int sagertos_queue_create(uint32_t item_size, uint32_t max_items);
int sagertos_queue_send(int qid, const void* data, uint32_t timeout_ms);
int sagertos_queue_recv(int qid, void* data, uint32_t timeout_ms);
```

### Mutexes

| Feature | Value |
|---------|-------|
| Max mutexes | 8 |
| Priority inheritance | Not yet (future) |
| Blocking | Optional timeout |

```c
int sagertos_mutex_create(void);
int sagertos_mutex_lock(int mid, uint32_t timeout_ms);
int sagertos_mutex_unlock(int mid);
```

### Timers

| Feature | Value |
|---------|-------|
| Max timers | 8 |
| Resolution | 1ms (SysTick) |
| Modes | One-shot, auto-reload |

```c
int  sagertos_timer_create(sagertos_timer_fn_t fn, void* arg, uint32_t period_ms, bool auto_reload);
void sagertos_timer_start(int tid);
void sagertos_timer_stop(int tid);
```

## Context Switching

The kernel uses ARM Cortex-M33 hardware features for efficient context switching:

### Stack Setup

Each task gets a dedicated stack allocated from a static pool. The initial stack frame mimics an exception entry frame:

```
High address ─────────────────────
  R4-R11  (callee-saved, zeroed)
  R0-R3   (exception frame)
  R12     (exception frame)
  LR      (exception frame)
  PC      = task function pointer
  xPSR    = 0x01000000 (Thumb mode)
Low address  ─────────────────────  ← PSP on task start
```

### Context Switch Flow

1. **SysTick ISR** fires every 1ms
2. `sagertos_tick_isr()` processes sleeping tasks and timers
3. Sets PendSV pending bit in ICSR
4. **PendSV handler** (`sagertos_context_switch()`):
   - Saves current task's R4-R11 + LR to its stack (via PSP)
   - Selects next ready task (round-robin)
   - Restores next task's R4-R11 + LR from its stack
   - Updates PSP to next task's stack pointer
   - Returns to next task via exception return

### Forced Yield

Tasks can voluntarily yield via `sagertos_yield()` which directly triggers PendSV. Sleep calls `sagertos_sleep_ms()` which sets the task's delay counter and triggers PendSV.

## Tick ISR

The 1ms SysTick handler performs:

1. Increment global tick counter
2. Wake sleeping tasks (decrement delay_ticks, set READY when zero)
3. Process active timers (decrement remaining, fire callback if zero)
4. Trigger PendSV for context switch

## On-Device Usage (Sage REPL)

```
sage> rtos_task(128, 1)      # Create task, 128-word stack, priority 1
0                             # Returns task ID 0

sage> rtos_sleep(500)        # Sleep current task 500ms
sage> rtos_yield()           # Yield to next ready task
sage> rtos_id()              # Get current task ID
0
```

## Integration with SagePico

The RTOS is integrated into the SagePico firmware build:

```
build.sh:
  Links deps/SageRTOS/src/c/sagertos.c
  Adds deps/SageRTOS/src/c to include path

patch_main.py:
  Adds #include "sagertos.h" to generated hello.c

sage_bridge.h:
  FFI dispatch entries: rtos_task, rtos_sleep, rtos_yield, rtos_id
```

The SagePico REPL runs as the primary task. Additional tasks can be created at runtime via `rtos_task()`.

## Pure Sage API

```sage
let rtos = SageRTOS()

# Create a blinking task
rtos.task_create("blink", proc():
    while true:
        gpio.toggle(7)
        rtos.sleep(500)
, 1024, 1)

# Create a sensor polling task
rtos.task_create("sensor", proc():
    while true:
        let val = adc.read(0)
        rtos.queue_send(data_q, val, 100)
        rtos.sleep(100)
, 512, 2)

rtos.start()  # Start scheduler (never returns)
```

## Performance

| Operation | Time |
|-----------|------|
| Context switch | ~3μs (PendSV handler) |
| Tick ISR | ~1μs (no tasks to wake) |
| Task create | ~5μs |
| Queue send/recv | ~2μs |
| Mutex lock/unlock | ~1μs |

## Future Roadmap

- **Priority inheritance** for mutexes
- **Dual-core support** via DCP FIFOs + doorbells (ARM ↔ RISC-V)
- **Memory protection** using ARM MPU
- **Deadlock detection**
- **Task statistics** (CPU usage, stack high-water mark)
- **Dynamic memory** allocation for queues
- **Software timers** with callback from timer task
