#ifndef _STDDEF_H
#define _STDDEF_H

#ifndef NULL
#define NULL ((void *)0)
#endif


typedef unsigned long  size_t;
typedef      int long ssize_t;
typedef unsigned long long big_size_t;


#define offsetof(t, m) ((size_t)(&(((t *)0)->m)))

#endif
