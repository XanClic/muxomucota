#include <arch-constants.hpp>
#include <compiler.hpp>
#include <cpu.hpp>
#include <cpu-state.hpp>
#include <cstdint>
#include <isr.hpp>
#include <kassert.hpp>
#include <panic.hpp>
#include <port-io.hpp>
#include <process.hpp>
#include <syscall.hpp>
#include <vm86.hpp>
#include <x86-segments.hpp>


extern bool handle_pagefault(cpu_state *state);


static uint64_t idt[256];


extern "C" {
extern void int_stub_0x00(void);
extern void int_stub_0x01(void);
extern void int_stub_0x02(void);
extern void int_stub_0x03(void);
extern void int_stub_0x04(void);
extern void int_stub_0x05(void);
extern void int_stub_0x06(void);
extern void int_stub_0x07(void);
extern void int_stub_0x08(void);
extern void int_stub_0x09(void);
extern void int_stub_0x0a(void);
extern void int_stub_0x0b(void);
extern void int_stub_0x0c(void);
extern void int_stub_0x0d(void);
extern void int_stub_0x0e(void);
extern void int_stub_0x10(void);
extern void int_stub_0x11(void);
extern void int_stub_0x12(void);
extern void int_stub_0x13(void);
extern void int_stub_0x20(void);
extern void int_stub_0x21(void);
extern void int_stub_0x22(void);
extern void int_stub_0x23(void);
extern void int_stub_0x24(void);
extern void int_stub_0x25(void);
extern void int_stub_0x26(void);
extern void int_stub_0x27(void);
extern void int_stub_0x28(void);
extern void int_stub_0x29(void);
extern void int_stub_0x2a(void);
extern void int_stub_0x2b(void);
extern void int_stub_0x2c(void);
extern void int_stub_0x2d(void);
extern void int_stub_0x2e(void);
extern void int_stub_0x2f(void);
extern void int_stub_0xa2(void);
}


#define IDT_SYS_INTR_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xffff) | (SEG_SYS_CS << 16) | (UINT64_C(0x8e) << 40);
#define IDT_USR_INTR_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xffff) | (SEG_SYS_CS << 16) | (UINT64_C(0xee) << 40);
#define IDT_SYS_TRAP_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xffff) | (SEG_SYS_CS << 16) | (UINT64_C(0x8f) << 40);
#define IDT_USR_TRAP_ENTRY(num) idt[num] = ((uint64_t)((uintptr_t)&int_stub_##num >> 16) << 48) | ((uintptr_t)&int_stub_##num & 0xffff) | (SEG_SYS_CS << 16) | (UINT64_C(0xef) << 40);


static void enable_irq(int irq, bool status)
{
    static uint8_t disabled_status[2] = {0, 0};

    disabled_status[irq >> 3] = (disabled_status[irq >> 3] & ~(1 << (irq & 7))) | (!status << (irq & 7));

    out8((irq >> 3) ? 0xa1 : 0x21, disabled_status[irq >> 3]);
}


static void init_pic(uint16_t iobase, uint8_t irq0_vector, uint8_t icw3)
{
    out8(iobase++, 0x11);        // ICW1 (begin initialization)
    out8(iobase,   irq0_vector); // ICW2 (interrupt vector for IRQ 0)
    out8(iobase,   icw3);        // ICW3 (no kidding; master slave connection wire)
    out8(iobase,   0x01);        // ICW4 (mode of operation)

    out8(iobase--, 0x00);        // OCW1 (allow all IRQs)
    out8(iobase,   0x0b);        // OCW3 (reads from the command port should return the interrupt status register)
}


void init_interrupts(void)
{
    init_pic(0x20, 0x20, 1 << 2); // master
    init_pic(0xa0, 0x28,      2); // slave


    // exceptions
    IDT_SYS_INTR_ENTRY(0x00);
    IDT_SYS_INTR_ENTRY(0x01);
    IDT_SYS_INTR_ENTRY(0x02);
    IDT_SYS_INTR_ENTRY(0x03);
    IDT_SYS_INTR_ENTRY(0x04);
    IDT_SYS_INTR_ENTRY(0x05);
    IDT_SYS_INTR_ENTRY(0x06);
    IDT_SYS_TRAP_ENTRY(0x07);
    IDT_SYS_INTR_ENTRY(0x08);
    IDT_SYS_INTR_ENTRY(0x09);
    IDT_SYS_INTR_ENTRY(0x0a);
    IDT_SYS_INTR_ENTRY(0x0b);
    IDT_SYS_INTR_ENTRY(0x0c);
    IDT_SYS_INTR_ENTRY(0x0d);
    IDT_SYS_TRAP_ENTRY(0x0e);
    IDT_SYS_INTR_ENTRY(0x10);
    IDT_SYS_INTR_ENTRY(0x11);
    IDT_SYS_INTR_ENTRY(0x12);
    IDT_SYS_INTR_ENTRY(0x13);

    // IRQs
    IDT_SYS_INTR_ENTRY(0x20);
    IDT_SYS_INTR_ENTRY(0x21);
    IDT_SYS_INTR_ENTRY(0x22);
    IDT_SYS_INTR_ENTRY(0x23);
    IDT_SYS_INTR_ENTRY(0x24);
    IDT_SYS_INTR_ENTRY(0x25);
    IDT_SYS_INTR_ENTRY(0x26);
    IDT_SYS_INTR_ENTRY(0x27);
    IDT_SYS_INTR_ENTRY(0x28);
    IDT_SYS_INTR_ENTRY(0x29);
    IDT_SYS_INTR_ENTRY(0x2a);
    IDT_SYS_INTR_ENTRY(0x2b);
    IDT_SYS_INTR_ENTRY(0x2c);
    IDT_SYS_INTR_ENTRY(0x2d);
    IDT_SYS_INTR_ENTRY(0x2e);
    IDT_SYS_INTR_ENTRY(0x2f);

    // syscall
    IDT_USR_TRAP_ENTRY(0xa2);


    struct {
        uint16_t limit;
        uintptr_t base;
    } cxx_packed idtr = {
        sizeof(idt) - 1,
        reinterpret_cast<uintptr_t>(idt)
    };

    __asm__ __volatile__ ("" ::: "memory");

    __asm__ __volatile__ ("lidt [%0]; sti" :: "b"(&idtr));
}


