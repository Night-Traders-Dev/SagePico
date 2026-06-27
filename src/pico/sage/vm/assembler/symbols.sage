## symbols.sage — Symbol table with labels, forward references, scopes

class Symbol:
    proc init(self, name, addr, defined):
        self.name = name
        self.addr = addr     # address/offset
        self.defined = defined  # true = defined, false = forward reference
        self.refs = []       # list of instruction indices that reference this symbol

class SymbolTable:
    proc init(self):
        self.symbols = {}    # name -> Symbol
        self.errors = []

    proc define(self, name, addr):
        if dict_has(self.symbols, name):
            push(self.errors, "duplicate symbol: " + name)
            return
        let sym = Symbol(name, addr, true)
        self.symbols[name] = sym
        # Resolve any forward references
        for ref in sym.refs:
            ref.resolved = true

    proc reference(self, name, node):
        if dict_has(self.symbols, name):
            let sym = self.symbols[name]
            if sym.defined:
                return sym.addr
            push(sym.refs, node)
        else:
            let sym = Symbol(name, 0, false)
            push(sym.refs, node)
            self.symbols[name] = sym
        return 0  # unresolved — patch later

    proc resolve_all(self):
        for name in dict_keys(self.symbols):
            let sym = self.symbols[name]
            if not sym.defined:
                push(self.errors, "undefined symbol: " + name)
