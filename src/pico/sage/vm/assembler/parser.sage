## parser.sage — Recursive-descent parser for RISC-V assembly
## Converts token stream → AST (ProgramNode with Instructions, Labels, Directives)
## Handles: registers, immediates, labels, strings, offset(rs) memory syntax

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

    ## Parse a single operand — handles offset(rs) for load/store instructions
    proc parse_operand(self):
        self.skip_newlines()
        let tok = self.peek()
        if tok == nil or tok.kind == TOK_EOF or tok.kind == TOK_NEWLINE:
            return nil

        # offset(rs) addressing: integer followed by (register)
        if tok.kind == TOK_INTEGER or tok.kind == TOK_MINUS:
            let sign_val = 1
            if tok.kind == TOK_MINUS:
                self.advance()
                sign_val = -1
                tok = self.peek()

            if tok.kind == TOK_INTEGER:
                let imm = tok.value * sign_val
                self.advance()
                # Look ahead for ( ... register ... )
                let next = self.peek()
                if next != nil and next.kind == TOK_LPAREN:
                    self.advance()  # skip (
                    let base = self.advance()
                    if base != nil and base.kind == TOK_REGISTER:
                        self.expect(TOK_RPAREN)
                        # Return as three operands: base, offset_imm, rs — for loads: rd, base, offset
                        return OperandNode("immediate", imm)
                    push(self.errors, "expected register in offset(rs)")
                    return nil
                return OperandNode("immediate", imm)

        if tok.kind == TOK_REGISTER:
            self.advance()
            return OperandNode("register", tok.text)

        if tok.kind == TOK_IDENTIFIER or tok.kind == TOK_LABEL:
            self.advance()
            return OperandNode("label", tok.text)

        if tok.kind == TOK_STRING:
            self.advance()
            return OperandNode("string", tok.text)

        # Handle bare (rs) for jr/jalr-style indirect
        if tok.kind == TOK_LPAREN:
            self.advance()
            let base = self.advance()
            if base != nil and base.kind == TOK_REGISTER:
                self.expect(TOK_RPAREN)
                return OperandNode("register", base.text)
            push(self.errors, "expected register after (")
            return nil

        push(self.errors, "unexpected token: " + TOKEN_NAMES[tok.kind] + " '" + tok.text + "'")
        return nil

    ## Parse operand list (comma-separated), with special handling for load/store memory syntax
    proc parse_operands(self):
        let ops = []
        self.skip_newlines()

        let first = self.peek()
        if first == nil or first.kind == TOK_EOF or first.kind == TOK_NEWLINE:
            return ops

        # Loads/stores: rd, offset(rs) → parse as [rd, rs, offset]
        # We peek ahead to detect the offset(rs) pattern
        let saved = self.pos
        let rd = self.parse_operand()
        if rd != nil and rd.kind == "register":
            self.skip_newlines()
            if self.peek() != nil and self.peek().kind == TOK_COMMA:
                self.advance()  # skip comma
                self.skip_newlines()
                # Check for offset(rs) at position 2
                let tok2 = self.peek()
                if tok2 != nil and tok2.kind == TOK_INTEGER:
                    let imm_tok = self.advance()
                    let lparen = self.peek()
                    if lparen != nil and lparen.kind == TOK_LPAREN:
                        # It's offset(rs) — flatten to [rd, rs, offset]
                        self.advance()  # skip (
                        let base = self.advance()
                        if base != nil and base.kind == TOK_REGISTER:
                            self.expect(TOK_RPAREN)
                            push(ops, rd)
                            push(ops, OperandNode("register", base.text))
                            push(ops, OperandNode("immediate", imm_tok.value))
                            return ops
                    # Not offset(rs), just offset — put things back
                    self.pos = saved
                # Normal comma-separated operands
                self.pos = saved
            else:
                self.pos = saved

        # Default: flat comma-separated operands
        let first_op = self.parse_operand()
        if first_op != nil:
            push(ops, first_op)
            while self.peek() != nil and self.peek().kind == TOK_COMMA:
                self.advance()
                self.skip_newlines()
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
