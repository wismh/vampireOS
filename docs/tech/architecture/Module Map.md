---
tags: [architecture]
---

# Module map

Who talks to whom after `kmain` has switched to the HHDM stack.

```mermaid
flowchart TB
  subgraph boot [Boot leftovers]
    E820["e820_map"]
    FBINFO["FB_INFO at 0x4F00"]
  end

  subgraph mem [Memory]
    PMM["pmm"]
    VMM["vmm HHDM"]
    HEAP["kheap"]
  end

  subgraph blk [Block]
    BIO["bio + PART_LBA"]
    VT["virtio-blk"]
    AH["ahci"]
    ATA["ata PIO"]
  end

  subgraph vol [FS]
    FAT["fs FAT12"]
  end

  subgraph tasks [Tasks]
    GDT["gdt + TSS"]
    IDT["idt int 0x30"]
    SCH["sched"]
    USR["user / elf"]
  end

  subgraph cons [Console]
    VGA["vga"]
    SER["serial COM1"]
    KBD["kbd"]
    FB["fb"]
  end

  subgraph net [Net]
    VN["virtio-net TX"]
  end

  E820 --> PMM
  PMM --> VMM
  FBINFO --> FB
  VMM --> HEAP
  VT --> BIO
  AH --> BIO
  ATA --> BIO
  BIO --> FAT
  FAT --> USR
  USR --> SCH
  GDT --> SCH
  IDT --> USR
  VGA --> SER
  KBD --> SCH
  FB --> VGA
  VN --> SER
```

- FAT never talks PCI. It calls `bread` / `bwrite` / `bflush` ([[kernel.bio.h]]).
- `user_on_syscall` is the only ring-3 entry besides exceptions ([[kernel.user.c]]).
- Console `write` on fd 1/2 hits VGA and COM1; the LFB overlay is a second view of the same line buffer ([[features/Framebuffer]]).
- VirtIO-net does not go through `bio`. One datagram at boot, then idle ([[modules/Net]]).

See [[architecture/Boundaries]] for what each side must not include.
