#include <arch-constants.hpp>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <compiler.hpp>
#include <cpu-state.hpp>
#include <kassert.hpp>
#include <kmalloc.hpp>
#include <parith.hpp>
#include <port-io.hpp>
#include <process.hpp>
#include <vmem.hpp>
#include <vm86.hpp>


void vm86_interrupt(int intr, vm86_registers *regs, vm86_memory_area *mem, int areas)
{
    if (intr & ~0xff) {
        current_process->tls->errno = EINVAL;
        return;
    }


    for (int i = 0; i < areas; i++) {
        if (mem[i].in && ((mem[i].vm >= 0xa0000) ||
                          (mem[i].vm + mem[i].size > 0xa0000) ||
                          (mem[i].size > 0xa0000) ||
                          IS_KERNEL(mem[i].caller)))
        {
            current_process->tls->errno = EFAULT;
            return;
        }

        if (mem[i].out && ((mem[i].vm >= 0x100000) ||
                           (mem[i].vm + mem[i].size > 0x100000) ||
                           (mem[i].size > 0x100000) ||
                           IS_KERNEL(mem[i].caller)))
        {
            current_process->tls->errno = EFAULT;
            return;
        }
    }


    vmm_context *vmm = current_process->vmm;

    void *first_mb_base = kmalloc(0xa0fff);
    void *first_mb_copy = reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(first_mb_base) + PAGE_SIZE - 1) & ~PAGE_MASK);

    void *first_mb = kernel_map(0, 0xa0000);
    memcpy(first_mb_copy, first_mb, 0xa0000);
    kernel_unmap(first_mb, 0xa0000);


    for (int i = 0; i < areas; i++) {
        if (mem[i].in) {
            memcpy(byte_ptr(first_mb_copy) + mem[i].vm, mem[i].caller, mem[i].size);
        }
    }


    struct ivt {
        uint16_t ip, cs;
    } cxx_packed *ivt = static_cast<struct ivt *>(first_mb_copy);


    for (uintptr_t addr = 0x00000000; addr < 0x00100000; addr += PAGE_SIZE) {
        if (addr >= 0xa0000) {
            vmm->map(reinterpret_cast<void *>(addr), addr, VMM_UR | VMM_UW | VMM_UX | VMM_PR | VMM_PW);
        } else {
            uintptr_t mapping;
            bool readable = vmm->mapped(byte_ptr(first_mb_copy) + addr, &mapping) & VMM_PR;

            kassert_exec_print(readable, "Address: %p", reinterpret_cast<uintptr_t>(first_mb_copy) + addr);

            vmm->map(reinterpret_cast<void *>(addr), addr, VMM_UR | VMM_UW | VMM_UX | VMM_PR | VMM_PW);
        }
    }


    process *p = process::user_thread(nullptr, reinterpret_cast<void *>(0x7c00), nullptr);

    p->cpu->eax = regs->eax;
    p->cpu->ebx = regs->ebx;
    p->cpu->ecx = regs->ecx;
    p->cpu->edx = regs->edx;
    p->cpu->esi = regs->esi;
    p->cpu->edi = regs->edi;
    p->cpu->ebp = regs->ebp;

    p->cpu->cs = ivt[intr].cs;
    p->cpu->ss = 0x0000;

    p->cpu->vm86_ds = regs->ds;
    p->cpu->vm86_es = regs->es;
    p->cpu->vm86_fs = regs->fs;
    p->cpu->vm86_gs = regs->gs;

    p->cpu->eip = ivt[intr].ip;
    p->cpu->esp = 0x7c00;

    p->cpu->eflags = 0x20202; // vm86


    p->vm86_exit_regs = regs;


    p->run();

    p->wait(nullptr, 0, nullptr, nullptr);


    kfree(first_mb_base);


    for (int i = 0; i < areas; i++) {
        if (mem[i].out) {
            memcpy(mem[i].caller, reinterpret_cast<void *>(mem[i].vm), mem[i].size);
        }
    }

    vmm->unmap(nullptr, 0xa0000);
}


#define lin(seg, ofs) ((((seg) & 0xffff) << 4) + ((ofs) & 0xffff))

