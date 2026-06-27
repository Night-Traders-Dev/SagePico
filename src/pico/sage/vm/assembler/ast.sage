## ast.sage — AST node types for the RISC-V assembler

class ASTNode:
    proc init(self):
        self.line = 0

class InstructionNode(ASTNode):
    proc init(self, mnemonic, operands):
        self.mnemonic = mnemonic
        self.operands = operands  # list of OperandNode

class LabelNode(ASTNode):
    proc init(self, name):
        self.name = name

class DirectiveNode(ASTNode):
    proc init(self, name, args):
        self.name = name
        self.args = args  # list of strings/ints

class OperandNode(ASTNode):
    proc init(self, kind, value):
        self.kind = kind  # "register", "immediate", "label", "string"
        self.value = value

class SectionNode(ASTNode):
    proc init(self, name):
        self.name = name

class ProgramNode(ASTNode):
    proc init(self):
        self.nodes = []  # list of ASTNodes (instructions, labels, directives)

    proc add(self, node):
        push(self.nodes, node)
