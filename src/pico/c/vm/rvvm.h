/* RISC-V RV32I Virtual Machine Core for SagePico
   Register-based VM with 32 regs, 64KB RAM, custom graphics extensions.
   Executes from bytecode in RAM or flash. Memory-mapped I/O for framebuffer/PIO/DMA.

   Instruction set: RV32I subset + custom graphics (opcode 0x0B, funct3-encoded)

   Memory map:
     0x00000000 - 0x0000FFFF : RAM (64KB)
     0x10000000 - 0x1009FFFF : Framebuffer (640x400 = 256KB)  
     0x20000000 - 0x200000FF : MMIO (GPU control registers)
     0x30000000 - 0x300FFFFF : Flash (1MB code/data) */

#ifndef SAGE_RVVM_H
#define SAGE_RVVM_H

#include <stdint.h>
#include <string.h>

/* ---- VM State ---- */
typedef struct {
    uint32_t regs[32];    /* x0=zero, x1=ra, x2=sp, x3=gp, x4=tp, x5-7=t0-2, x8=s0/fp,
                             x9=s1, x10-17=a0-7, x18-27=s2-11, x28-31=t3-6 */
    uint32_t pc;           /* program counter (byte offset into code) */
    uint32_t cycles;       /* instruction counter */
    uint8_t  running;

    uint8_t* ram;          /* 64KB RAM */
    uint8_t* framebuf;     /* pointer to external framebuffer */
    uint32_t fb_width;
    uint32_t fb_height;

    /* MMIO callbacks */
    uint32_t (*mmio_read)(uint32_t addr);
    void     (*mmio_write)(uint32_t addr, uint32_t val);

    /* SRVM heap: simple dict for globals (key -> value) */
    uint32_t heap_keys[64];
    uint32_t heap_vals[64];
    int      heap_count;
} rvvm_t;

/* ---- Instruction decoder helpers ---- */
#define RV_OPCODE(inst)  ((inst) & 0x7f)
#define RV_RD(inst)      (((inst) >> 7) & 0x1f)
#define RV_RS1(inst)     (((inst) >> 15) & 0x1f)
#define RV_RS2(inst)     (((inst) >> 20) & 0x1f)
#define RV_FUNCT3(inst)  (((inst) >> 12) & 0x7)
#define RV_FUNCT7(inst)  (((inst) >> 25) & 0x7f)

/* I-type immediate (sign-extended 12-bit) */
static inline int32_t rv_imm_i(uint32_t inst) {
    int32_t v = (inst >> 20) & 0xfff;
    if (v & 0x800) v |= 0xfffff000;
    return v;
}
/* S-type immediate */
static inline int32_t rv_imm_s(uint32_t inst) {
    int32_t v = ((inst >> 7) & 0x1f) | ((inst >> 20) & 0xfe0);
    if (v & 0x800) v |= 0xfffff000;
    return v;
}
/* B-type immediate */
static inline int32_t rv_imm_b(uint32_t inst) {
    int32_t v = ((inst >> 7) & 0x1e) | ((inst >> 20) & 0x7e0) |
                ((inst << 4) & 0x800) | ((inst >> 19) & 0x1000);
    if (v & 0x1000) v |= 0xffffe000;
    return v;
}
/* U-type immediate */
static inline int32_t rv_imm_u(uint32_t inst) {
    return (int32_t)(inst & 0xfffff000);
}
/* J-type immediate */
static inline int32_t rv_imm_j(uint32_t inst) {
    int32_t v = ((inst >> 20) & 0x7fe) | ((inst >> 9) & 0x800) |
                (inst & 0xff000) | ((inst >> 11) & 0x100000);
    if (v & 0x100000) v |= 0xffe00000;
    return v;
}

