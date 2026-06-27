## test_assembler.sage — Unit tests for the RISC-V assembler pipeline

print("=== Assembler Pipeline Tests ===")

# Test 1: Basic R-type encoding
let src1 = "
add a0, a1, a2
"
let r1 = assemble(src1, "test1.sage")
let inst1 = r1["code"][0]
assert(inst1 == 0x00C58533, "add a0,a1,a2 = " + hex(inst1))
print("PASS: add a0,a1,a2 = 0x" + hex(inst1))

# Test 2: SUB encoding
let src2 = "
sub a0, a1, a2
"
let r2 = assemble(src2, "test2.sage")
let inst2 = r2["code"][0]
assert(inst2 == 0x40C58533, "sub a0,a1,a2 = " + hex(inst2))
print("PASS: sub a0,a1,a2 = 0x" + hex(inst2))

# Test 3: I-type ADDI
let src3 = "
addi a0, a1, 42
"
let r3 = assemble(src3, "test3.sage")
let inst3 = r3["code"][0]
assert(inst3 == 0x02A58513, "addi a0,a1,42 = " + hex(inst3))
print("PASS: addi a0,a1,42 = 0x" + hex(inst3))

# Test 4: XOR
let src4 = "
xor a0, a1, a2
"
let r4 = assemble(src4, "test4.sage")
let inst4 = r4["code"][0]
assert(inst4 == 0x00C5C533, "xor a0,a1,a2 = " + hex(inst4))
print("PASS: xor a0,a1,a2 = 0x" + hex(inst4))

# Test 5: SLL
let src5 = "
sll a0, a1, a2
"
let r5 = assemble(src5, "test5.sage")
let inst5 = r5["code"][0]
assert(inst5 == 0x00C59533, "sll a0,a1,a2 = " + hex(inst5))
print("PASS: sll a0,a1,a2 = 0x" + hex(inst5))

# Test 6: Branch BEQ (forward reference via label)
let src6 = "
beq a0, a1, target
nop
target: addi a0, a0, 1
"
let r6 = assemble(src6, "test6.sage")
assert(len(r6["code"]) == 3, "expected 3 instructions, got " + str(len(r6["code"])))
print("PASS: branch beq with label = " + str(len(r6["code"])) + " instructions")

# Test 7: LUI
let src7 = "
lui a0, 0x12345
"
let r7 = assemble(src7, "test7.sage")
let inst7 = r7["code"][0]
assert(inst7 == 0x12345537, "lui a0,0x12345 = " + hex(inst7))
print("PASS: lui a0,0x12345 = 0x" + hex(inst7))

# Test 8: SW (store) — sw rs2, offset(rs1) parsed as [rs2, rs1, offset]
let src8 = "
sw a1, 4(a0)
"
let r8 = assemble(src8, "test8.sage")
let inst8 = r8["code"][0]
assert(inst8 == 0x00B52223, "sw a1,4(a0) = " + hex(inst8))
print("PASS: sw a1,4(a0) = 0x" + hex(inst8))

# Test 9: LW (load)
let src9 = "
lw a1, 4(a0)
"
let r9 = assemble(src9, "test9.sage")
let inst9 = r9["code"][0]
assert(inst9 == 0x00452583, "lw a1,4(a0) = " + hex(inst9))
print("PASS: lw a1,4(a0) = 0x" + hex(inst9))

# Test 10: Pseudonym NOP
let src10 = "
nop
"
let r10 = assemble(src10, "test10.sage")
let inst10 = r10["code"][0]
assert(inst10 == 0x00000013, "nop = " + hex(inst10))
print("PASS: nop = 0x" + hex(inst10))

# Test 11: Pseudonym MV
let src11 = "
mv a0, a1
"
let r11 = assemble(src11, "test11.sage")
let inst11 = r11["code"][0]
assert(inst11 == 0x00058513, "mv a0,a1 = " + hex(inst11))
print("PASS: mv a0,a1 = 0x" + hex(inst11))

# Test 12: Pseudonym LI
let src12 = "
li a0, 42
"
let r12 = assemble(src12, "test12.sage")
let inst12 = r12["code"][0]
assert(inst12 == 0x02A00513, "li a0,42 = " + hex(inst12))
print("PASS: li a0,42 = 0x" + hex(inst12))

# Test 13: ECALL
let src13 = "
ecall
"
let r13 = assemble(src13, "test13.sage")
let inst13 = r13["code"][0]
assert(inst13 == 0x00000073, "ecall = " + hex(inst13))
print("PASS: ecall = 0x" + hex(inst13))

# Test 14: JAL
let src14 = "
jal ra, loop
nop
loop: addi a0, a0, 1
"
let r14 = assemble(src14, "test14.sage")
assert(len(r14["code"]) == 3, "jal + nop + loop = 3 instrs")
print("PASS: jal with label = " + str(len(r14["code"])) + " instructions")

# Test 15: M-extension MUL
let src15 = "
mul a0, a1, a2
"
let r15 = assemble(src15, "test15.sage")
let inst15 = r15["code"][0]
assert(inst15 == 0x02C58533, "mul a0,a1,a2 = " + hex(inst15))
print("PASS: mul a0,a1,a2 = 0x" + hex(inst15))

# Test 16: Pseudonym RET
let src16 = "
ret
"
let r16 = assemble(src16, "test16.sage")
let inst16 = r16["code"][0]
assert(inst16 == 0x00008067, "ret = " + hex(inst16))
print("PASS: ret = 0x" + hex(inst16))

# Test 17: Pseudonym NOT
let src17 = "
not a0, a1
"
let r17 = assemble(src17, "test17.sage")
let inst17 = r17["code"][0]
assert(inst17 == 0xFFF5C513, "not a0,a1 = " + hex(inst17))
print("PASS: not a0,a1 = 0x" + hex(inst17))

# Test 18: AUIPC
let src18 = "
auipc a0, 0x1000
"
let r18 = assemble(src18, "test18.sage")
let inst18 = r18["code"][0]
assert(inst18 == 0x01000517, "auipc a0,0x1000 = " + hex(inst18))
print("PASS: auipc a0,0x1000 = 0x" + hex(inst18))

print("")
print("All assembler tests passed!")
