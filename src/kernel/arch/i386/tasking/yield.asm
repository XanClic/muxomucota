format ELF
use32

public yield_to
extrn dispatch

section '.text' executable

yield_to:
pushfd
push    dword 0x08
push    dword yield_return
push    dword 0
push    dword 0xa2
push    ds
push    es
push    eax
push    ebx
push    ecx
push    edx
push    esi
push    edi
push    ebp

mov     eax,esp

push    dword [eax+0x4c] ; Erster Parameter
push    eax
call    dispatch
mov     esp,eax

mov     ax,0x33
mov     fs,ax

pop     ebp
pop     edi
pop     esi
pop     edx
pop     ecx
pop     ebx
pop     eax
pop     es
pop     ds
add     esp,8
iret

yield_return:
ret
