## lexer.sage — Full RISC-V assembly lexer with line/column tracking
## Tokenizes source text into a stream of Token objects.
## Handles: labels, mnemonics, registers, immediates, strings, directives, comments.

class Lexer:
    proc init(self, source, filename):
        self.source = source      # full source text
        self.pos = 0              # current position in source
        self.line = 1             # current line (1-based)
        self.col = 1              # current column (1-based)
        self.filename = filename  # source filename for diagnostics
        self.tokens = []          # token stream
        self.errors = []          # lexer errors

    ## Return the current character (or "" if EOF)
    proc peek(self):
        if self.pos >= len(self.source):
            return ""
        return self.source[self.pos]

    ## Advance one character, return it
    proc advance(self):
        if self.pos >= len(self.source):
            return ""
        let c = self.source[self.pos]
        self.pos = self.pos + 1
        if c == "\n":
            self.line = self.line + 1
            self.col = 1
        else:
            self.col = self.col + 1
        return c

    ## Add a token to the stream
    proc emit(self, kind, text):
        let tok = Token(kind, text, self.line, self.col - len(text))
        push(self.tokens, tok)
        return tok

    ## Add a lexer error
    proc error(self, msg):
        push(self.errors, self.filename + ":" + str(self.line) + ":" + str(self.col) + ": " + msg)

    ## Skip whitespace and comments
    proc skip_whitespace(self):
        while self.pos < len(self.source):
            let c = self.peek()
            if c == " " or c == "\t":
                self.advance()
            else:
                if c == "\n":
                    self.advance()
                    self.emit(TOK_NEWLINE, "\\n")
                else:
                    if c == ";":
                        # Single-line comment
                        while self.pos < len(self.source) and self.peek() != "\n":
                            self.advance()
                    else:
                        if c == "#":
                            # Hash comment
                            while self.pos < len(self.source) and self.peek() != "\n":
                                self.advance()
                        else:
                            return

    ## Read a hex digit
    proc is_hex(self, c):
        return (c >= "0" and c <= "9") or (c >= "a" and c <= "f") or (c >= "A" and c <= "F")

    ## Read a binary digit
    proc is_bin(self, c):
        return c == "0" or c == "1"

    ## Read a number literal (decimal, hex 0x, binary 0b)
    proc read_number(self):
        let start = self.pos
        # Check for hex prefix
        if self.peek() == "0" and self.pos + 1 < len(self.source):
            let next = self.source[self.pos + 1]
            if next == "x" or next == "X":
                self.advance()  # 0
                self.advance()  # x
                while self.pos < len(self.source) and self.is_hex(self.peek()):
                    self.advance()
                let text = self.source[start:self.pos]
                let tok = self.emit(TOK_INTEGER, text)
                tok.value = tonumber(text)
                return tok
            if next == "b" or next == "B":
                self.advance()  # 0
                self.advance()  # b
                while self.pos < len(self.source) and self.is_bin(self.peek()):
                    self.advance()
                let text = self.source[start:self.pos]
                let tok = self.emit(TOK_INTEGER, text)
                # Parse binary
                let val = 0
                let i = 2
                while i < len(text):
                    val = val * 2
                    if text[i] == "1":
                        val = val + 1
                    i = i + 1
                tok.value = val
                return tok

        # Decimal or hex digit-only
        while self.pos < len(self.source):
            let c = self.peek()
            if (c >= "0" and c <= "9") or self.is_hex(c):
                self.advance()
            else:
                break
        let text = self.source[start:self.pos]
        let tok = self.emit(TOK_INTEGER, text)
        tok.value = tonumber(text)
        return tok

    ## Read an identifier or keyword
    proc read_ident(self):
        let start = self.pos
        while self.pos < len(self.source):
            let c = self.peek()
            if (c >= "a" and c <= "z") or (c >= "A" and c <= "Z") or (c >= "0" and c <= "9") or c == "_" or c == ".":
                self.advance()
            else:
                break
        let text = self.source[start:self.pos]

        # Check for label (ends with colon)
        if self.peek() == ":":
            self.advance()  # consume colon
            let tok = self.emit(TOK_LABEL, text)
            return tok

        # Check for directive (starts with dot)
        if startswith(text, "."):
            return self.emit(TOK_DIRECTIVE, text)

        # Check for register
        if is_register(text):
            return self.emit(TOK_REGISTER, text)

        # Check for mnemonic (case-insensitive)
        let lower = ""
        let i = 0
        while i < len(text):
            let c = text[i]
            if c >= "A" and c <= "Z":
                lower = lower + chr(ord(c) + 32)
            else:
                lower = lower + c
            i = i + 1
        if is_mnemonic(lower):
            let tok = self.emit(TOK_MNEMONIC, lower)
            return tok

        # Generic identifier
        return self.emit(TOK_IDENTIFIER, text)

    ## Read a string literal
    proc read_string(self):
        let quote = self.advance()  # consume opening quote
        let start = self.pos
        while self.pos < len(self.source):
            let c = self.peek()
            if c == quote:
                self.advance()  # closing quote
                let text = self.source[start:self.pos - 1]
                let tok = self.emit(TOK_STRING, text)
                return tok
            if c == "\\":
                self.advance()  # escape
            self.advance()
        self.error("unterminated string")
        return nil

    ## Tokenize the entire source
    proc tokenize(self):
        while self.pos < len(self.source):
            self.skip_whitespace()
            if self.pos >= len(self.source):
                break

            let c = self.peek()

            # Numbers
            if c >= "0" and c <= "9":
                self.read_number()
            else:
                # Identifiers
                if (c >= "a" and c <= "z") or (c >= "A" and c <= "Z") or c == "_" or c == ".":
                    self.read_ident()
                else:
                    # Strings
                    if c == "\"" or c == "'":
                        self.read_string()
                    else:
                        # Single-character tokens
                        self.advance()
                        if c == ",":
                            self.emit(TOK_COMMA, ",")
                        else:
                            if c == "(":
                                self.emit(TOK_LPAREN, "(")
                            else:
                                if c == ")":
                                    self.emit(TOK_RPAREN, ")")
                                else:
                                    if c == "[":
                                        self.emit(TOK_LBRACKET, "[")
                                    else:
                                        if c == "]":
                                            self.emit(TOK_RBRACKET, "]")
                                        else:
                                            if c == "+":
                                                self.emit(TOK_PLUS, "+")
                                            else:
                                                if c == "-":
                                                    self.emit(TOK_MINUS, "-")
                                                else:
                                                    if c == "*":
                                                        self.emit(TOK_STAR, "*")
                                                    else:
                                                        if c == "/":
                                                            self.emit(TOK_SLASH, "/")
                                                        else:
                                                            if c == "=":
                                                                self.emit(TOK_EQU, "=")
                                                            else:
                                                                if c == "!":
                                                                    self.emit(TOK_EXCLAM, "!")
                                                                else:
                                                                    if c == "~":
                                                                        self.emit(TOK_TILDE, "~")
                                                                    else:
                                                                        self.error("unexpected character: " + c)

        self.emit(TOK_EOF, "")
        return self.tokens

# Convenience function
proc lex(source, filename):
    let l = Lexer(source, filename)
    return l.tokenize()
