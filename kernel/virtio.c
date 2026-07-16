#include "virtio.h"
#include "bio.h"
#include "fb.h"
#include "io.h"
#include "pmm.h"
#include "serial.h"
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
#define PCI_DEV_NET_LEGACY 0x1000u
#define PCI_DEV_NET_MODERN 0x1041u

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
#define VIRTIO_BLK_T_OUT 1u
#define VIRTIO_BLK_T_FLUSH 4u
#define VIRTIO_BLK_S_OK 0u
#define VIRTIO_BLK_S_IOERR 1u
#define VIRTIO_BLK_F_FLUSH 0u
#define VIRTIO_NET_F_MAC 5u
#define VIRTIO_NET_TXQ 1u
#define VIRTIO_NET_HDR 12u
#define ETH_ZLEN 60u
#define UDP_PORT 5555u

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
static volatile uint8_t *devcfg;
static uint32_t notify_mult;
static uint32_t blk_secs;
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
static int wr_said;
static int flush_said;
static int say_row;
static int have_flush;
static uint16_t net_io;
static volatile uint8_t *net_common;
static volatile uint8_t *net_notify;
static volatile uint8_t *net_devcfg;
static uint32_t net_notify_mult;
static uint16_t net_qsz;
static uint16_t net_q_notify_off;
static uint64_t net_dma_phys;
static uint8_t *net_dma_virt;
static uint32_t net_desc_off;
static uint32_t net_avail_off;
static uint32_t net_used_off;
static uint32_t net_hdr_off;
static uint16_t net_avail_idx;
static uint16_t net_used_seen;
static int net_modern;
static uint8_t net_mac[6];

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

