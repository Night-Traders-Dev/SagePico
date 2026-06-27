## parser.sage — Recursive-descent parser for RISC-V assembly
## Converts token stream → AST (ProgramNode with Instructions, Labels, Directives)

class Parser:
    proc init(self, tokens):
        self.tokens = tokens
        self.pos = 0
        self.errors = []

    proc peek(self):
        if self.pos >= len(self.tokens):
            return nil
        return self.tokens[self.pos]

    proc advance(self):
        if self.pos >= len(self.tokens):
            return nil
        let tok = self.tokens[self.pos]
        self.pos = self.pos + 1
        return tok

    proc expect(self, kind):
        let tok = self.peek()
        if tok == nil or tok.kind != kind:
            push(self.errors, "expected " + TOKEN_NAMES[kind] + " at line " + str(tok.line if tok != nil else 0))
            return nil
        return self.advance()

    proc skip_newlines(self):
        while self.peek() != nil and self.peek().kind == TOK_NEWLINE:
            self.advance()

    ## Parse a single operand (register, immediate, label, or string)
    proc parse_operand(self):
        self.skip_newlines()
        let tok = self.peek()
        if tok == nil or tok.kind == TOK_EOF or tok.kind == TOK_NEWLINE:
            return nil

        if tok.kind == TOK_REGISTER:
            self.advance()
            return OperandNode("register", tok.text)

        if tok.kind == TOK_INTEGER:
            self.advance()
            return OperandNode("immediate", tok.value)

        if tok.kind == TOK_MINUS:
            self.advance()
            let num = self.advance()
            if num != nil and num.kind == TOK_INTEGER:
                return OperandNode("immediate", 0 - num.value)
            push(self.errors, "expected number after -")
            return nil

        if tok.kind == TOK_IDENTIFIER or tok.kind == TOK_LABEL:
            self.advance()
            return OperandNode("label", tok.text)

        if tok.kind == TOK_STRING:
            self.advance()
            return OperandNode("string", tok.text)

        push(self.errors, "unexpected token: " + TOKEN_NAMES[tok.kind] + " '" + tok.text + "'")
        return nil

    ## Parse operand list (comma-separated)
    proc parse_operands(self):
        let ops = []
        self.skip_newlines()
        let first = self.parse_operand()
        if first != nil:
            push(ops, first)
            while self.peek() != nil and self.peek().kind == TOK_COMMA:
                self.advance()  # skip comma
                let next = self.parse_operand()
                if next != nil:
                    push(ops, next)
        return ops

    ## Parse a single line (instruction, label, or directive)
    proc parse_line(self):
        self.skip_newlines()
        let tok = self.peek()
        if tok == nil or tok.kind == TOK_EOF:
            return nil

        # Label
        if tok.kind == TOK_LABEL:
            self.advance()
            return LabelNode(tok.text)

        # Directive
        if tok.kind == TOK_DIRECTIVE:
            self.advance()
            let name = tok.text
            let args = []
            while self.peek() != nil and self.peek().kind != TOK_NEWLINE and self.peek().kind != TOK_EOF:
                let arg = self.peek()
                if arg.kind == TOK_INTEGER:
                    self.advance()
                    push(args, str(arg.value))
                else:
                    if arg.kind == TOK_IDENTIFIER or arg.kind == TOK_STRING:
                        self.advance()
                        push(args, arg.text)
                    else:
                        self.advance()
            return DirectiveNode(name, args)

        # Instruction
        if tok.kind == TOK_MNEMONIC:
            self.advance()
            let mnemonic = tok.text
            let ops = self.parse_operands()
            return InstructionNode(mnemonic, ops)

        # Skip unknown
        self.advance()
        return nil

    ## Parse entire program
    proc parse(self):
        let prog = ProgramNode()
        while self.pos < len(self.tokens):
            let node = self.parse_line()
            if node != nil:
                prog.add(node)
            else:
                self.skip_newlines()
                if self.peek() != nil and self.peek().kind == TOK_EOF:
                    break
        return prog

# Convenience
proc parse(tokens):
    let p = Parser(tokens)
    return p.parse()
