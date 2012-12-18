format ELF
use32

public _popup_ll_trampoline
extrn _popup_trampoline

_popup_ll_trampoline:
and     esp,not 0xF
sub     esp,0x10
mov     [esp+0],eax
mov     [esp+4],ebx
call    _popup_trampoline