static int pci_find_net(uint8_t *bus_out, uint8_t *dev_out, uint8_t *fn_out)
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
                    && (did == PCI_DEV_NET_LEGACY || did == PCI_DEV_NET_MODERN)) {
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
    devcfg = 0;
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
            } else if (type == VIRTIO_PCI_CAP_DEVICE) {
                devcfg = map_bar(bus, dev, fn, bar, off);
            } else if (type == VIRTIO_PCI_CAP_ISR) {
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

static int vq_kick(void)
{
    vq_avail_ring()[avail_idx % qsz] = 0;
    __asm__ volatile ("mfence" ::: "memory");
    avail_idx++;
    *vq_avail_idx() = avail_idx;
    __asm__ volatile ("mfence" ::: "memory");
    notify_q();
    if (wait_used() != 0) {
        return -1;
    }
    __asm__ volatile ("mfence" ::: "memory");
    return 0;
}

static int modern_features(void)
{
    uint32_t hi;
    uint32_t lo;
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
    mmio_w32(common, CFG_DFSELECT, 0);
    lo = mmio_r32(common, CFG_DF);
    have_flush = ((lo & (1u << VIRTIO_BLK_F_FLUSH)) != 0) ? 1 : 0;
    mmio_w32(common, CFG_GFSELECT, 0);
    mmio_w32(common, CFG_GF, have_flush != 0 ? (1u << VIRTIO_BLK_F_FLUSH) : 0);
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
    uint32_t host;
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
    host = inl((uint16_t)(io_base + LEG_HOST_FEATURES));
    have_flush = ((host & (1u << VIRTIO_BLK_F_FLUSH)) != 0) ? 1 : 0;
    outl((uint16_t)(io_base + LEG_GUEST_FEATURES),
         have_flush != 0 ? (1u << VIRTIO_BLK_F_FLUSH) : 0);
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

static void copy_sec(void *dst, const void *src)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    unsigned i;

    for (i = 0; i < SEC_SIZE; i++) {
        d[i] = s[i];
    }
}

static int vq_read_one(uint64_t lba, void *dst)
{
    struct virtq_desc *d;
    struct virtio_blk_req *req;
    uint8_t *data;
    uint8_t *st;
    unsigned i;

    if (dma_virt == 0 || qsz < 3u) {
        return -1;
    }
    d = vq_desc();
    req = (struct virtio_blk_req *)(dma_virt + req_off);
    data = dma_virt + data_off;
    st = dma_virt + st_off;
    req->type = VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = lba;
    *st = 0xFFu;
    for (i = 0; i < SEC_SIZE; i++) {
        data[i] = 0;
    }

    d[0].addr = dma_phys + req_off;
    d[0].len = (uint32_t)sizeof(struct virtio_blk_req);
    d[0].flags = VIRTQ_DESC_F_NEXT;
    d[0].next = 1;
    d[1].addr = dma_phys + data_off;
    d[1].len = SEC_SIZE;
    d[1].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
    d[1].next = 2;
    d[2].addr = dma_phys + st_off;
    d[2].len = 1;
    d[2].flags = VIRTQ_DESC_F_WRITE;
    d[2].next = 0;

    if (vq_kick() != 0) {
        return -1;
    }
    if (*st == VIRTIO_BLK_S_IOERR || *st != VIRTIO_BLK_S_OK) {
        return -1;
    }
    if (dst != 0) {
        copy_sec(dst, data);
    }
    return 0;
}

static int vq_write_one(uint64_t lba, const void *src)
{
    struct virtq_desc *d;
    struct virtio_blk_req *req;
    uint8_t *data;
    uint8_t *st;

    if (dma_virt == 0 || qsz < 3u || src == 0) {
        return -1;
    }
    d = vq_desc();
    req = (struct virtio_blk_req *)(dma_virt + req_off);
    data = dma_virt + data_off;
    st = dma_virt + st_off;
    req->type = VIRTIO_BLK_T_OUT;
    req->reserved = 0;
    req->sector = lba;
    *st = 0xFFu;
    copy_sec(data, src);

    d[0].addr = dma_phys + req_off;
    d[0].len = (uint32_t)sizeof(struct virtio_blk_req);
    d[0].flags = VIRTQ_DESC_F_NEXT;
    d[0].next = 1;
    d[1].addr = dma_phys + data_off;
    d[1].len = SEC_SIZE;
    d[1].flags = VIRTQ_DESC_F_NEXT;
    d[1].next = 2;
    d[2].addr = dma_phys + st_off;
    d[2].len = 1;
    d[2].flags = VIRTQ_DESC_F_WRITE;
    d[2].next = 0;

    if (vq_kick() != 0) {
        return -1;
    }
    if (*st == VIRTIO_BLK_S_IOERR || *st != VIRTIO_BLK_S_OK) {
        return -1;
    }
    return 0;
}

static int vq_flush(void)
{
    struct virtq_desc *d;
    struct virtio_blk_req *req;
    uint8_t *st;

    if (dma_virt == 0 || qsz < 2u) {
        return -1;
    }
    d = vq_desc();
    req = (struct virtio_blk_req *)(dma_virt + req_off);
    st = dma_virt + st_off;
    req->type = VIRTIO_BLK_T_FLUSH;
    req->reserved = 0;
    req->sector = 0;
    *st = 0xFFu;

    d[0].addr = dma_phys + req_off;
    d[0].len = (uint32_t)sizeof(struct virtio_blk_req);
    d[0].flags = VIRTQ_DESC_F_NEXT;
    d[0].next = 1;
    d[1].addr = dma_phys + st_off;
    d[1].len = 1;
    d[1].flags = VIRTQ_DESC_F_WRITE;
    d[1].next = 0;

    if (vq_kick() != 0) {
        return -1;
    }
    if (*st == VIRTIO_BLK_S_IOERR || *st != VIRTIO_BLK_S_OK) {
        return -1;
    }
    return 0;
}

static char hex_digit(unsigned n)
{
    n &= 0xFu;
    if (n < 10u) {
        return (char)('0' + n);
    }
    return (char)('a' + (n - 10u));
}

static int virt_say(int row, const char *msg)
{
    say_row = row;
    vga_write_at(row, 0, msg);
    fb_draw_text(8, 40, msg);
    return row + 1;
}

static void virt_say_wr(void)
{
    int row;

    if (wr_said != 0) {
        return;
    }
    wr_said = 1;
    row = say_row;
    if (row <= 0 || row >= VGA_HEIGHT) {
        row = 2;
    }
    vga_write_at(row, 10, "virt wr");
    fb_draw_text(8, 56, "virt wr");
}

static void virt_say_flush(void)
{
    int row;

    if (flush_said != 0) {
        return;
    }
    flush_said = 1;
    row = say_row;
    if (row <= 0 || row >= VGA_HEIGHT) {
        row = 2;
    }
    vga_write_at(row, 18, "virt flush");
    fb_draw_text(128, 56, "virt flush");
}

int virtio_ready(void)
{
    return live;
}

static int virt_past(uint32_t lba, unsigned sectors)
{
    if (sectors == 0) {
        return 1;
    }
    if (blk_secs == 0) {
        return 0;
    }
    if (lba >= blk_secs || sectors > blk_secs - lba) {
        return 1;
    }
    return 0;
}

static uint32_t virt_capacity(void)
{
    uint32_t lo;
    uint32_t hi;

    if (modern != 0 && devcfg != 0) {
        lo = mmio_r32(devcfg, 0);
        hi = mmio_r32(devcfg, 4);
        if (hi != 0) {
            return 0xFFFFFFFFu;
        }
        return lo;
    }
    if (io_base != 0) {
        lo = inl((uint16_t)(io_base + 20u));
        hi = inl((uint16_t)(io_base + 24u));
        if (hi != 0) {
            return 0xFFFFFFFFu;
        }
        return lo;
    }
    return 0;
}

int virtio_read(uint32_t lba, unsigned sectors, void *dst)
{
    uint8_t *buf = (uint8_t *)dst;
    unsigned s;

    if (live == 0 || dma_virt == 0 || dst == 0 || virt_past(lba, sectors)) {
        return -1;
    }
    for (s = 0; s < sectors; s++) {
        if (vq_read_one((uint64_t)lba + s, buf + s * SEC_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

int virtio_write(uint32_t lba, unsigned sectors, const void *src)
{
    const uint8_t *buf = (const uint8_t *)src;
    unsigned s;

    if (live == 0 || dma_virt == 0 || src == 0 || virt_past(lba, sectors)) {
        return -1;
    }
    for (s = 0; s < sectors; s++) {
        if (vq_write_one((uint64_t)lba + s, buf + s * SEC_SIZE) != 0) {
            return -1;
        }
    }
    virt_say_wr();
    return 0;
}

static int virtio_flush(void)
{
    if (live == 0 || dma_virt == 0) {
        return -1;
    }
    if (vq_flush() != 0) {
        return -1;
    }
    virt_say_flush();
    return 0;
}

int virtio_init(int row)
{
    uint8_t bus;
    uint8_t dev;
    uint8_t fn;
    const uint8_t *data;
    char msg[12];

    io_base = 0;
    common = 0;
    notify = 0;
    devcfg = 0;
    notify_mult = 0;
    blk_secs = 0;
    qsz = 0;
    q_notify_off = 0;
    dma_phys = 0;
    dma_virt = 0;
    modern = 0;
    live = 0;
    wr_said = 0;
    flush_said = 0;
    say_row = 0;
    have_flush = 0;
    avail_idx = 0;
    used_seen = 0;

    if (row >= VGA_HEIGHT - 1) {
        return row;
    }
    if (pci_find_blk(&bus, &dev, &fn) != 0) {
        return virt_say(row, "virt none");
    }
    pci_enable(bus, dev, fn);
    if (modern_init(bus, dev, fn) != 0) {
        common = 0;
        notify = 0;
        devcfg = 0;
        dma_phys = 0;
        dma_virt = 0;
        if (legacy_init(bus, dev, fn) != 0) {
            return virt_say(row, "virt fail");
        }
    }
    blk_secs = virt_capacity();
    if (vq_read_one(0, 0) != 0) {
        return virt_say(row, "virt fail");
    }
    data = dma_virt + data_off;
    msg[0] = 'v';
    msg[1] = 'i';
    msg[2] = 'r';
    msg[3] = 't';
    msg[4] = ' ';
    msg[5] = hex_digit((unsigned)data[510] >> 4);
    msg[6] = hex_digit((unsigned)data[510]);
    msg[7] = hex_digit((unsigned)data[511] >> 4);
    msg[8] = hex_digit((unsigned)data[511]);
    msg[9] = '\0';
    live = 1;
    (void)bdev_register("virt", virtio_read, virtio_write);
    bdev_set_flush(virtio_flush);
    if (blk_secs != 0) {
        bdev_set_sectors(blk_secs);
    }
    return virt_say(row, msg);
}

static int pci_net_caps(uint8_t bus, uint8_t dev, uint8_t fn)
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

    net_common = 0;
    net_notify = 0;
    net_devcfg = 0;
    net_notify_mult = 0;
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
                net_common = map_bar(bus, dev, fn, bar, off);
            } else if (type == VIRTIO_PCI_CAP_NOTIFY) {
                net_notify = map_bar(bus, dev, fn, bar, off);
                net_notify_mult = pci_read32(bus, dev, fn, (uint8_t)(cap + 16u));
            } else if (type == VIRTIO_PCI_CAP_DEVICE) {
                net_devcfg = map_bar(bus, dev, fn, bar, off);
            } else if (type == VIRTIO_PCI_CAP_ISR) {
                (void)map_bar(bus, dev, fn, bar, off);
            }
        }
        cap = (uint8_t)(d0 >> 8);
        n++;
        if (cap == 0) {
            break;
        }
    }
    if (net_common == 0 || net_devcfg == 0) {
        return -1;
    }
    return 0;
}

static int net_modern_features(void)
{
    uint32_t hi;
    uint32_t lo;
    uint8_t st;

    mmio_w8(net_common, CFG_STATUS, 0);
    for (hi = 0; hi < 10000u; hi++) {
        if (mmio_r8(net_common, CFG_STATUS) == 0) {
            break;
        }
        __asm__ volatile ("pause");
    }
    mmio_w8(net_common, CFG_STATUS, VIRTIO_STATUS_ACK);
    mmio_w8(net_common, CFG_STATUS,
            (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER));
    mmio_w32(net_common, CFG_DFSELECT, 1u);
    hi = mmio_r32(net_common, CFG_DF);
    if ((hi & (1u << (VIRTIO_F_VERSION_1 - 32u))) == 0) {
        return -1;
    }
    mmio_w32(net_common, CFG_DFSELECT, 0);
    lo = mmio_r32(net_common, CFG_DF);
    if ((lo & (1u << VIRTIO_NET_F_MAC)) == 0) {
        return -1;
    }
    mmio_w32(net_common, CFG_GFSELECT, 0);
    mmio_w32(net_common, CFG_GF, 1u << VIRTIO_NET_F_MAC);
    mmio_w32(net_common, CFG_GFSELECT, 1u);
    mmio_w32(net_common, CFG_GF, 1u << (VIRTIO_F_VERSION_1 - 32u));
    st = mmio_r8(net_common, CFG_STATUS);
    mmio_w8(net_common, CFG_STATUS, (uint8_t)(st | VIRTIO_STATUS_FEATURES_OK));
    st = mmio_r8(net_common, CFG_STATUS);
    if ((st & VIRTIO_STATUS_FEATURES_OK) == 0) {
        return -1;
    }
    return 0;
}

static int net_modern_init(uint8_t bus, uint8_t dev, uint8_t fn)
{
    if (pci_net_caps(bus, dev, fn) != 0) {
        return -1;
    }
    if (net_modern_features() != 0) {
        return -1;
    }
    return 0;
}

static int net_legacy_init(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t bar0;
    uint32_t host;
    unsigned i;

    bar0 = pci_bar_raw(bus, dev, fn, 0);
    if ((bar0 & 1u) == 0) {
        return -1;
    }
    net_io = (uint16_t)(bar0 & ~3u);
    if (net_io == 0) {
        return -1;
    }
    outb((uint16_t)(net_io + LEG_STATUS), 0);
    for (i = 0; i < 10000u; i++) {
        if (inb((uint16_t)(net_io + LEG_STATUS)) == 0) {
            break;
        }
        __asm__ volatile ("pause");
    }
    outb((uint16_t)(net_io + LEG_STATUS), VIRTIO_STATUS_ACK);
    outb((uint16_t)(net_io + LEG_STATUS),
         (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER));
    host = inl((uint16_t)(net_io + LEG_HOST_FEATURES));
    if ((host & (1u << VIRTIO_NET_F_MAC)) == 0) {
        return -1;
    }
    outl((uint16_t)(net_io + LEG_GUEST_FEATURES), 1u << VIRTIO_NET_F_MAC);
    return 0;
}

static int net_read_mac(uint8_t mac[6])
{
    unsigned i;
    unsigned nz;

    if (net_devcfg != 0) {
        for (i = 0; i < 6u; i++) {
            mac[i] = mmio_r8(net_devcfg, i);
        }
    } else if (net_io != 0) {
        for (i = 0; i < 6u; i++) {
            mac[i] = inb((uint16_t)(net_io + 20u + i));
        }
    } else {
        return -1;
    }
    nz = 0;
    for (i = 0; i < 6u; i++) {
        if (mac[i] != 0) {
            nz = 1;
        }
    }
    if (nz == 0) {
        return -1;
    }
    return 0;
}

static void net_fmt_mac(char *msg, const uint8_t *mac)
{
    unsigned i;
    unsigned p;

    msg[0] = 'n';
    msg[1] = 'e';
    msg[2] = 't';
    msg[3] = ' ';
    p = 4;
    for (i = 0; i < 6u; i++) {
        if (i != 0) {
            msg[p] = ':';
            p++;
        }
        msg[p] = hex_digit((unsigned)mac[i] >> 4);
        msg[p + 1] = hex_digit((unsigned)mac[i]);
        p += 2;
    }
    msg[p] = '\0';
}

static int net_say_at(int row, int fb_y, const char *msg)
{
    unsigned n;

    vga_write_at(row, 0, msg);
    fb_draw_text(8, fb_y, msg);
    if (msg != 0) {
        for (n = 0; msg[n] != '\0'; n++) {
        }
        serial_write(msg, n);
        serial_putc('\n');
    }
    return row + 1;
}

static int net_say(int row, const char *msg)
{
    return net_say_at(row, 72, msg);
}

static int net_vq_layout(uint16_t size)
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
    net_qsz = size;
    net_desc_off = 0;
    desc_bytes = (uint32_t)size * 16u;
    net_avail_off = desc_bytes;
    avail_bytes = 6u + 2u * (uint32_t)size;
    net_used_off = align_up32(net_avail_off + avail_bytes, (uint32_t)PAGE_SIZE);
    used_bytes = 6u + 8u * (uint32_t)size;
    net_hdr_off = align_up32(net_used_off + used_bytes, 16u);
    total = net_hdr_off + VIRTIO_NET_HDR + ETH_ZLEN;
    pages = ((uint64_t)total + PAGE_SIZE - 1ull) / PAGE_SIZE;
    if (pages == 0) {
        pages = 1;
    }
    net_dma_phys = pmm_alloc_span(pages);
    if (net_dma_phys == 0) {
        net_dma_phys = pmm_alloc_above(DMA_MIN);
        if (net_dma_phys == 0 || pages > 1) {
            return -1;
        }
    }
    net_dma_virt = (uint8_t *)(uintptr_t)phys_to_virt(net_dma_phys);
    for (i = 0; i < pages * PAGE_SIZE; i++) {
        net_dma_virt[i] = 0;
    }
    net_avail_idx = 0;
    net_used_seen = 0;
    return 0;
}

