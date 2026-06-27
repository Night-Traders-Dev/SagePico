"""REPL expression tests — arithmetic, variables, comparisons, strings."""

def run_repl_tests(t):
    # Arithmetic
    t.assert_contains("1+1", t.cmd("1+1"), "2")
    t.assert_contains("2+3", t.cmd("2+3"), "5")
    t.assert_contains("10-3", t.cmd("10-3"), "7")
    t.assert_contains("4*5", t.cmd("4*5"), "20")
    t.assert_contains("10/3", t.cmd("10/3"), "3.333")
    t.assert_contains("10%3", t.cmd("10%3"), "1")

    # Negative numbers
    t.assert_contains("neg: -5+3", t.cmd("-5+3"), "-2")

    # Comparisons
    t.assert_contains("5==5", t.cmd("5==5"), "true")
    t.assert_contains("5!=3", t.cmd("5!=3"), "true")
    t.assert_contains("5<10", t.cmd("5<10"), "true")
    t.assert_contains("10>5", t.cmd("10>5"), "true")
    t.assert_contains("5>=5", t.cmd("5>=5"), "true")
    t.assert_contains("3<=5", t.cmd("3<=5"), "true")

    # Strings
    t.assert_contains("string concat", t.cmd('"hello" + " world"'), "hello world")
    t.assert_contains("string with spaces", t.cmd('"a b c"'), "a b c")

    # Boolean literals
    t.assert_contains("true literal", t.cmd("true"), "true")
    t.assert_contains("false literal", t.cmd("false"), "false")
    t.assert_contains("nil literal", t.cmd("nil"), "nil")

    # Variables
    t.cmd("let x = 42")
    t.assert_contains("var lookup", t.cmd("x"), "42")
    t.assert_contains("var in expr", t.cmd("x * 2"), "84")
    t.cmd("let y = 10")
    t.assert_contains("var + var", t.cmd("x + y"), "52")

    # Arrays
    t.assert_contains("array literal", t.cmd("[1, 2, 3]"), "[1, 2, 3]")

    # Parenthesized
    t.assert_contains("paren expr", t.cmd("(2+3)*4"), "20")
