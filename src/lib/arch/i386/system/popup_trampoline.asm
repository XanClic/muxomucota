format ELF
use32

public _popup_ll_trampoline
extrn _popup_trampoline

_popup_ll_trampoline:
sub     esp,0x10
mov     [esp],eax
call    _popup_trampoline
