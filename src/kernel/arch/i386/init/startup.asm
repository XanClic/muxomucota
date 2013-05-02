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

; Dieser Stub aktiviert rudimentäres Paging, um in einen über 0xF0000000 gelinkten
; Kernel (zwischen 0xF0000000 und 0xF0400000) zu springen.

section '.entry' executable
extrn main

; Der Kernel ist nach 0xF0100000 gelinkt, daher muss zu jeder Adresse 0x10000000
; addiert werden, um die physische Entsprechung an 0x00100000 zu erhalten.
mov     esp, kernel_stack + 0x10000000
push    ebx
push    eax
push    dword cs
push    dword next + 0x10000000
retf

section '.largedata' writable align 4096
public hpstructs

; Speicher für die Kernelpagingstrukturen (62 PTs nötig)
hpstructs:
times 0x01000 db ? ; page dir
times 0x3E000 db ? ; page tables

; Stack für die Initialisierung des Kernels
times 8192 db ?
kernel_stack:

; Wir brauchen wieder die physische Adresse
pstructs = hpstructs + 0x10000000

section '.text' executable

next:

mov     edi, pstructs   ; 0x00000000
mov     cr3, edi
mov     eax, 0x00000083 ; PR/RW/OS/4M
stosd                   ; 0x00000000 bis 0x003FFFFF

add     edi, 0x0F00 - 4 ; 0xF0000000
stosd                   ; 0xF0000000 bis 0xF03FFFFF

mov     eax, cr4
; PGE, MCE, !PCE, !PAE, PSE, DE, !TSD, !PVI, !VME
and     eax, 11111111111111111111111000000000b
or      eax, 00000000000000000000000011011000b
mov     cr4, eax

mov     eax, cr0
; PG, WP, PE, !CD, !NW, !AM, !NE, !EM, !MP
and     eax, 00011111111110101111111111010000b
or      eax, 10000000000000010000000000000001b
mov     cr0, eax

; Paging ist jetzt aktiviert

or      esp, 0xF0000000
push    dword cs
push    dword higher_half
retf

higher_half:
; So hören Backtraces auch irgendwann auf (solange gcc überhaupt einen
; Framepointer mitführt)
xor     ebp,ebp
; Pointer auf die Bootloaderinformationen
push    esp
call    main

hangman:
cli
hlt
jmp    hangman
