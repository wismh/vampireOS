#include "serial.h"
#include "io.h"
#include "kbd.h"

#include <stdint.h>

#define COM1 0x3F8
#define COM_DATA (COM1 + 0)
#define COM_IER (COM1 + 1)
#define COM_FCR (COM1 + 2)
#define COM_LCR (COM1 + 3)
#define COM_MCR (COM1 + 4)
#define COM_LSR (COM1 + 5)
#define COM_SCR (COM1 + 7)

#define LSR_DR 0x01
#define LSR_THRE 0x20
#define SERIAL_TIMEOUT 100000u

static int serial_ok;

static void serial_raw(char c)
{
    unsigned i;

    if (serial_ok == 0) {
        return;
    }
    for (i = 0; i < SERIAL_TIMEOUT; i++) {
        if ((inb(COM_LSR) & LSR_THRE) != 0) {
            break;
        }
    }
    outb(COM_DATA, (uint8_t)c);
}

void serial_init(void)
{
    serial_ok = 0;
    outb(COM_IER, 0);
    outb(COM_LCR, 0x80);
    outb(COM_DATA, 0x01);
    outb(COM_IER, 0x00);
    outb(COM_LCR, 0x03);
    outb(COM_FCR, 0x07);
    outb(COM_MCR, 0x0B);
    outb(COM_SCR, 0xAE);
    if (inb(COM_SCR) != 0xAE) {
        return;
    }
    outb(COM_SCR, 0x00);
    serial_ok = 1;
}

void serial_irq_on(void)
{
    if (serial_ok != 0) {
        outb(COM_IER, 0x01);
    }
}

void serial_putc(char c)
{
    if (c == '\n') {
        serial_raw('\r');
        serial_raw('\n');
        return;
    }
    if (c == '\b') {
        serial_raw('\b');
        serial_raw(' ');
        serial_raw('\b');
        return;
    }
    serial_raw(c);
}

void serial_write(const char *s, unsigned n)
{
    unsigned i;

    if (s == 0) {
        return;
    }
    for (i = 0; i < n; i++) {
        serial_putc(s[i]);
    }
}

void serial_poll(struct interrupt_frame *frame)
{
    int n;

    if (serial_ok == 0) {
        return;
    }
    kbd_set_irq_frame(frame);
    for (n = 0; n < 16; n++) {
        uint8_t c;

        if ((inb(COM_LSR) & LSR_DR) == 0) {
            break;
        }
        c = inb(COM_DATA);
        if (c == '\r') {
            c = '\n';
        } else if (c == 0x7F) {
            c = '\b';
        }
        kbd_feed((char)c);
    }
}
