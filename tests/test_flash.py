"""Flash storage tests — save, load, delete, list, persistence."""

def run_flash_tests(t):
    # Save a value
    t.assert_contains("flash_save", t.cmd('flash_save("test_key", "hello flash")'), ">>>")

    # Load it back
    r = t.cmd('flash_load("test_key")')
    t.assert_contains("flash_load", r, "hello flash")

    # Delete
    t.cmd('flash_del("test_key")')

    # Verify deleted
    r = t.cmd('flash_load("test_key")')
    t.assert_contains("flash_load deleted is nil", r, "nil")

    # flash_keys
    t.cmd('flash_save("k1", "v1")')
    t.cmd('flash_save("k2", "v2")')
    r = t.cmd("flash_keys()")
    t.assert_contains("flash_keys has k1", r, "k1")
    t.assert_contains("flash_keys has k2", r, "k2")
    t.cmd('flash_del("k1")')
    t.cmd('flash_del("k2")')

    # Variable persistence (let + quit doesn't work in test harness)
    t.cmd("let persist_var = 999")
    r = t.cmd("persist_var")
    t.assert_contains("var persist", r, "999")