static int unsettled_procs[16];


void i386_common_isr(cpu_state *state);

void i386_common_isr(cpu_state *state)
{
    if (state->int_vector < 0x20) {
        // #PF
        if ((state->int_vector == 0x0e) && handle_pagefault(state)) {
            return;
        }

        // #NM
        if (state->int_vector == 0x07) {
            save_and_restore_fpu_and_sse();
            return;
        }

        // #GP
        if ((state->int_vector == 0x0d) && (state->eflags & 0x20000) && handle_vm86_gpf(state)) {
            return;
        }

        __asm__ __volatile__ ("cli");

        uintptr_t cr2;
        __asm__ __volatile__ ("mov %0,cr2" : "=r"(cr2));

        uintptr_t ksp = reinterpret_cast<uintptr_t>(state + 1) -
                        ((state->eflags & 0x20000) ? 0 : (sizeof(state->vm86_ds)
                                                        + sizeof(state->vm86_es)
                                                        + sizeof(state->vm86_fs)
                                                        + sizeof(state->vm86_gs))) -
                        ((state->cs     &       3) ? 0 : (sizeof(state->ss)
                                                        + sizeof(state->esp)));

        process *main_process = current_process ? process::find(current_process->pgid) : nullptr;

        format_panic("Unhandled exception\n\ncurrent process: %s in %s (%i in %i)\n\n"
                "exc: %p err: %p efl: %p cr2: %p\n"
                " cs: %p eip: %p  ss: %p esp: %p\n"
                " ds: %p  es: %p\n"
                "eax: %p ebx: %p ecx: %p edx: %p\n"
                "esi: %p edi: %p ebp: %p ksp: %p\n\n"
                "errno: %i",
                current_process ? current_process->name : "none",
                main_process ? main_process->name : "none",
                current_process ? current_process->pid : -1,
                current_process ? current_process->pgid : -1,
                state->int_vector, state->err_code, state->eflags, cr2,
                state->cs, state->eip, (state->cs & 3) ? state->ss : SEG_SYS_DS, (state->cs & 3) ? state->esp : ksp,
                state->ds, state->es,
                state->eax, state->ebx, state->ecx, state->edx,
                state->esi, state->edi, state->ebp, ksp,
                (current_process && current_process->tls) ? current_process->tls->errno : -1);
    } else if ((state->int_vector & 0xf0) == 0x20) {
        int irq = state->int_vector - 0x20;

        // potentially spurious interrupt
        if ((irq & ~8) == 7) {
            if (!(in8((irq & 8) ? 0xa0 : 0x20) & (1 << 7))) {
                // IRQ 7 is not handled at all, therefore this is no real IRQ
                if (irq & 8) {
                    // A spurious interrupt at the slave requires an EOI to the
                    // master nonetheless
                    out8(0x20, 0x20);
                }

                return;
            }
        }


        kassert(!unsettled_procs[irq]);

        unsettled_procs[irq] = common_irq_handler(irq, state);


        if (unsettled_procs[irq]) {
            enable_irq(irq, false);
        }

        if (irq & 8) {
            out8(0xa0, 0x20);
        }
        out8(0x20, 0x20);
    }
}


void irq_settled(int irq)
{
    kassert(unsettled_procs[irq]);

    if (!__sync_sub_and_fetch(&unsettled_procs[irq], 1)) {
        enable_irq(irq, true);
    }
}


uintptr_t i386_syscall(cpu_state *state);

uintptr_t i386_syscall(cpu_state *state)
{
    return syscall_krn(state->eax, state->ebx, state->ecx, state->edx, state->esi, state->edi, state);
}
