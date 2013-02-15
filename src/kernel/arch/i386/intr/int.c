#include <arch-constants.h>
#include <compiler.h>
#include <cpu.h>
#include <cpu-state.h>
#include <isr.h>
#include <panic.h>
#include <port-io.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <syscall.h>
#include <x86-segments.h>


extern bool handle_pagefault(struct cpu_state *state);


static uint64_t idt[256];


extern const void int_stub_0x00;
extern const void int_stub_0x01;
extern const void int_stub_0x02;
extern const void int_stub_0x03;
extern const void int_stub_0x04;
extern const void int_stub_0x05;
extern const void int_stub_0x06;
extern const void int_stub_0x07;
extern const void int_stub_0x08;
extern const void int_stub_0x09;
extern const void int_stub_0x0A;
extern const void int_stub_0x0B;
extern const void int_stub_0x0C;
extern const void int_stub_0x0D;
extern const void int_stub_0x0E;
extern const void int_stub_0x10;
extern const void int_stub_0x11;
extern const void int_stub_0x12;
extern const void int_stub_0x13;
extern const void int_stub_0x20;
extern const void int_stub_0x21;
extern const void int_stub_0x22;
extern const void int_stub_0x23;
extern const void int_stub_0x24;
extern const void int_stub_0x25;
extern const void int_stub_0x26;
extern const void int_stub_0x27;
extern const void int_stub_0x28;
extern const void int_stub_0x29;
extern const void int_stub_0x2A;
extern const void int_stub_0x2B;
extern const void int_stub_0x2C;
extern const void int_stub_0x2D;
extern const void int_stub_0x2E;
extern const void int_stub_0x2F;
extern const void int_stub_0xA2;


#define IDT_SYS_INTR_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xFFFF) | (SEG_SYS_CS << 16) | (UINT64_C(0x8E) << 40);
#define IDT_SYS_TRAP_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xFFFF) | (SEG_SYS_CS << 16) | (UINT64_C(0x8F) << 40);
#define IDT_USR_TRAP_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xFFFF) | (SEG_SYS_CS << 16) | (UINT64_C(0xEF) << 40);


static void init_pic(uint16_t iobase, uint8_t irq0_vector, uint8_t icw3)
{
    out8(iobase++, 0x11);        // ICW1 (Initialisierung beginnen)
    out8(iobase,   irq0_vector); // ICW2 (Interruptvektor für IRQ 0)
    out8(iobase,   icw3);        // ICW3 (no kidding; Master-Slave-Anbindungsleitung)
    out8(iobase,   0x01);        // ICW4 (Betriebsart)

    out8(iobase--, 0x00);        // OCW1 (alle IRQs zulassen)
    out8(iobase,   0x0B);        // OCW3 (Lesen vom Befehlsport soll das Interrupt Status Register zurückgeben)
}


