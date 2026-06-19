STAGE2_OFFSET   equ 0x8000
STAGE2_SEGMENT  equ 0x0000
STAGE2_LBA      equ 1
STAGE2_SECTORS  equ 16

KERNEL_PHYS     equ 0x100000
KERNEL_TEMP     equ 0x10000
KERNEL_SEGMENT  equ 0x1000
KERNEL_OFFSET   equ 0
KERNEL_LBA      equ STAGE2_LBA + STAGE2_SECTORS
KERNEL_SECTORS  equ 256
; BIOS AH=0x42 is happiest at <= 128 sectors per DAP.
KERNEL_READ1    equ 128
KERNEL_READ2    equ KERNEL_SECTORS - KERNEL_READ1
KERNEL_TEMP2    equ KERNEL_TEMP + KERNEL_READ1 * 512
KERNEL_SEGMENT2 equ KERNEL_TEMP2 / 16
KERNEL_LBA2     equ KERNEL_LBA + KERNEL_READ1
KERNEL_VMA      equ 0xFFFFFFFF80000000
KERNEL_VIRT     equ KERNEL_VMA + KERNEL_PHYS

; Type 0x01 FAT12. Start sits after MBR + stage 2 + kernel so the VBR/BPB
; is not LBA 0. Image packer concatenates those then the volume; keep this
; equal to 1 + STAGE2_SECTORS + KERNEL_SECTORS.
PART_TYPE       equ 0x01
PART_LBA        equ KERNEL_LBA + KERNEL_SECTORS

FAT_SPC         equ 1
FAT_RESERVED    equ 1
FAT_COUNT       equ 2
FAT_SEC_PER_FAT equ 4
FAT_ROOT_ENT    equ 16
FAT_MEDIA       equ 0xF8
FAT_DATA_CLUSTERS equ 1024
FAT_ROOT_SECS   equ ((FAT_ROOT_ENT * 32) + 511) / 512
FAT_TOTAL_SECS  equ FAT_RESERVED + FAT_COUNT * FAT_SEC_PER_FAT + FAT_ROOT_SECS + FAT_DATA_CLUSTERS

PML4            equ 0x1000
PDPT            equ 0x2000
PD              equ 0x3000
HIGH_PDPT       equ 0x5000
HIGH_PD         equ 0x6000

E820_BASE       equ 0x4000
E820_MAX        equ 32
E820_ENTRY_SIZE equ 24

; Stage 2 writes this after a VBE linear mode set (zero magic = text VGA).
FB_INFO         equ 0x4F00
FB_MAGIC        equ 0x31424656
VBE_INFO_BUF    equ 0x7000
VBE_MODE_BUF    equ 0x7200