static void net_avail_init(void)
{
    *(volatile uint16_t *)(net_dma_virt + net_avail_off) = VIRTQ_AVAIL_F_NO_INTERRUPT;
    *(volatile uint16_t *)(net_dma_virt + net_avail_off + 2u) = 0;
}

static int net_modern_tx_queue(void)
{
    uint16_t size;
    uint16_t nq;

    if (net_common == 0 || net_notify == 0) {
        return -1;
    }
    mmio_w16(net_common, CFG_MSIX, MSIX_NONE);
    nq = mmio_r16(net_common, CFG_NUMQ);
    if (nq < 2u) {
        return -1;
    }
    mmio_w16(net_common, CFG_QSEL, VIRTIO_NET_TXQ);
    mmio_w16(net_common, CFG_QMSIX, MSIX_NONE);
    size = mmio_r16(net_common, CFG_QSIZE);
    if (size > QSZ_MAX) {
        size = QSZ_MAX;
        mmio_w16(net_common, CFG_QSIZE, size);
    }
    if (net_vq_layout(size) != 0) {
        return -1;
    }
    net_q_notify_off = mmio_r16(net_common, CFG_QNOTIFY);
    mmio_w64(net_common, CFG_QDESC, net_dma_phys + net_desc_off);
    mmio_w64(net_common, CFG_QDRIVER, net_dma_phys + net_avail_off);
    mmio_w64(net_common, CFG_QDEVICE, net_dma_phys + net_used_off);
    mmio_w16(net_common, CFG_QENA, 1);
    net_avail_init();
    mmio_w8(net_common, CFG_STATUS,
            (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER
                      | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK));
    net_modern = 1;
    return 0;
}

