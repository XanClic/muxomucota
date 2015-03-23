#ifndef _KMALLOC_HPP
#define _KMALLOC_HPP

#include <cstddef>
#include <cstring>

void *kmalloc(size_t size);
void kfree(void *ptr);

static inline void *kcalloc(size_t size)
{ return memset(kmalloc(size), 0, size); }


inline void *operator new(size_t size)
{ return kmalloc(size); }

inline void *operator new[](size_t size)
{ return kmalloc(size); }


inline void *operator new(size_t, void *ptr)
{ return ptr; }

inline void *operator new[](size_t, void *ptr)
{ return ptr; }


inline void operator delete(void *ptr)
{ kfree(ptr); }

inline void operator delete[](void *ptr)
{ kfree(ptr); }

#endif
