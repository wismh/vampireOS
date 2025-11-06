%include "const.inc"

incbin "kernel.raw.bin"
times (KERNEL_SECTORS * 512) - ($ - $$) db 0