static int net_legacy_tx_queue(void)
{
    uint16_t size;

    if (net_io == 0) {
        return -1;
    }
    outw((uint16_t)(net_io + LEG_QUEUE_SEL), VIRTIO_NET_TXQ);
    size = inw((uint16_t)(net_io + LEG_QUEUE_NUM));
    if (net_vq_layout(size) != 0) {
        return -1;
    }
    if ((net_dma_phys & (PAGE_SIZE - 1ull)) != 0) {
        return -1;
    }
    net_avail_init();
    outl((uint16_t)(net_io + LEG_QUEUE_PFN), (uint32_t)(net_dma_phys / PAGE_SIZE));
    (void)inb((uint16_t)(net_io + LEG_ISR));
    outb((uint16_t)(net_io + LEG_STATUS),
         (uint8_t)(VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER
                   | VIRTIO_STATUS_DRIVER_OK));
    net_modern = 0;
    return 0;
}

static void net_notify_q(void)
{
    volatile uint16_t *n;
    uint32_t off;

    if (net_modern != 0) {
        off = (uint32_t)net_q_notify_off * net_notify_mult;
        n = (volatile uint16_t *)(net_notify + off);
        *n = VIRTIO_NET_TXQ;
        return;
    }
    outw((uint16_t)(net_io + LEG_QUEUE_NOTIFY), VIRTIO_NET_TXQ);
}

