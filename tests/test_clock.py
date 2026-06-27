"""Clock tests — clock_get, clock_set."""

def run_clock_tests(t):
    # clock_get
    r = t.cmd("clock_get()")
    t.assert_contains("clock_get returns date", r, "2026-")

    # clock_set
    t.assert_contains("clock_set returns true", t.cmd('clock_set("2026-06-26 20:00:00")'), "true")

    # Verify set worked
    r = t.cmd("clock_get()")
    t.assert_contains("clock_get after set", r, "2026-06-26 20:00:")
