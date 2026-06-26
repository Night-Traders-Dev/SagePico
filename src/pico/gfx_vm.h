/* Graphics Virtual Machine — RISC-V VM + PIO accelerator + flash storage
   Ties together rvvm.h, pio_bridge.h, dma_bridge.h, flash_store.h
   Memory map:
     0x00000000 - 0x0000FFFF : RAM (64KB, VM code + data)
     0x10000000 - 0x1009FFFF : Framebuffer (640x400, palette indices)
     0x20000000 - 0x200000FF : MMIO GPU registers
     0x30000000 - 0x300FFFFF : Flash storage (1MB, read via XIP)

   MMIO GPU registers:
     0x20000000 : VBLANK status (read: 1=in vblank, write: ignored)
     0x20000004 : FLIP trigger  (write 1 to swap framebuffers)
     0x20000008 : PIO_FILL color (write: sets fill color for PIO accelerator)
     0x2000000C : PIO_FILL rect  (write: x|y<<16, triggers PIO fill at x,y with w/h=rs1)
     0x20000010 : DMA_BLIT src   (write: source address for DMA blit)
     0x20000014 : DMA_BLIT dst   (write: dest address in framebuffer)
     0x20000018 : DMA_BLIT size  (write: w|h<<16, triggers DMA transfer)
     0x2000001C : VM_CYCLES      (read: cycles executed this frame)
     0x20000020 : VM_STATUS      (read: 0=halted, 1=running) */

#ifndef SAGE_GFX_VM_H
#define SAGE_GFX_VM_H

#include "rvvm.h"
#include <string.h>

/* ---- GPU state ---- */
typedef struct {
    rvvm_t  vm;
    uint8_t ram[65536] __attribute__((aligned(4)));  /* 64KB VM RAM */

    /* GPU MMIO registers */
    uint32_t gpu_vblank;
    uint32_t gpu_flip;
    uint32_t gpu_pio_fill_color;
    uint32_t gpu_pio_fill_rect;
    uint32_t gpu_dma_src;
    uint32_t gpu_dma_dst;
    uint32_t gpu_dma_size;
    uint32_t gpu_vm_status;   /* 1=running, 0=halted */
} gfx_vm_t;

static gfx_vm_t* gfx_active_vm = NULL;

/* ---- MMIO read callback ---- */
static uint32_t gfx_mmio_read(uint32_t addr) {
    if (!gfx_active_vm) return 0;

    switch (addr) {
    case 0x20000000: return gfx_active_vm->gpu_vblank;
    case 0x20000004: return gfx_active_vm->gpu_flip;
    case 0x2000001C: return gfx_active_vm->vm.cycles;
    case 0x20000020: return gfx_active_vm->gpu_vm_status;
    default: return 0;
    }
}

/* ---- MMIO write callback ---- */
static void gfx_mmio_write(uint32_t addr, uint32_t val) {
    if (!gfx_active_vm) return;

    switch (addr) {
    case 0x20000004: gfx_active_vm->gpu_flip = val; break;
    case 0x20000008: gfx_active_vm->gpu_pio_fill_color = val; break;
    case 0x2000000C: gfx_active_vm->gpu_pio_fill_rect = val; break;
    case 0x20000010: gfx_active_vm->gpu_dma_src = val; break;
    case 0x20000014: gfx_active_vm->gpu_dma_dst = val; break;
    case 0x20000018: gfx_active_vm->gpu_dma_size = val; break;
    }
}

/* ---- Initialize Graphics VM ---- */
static void gfx_vm_init(gfx_vm_t* gfx, uint8_t* framebuf) {
    memset(gfx, 0, sizeof(*gfx));
    rvvm_init(&gfx->vm, gfx->ram, framebuf, 640, 400);
    gfx->vm.mmio_read = gfx_mmio_read;
    gfx->vm.mmio_write = gfx_mmio_write;
    gfx->gpu_vm_status = 0;
    gfx_active_vm = gfx;
}

/* ---- Load program from flash into VM RAM ---- */
static int gfx_vm_load_flash(gfx_vm_t* gfx, uint32_t flash_offset) {
    if (flash_offset >= 0x100000) return -1; /* beyond 1MB flash window */

    const uint8_t* src = (const uint8_t*)(XIP_BASE + 0x30000000 - 0x30000000 + flash_offset);
    /* Read program size from first 4 bytes */
    uint32_t prog_size;
    memcpy(&prog_size, src, 4);
    if (prog_size == 0 || prog_size > 65536) return -1;
    src += 4;
    /* Read entry point from next 4 bytes */
    uint32_t entry;
    memcpy(&entry, src, 4);
    src += 4;

    rvvm_load(&gfx->vm, src, prog_size, entry);
    return 0;
}

/* ---- Store program to flash ---- */
static int gfx_vm_store_flash(const uint8_t* program, uint32_t prog_size,
                               uint32_t entry, uint32_t flash_offset) {
    /* flash_store_* functions are static, defined in flash_store.h (injected before us) */

    /* Pack header: size (4 bytes) + entry (4 bytes) + program */
    uint8_t buf[256];
    if (prog_size > 245) return -1; /* limited by flash_store_put max value size */
    memcpy(buf, &prog_size, 4);
    memcpy(buf + 4, &entry, 4);
    memcpy(buf + 8, program, prog_size);

    char key[16];
    snprintf(key, sizeof(key), "gfx_prog_%04x", (uint16_t)flash_offset);
    return flash_store_put(key, buf, (uint16_t)(prog_size + 8));
}

/* ---- Retrieve program from flash --- */
static int gfx_vm_retrieve_flash(gfx_vm_t* gfx, uint32_t flash_offset) {
    /* flash_store_get is static, defined in flash_store.h (injected before us) */

    char key[16];
    snprintf(key, sizeof(key), "gfx_prog_%04x", (uint16_t)flash_offset);

    uint8_t buf[256]; uint16_t len = 0;
    if (flash_store_get(key, buf, &len) != 0) return -1;
    if (len < 8) return -1;

    uint32_t prog_size, entry;
    memcpy(&prog_size, buf, 4);
    memcpy(&entry, buf + 4, 4);
    if (prog_size + 8 > len) return -1;

    rvvm_load(&gfx->vm, (const uint8_t*)(buf + 8), prog_size, entry);
    return 0;
}

/* ---- Run VM for one frame ---- */
static void gfx_vm_run_frame(gfx_vm_t* gfx) {
    if (!gfx->vm.running) return;
    gfx->gpu_vm_status = 1;
    rvvm_run(&gfx->vm, 50000); /* max cycles per frame */
    gfx->gpu_vm_status = gfx->vm.running ? 1 : 0;
}

#endif
