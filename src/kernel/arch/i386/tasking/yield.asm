format ELF
use32

public yield
extrn dispatch

section '.text' executable

yield:
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

push    esp
call    dispatch
mov     esp,eax

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