/* ---- Memory access ---- */
static uint32_t rvvm_mem_read(rvvm_t* vm, uint32_t addr, int size) {
    if (addr < 0x10000) {
        /* RAM */
        uint32_t v = 0;
        if (size == 4) { memcpy(&v, vm->ram + addr, 4); return v; }
        if (size == 2) { uint16_t w; memcpy(&w, vm->ram + addr, 2); return w; }
        return vm->ram[addr];
    }
    if (addr >= 0x10000000 && addr < 0x10000000 + vm->fb_width * vm->fb_height) {
        /* Framebuffer: 1 byte per pixel (palette index) */
        uint32_t off = addr - 0x10000000;
        if (off < vm->fb_width * vm->fb_height) return vm->framebuf[off];
        return 0;
    }
    if (addr >= 0x20000000 && addr <= 0x200000ff) {
        /* MMIO */
        if (vm->mmio_read) return vm->mmio_read(addr);
        return 0;
    }
    if (addr >= 0x30000000 && addr < 0x30100000) {
        /* Flash (read-only, direct XIP access) */
        uint32_t off = addr - 0x30000000;
        return *(volatile uint8_t*)(XIP_BASE + off);
    }
    return 0;
}

static void rvvm_mem_write(rvvm_t* vm, uint32_t addr, uint32_t val, int size) {
    if (addr < 0x10000) {
        if (size == 4) { memcpy(vm->ram + addr, &val, 4); return; }
        if (size == 2) { uint16_t w = (uint16_t)val; memcpy(vm->ram + addr, &w, 2); return; }
        vm->ram[addr] = (uint8_t)val;
        return;
    }
    if (addr >= 0x10000000 && addr < 0x10000000 + vm->fb_width * vm->fb_height) {
        uint32_t off = addr - 0x10000000;
        if (off < vm->fb_width * vm->fb_height) vm->framebuf[off] = (uint8_t)val;
        return;
    }
    if (addr >= 0x20000000 && addr <= 0x200000ff) {
        if (vm->mmio_write) vm->mmio_write(addr, val);
        return;
    }
}

/* ---- Initialize VM ---- */
static void rvvm_init(rvvm_t* vm, uint8_t* ram, uint8_t* fb, uint32_t w, uint32_t h) {
    memset(vm, 0, sizeof(*vm));
    vm->ram = ram;
    vm->framebuf = fb;
    vm->fb_width = w;
    vm->fb_height = h;
    vm->running = 0;
}

