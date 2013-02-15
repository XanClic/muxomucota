#ifndef _KASSERT_H
#define _KASSERT_H

#include <panic.h>
#include <stdnoreturn.h>


// kassert_exec wird die Kondition auch bei NDEBUG noch auswerten (im Gegensatz zum normalen assert)
#define kassert_exec(cond) do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s", __FILE__, __LINE__, __func__, #cond); }  while (0)
#define kassert(cond)      do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s", __FILE__, __LINE__, __func__, #cond); }  while (0)

// kassert_exec_print wird die Kondition auswerten und außerdem den übergebenen Formatstring ausgeben (der nur die
// Formatangaben %i, %s und %p enthalten darf).
#define kassert_exec_print(cond, format, ...) do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s\n\n" format, __FILE__, __LINE__, __func__, #cond, ##__VA_ARGS__); } while (0)
#define kassert_print(cond, format, ...)      do { if (!(cond)) format_panic("Assertion failed\n\n%s:%i: %s():\n%s\n\n" format, __FILE__, __LINE__, __func__, #cond, ##__VA_ARGS__); } while (0)

#endif
