#include "virtio.h"
#include "fb.h"
#include "io.h"
#include "pmm.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
#define PCI_ENABLE 0x80000000u
#define PCI_CMD_IO 0x0001u
#define PCI_CMD_MEM 0x0002u
#define PCI_CMD_BUSMASTER 0x0004u
#define PCI_STATUS_CAPS 0x0010u
#define PCI_CAP_VNDR 0x09u
#define PCI_VENDOR_VIRTIO 0x1AF4u
#define PCI_DEV_BLK_LEGACY 0x1001u
#define PCI_DEV_BLK_MODERN 0x1042u

#define VIRTIO_PCI_CAP_COMMON 1u
#define VIRTIO_PCI_CAP_NOTIFY 2u
#define VIRTIO_PCI_CAP_ISR 3u
#define VIRTIO_PCI_CAP_DEVICE 4u

#define VIRTIO_STATUS_ACK 1u
#define VIRTIO_STATUS_DRIVER 2u
#define VIRTIO_STATUS_DRIVER_OK 4u
#define VIRTIO_STATUS_FEATURES_OK 8u

#define VIRTIO_F_VERSION_1 32u
#define VIRTQ_DESC_F_NEXT 1u
#define VIRTQ_DESC_F_WRITE 2u
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1u
#define VIRTIO_BLK_T_IN 0u
#define VIRTIO_BLK_S_OK 0u

#define LEG_HOST_FEATURES 0u
#define LEG_GUEST_FEATURES 4u
#define LEG_QUEUE_PFN 8u
#define LEG_QUEUE_NUM 12u
#define LEG_QUEUE_SEL 14u
#define LEG_QUEUE_NOTIFY 16u
#define LEG_STATUS 18u
#define LEG_ISR 19u

#define CFG_DFSELECT 0u
#define CFG_DF 4u
#define CFG_GFSELECT 8u
#define CFG_GF 12u
#define CFG_MSIX 16u
#define CFG_NUMQ 18u
#define CFG_STATUS 20u
#define CFG_QSEL 22u
#define CFG_QSIZE 24u
#define CFG_QMSIX 26u
#define CFG_QENA 28u
#define CFG_QNOTIFY 30u
#define CFG_QDESC 32u
#define CFG_QDRIVER 40u
#define CFG_QDEVICE 48u
#define MSIX_NONE 0xFFFFu

#define PAGE_SIZE 0x1000ull
#define DMA_MIN 0x200000ull
#define SEC_SIZE 512u
#define QSZ_MAX 256u
#define WAIT_MAX 1000000u
#define BAR_MAP 0x10000u

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static uint16_t io_base;
static volatile uint8_t *common;
static volatile uint8_t *notify;
static uint32_t notify_mult;
static uint16_t qsz;
static uint16_t q_notify_off;
static uint64_t dma_phys;
static uint8_t *dma_virt;
static uint32_t desc_off;
static uint32_t avail_off;
static uint32_t used_off;
static uint32_t req_off;
static uint32_t data_off;
static uint32_t st_off;
static uint16_t avail_idx;
static uint16_t used_seen;
static int modern;
static int live;

static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    uint32_t addr;

    addr = PCI_ENABLE
        | ((uint32_t)bus << 16)
        | ((uint32_t)dev << 11)
        | ((uint32_t)fn << 8)
        | ((uint32_t)off & 0xFCu);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

static void pci_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off,
                        uint32_t value)
{
    uint32_t addr;

    addr = PCI_ENABLE
        | ((uint32_t)bus << 16)
        | ((uint32_t)dev << 11)
        | ((uint32_t)fn << 8)
        | ((uint32_t)off & 0xFCu);
    outl(PCI_ADDR, addr);
    outl(PCI_DATA, value);
}

static int pci_find_blk(uint8_t *bus_out, uint8_t *dev_out, uint8_t *fn_out)
{
    unsigned bus;
    unsigned dev;
    unsigned fn;

    for (bus = 0; bus < 8u; bus++) {
        for (dev = 0; dev < 32u; dev++) {
            for (fn = 0; fn < 8u; fn++) {
                uint32_t id;
                uint16_t ven;
                uint16_t did;

                id = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)fn, 0);
                ven = (uint16_t)id;
                did = (uint16_t)(id >> 16);
                if (ven == 0xFFFFu) {
                    if (fn == 0) {
                        break;
                    }
                    continue;
                }
                if (ven == PCI_VENDOR_VIRTIO
                    && (did == PCI_DEV_BLK_LEGACY || did == PCI_DEV_BLK_MODERN)) {
                    *bus_out = (uint8_t)bus;
                    *dev_out = (uint8_t)dev;
                    *fn_out = (uint8_t)fn;
                    return 0;
                }
            }
        }
    }
    return -1;
}

