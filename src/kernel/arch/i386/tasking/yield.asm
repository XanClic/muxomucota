; Copyright (C) 2013 by Hanna Reitz
;
; This file is part of µxoµcota.
;
; µxoµcota is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; µxoµcota is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.

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
