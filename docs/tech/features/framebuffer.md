# Framebuffer

Stage 2 sets a VBE linear mode when the BIOS offers it and stores info at `0x4F00` (`FB_MAGIC` `0x31424656`).

## Kernel

- Map LFB in the HHDM; paint bitmap-font `Vampire OS`.
- PMM shadow; `SYS_FBPIX` fills a rect; `SYS_FBPRESENT` copies to scanout.
- Overlay row: `$` / `kbd>` plus the line buffer so typing is visible on the scanout.
- Console sync: blit 80×25 VGA glyphs onto the shadow, then the overlay, then present. Graphics `SYS_FBPRESENT` does not blit VGA (it would wipe the fill).
- If mode set fails: 80×25 text VGA only.

## User

- `run fbinfo` → `640x480` (or whatever booted).
- `run fbtest` / `run fbhello` / `run fbclear` via syscalls 24–26.
- PS/2 left click: `x,y` once on VGA and LFB.

See [Console](../modules/console.md), [kernel/fb.c](../files/kernel.fb.c.md), [boot/const.inc](../files/boot.const.inc.md).
