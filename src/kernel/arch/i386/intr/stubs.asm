format ELF
use32

section '.text' executable

macro isr num
{
    public int_stub_#num
    int_stub_#num#:
    push  dword 0
    push  dword num
    jmp   _asm_common_isr
}

macro err_isr num
{
    public int_stub_#num
    int_stub_#num#:
    push  dword num
    jmp   _asm_common_isr
}


isr 0x00
isr 0x01
isr 0x02
isr 0x03
isr 0x04
isr 0x05
isr 0x06
isr 0x07
err_isr 0x08
isr 0x09
err_isr 0x0A
err_isr 0x0B
err_isr 0x0C
err_isr 0x0D
err_isr 0x0E
isr 0x10
err_isr 0x11
isr 0x12
isr 0x13

isr 0x20
isr 0x21
isr 0x22
isr 0x23
isr 0x24
isr 0x25
isr 0x26
isr 0x27
isr 0x28
isr 0x29
isr 0x2A
isr 0x2B
isr 0x2C
isr 0x2D
isr 0x2E
isr 0x2F


extrn i386_common_isr
public _asm_common_isr ; Für Backtraces
_asm_common_isr:
cld

push  ds
push  es

push  eax
push  ebx
push  ecx
push  edx
push  esi
push  edi
push  ebp

mov   ax,0x10
mov   ds,ax
mov   es,ax

push  esp
call  i386_common_isr

mov   ax,0x33
mov   fs,ax

add   esp,4

pop   ebp
pop   edi
pop   esi
pop   edx
pop   ecx
pop   ebx
pop   eax

pop   es
pop   ds

add   esp,8

iret


extrn i386_syscall
public int_stub_0xA2
int_stub_0xA2:
cld

sub   esp,8

push  ds
push  es

push  eax
push  ebx
push  ecx
push  edx
push  esi
push  edi
push  ebp

mov   ax,0x10
mov   ds,ax
mov   es,ax

push  esp
call  i386_syscall

push  dword 0x33
pop   fs

add   esp,4

pop   ebp
pop   edi
pop   esi
pop   edx
pop   ecx
pop   ebx
add   esp,4

pop   es
pop   ds

add   esp,8

iret
