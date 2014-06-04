#ifndef _ARCH_PROCESS_HPP
#define _ARCH_PROCESS_HPP

#include <cstdint>
#include <compiler.hpp>
#include <vm86.hpp>


struct fxsave_space {
    uint16_t fcw, fsw;
    uint8_t ftw;
    uint8_t rsvd0;
    uint16_t fop;
    uint32_t fpu_ip;
    uint16_t cs;
    uint16_t rsvd1;
    uint32_t fpu_dp;
    uint16_t ds;
    uint16_t rsvd2;
    uint32_t mxcsr, mxcsr_mask;

    struct {
        union {
            uint8_t st[10];
            uint8_t mm[10];
        };
        uint8_t rsvd3[6];
    } cxx_packed st[8];

    struct {
        uint8_t bytes[16];
    } cxx_packed xmm[8];

    uint8_t rsvd4[11 * 16];
    uint8_t avail[3 * 16];
} cxx_packed;

struct process_arch_info {
    uintptr_t kernel_stack, kernel_stack_top;
    int iopl;

    bool fxsave_valid;
    fxsave_space *fxsave;

    uint8_t fxsave_real_space[527];

    vm86_registers *vm86_exit_regs;
};


void save_and_restore_fpu_and_sse(void);

struct process;
void fpu_sse_unregister(process *proc);

#endif
