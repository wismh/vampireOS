#include "idt.h"
#include "kbd.h"
#include "pic.h"
#include "pit.h"
#include "sched.h"
#include "user.h"
#include "vga.h"
#include "vmm.h"

#include <stdint.h>

/* Must match the 64-bit code selector in boot/stage2.asm. */
#define KERNEL_CS 0x10
#define IDT_COUNT 49
#define IDT_INTERRUPT_GATE 0x8E
#define IDT_USER_INTERRUPT_GATE 0xEE
#define SYSCALL_VEC 48

struct idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

extern uint64_t isr_stubs[IDT_COUNT];

static struct idt_gate idt[IDT_COUNT];
static volatile unsigned ticks;

static void idt_set_gate(int vec, uint64_t isr, uint8_t flags)
{
    idt[vec].offset_low = (uint16_t)(isr & 0xFFFF);
    idt[vec].selector = KERNEL_CS;
    idt[vec].ist = 0;
    idt[vec].flags = flags;
    idt[vec].offset_mid = (uint16_t)((isr >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)(isr >> 32);
    idt[vec].reserved = 0;
}

void exception_handler(struct interrupt_frame *frame)
{
    uint64_t cr2;

    if (frame != 0 && frame->vector == 14) {
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        if (vmm_handle_page_fault(frame->error, cr2) == 0) {
            return;
        }
    }
    vga_write_at(1, 0, "exception ");
    vga_write_dec_at(1, 10, (unsigned)frame->vector);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void irq_handler(struct interrupt_frame *frame)
{
    unsigned irq = (unsigned)frame->vector - 32;

    if (irq == 0) {
        ticks++;
        vga_write_at(1, 0, "ticks ");
        vga_write_dec_at(1, 6, ticks);
        sched_on_tick(frame);
    } else if (irq == 1) {
        kbd_handle(frame);
    }

    pic_eoi(irq);
}

unsigned idt_ticks(void)
{
    return ticks;
}

void syscall_handler(struct interrupt_frame *frame)
{
    user_on_syscall(frame);
}

void idt_init(void)
{
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr;
    int i;
    uint8_t flags;

    pic_remap();

    for (i = 0; i < IDT_COUNT; i++) {
        flags = (i == SYSCALL_VEC) ? IDT_USER_INTERRUPT_GATE : IDT_INTERRUPT_GATE;
        idt_set_gate(i, isr_stubs[i], flags);
    }

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));

    pit_init(100);
    kbd_init();
    pic_unmask(0);
    pic_unmask(1);
}
