## assembler.sage — Main assembler entry point
## Ties together lexer, parser, and encoder into a single pipeline.

# Full assembler pipeline: source → tokens → AST → machine code
proc assemble(source, filename):
    let tokens = lex(source, filename)
    let prog = parse(tokens)
    let enc = RV32IEncoder()
    let code = enc.encode_program(prog)
    return dict("code":code, "errors":enc.errors, "symbols":enc.symbols, "ast":prog)
