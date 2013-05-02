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

public _longjmp

_longjmp:
mov   edx,[esp+0x04]
mov   eax,[esp+0x08]

mov   esp,[edx+0x00]
add   esp,8
push  edx
push  dword [edx+0x04]
mov   ebx,[edx+0x08]
mov   esi,[edx+0x0C]
mov   edi,[edx+0x10]
mov   ebp,[edx+0x14]

test  eax,eax
jnz   return
add   eax,1
return:
ret
