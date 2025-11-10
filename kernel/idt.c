#include "idt.h"
#include "vga.h"

#include <stdint.h>

/* Must match the 64-bit code selector in boot/stage2.asm. */
#define KERNEL_CS 0x10
#define IDT_COUNT 32
#define IDT_INTERRUPT_GATE 0x8E

struct idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct interrupt_frame {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;
    uint64_t error;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

extern uint64_t isr_stubs[IDT_COUNT];

static struct idt_gate idt[IDT_COUNT];

static void idt_set_gate(int vec, uint64_t isr)
{
    idt[vec].offset_low = (uint16_t)(isr & 0xFFFF);
    idt[vec].selector = KERNEL_CS;
    idt[vec].ist = 0;
    idt[vec].flags = IDT_INTERRUPT_GATE;
    idt[vec].offset_mid = (uint16_t)((isr >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)(isr >> 32);
    idt[vec].reserved = 0;
}

static void pic_mask_all(void)
{
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0x21));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

void exception_handler(struct interrupt_frame *frame)
{
    vga_write_at(1, 0, "exception ");
    vga_write_dec_at(1, 10, (unsigned)frame->vector);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void idt_init(void)
{
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idtr;
    int i;

    pic_mask_all();

    for (i = 0; i < IDT_COUNT; i++) {
        idt_set_gate(i, isr_stubs[i]);
    }

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
