#ifndef _KASSERT_H
#define _KASSERT_H

#include <stdnoreturn.h>


noreturn void assertion_panic(const char *file, int line, const char *function, const char *condition);


// kassert_exec wird die Kondition auch bei NDEBUG noch auswerten (im Gegensatz zum normalen assert)
#define kassert_exec(cond) do { if (!(cond)) assertion_panic(__FILE__, __LINE__, __func__, #cond); }  while (0)
#define kassert(cond)      do { if (!(cond)) assertion_panic(__FILE__, __LINE__, __func__, #cond); } while (0)

#endif