static void pci_enable(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t cmd;

    cmd = pci_read32(bus, dev, fn, 0x04);
    cmd |= PCI_CMD_IO | PCI_CMD_MEM | PCI_CMD_BUSMASTER;
    pci_write32(bus, dev, fn, 0x04, cmd);
}

static uint32_t pci_bar_raw(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t bar)
{
    return pci_read32(bus, dev, fn, (uint8_t)(0x10u + bar * 4u));
}

static uint64_t pci_bar_addr(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t bar)
{
    uint32_t lo;
    uint32_t hi;
    uint64_t addr;

    lo = pci_bar_raw(bus, dev, fn, bar);
    if ((lo & 1u) != 0) {
        return (uint64_t)(lo & ~3u);
    }
    addr = (uint64_t)(lo & ~0xFu);
    if (((lo >> 1) & 3u) == 2u) {
        hi = pci_bar_raw(bus, dev, fn, (uint8_t)(bar + 1u));
        addr |= (uint64_t)hi << 32;
    }
    return addr;
}

static volatile uint8_t *map_bar(uint8_t bus, uint8_t dev, uint8_t fn,
                                 uint8_t bar, uint32_t off)
{
    uint32_t raw;
    uint64_t addr;

    if (bar > 5u) {
        return 0;
    }
    raw = pci_bar_raw(bus, dev, fn, bar);
    if ((raw & 1u) != 0) {
        return 0;
    }
    addr = pci_bar_addr(bus, dev, fn, bar);
    if (addr == 0) {
        return 0;
    }
    if (vmm_map_uncached(addr, BAR_MAP) != 0) {
        return 0;
    }
    return (volatile uint8_t *)(uintptr_t)phys_to_virt(addr + (uint64_t)off);
}

static uint8_t mmio_r8(volatile uint8_t *p, unsigned off)
{
    return p[off];
}

static void mmio_w8(volatile uint8_t *p, unsigned off, uint8_t v)
{
    p[off] = v;
}

static uint16_t mmio_r16(volatile uint8_t *p, unsigned off)
{
    return *(volatile uint16_t *)(p + off);
}

static void mmio_w16(volatile uint8_t *p, unsigned off, uint16_t v)
{
    *(volatile uint16_t *)(p + off) = v;
}

static uint32_t mmio_r32(volatile uint8_t *p, unsigned off)
{
    return *(volatile uint32_t *)(p + off);
}

static void mmio_w32(volatile uint8_t *p, unsigned off, uint32_t v)
{
    *(volatile uint32_t *)(p + off) = v;
}

static void mmio_w64(volatile uint8_t *p, unsigned off, uint64_t v)
{
    mmio_w32(p, off, (uint32_t)v);
    mmio_w32(p, off + 4u, (uint32_t)(v >> 32));
}

static int pci_caps(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t st;
    uint8_t cap;
    uint8_t n;
    uint32_t d0;
    uint32_t d1;
    uint32_t d2;
    uint8_t type;
    uint8_t bar;
    uint32_t off;

    common = 0;
    notify = 0;
    notify_mult = 0;
    st = pci_read32(bus, dev, fn, 0x04);
    if (((st >> 16) & PCI_STATUS_CAPS) == 0) {
        return -1;
    }
    cap = (uint8_t)pci_read32(bus, dev, fn, 0x34);
    n = 0;
    while (cap >= 0x40u && n < 32u) {
        d0 = pci_read32(bus, dev, fn, cap);
        d1 = pci_read32(bus, dev, fn, (uint8_t)(cap + 4u));
        d2 = pci_read32(bus, dev, fn, (uint8_t)(cap + 8u));
        if ((d0 & 0xFFu) == PCI_CAP_VNDR) {
            type = (uint8_t)(d0 >> 24);
            bar = (uint8_t)d1;
            off = d2;
            if (type == VIRTIO_PCI_CAP_COMMON) {
                common = map_bar(bus, dev, fn, bar, off);
            } else if (type == VIRTIO_PCI_CAP_NOTIFY) {
                notify = map_bar(bus, dev, fn, bar, off);
                notify_mult = pci_read32(bus, dev, fn, (uint8_t)(cap + 16u));
            } else if (type == VIRTIO_PCI_CAP_ISR
                       || type == VIRTIO_PCI_CAP_DEVICE) {
                (void)map_bar(bus, dev, fn, bar, off);
            }
        }
        cap = (uint8_t)(d0 >> 8);
        n++;
        if (cap == 0) {
            break;
        }
    }
    if (common == 0 || notify == 0) {
        return -1;
    }
    return 0;
}

