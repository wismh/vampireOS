#pragma once

#include <stdint.h>

struct interrupt_frame;

void serial_init(void);
void serial_irq_on(void);
void serial_putc(char c);
void serial_write(const char *s, unsigned n);
/* Drain COM1 RX into the same line buffer as PS/2. */
void serial_poll(struct interrupt_frame *frame);