#define stack(o) *reinterpret_cast<uint16_t *>(lin(state->ss, state->esp + o * 2))
#define stack_add(val) state->esp = (state->esp + (val) * 2) & 0xffff
#define code(o) *reinterpret_cast<uint8_t *>(lin(state->cs, state->eip + o))
#define code_add(val) state->eip = (state->eip + (val)) & 0xffff


bool handle_vm86_gpf(cpu_state *state)
{
    struct {
        uint16_t ip, cs;
    } cxx_packed *ivt = nullptr;


    switch (code(0)) {
        case 0x66: // operand-size override prefix
            switch (code(1)) {
                case 0x6d: // insd
                    if ((code(-1) == 0xf2) || (code(-1) == 0xf3)) { // rep (resp. repnz/repz)
                        __asm__ __volatile__ ("rep insd" :: "c"(state->ecx & 0xffff), "d"(state->edx & 0xffff), "D"(lin(state->vm86_es, state->edi)));

                        state->edi = (state->edi & ~0xffff) | ((state->edi + (state->ecx & 0xffff) * 4) & 0xffff);
                        state->ecx &= ~0xffff;
                    } else {
                        *reinterpret_cast<uint32_t *>(lin(state->vm86_es, state->edi)) = in32(state->edx & 0xffff);
                        state->edi = (state->edi & ~0xffff) | ((state->edi + 4) & 0xffff);
                    }
                    break;

                case 0x6f: // outsd
                    if ((code(-1) == 0xf2) || (code(-1) == 0xf3)) { // rep (resp. repnz/repz)
                        __asm__ __volatile__ ("rep outsd" :: "c"(state->ecx & 0xffff), "d"(state->edx & 0xffff), "S"(lin(state->vm86_ds, state->esi)));

                        state->esi = (state->esi & ~0xffff) | ((state->esi + (state->ecx & 0xffff) * 4) & 0xffff);
                        state->ecx &= ~0xffff;
                    } else {
                        out32(state->edx & 0xffff, *reinterpret_cast<uint32_t *>(lin(state->vm86_ds, state->esi)));
                        state->esi = (state->esi & ~0xffff) | ((state->esi + 4) & 0xffff);
                    }
                    break;

                case 0xe5: // in eax,0x??
                    state->eax = in32(code(2));
                    code_add(1);
                    break;

                case 0xe7: // out 0x??,eax
                    out32(code(2), state->eax);
                    code_add(1);
                    break;

                case 0xed: // in eax,dx
                    state->eax = in32(state->edx & 0xffff);
                    break;

                case 0xef: // out dx,eax
                    out32(state->edx & 0xffff, state->eax);
                    break;

                default:
                    return false;
            }

            code_add(1);
            break;

        case 0x6c: // insb
            if ((code(-1) == 0xf2) || (code(-1) == 0xf3)) { // rep (resp. repnz/repz)
                __asm__ __volatile__ ("rep insb" :: "c"(state->ecx & 0xffff), "d"(state->edx & 0xffff), "D"(lin(state->vm86_es, state->edi)));

                state->edi = (state->edi & ~0xffff) | ((state->edi + (state->ecx & 0xffff)) & 0xffff);
                state->ecx &= ~0xffff;
            } else {
                *reinterpret_cast<uint8_t *>(lin(state->vm86_es, state->edi)) = in8(state->edx & 0xffff);
                state->edi = (state->edi & ~0xffff) | ((state->edi + 1) & 0xffff);
            }
            break;

        case 0x6d: // insw
            if ((code(-1) == 0xf2) || (code(-1) == 0xf3)) { // rep (resp. repnz/repz)
                __asm__ __volatile__ ("rep insw" :: "c"(state->ecx & 0xffff), "d"(state->edx & 0xffff), "D"(lin(state->vm86_es, state->edi)));

                state->edi = (state->edi & ~0xffff) | ((state->edi + (state->ecx & 0xffff) * 2) & 0xffff);
                state->ecx &= ~0xffff;
            } else {
                *reinterpret_cast<uint16_t *>(lin(state->vm86_es, state->edi)) = in16(state->edx & 0xffff);
                state->edi = (state->edi & ~0xffff) | ((state->edi + 2) & 0xffff);
            }
            break;

        case 0x6e: // outsb
            if ((code(-1) == 0xf2) || (code(-1) == 0xf3)) { // rep (resp. repnz/repz)
                __asm__ __volatile__ ("rep outsb" :: "c"(state->ecx & 0xffff), "d"(state->edx & 0xffff), "S"(lin(state->vm86_ds, state->esi)));

                state->esi = (state->esi & ~0xffff) | ((state->esi + (state->ecx & 0xffff)) & 0xffff);
                state->ecx &= ~0xffff;
            } else {
                out8(state->edx & 0xffff, *reinterpret_cast<uint8_t *>(lin(state->vm86_ds, state->esi)));
                state->esi = (state->esi & ~0xffff) | ((state->esi + 1) & 0xffff);
            }
            break;

        case 0x6f: // outsw
            if ((code(-1) == 0xf2) || (code(-1) == 0xf3)) { // rep (resp. repnz/repz)
                __asm__ __volatile__ ("rep outsw" :: "c"(state->ecx & 0xffff), "d"(state->edx & 0xffff), "S"(lin(state->vm86_ds, state->esi)));

                state->esi = (state->esi & ~0xffff) | ((state->esi + (state->ecx & 0xffff) * 2) & 0xffff);
                state->ecx &= ~0xffff;
            } else {
                out16(state->edx & 0xffff, *reinterpret_cast<uint16_t *>(lin(state->vm86_ds, state->esi)));
                state->esi = (state->esi & ~0xffff) | ((state->esi + 2) & 0xffff);
            }
            break;

        case 0x9c: // pushf
            stack_add(-1);
            stack(0) = state->eflags & 0xffff;
            break;

        case 0x9d: // popf
            state->eflags = (state->eflags & ~0xffff) | stack(0) | 0x0200;
            stack_add(1);
            break;

        case 0xcd: // int
            stack_add(-3);
            stack(2) = state->eflags & 0xffff;
            stack(1) = state->cs     & 0xffff;
            stack(0) = state->eip    & 0xffff;

            state->eip = ivt[code(1)].ip;
            state->cs  = ivt[code(1)].cs;
            return true;

        case 0xcf: // iret
            if (!(state->ss & 0xffff) && ((state->esp & 0xffff) == 0x7c00)) {
                // Final iret
                current_process->vm86_exit_regs->eax = state->eax;
                current_process->vm86_exit_regs->ebx = state->ebx;
                current_process->vm86_exit_regs->ecx = state->ecx;
                current_process->vm86_exit_regs->edx = state->edx;
                current_process->vm86_exit_regs->esi = state->esi;
                current_process->vm86_exit_regs->edi = state->edi;
                current_process->vm86_exit_regs->ebp = state->ebp;

                current_process->vm86_exit_regs->ds = state->vm86_ds;
                current_process->vm86_exit_regs->es = state->vm86_es;
                current_process->vm86_exit_regs->fs = state->vm86_fs;
                current_process->vm86_exit_regs->gs = state->vm86_gs;

                current_process->destroy(0);
            }

            state->eip    = stack(0);
            state->cs     = stack(1);
            state->eflags = (state->eflags & ~0xffff) | stack(2) | 0x0200;
            stack_add(3);
            break;

        case 0xe4: // in al,0x??
            state->eax = (state->eax & ~0xff) | in8(code(1));
            code_add(1);
            break;

        case 0xe5: // in ax,0x??
            state->eax = (state->eax & ~0xffff) | in16(code(1));
            code_add(1);
            break;

        case 0xe6: // out 0x??,al
            out8(code(1), state->eax & 0xff);
            code_add(1);
            break;

        case 0xe7: // out 0x??,ax
            out16(code(1), state->eax & 0xffff);
            code_add(1);

        case 0xec: // in al,dx
            state->eax = (state->eax & ~0xff) | in8(state->edx & 0xffff);
            break;

        case 0xed: // in ax,dx
            state->eax = (state->eax & ~0xffff) | in16(state->edx & 0xffff);
            break;

        case 0xee: // out dx,al
            out8(state->edx & 0xffff, state->eax & 0xff);
            break;

        case 0xef: // out dx,ax
            out16(state->edx & 0xffff, state->eax & 0xffff);
            break;

        case 0xfa: // cli
        case 0xfb: // sti
            break;

        default:
            return false;
    }

    code_add(1);

    return true;
}