static int net_vq_kick(void)
{
    volatile uint16_t *ring;
    volatile uint16_t *idx;
    volatile uint16_t *used;
    unsigned i;

    ring = (volatile uint16_t *)(net_dma_virt + net_avail_off + 4u);
    idx = (volatile uint16_t *)(net_dma_virt + net_avail_off + 2u);
    used = (volatile uint16_t *)(net_dma_virt + net_used_off + 2u);
    ring[net_avail_idx % net_qsz] = 0;
    __asm__ volatile ("mfence" ::: "memory");
    net_avail_idx++;
    *idx = net_avail_idx;
    __asm__ volatile ("mfence" ::: "memory");
    net_notify_q();
    for (i = 0; i < WAIT_MAX; i++) {
        __asm__ volatile ("mfence" ::: "memory");
        if (*used != net_used_seen) {
            net_used_seen = *used;
            return 0;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static uint16_t ip_csum(const uint8_t *p, unsigned n)
{
    uint32_t s;
    unsigned i;

    s = 0;
    for (i = 0; i + 1u < n; i += 2u) {
        s += ((uint32_t)p[i] << 8) | p[i + 1u];
    }
    if (i < n) {
        s += (uint32_t)p[i] << 8;
    }
    while ((s >> 16) != 0) {
        s = (s & 0xFFFFu) + (s >> 16);
    }
    return (uint16_t)~s;
}

static unsigned net_build_frame(uint8_t *f, const uint8_t *src_mac)
{
    unsigned i;
    uint8_t *ip;
    uint8_t *udp;
    uint16_t tot;
    uint16_t csum;

    /* QEMU user-net gateway 10.0.2.2, MAC 52:55:0a:00:02:02. */
    f[0] = 0x52;
    f[1] = 0x55;
    f[2] = 0x0a;
    f[3] = 0x00;
    f[4] = 0x02;
    f[5] = 0x02;
    for (i = 0; i < 6u; i++) {
        f[6 + i] = src_mac[i];
    }
    f[12] = 0x08;
    f[13] = 0x00;
    ip = f + 14;
    tot = 20u + 8u + 2u;
    ip[0] = 0x45;
    ip[1] = 0x00;
    ip[2] = (uint8_t)(tot >> 8);
    ip[3] = (uint8_t)tot;
    ip[4] = 0;
    ip[5] = 0;
    ip[6] = 0;
    ip[7] = 0;
    ip[8] = 64;
    ip[9] = 17;
    ip[10] = 0;
    ip[11] = 0;
    ip[12] = 10;
    ip[13] = 0;
    ip[14] = 2;
    ip[15] = 15;
    ip[16] = 10;
    ip[17] = 0;
    ip[18] = 2;
    ip[19] = 2;
    csum = ip_csum(ip, 20);
    ip[10] = (uint8_t)(csum >> 8);
    ip[11] = (uint8_t)csum;
    udp = ip + 20;
    udp[0] = (uint8_t)(UDP_PORT >> 8);
    udp[1] = (uint8_t)UDP_PORT;
    udp[2] = (uint8_t)(UDP_PORT >> 8);
    udp[3] = (uint8_t)UDP_PORT;
    udp[4] = 0;
    udp[5] = 10;
    udp[6] = 0;
    udp[7] = 0;
    udp[8] = 'h';
    udp[9] = 'i';
    for (i = 14u + tot; i < ETH_ZLEN; i++) {
        f[i] = 0;
    }
    return ETH_ZLEN;
}

static int net_tx_udp(void)
{
    struct virtq_desc *d;
    uint8_t *hdr;
    unsigned n;
    unsigned i;

    if (net_dma_virt == 0 || net_qsz < 2u) {
        return -1;
    }
    d = (struct virtq_desc *)(net_dma_virt + net_desc_off);
    hdr = net_dma_virt + net_hdr_off;
    for (i = 0; i < VIRTIO_NET_HDR; i++) {
        hdr[i] = 0;
    }
    n = net_build_frame(hdr + VIRTIO_NET_HDR, net_mac);
    d[0].addr = net_dma_phys + net_hdr_off;
    d[0].len = VIRTIO_NET_HDR;
    d[0].flags = VIRTQ_DESC_F_NEXT;
    d[0].next = 1;
    d[1].addr = net_dma_phys + net_hdr_off + VIRTIO_NET_HDR;
    d[1].len = n;
    d[1].flags = 0;
    d[1].next = 0;
    return net_vq_kick();
}

int virtio_net_init(int row)
{
    uint8_t bus;
    uint8_t dev;
    uint8_t fn;
    uint8_t mac[6];
    char msg[22];

    net_io = 0;
    net_common = 0;
    net_devcfg = 0;

    if (row >= VGA_HEIGHT - 1) {
        return row;
    }
    if (pci_find_net(&bus, &dev, &fn) != 0) {
        return net_say(row, "net none");
    }
    pci_enable(bus, dev, fn);
    if (net_modern_init(bus, dev, fn) != 0) {
        net_common = 0;
        net_devcfg = 0;
        if (net_legacy_init(bus, dev, fn) != 0) {
            return net_say(row, "net fail");
        }
    }
    if (net_read_mac(mac) != 0) {
        return net_say(row, "net fail");
    }
    net_fmt_mac(msg, mac);
    return net_say(row, msg);
}