static uint32_t align_up32(uint32_t v, uint32_t a)
{
    return (v + a - 1u) & ~(a - 1u);
}

static int vq_layout(uint16_t size)
{
    uint32_t desc_bytes;
    uint32_t avail_bytes;
    uint32_t used_bytes;
    uint32_t total;
    uint64_t pages;
    uint64_t i;

    if (size == 0 || size > QSZ_MAX) {
        return -1;
    }
    qsz = size;
    desc_off = 0;
    desc_bytes = (uint32_t)size * 16u;
    avail_off = desc_bytes;
    avail_bytes = 6u + 2u * (uint32_t)size;
    used_off = align_up32(avail_off + avail_bytes, (uint32_t)PAGE_SIZE);
    used_bytes = 6u + 8u * (uint32_t)size;
    req_off = align_up32(used_off + used_bytes, 16u);
    data_off = req_off + (uint32_t)sizeof(struct virtio_blk_req);
    st_off = data_off + SEC_SIZE;
    total = st_off + 1u;
    pages = ((uint64_t)total + PAGE_SIZE - 1ull) / PAGE_SIZE;
    if (pages == 0) {
        pages = 1;
    }
    dma_phys = pmm_alloc_span(pages);
    if (dma_phys == 0) {
        dma_phys = pmm_alloc_above(DMA_MIN);
        if (dma_phys == 0 || pages > 1) {
            return -1;
        }
    }
    dma_virt = (uint8_t *)(uintptr_t)phys_to_virt(dma_phys);
    for (i = 0; i < pages * PAGE_SIZE; i++) {
        dma_virt[i] = 0;
    }
    avail_idx = 0;
    used_seen = 0;
    return 0;
}

static struct virtq_desc *vq_desc(void)
{
    return (struct virtq_desc *)(dma_virt + desc_off);
}

static volatile uint16_t *vq_avail_flags(void)
{
    return (volatile uint16_t *)(dma_virt + avail_off);
}

static volatile uint16_t *vq_avail_idx(void)
{
    return (volatile uint16_t *)(dma_virt + avail_off + 2u);
}

static volatile uint16_t *vq_avail_ring(void)
{
    return (volatile uint16_t *)(dma_virt + avail_off + 4u);
}

static volatile uint16_t *vq_used_idx(void)
{
    return (volatile uint16_t *)(dma_virt + used_off + 2u);
}

static void vq_avail_init(void)
{
    *vq_avail_flags() = VIRTQ_AVAIL_F_NO_INTERRUPT;
    *vq_avail_idx() = 0;
}

