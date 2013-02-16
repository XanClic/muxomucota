#ifndef _SYS__TYPES_H
#define _SYS__TYPES_H

#include <stdint.h>


typedef int_fast32_t id_t;

typedef int pid_t;
typedef int uid_t;
typedef int gid_t;

typedef  int32_t  blkcnt_t;
typedef uint32_t  blksize_t;
typedef     long  clock_t;
typedef uint32_t  clockid_t;
typedef uint64_t  dev_t;
typedef uint64_t  fsblkcnt_t;
typedef uint64_t  fsfilcnt_t;
typedef uint32_t  ino_t;
typedef uint32_t  mode_t;
typedef uint32_t  nlink_t;
typedef  int64_t  off_t;
typedef  int64_t  off64_t;
typedef  int64_t  suseconds_t;
typedef uint64_t  time_t;
typedef uint64_t  useconds_t;

typedef uint64_t  blkcnt64_t;
typedef uint64_t  blksize64_t;
typedef uint64_t  blknum64_t;

#endif
