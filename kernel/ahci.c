#include "ahci.h"
#include "bio.h"
#include "fb.h"
#include "io.h"
#include "pmm.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
#define PCI_ENABLE 0x80000000u
#define PCI_CLASS_AHCI 0x0106u
#define PCI_CMD_MEM 0x0002u
#define PCI_CMD_BUSMASTER 0x0004u

#define GHC_AE 0x80000000u
#define GHC_IE 0x00000002u

#define PORT_CLB 0x00
#define PORT_CLBU 0x04
#define PORT_FB 0x08
#define PORT_FBU 0x0C
#define PORT_IS 0x10
#define PORT_IE 0x14
#define PORT_CMD 0x18
#define PORT_TFD 0x20
#define PORT_SIG 0x24
#define PORT_SSTS 0x28
#define PORT_SCTL 0x2C
#define PORT_SERR 0x30
#define PORT_CI 0x38

#define CMD_ST (1u << 0)
#define CMD_SUD (1u << 1)
#define CMD_POD (1u << 2)
#define CMD_FRE (1u << 4)
#define CMD_FR (1u << 14)
#define CMD_CR (1u << 15)

#define SSTS_DET 0x0Fu
#define SSTS_DET_PRESENT 3u
#define SSTS_IPM 0xF00u
#define SSTS_IPM_ACTIVE 0x100u

#define TFD_ERR 0x01u
#define TFD_DRQ 0x08u
#define TFD_BSY 0x80u
#define TFD_BUSY (TFD_BSY | TFD_DRQ)
#define IS_TFES (1u << 30)

#define SIG_ATA 0x00000101u
#define ATA_IDENTIFY 0xECu
#define ATA_READ_DMA_EXT 0x25u
#define ATA_WRITE_DMA_EXT 0x35u
#define ATA_FLUSH_CACHE 0xE7u
#define HDR_WRITE (1u << 6)

#define FIS_H2D 0x27u
#define FIS_CMD (1u << 7)
#define CFL_H2D 5u
#define PRDT_IOC (1u << 31)

#define PAGE_SIZE 0x1000ull
#define DMA_MIN 0x200000ull
#define CL_OFF 0x000u
#define FIS_OFF 0x400u
#define CT_OFF 0x500u
#define DATA_OFF 0x800u
#define SEC_SIZE 512u
#define WAIT_MAX 1000000u
#define ABAR_MAP 0x2000u

struct cmd_hdr {
    uint16_t flags;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed));

struct prdt_ent {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc;
} __attribute__((packed));

static volatile uint32_t *hba;
static uint64_t dma_phys;
static uint8_t *dma_virt;
static unsigned used_port;
static uint32_t blk_secs;
static int live;
static int wr_said;
static int flush_said;
static int say_row;

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

