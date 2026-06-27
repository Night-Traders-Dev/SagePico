"""SageRTOS tests — validates RTOS kernel operations."""

def run_rtos_tests(t):
    # Task creation
    r = t.cmd("rtos_task(128, 1)")
    t.assert_contains("rtos: task create", r, "0")  # returns task ID 0

    # Task sleep
    r = t.cmd("rtos_sleep(10)")
    t.assert_contains("rtos: sleep returns", r, ">>>")

    # Task yield
    r = t.cmd("rtos_yield()")
    t.assert_contains("rtos: yield returns", r, ">>>")

    # Task ID
    r = t.cmd("rtos_id()")
    t.assert_contains("rtos: task id", r, "0")

    # Create second task
    r = t.cmd("rtos_task(128, 2)")
    t.assert_contains("rtos: task 2", r, "1")

    # Multiple task creates
    for i in range(3, 6):
        r = t.cmd(f"rtos_task(128, {i})")
        t.assert_contains(f"rtos: task {i}", r, str(i-1))

    print(f"\n  SageRTOS: {t.passed} passed, {t.failed} failed")
