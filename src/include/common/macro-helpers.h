#ifndef _MACRO_HELPERS_H
#define _MACRO_HELPERS_H

#define __eval(x) x
#define __cat(a, b) a ## b
#define __evalcat(a, b) __eval(__cat(a, b))

// line unique (eindeutiger Variablenname für eine Datei)
#define __lu(varname) __evalcat(_, __evalcat(__LINE__, __ ## varname))

#endif