static int wait_used(void)
{
    unsigned i;

    for (i = 0; i < WAIT_MAX; i++) {
        __asm__ volatile ("mfence" ::: "memory");
        if (*vq_used_idx() != used_seen) {
            used_seen = *vq_used_idx();
            return 0;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static void notify_q(void)
{
    volatile uint16_t *n;
    uint32_t off;

    if (modern != 0) {
        off = (uint32_t)q_notify_off * notify_mult;
        n = (volatile uint16_t *)(notify + off);
        *n = 0;
        return;
    }
    outw((uint16_t)(io_base + LEG_QUEUE_NOTIFY), 0);
}

static int modern_features(void)
{
    uint32_t hi;
    uint8_t st;

    mmio_w8(common, CFG_STATUS, 0);
    for (hi = 0; hi < 10000u; hi++) {
        if (mmio_r8(common, CFG_STATUS) == 0) {
            break;
        }
        __asm__ volatile ("pause");
    }
    mmio_w8(common, CFG_STATUS, VIRTIO_STATUS_ACK);
    mmio_w8(common, CFG_STATUS, (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER));
    mmio_w32(common, CFG_DFSELECT, 1u);
    hi = mmio_r32(common, CFG_DF);
    if ((hi & (1u << (VIRTIO_F_VERSION_1 - 32u))) == 0) {
        return -1;
    }
    mmio_w32(common, CFG_GFSELECT, 0);
    mmio_w32(common, CFG_GF, 0);
    mmio_w32(common, CFG_GFSELECT, 1u);
    mmio_w32(common, CFG_GF, 1u << (VIRTIO_F_VERSION_1 - 32u));
    st = mmio_r8(common, CFG_STATUS);
    mmio_w8(common, CFG_STATUS, (uint8_t)(st | VIRTIO_STATUS_FEATURES_OK));
    st = mmio_r8(common, CFG_STATUS);
    if ((st & VIRTIO_STATUS_FEATURES_OK) == 0) {
        return -1;
    }
    return 0;
}

static int modern_queue(void)
{
    uint16_t size;
    uint16_t nq;

    mmio_w16(common, CFG_MSIX, MSIX_NONE);
    nq = mmio_r16(common, CFG_NUMQ);
    if (nq == 0) {
        return -1;
    }
    mmio_w16(common, CFG_QSEL, 0);
    mmio_w16(common, CFG_QMSIX, MSIX_NONE);
    size = mmio_r16(common, CFG_QSIZE);
    if (size > QSZ_MAX) {
        size = QSZ_MAX;
        mmio_w16(common, CFG_QSIZE, size);
    }
    if (vq_layout(size) != 0) {
        return -1;
    }
    q_notify_off = mmio_r16(common, CFG_QNOTIFY);
    mmio_w64(common, CFG_QDESC, dma_phys + desc_off);
    mmio_w64(common, CFG_QDRIVER, dma_phys + avail_off);
    mmio_w64(common, CFG_QDEVICE, dma_phys + used_off);
    mmio_w16(common, CFG_QENA, 1);
    vq_avail_init();
    mmio_w8(common, CFG_STATUS,
            (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER
                      | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK));
    return 0;
}

static int modern_init(uint8_t bus, uint8_t dev, uint8_t fn)
{
    if (pci_caps(bus, dev, fn) != 0) {
        return -1;
    }
    if (modern_features() != 0) {
        return -1;
    }
    if (modern_queue() != 0) {
        return -1;
    }
    modern = 1;
    return 0;
}

static int legacy_init(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t bar0;
    uint16_t size;
    unsigned i;

    bar0 = pci_bar_raw(bus, dev, fn, 0);
    if ((bar0 & 1u) == 0) {
        return -1;
    }
    io_base = (uint16_t)(bar0 & ~3u);
    if (io_base == 0) {
        return -1;
    }
    outb((uint16_t)(io_base + LEG_STATUS), 0);
    for (i = 0; i < 10000u; i++) {
        if (inb((uint16_t)(io_base + LEG_STATUS)) == 0) {
            break;
        }
        __asm__ volatile ("pause");
    }
    outb((uint16_t)(io_base + LEG_STATUS), VIRTIO_STATUS_ACK);
    outb((uint16_t)(io_base + LEG_STATUS),
         (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER));
    (void)inl((uint16_t)(io_base + LEG_HOST_FEATURES));
    outl((uint16_t)(io_base + LEG_GUEST_FEATURES), 0);
    outw((uint16_t)(io_base + LEG_QUEUE_SEL), 0);
    size = inw((uint16_t)(io_base + LEG_QUEUE_NUM));
    if (vq_layout(size) != 0) {
        return -1;
    }
    if ((dma_phys & (PAGE_SIZE - 1ull)) != 0) {
        return -1;
    }
    vq_avail_init();
    outl((uint16_t)(io_base + LEG_QUEUE_PFN), (uint32_t)(dma_phys / PAGE_SIZE));
    (void)inb((uint16_t)(io_base + LEG_ISR));
    outb((uint16_t)(io_base + LEG_STATUS),
         (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER
                   | VIRTIO_STATUS_DRIVER_OK));
    modern = 0;
    return 0;
}