/* ---- Execute one instruction ---- */
static int rvvm_step(rvvm_t* vm) {
    if (!vm->running || vm->pc >= 0x10000) { vm->running = 0; return 0; }

    uint32_t inst;
    memcpy(&inst, vm->ram + vm->pc, 4);
    uint32_t npc = vm->pc + 4;
    vm->cycles++;

    uint8_t opcode = RV_OPCODE(inst);
    uint8_t rd = RV_RD(inst);
    uint8_t rs1 = RV_RS1(inst);
    uint8_t rs2 = RV_RS2(inst);
    uint8_t funct3 = RV_FUNCT3(inst);
    uint8_t funct7 = RV_FUNCT7(inst);
    int32_t imm_i = rv_imm_i(inst);

    uint32_t rs1v = vm->regs[rs1];
    uint32_t rs2v = vm->regs[rs2];
    uint32_t result = 0;
    int branch_taken = 0;
    uint32_t branch_target = npc + (uint32_t)rv_imm_b(inst);

    switch (opcode) {
    /* ---- OP-IMM (0x13) ---- */
    case 0x13:
        switch (funct3) {
        case 0: result = rs1v + (uint32_t)imm_i; break;       /* ADDI */
        case 1: result = rs1v << (imm_i & 0x1f); break;       /* SLLI */
        case 2: result = ((int32_t)rs1v < imm_i) ? 1 : 0; break; /* SLTI */
        case 3: result = (rs1v < (uint32_t)imm_i) ? 1 : 0; break; /* SLTIU */
        case 4: result = rs1v ^ (uint32_t)imm_i; break;       /* XORI */
        case 5:  /* SRLI / SRAI */
            if (funct7 == 0x20) result = (uint32_t)((int32_t)rs1v >> (imm_i & 0x1f));
            else result = rs1v >> (imm_i & 0x1f);
            break;
        case 6: result = rs1v | (uint32_t)imm_i; break;       /* ORI */
        case 7: result = rs1v & (uint32_t)imm_i; break;       /* ANDI */
        }
        if (rd) vm->regs[rd] = result;
        break;

    /* ---- OP (0x33) ---- */
    case 0x33:
        switch (funct3) {
        case 0: result = (funct7 == 0x20) ? (uint32_t)((int32_t)rs1v - (int32_t)rs2v) : rs1v + rs2v; break; /* ADD/SUB */
        case 1: result = rs1v << (rs2v & 0x1f); break;         /* SLL */
        case 2: result = ((int32_t)rs1v < (int32_t)rs2v) ? 1 : 0; break; /* SLT */
        case 3: result = (rs1v < rs2v) ? 1 : 0; break;         /* SLTU */
        case 4: result = rs1v ^ rs2v; break;                   /* XOR */
        case 5:  /* SRL/SRA */
            result = (funct7 == 0x20) ? (uint32_t)((int32_t)rs1v >> (rs2v & 0x1f)) : rs1v >> (rs2v & 0x1f);
            break;
        case 6: result = rs1v | rs2v; break;                   /* OR */
        case 7: result = rs1v & rs2v; break;                   /* AND */
        }
        if (rd) vm->regs[rd] = result;
        break;

    /* ---- LUI (0x37) ---- */
    case 0x37:
        if (rd) vm->regs[rd] = (uint32_t)rv_imm_u(inst);
        break;

    /* ---- AUIPC (0x17) ---- */
    case 0x17:
        if (rd) vm->regs[rd] = vm->pc + (uint32_t)rv_imm_u(inst);
        break;

    /* ---- JAL (0x6F) ---- */
    case 0x6f:
        if (rd) vm->regs[rd] = npc;
        npc = vm->pc + (uint32_t)rv_imm_j(inst);
        break;

    /* ---- JALR (0x67) ---- */
    case 0x67:
        if (rd) vm->regs[rd] = npc;
        npc = (rs1v + (uint32_t)imm_i) & ~1u;
        break;

    /* ---- BRANCH (0x63) ---- */
    case 0x63:
        switch (funct3) {
        case 0: branch_taken = (rs1v == rs2v); break;                    /* BEQ */
        case 1: branch_taken = (rs1v != rs2v); break;                    /* BNE */
        case 4: branch_taken = ((int32_t)rs1v < (int32_t)rs2v); break;   /* BLT */
        case 5: branch_taken = ((int32_t)rs1v >= (int32_t)rs2v); break;  /* BGE */
        case 6: branch_taken = (rs1v < rs2v); break;                     /* BLTU */
        case 7: branch_taken = (rs1v >= rs2v); break;                    /* BGEU */
        }
        if (branch_taken) npc = branch_target;
        break;

    /* ---- LOAD (0x03) ---- */
    case 0x03: {
        uint32_t addr = rs1v + (uint32_t)imm_i;
        uint32_t v = rvvm_mem_read(vm, addr, (funct3 & 3) == 0 ? 1 : (funct3 & 3) == 1 ? 2 : 4);
        if (funct3 == 4) { /* LBU */ }
        else if (funct3 == 5) { /* LHU */ }
        else if (funct3 == 0) { if (v & 0x80) v |= 0xffffff00; }  /* LB sign-extend */
        else if (funct3 == 1) { if (v & 0x8000) v |= 0xffff0000; } /* LH sign-extend */
        if (rd) vm->regs[rd] = v;
        break;
    }

    /* ---- STORE (0x23) ---- */
    case 0x23: {
        uint32_t addr = rs1v + (uint32_t)rv_imm_s(inst);
        int size = (funct3 == 0) ? 1 : (funct3 == 1) ? 2 : 4;
        rvvm_mem_write(vm, addr, rs2v, size);
        break;
    }

    /* ---- ECALL / VMSYS (0x73) — SRVM compatible ---- */
    case 0x0f:
    case 0x73:
        /* SRVM VMSYS multiplexing via funct3:
           funct3=0: VM ops (HALT=1, PRINT=10)
           funct3=2: Object ops (GET_GLOBAL=15, SET_GLOBAL=16, ARRAY_NEW=30, GET/SET_INDEX=32/33) */
        if (opcode == 0x0f && inst == 0x00000073) { vm->running = 0; return 0; }
        if (opcode == 0x73) {
            uint8_t vmsys_op = rs1;  /* SRVM: rs1 encodes the specific VMSYS operation */
            switch (funct3) {
            case 0: /* VM operations */
                if (vmsys_op == 1) { vm->running = 0; return 0; } /* HALT */
                if (vmsys_op == 10 && vm->mmio_write) {
                    /* PRINT: write a0 to MMIO print port */
                    vm->mmio_write(0x20000100, vm->regs[10]); /* a0 */
                }
                break;
            case 2: /* Object operations */
                if (vmsys_op == 15 && vm->mmio_read) {
                    /* GET_GLOBAL: rd = heap[imm_i] (index in constant pool) */
                    if (rd) vm->regs[rd] = vm->mmio_read(0x20000200 + (uint32_t)imm_i);
                }
                if (vmsys_op == 16 && vm->mmio_write) {
                    /* SET_GLOBAL: heap[imm_i] = rs2v */
                    vm->mmio_write(0x20000200 + (uint32_t)imm_i, rs2v);
                }
                break;
            }
        }
        break;

    /* ---- Custom Graphics Extensions (opcode 0x0B, funct3-encoded) ---- */
    case 0x0b:
        switch (funct3) {
        case 0: { /* FILL_RECT: x=rs1, y=rs2, w=imm_i[15:0], h=imm_i[31:16], color=rd */
            uint32_t color = vm->regs[rd];
            uint32_t x = rs1v, y = rs2v;
            uint32_t w_ = (uint32_t)imm_i & 0xffff;
            uint32_t h_ = ((uint32_t)imm_i >> 16) & 0xffff;
            if (x < vm->fb_width && y < vm->fb_height) {
                for (uint32_t dy = 0; dy < h_ && (y+dy) < vm->fb_height; dy++) {
                    uint32_t row_start = (y+dy) * vm->fb_width + x;
                    uint32_t row_len = (x + w_ <= vm->fb_width) ? w_ : vm->fb_width - x;
                    if (row_len > 0) memset(vm->framebuf + row_start, (uint8_t)color, row_len);
                }
            }
            break;
        }
        case 1: { /* DRAW_LINE: x0=rs1 y0=rs2 x1=rd y1=imm_i with color */
            break;
        }
        case 2: { /* BLIT: dst=rd (x|y<<16), src=rs1, w/h=rs2 */
            uint32_t dst_x = vm->regs[rd] & 0xffff;
            uint32_t dst_y = vm->regs[rd] >> 16;
            uint32_t src_addr = rs1v;
            uint32_t w_ = rs2v & 0xffff;
            uint32_t h_ = rs2v >> 16;
            for (uint32_t dy = 0; dy < h_; dy++) {
                uint32_t src_off = dy * w_;
                uint32_t dst_off = (dst_y + dy) * vm->fb_width + dst_x;
                for (uint32_t dx = 0; dx < w_; dx++)
                    if (dst_off + dx < vm->fb_width * vm->fb_height)
                        vm->framebuf[dst_off + dx] = vm->ram[src_off + dx];
            }
            break;
        }
        case 3: { /* SPRITE: x=rs1, y=rs2, id=rd, transparent=imm_i */
            break;
        }
        case 4: { /* FLIP: trigger framebuffer swap via MMIO */
            if (vm->mmio_write) vm->mmio_write(0x20000004, 1);
            break;
        }
        case 5: { /* VSYNC: yield until vsync MMIO bit clears */
            if (vm->mmio_read && vm->mmio_read(0x20000000)) { npc = vm->pc; } /* spin */
            break;
        }
        }
        break;

    default:
        break;
    }

    vm->pc = npc;
    return 1;
}

/* ---- Run VM for N cycles or until halt ---- */
static void rvvm_run(rvvm_t* vm, uint32_t max_cycles) {
    vm->running = 1;
    while (vm->running && vm->cycles < max_cycles) {
        rvvm_step(vm);
    }
}

/* ---- Load program into VM RAM and reset ---- */
static void rvvm_load(rvvm_t* vm, const uint8_t* program, uint32_t size, uint32_t entry) {
    memcpy(vm->ram, program, size > 65536 ? 65536 : size);
    vm->pc = entry;
    vm->cycles = 0;
}

#endif
