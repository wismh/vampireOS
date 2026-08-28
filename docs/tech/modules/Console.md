---
tags: [module]
---

# Console

VGA text, COM1 mirror, PS/2 keyboard and mouse, VBE overlay.

## Capabilities

- 80×25 VGA; `vga_putc` also writes COM1 (`0x3F8`, 115200 8N1).
- Line buffer: `$` (user `sh`) or `kbd>` (kernel fallback). COM1 RX injects into the same buffer.
- Ctrl+C → SIGINT on the last `run` ELF.
- VBE LFB: banner, prompt row, `SYS_FBPIX` / `SYS_FBPRESENT`.
- IRQ12 left click prints `x,y` once.

## How it is implemented

- [[kernel.vga.c]] — text cells through HHDM.
- [[kernel.serial.c]] — 16550; stage 2 already printed `boot`.
- [[kernel.kbd.c]] — scancodes, kernel commands (`help` `ls` `run` `sync` …), overlay redraw.
- [[kernel.fb.c]] — shadow, present, font, overlay.
- [[kernel.mouse.c]] — PS/2 mouse.

## See also

- [[features/Framebuffer]]
- [[features/Shell]]
