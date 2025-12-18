#include "ata.h"
#include "io.h"

#define ATA_DATA 0x1F0
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA0 0x1F3
#define ATA_LBA1 0x1F4
#define ATA_LBA2 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_CMD 0x1F7
#define ATA_ALT 0x3F6
#define ATA_READ 0x20
#define ATA_WRITE 0x30
#define ATA_BSY 0x80
#define ATA_DRQ 0x08
#define ATA_ERR 0x01
#define ATA_DF 0x20

static void ata_delay(void)
{
    (void)inb(ATA_ALT);
    (void)inb(ATA_ALT);
    (void)inb(ATA_ALT);
    (void)inb(ATA_ALT);
}

static int ata_wait(void)
{
    unsigned i;
    uint8_t st;

    for (i = 0; i < 1000000u; i++) {
        st = inb(ATA_CMD);
        if (st == 0xFF) {
            return -1;
        }
        if ((st & ATA_BSY) == 0) {
            return (int)st;
        }
    }
    return -1;
}

static int ata_issue(uint32_t lba, unsigned sectors, uint8_t cmd)
{
    int st;

    if (sectors == 0 || sectors > 255u) {
        return -1;
    }
    st = ata_wait();
    if (st < 0) {
        return -1;
    }
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_delay();
    outb(ATA_SECCOUNT, (uint8_t)sectors);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_CMD, cmd);
    return 0;
}

static int ata_wait_drq(void)
{
    unsigned w = 0;
    int st;

    ata_delay();
    st = ata_wait();
    if (st < 0 || (st & (ATA_ERR | ATA_DF)) != 0) {
        return -1;
    }
    while ((st & ATA_DRQ) == 0) {
        st = inb(ATA_CMD);
        if ((st & (ATA_ERR | ATA_DF)) != 0) {
            return -1;
        }
        w++;
        if (w > 1000000u) {
            return -1;
        }
    }
    return 0;
}

int ata_read(uint32_t lba, unsigned sectors, void *dst)
{
    uint16_t *buf = (uint16_t *)dst;
    unsigned s;
    unsigned w;

    if (dst == 0) {
        return -1;
    }
    if (ata_issue(lba, sectors, ATA_READ) != 0) {
        return -1;
    }
    for (s = 0; s < sectors; s++) {
        if (ata_wait_drq() != 0) {
            return -1;
        }
        for (w = 0; w < 256; w++) {
            buf[s * 256u + w] = inw(ATA_DATA);
        }
    }
    return 0;
}

int ata_write(uint32_t lba, unsigned sectors, const void *src)
{
    const uint16_t *buf = (const uint16_t *)src;
    unsigned s;
    unsigned w;
    int st;

    if (src == 0) {
        return -1;
    }
    if (ata_issue(lba, sectors, ATA_WRITE) != 0) {
        return -1;
    }
    for (s = 0; s < sectors; s++) {
        if (ata_wait_drq() != 0) {
            return -1;
        }
        for (w = 0; w < 256; w++) {
            outw(ATA_DATA, buf[s * 256u + w]);
        }
    }
    st = ata_wait();
    if (st < 0 || (st & (ATA_ERR | ATA_DF)) != 0) {
        return -1;
    }
    return 0;
}