void init_interrupts(void)
{
    init_pic(0x20, 0x20, 1 << 2); // Master
    init_pic(0xA0, 0x28,      2); // Slave


    // Exceptions
    IDT_SYS_TRAP_ENTRY(0x00);
    IDT_SYS_TRAP_ENTRY(0x01);
    IDT_SYS_TRAP_ENTRY(0x02);
    IDT_SYS_TRAP_ENTRY(0x03);
    IDT_SYS_TRAP_ENTRY(0x04);
    IDT_SYS_TRAP_ENTRY(0x05);
    IDT_SYS_TRAP_ENTRY(0x06);
    IDT_SYS_TRAP_ENTRY(0x07);
    IDT_SYS_TRAP_ENTRY(0x08);
    IDT_SYS_TRAP_ENTRY(0x09);
    IDT_SYS_TRAP_ENTRY(0x0A);
    IDT_SYS_TRAP_ENTRY(0x0B);
    IDT_SYS_TRAP_ENTRY(0x0C);
    IDT_SYS_TRAP_ENTRY(0x0D);
    IDT_SYS_TRAP_ENTRY(0x0E);
    IDT_SYS_TRAP_ENTRY(0x10);
    IDT_SYS_TRAP_ENTRY(0x11);
    IDT_SYS_TRAP_ENTRY(0x12);
    IDT_SYS_TRAP_ENTRY(0x13);

    // IRQs
    IDT_SYS_TRAP_ENTRY(0x20);
    IDT_SYS_TRAP_ENTRY(0x21);
    IDT_SYS_TRAP_ENTRY(0x22);
    IDT_SYS_TRAP_ENTRY(0x23);
    IDT_SYS_TRAP_ENTRY(0x24);
    IDT_SYS_TRAP_ENTRY(0x25);
    IDT_SYS_TRAP_ENTRY(0x26);
    IDT_SYS_TRAP_ENTRY(0x27);
    IDT_SYS_TRAP_ENTRY(0x28);
    IDT_SYS_TRAP_ENTRY(0x29);
    IDT_SYS_TRAP_ENTRY(0x2A);
    IDT_SYS_TRAP_ENTRY(0x2B);
    IDT_SYS_TRAP_ENTRY(0x2C);
    IDT_SYS_TRAP_ENTRY(0x2D);
    IDT_SYS_TRAP_ENTRY(0x2E);
    IDT_SYS_TRAP_ENTRY(0x2F);

    // Syscall
    IDT_USR_TRAP_ENTRY(0xA2);


    struct
    {
        uint16_t limit;
        uintptr_t base;
    } cc_packed idtr = {
        .limit = sizeof(idt) - 1,
        .base = (uintptr_t)idt
    };

    __asm__ __volatile__ ("" ::: "memory");

    __asm__ __volatile__ ("lidt [%0]; sti;" :: "b"(&idtr));
}


struct cpu_state *i386_common_isr(struct cpu_state *state);

struct cpu_state *i386_common_isr(struct cpu_state *state)
{
    if (state->int_vector < 0x20)
    {
        if ((state->int_vector == 0x0E) && handle_pagefault(state))
            return state;

        __asm__ __volatile__ ("cli");

        uintptr_t cr2;
        __asm__ __volatile__ ("mov %0,cr2" : "=r"(cr2));

        uintptr_t ksp = (state->cs & 3) ? (uintptr_t)(state + 1) : ((uintptr_t)(state + 1) - sizeof(state->ss) - sizeof(state->esp));

        format_panic("Unhandled exception\n\ncurrent process: %s (%i in %i)\n\n"
                "exc: %p err: %p efl: %p cr2: %p\n"
                " cs: %p eip: %p  ss: %p esp: %p\n"
                " ds: %p  es: %p\n"
                "eax: %p ebx: %p ecx: %p edx: %p\n"
                "esi: %p edi: %p ebp: %p ksp: %p\n\n"
                "errno: %i",
                (current_process == NULL) ? "none" : current_process->name,
                (current_process == NULL) ? -1 : current_process->pid,
                (current_process == NULL) ? -1 : current_process->pgid,
                state->int_vector, state->err_code, state->eflags, cr2,
                state->cs, state->eip, (state->cs & 3) ? state->ss : SEG_SYS_DS, (state->cs & 3) ? state->esp : ksp,
                state->ds, state->es,
                state->eax, state->ebx, state->ecx, state->edx,
                state->esi, state->edi, state->ebp, ksp,
                (current_process == NULL) ? -1 : *current_process->errno);
    }
    else if ((state->int_vector & 0xF0) == 0x20)
    {
        int irq = state->int_vector - 0x20;

        if ((irq & ~8) == 7) // evtl. Spurious Interrupt
        {
            if (!(in8((irq & 8) ? 0xA0 : 0x20) & (1 << 7)))
            {
                // IRQ 7 wird gar nicht behandelt, also ist es kein echter IRQ
                if (irq & 8)
                {
                    // Bei einem Spurious Interrupt beim Slave muss allerdings
                    // dennoch ein EOI an den Master geschickt werden
                    out8(0x20, 0x20);
                }

                return state;
            }
        }


        if (irq & 8)
            out8(0xA0, 0x20);
        out8(0x20, 0x20);

        if (!irq)
            return state;


        // IRQ hier behandeln
        common_irq_handler(irq);
    }


    return state;
}


uintptr_t i386_syscall(struct cpu_state *state);

uintptr_t i386_syscall(struct cpu_state *state)
{
    return syscall_krn(state->eax, state->ebx, state->ecx, state->edx, state->esi, state->edi, state);
}
