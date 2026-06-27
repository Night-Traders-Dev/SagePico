# Pure Sage DMA module
let pico = ffi_open("pico")

proc claim():
    return ffi_call(pico, "dma_claim", [])

proc config(ch, src, dst, count, dreq, data_size, inc_read, inc_write):
    ffi_call(pico, "dma_config", [ch, src, dst, count, dreq, data_size, inc_read, inc_write])

proc start(ch):
    ffi_call(pico, "dma_start", [ch])

proc wait(ch):
    ffi_call(pico, "dma_wait", [ch])

proc is_busy(ch):
    return ffi_call(pico, "dma_busy", [ch])

proc unclaim(ch):
    ffi_call(pico, "dma_unclaim", [ch])