static int pci_find_ahci(uint8_t *bus_out, uint8_t *dev_out, uint8_t *fn_out)
{
    unsigned bus;
    unsigned dev;
    unsigned fn;

    for (bus = 0; bus < 8u; bus++) {
        for (dev = 0; dev < 32u; dev++) {
            for (fn = 0; fn < 8u; fn++) {
                uint32_t id;
                uint32_t cc;

                id = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)fn, 0);
                if ((id & 0xFFFFu) == 0xFFFFu) {
                    if (fn == 0) {
                        break;
                    }
                    continue;
                }
                cc = pci_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)fn, 0x08);
                if (((cc >> 16) & 0xFFFFu) == PCI_CLASS_AHCI) {
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

static uint64_t pci_abar(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t bar;
    uint32_t hi;
    uint64_t addr;

    bar = pci_read32(bus, dev, fn, 0x24);
    if ((bar & 1u) != 0) {
        return 0;
    }
    addr = (uint64_t)(bar & ~0xFu);
    if (((bar >> 1) & 3u) == 2u) {
        hi = pci_read32(bus, dev, fn, 0x28);
        addr |= (uint64_t)hi << 32;
    }
    return addr;
}

static void pci_enable(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t cmd;

    cmd = pci_read32(bus, dev, fn, 0x04);
    cmd |= PCI_CMD_MEM | PCI_CMD_BUSMASTER;
    pci_write32(bus, dev, fn, 0x04, cmd);
}

static uint32_t hba_read(unsigned off)
{
    return hba[off / 4u];
}

static void hba_write(unsigned off, uint32_t value)
{
    hba[off / 4u] = value;
}

static uint32_t port_read(unsigned p, unsigned off)
{
    return hba_read(0x100u + p * 0x80u + off);
}

static void port_write(unsigned p, unsigned off, uint32_t value)
{
    hba_write(0x100u + p * 0x80u + off, value);
}

static int wait_clear(unsigned p, unsigned off, uint32_t bits)
{
    unsigned i;
    uint32_t v;

    for (i = 0; i < WAIT_MAX; i++) {
        v = port_read(p, off);
        if ((v & bits) == 0) {
            return 0;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static int wait_set(unsigned p, unsigned off, uint32_t bits)
{
    unsigned i;
    uint32_t v;

    for (i = 0; i < WAIT_MAX; i++) {
        v = port_read(p, off);
        if ((v & bits) == bits) {
            return 0;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static int port_stop(unsigned p)
{
    uint32_t cmd;

    cmd = port_read(p, PORT_CMD);
    cmd &= (uint32_t)~CMD_ST;
    port_write(p, PORT_CMD, cmd);
    if (wait_clear(p, PORT_CMD, CMD_CR) != 0) {
        return -1;
    }
    cmd = port_read(p, PORT_CMD);
    cmd &= (uint32_t)~CMD_FRE;
    port_write(p, PORT_CMD, cmd);
    if (wait_clear(p, PORT_CMD, CMD_FR) != 0) {
        return -1;
    }
    return 0;
}

static int port_start(unsigned p)
{
    uint32_t cmd;

    if (wait_clear(p, PORT_CMD, CMD_CR) != 0) {
        return -1;
    }
    cmd = port_read(p, PORT_CMD);
    cmd |= CMD_FRE;
    port_write(p, PORT_CMD, cmd);
    if (wait_set(p, PORT_CMD, CMD_FR) != 0) {
        return -1;
    }
    cmd = port_read(p, PORT_CMD);
    cmd |= CMD_ST;
    port_write(p, PORT_CMD, cmd);
    return 0;
}

static int port_ready(unsigned p)
{
    unsigned i;
    uint32_t ssts;
    uint32_t det;
    uint32_t cmd;
    uint32_t sctl;

    cmd = port_read(p, PORT_CMD);
    cmd |= CMD_SUD | CMD_POD;
    port_write(p, PORT_CMD, cmd);

    for (i = 0; i < WAIT_MAX; i++) {
        ssts = port_read(p, PORT_SSTS);
        det = ssts & SSTS_DET;
        if (det == SSTS_DET_PRESENT) {
            return 0;
        }
        __asm__ volatile ("pause");
    }

    sctl = port_read(p, PORT_SCTL);
    port_write(p, PORT_SCTL, (sctl & ~0xFu) | 1u);
    for (i = 0; i < 10000u; i++) {
        __asm__ volatile ("pause");
    }
    port_write(p, PORT_SCTL, sctl & ~0xFu);
    for (i = 0; i < WAIT_MAX; i++) {
        ssts = port_read(p, PORT_SSTS);
        if ((ssts & SSTS_DET) == SSTS_DET_PRESENT) {
            return 0;
        }
        __asm__ volatile ("pause");
    }
    return -1;
}

static int pick_port(uint32_t pi)
{
    unsigned p;
    uint32_t sig;
    uint32_t ssts;

    for (p = 0; p < 32u; p++) {
        if ((pi & (1u << p)) == 0) {
            continue;
        }
        if (port_ready(p) != 0) {
            continue;
        }
        ssts = port_read(p, PORT_SSTS);
        if ((ssts & SSTS_IPM) != 0 && (ssts & SSTS_IPM) != SSTS_IPM_ACTIVE) {
            continue;
        }
        sig = port_read(p, PORT_SIG);
        if (sig == SIG_ATA || sig == 0xFFFFFFFFu) {
            return (int)p;
        }
    }
    return -1;
}

static int dma_init(void)
{
    uint64_t i;

    dma_phys = pmm_alloc_above(DMA_MIN);
    if (dma_phys == 0) {
        dma_phys = pmm_alloc();
    }
    if (dma_phys == 0) {
        return -1;
    }
    dma_virt = (uint8_t *)(uintptr_t)phys_to_virt(dma_phys);
    for (i = 0; i < PAGE_SIZE; i++) {
        dma_virt[i] = 0;
    }
    return 0;
}

static int port_setup(unsigned p)
{
    if (port_stop(p) != 0) {
        return -1;
    }
    port_write(p, PORT_CLB, (uint32_t)(dma_phys + CL_OFF));
    port_write(p, PORT_CLBU, (uint32_t)((dma_phys + CL_OFF) >> 32));
    port_write(p, PORT_FB, (uint32_t)(dma_phys + FIS_OFF));
    port_write(p, PORT_FBU, (uint32_t)((dma_phys + FIS_OFF) >> 32));
    port_write(p, PORT_SERR, 0xFFFFFFFFu);
    port_write(p, PORT_IS, 0xFFFFFFFFu);
    port_write(p, PORT_IE, 0);
    return port_start(p);
}

static int wait_ci(unsigned p)
{
    unsigned i;
    uint32_t ci;
    uint32_t is;
    uint32_t tfd;

    for (i = 0; i < WAIT_MAX; i++) {
        ci = port_read(p, PORT_CI);
        is = port_read(p, PORT_IS);
        tfd = port_read(p, PORT_TFD);
        if ((is & IS_TFES) != 0 || (tfd & TFD_ERR) != 0) {
            port_write(p, PORT_IS, 0xFFFFFFFFu);
            port_write(p, PORT_SERR, 0xFFFFFFFFu);
            return -1;
        }
        if ((ci & 1u) == 0) {
            return 0;
        }
        __asm__ volatile ("pause");
    }
    port_write(p, PORT_IS, 0xFFFFFFFFu);
    port_write(p, PORT_SERR, 0xFFFFFFFFu);
    return -1;
}

static uint32_t ident_sectors(const uint8_t *id)
{
    uint32_t lba48;
    uint32_t lba28;

    lba48 = (uint32_t)id[200] | ((uint32_t)id[201] << 8) |
            ((uint32_t)id[202] << 16) | ((uint32_t)id[203] << 24);
    lba28 = (uint32_t)id[120] | ((uint32_t)id[121] << 8) |
            ((uint32_t)id[122] << 16) | ((uint32_t)id[123] << 24);
    if (lba48 != 0) {
        return lba48;
    }
    return lba28;
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

static int port_issue(unsigned p, uint8_t ata_cmd, uint64_t lba, uint16_t count,
                      int write, uint16_t prdtl)
{
    struct cmd_hdr *hdr;
    struct prdt_ent *prdt;
    uint8_t *cfis;
    uint8_t *data;
    unsigned i;
    uint32_t tfd;
    uint32_t is;
    uint32_t ci;

    hdr = (struct cmd_hdr *)(dma_virt + CL_OFF);
    cfis = dma_virt + CT_OFF;
    prdt = (struct prdt_ent *)(dma_virt + CT_OFF + 0x80);
    data = dma_virt + DATA_OFF;

    if (write == 0 && prdtl != 0) {
        for (i = 0; i < SEC_SIZE; i++) {
            data[i] = 0;
        }
    }
    for (i = 0; i < 64u; i++) {
        cfis[i] = 0;
    }

    cfis[0] = FIS_H2D;
    cfis[1] = FIS_CMD;
    cfis[2] = ata_cmd;
    cfis[4] = (uint8_t)lba;
    cfis[5] = (uint8_t)(lba >> 8);
    cfis[6] = (uint8_t)(lba >> 16);
    cfis[7] = 0x40;
    cfis[8] = (uint8_t)(lba >> 24);
    cfis[9] = (uint8_t)(lba >> 32);
    cfis[10] = (uint8_t)(lba >> 40);
    cfis[12] = (uint8_t)count;
    cfis[13] = (uint8_t)(count >> 8);

    if (prdtl != 0) {
        prdt->dba = (uint32_t)(dma_phys + DATA_OFF);
        prdt->dbau = (uint32_t)((dma_phys + DATA_OFF) >> 32);
        prdt->reserved = 0;
        prdt->dbc = (SEC_SIZE - 1u) | PRDT_IOC;
    }

    hdr->flags = CFL_H2D;
    if (write != 0) {
        hdr->flags |= HDR_WRITE;
    }
    hdr->prdtl = prdtl;
    hdr->prdbc = 0;
    hdr->ctba = (uint32_t)(dma_phys + CT_OFF);
    hdr->ctbau = (uint32_t)((dma_phys + CT_OFF) >> 32);
    hdr->reserved[0] = 0;
    hdr->reserved[1] = 0;
    hdr->reserved[2] = 0;
    hdr->reserved[3] = 0;

    if (wait_clear(p, PORT_TFD, TFD_BUSY) != 0) {
        return -1;
    }
    port_write(p, PORT_IS, 0xFFFFFFFFu);
    __asm__ volatile ("mfence" ::: "memory");
    port_write(p, PORT_CI, 1u);

    if (wait_ci(p) != 0) {
        return -1;
    }
    is = port_read(p, PORT_IS);
    tfd = port_read(p, PORT_TFD);
    ci = port_read(p, PORT_CI);
    (void)ci;
    if ((is & IS_TFES) != 0 || (tfd & TFD_ERR) != 0) {
        port_write(p, PORT_IS, 0xFFFFFFFFu);
        port_write(p, PORT_SERR, 0xFFFFFFFFu);
        return -1;
    }
    __asm__ volatile ("mfence" ::: "memory");
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

static int ahci_say(int row, const char *msg)
{
    say_row = row;
    vga_write_at(row, 0, msg);
    fb_draw_text(8, 8, msg);
    return row + 1;
}

static void ahci_say_wr(void)
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
    vga_write_at(row, 10, "ahci wr");
    fb_draw_text(8, 24, "ahci wr");
}

static void ahci_say_flush(void)
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
    vga_write_at(row, 18, "ahci flush");
    fb_draw_text(128, 24, "ahci flush");
}

int ahci_ready(void)
{
    return live;
}

static int ahci_past(uint32_t lba, unsigned sectors)
{
    uint32_t ssts;

    if (sectors == 0) {
        return 1;
    }
    ssts = port_read(used_port, PORT_SSTS);
    if ((ssts & SSTS_DET) != SSTS_DET_PRESENT) {
        return 1;
    }
    if (blk_secs != 0 && (lba >= blk_secs || sectors > blk_secs - lba)) {
        return 1;
    }
    return 0;
}

int ahci_read(uint32_t lba, unsigned sectors, void *dst)
{
    uint8_t *buf = (uint8_t *)dst;
    unsigned s;

    if (live == 0 || dma_virt == 0 || dst == 0 || ahci_past(lba, sectors)) {
        return -1;
    }
    for (s = 0; s < sectors; s++) {
        if (port_issue(used_port, ATA_READ_DMA_EXT, (uint64_t)lba + s, 1, 0, 1) != 0) {
            return -1;
        }
        copy_sec(buf + s * SEC_SIZE, dma_virt + DATA_OFF);
    }
    return 0;
}

int ahci_write(uint32_t lba, unsigned sectors, const void *src)
{
    const uint8_t *buf = (const uint8_t *)src;
    unsigned s;

    if (live == 0 || dma_virt == 0 || src == 0 || ahci_past(lba, sectors)) {
        return -1;
    }
    for (s = 0; s < sectors; s++) {
        copy_sec(dma_virt + DATA_OFF, buf + s * SEC_SIZE);
        if (port_issue(used_port, ATA_WRITE_DMA_EXT, (uint64_t)lba + s, 1, 1, 1) != 0) {
            return -1;
        }
    }
    ahci_say_wr();
    return 0;
}

static int ahci_flush(void)
{
    if (live == 0 || dma_virt == 0) {
        return -1;
    }
    if (port_issue(used_port, ATA_FLUSH_CACHE, 0, 0, 0, 0) != 0) {
        return -1;
    }
    ahci_say_flush();
    return 0;
}

int ahci_init(int row)
{
    uint8_t bus;
    uint8_t dev;
    uint8_t fn;
    uint64_t abar;
    uint32_t ghc;
    uint32_t pi;
    int p;
    const uint8_t *data;
    char msg[12];

    hba = 0;
    dma_phys = 0;
    dma_virt = 0;
    used_port = 0;
    blk_secs = 0;
    live = 0;
    wr_said = 0;
    flush_said = 0;
    say_row = 0;

    if (row >= VGA_HEIGHT - 1) {
        return row;
    }
    if (pci_find_ahci(&bus, &dev, &fn) != 0) {
        return ahci_say(row, "ahci none");
    }
    abar = pci_abar(bus, dev, fn);
    if (abar == 0) {
        return ahci_say(row, "ahci fail");
    }
    pci_enable(bus, dev, fn);
    if (vmm_map_uncached(abar, ABAR_MAP) != 0) {
        return ahci_say(row, "ahci fail");
    }
    hba = (volatile uint32_t *)(uintptr_t)phys_to_virt(abar);

    ghc = hba_read(0x04);
    ghc &= ~GHC_IE;
    ghc |= GHC_AE;
    hba_write(0x04, ghc);
    pi = hba_read(0x0C);
    if (pi == 0) {
        return ahci_say(row, "ahci fail");
    }
    if (dma_init() != 0) {
        return ahci_say(row, "ahci fail");
    }
    p = pick_port(pi);
    if (p < 0) {
        return ahci_say(row, "ahci fail");
    }
    used_port = (unsigned)p;
    if (port_setup(used_port) != 0) {
        return ahci_say(row, "ahci fail");
    }
    if (port_issue(used_port, ATA_IDENTIFY, 0, 1, 0, 1) == 0) {
        blk_secs = ident_sectors(dma_virt + DATA_OFF);
    }
    if (port_issue(used_port, ATA_READ_DMA_EXT, 0, 1, 0, 1) != 0) {
        return ahci_say(row, "ahci fail");
    }
    data = dma_virt + DATA_OFF;
    msg[0] = 'a';
    msg[1] = 'h';
    msg[2] = 'c';
    msg[3] = 'i';
    msg[4] = ' ';
    msg[5] = hex_digit((unsigned)data[510] >> 4);
    msg[6] = hex_digit((unsigned)data[510]);
    msg[7] = hex_digit((unsigned)data[511] >> 4);
    msg[8] = hex_digit((unsigned)data[511]);
    msg[9] = '\0';
    live = 1;
    (void)bdev_register("ahci", ahci_read, ahci_write);
    bdev_set_flush(ahci_flush);
    if (blk_secs != 0) {
        bdev_set_sectors(blk_secs);
    }
    return ahci_say(row, msg);
}
