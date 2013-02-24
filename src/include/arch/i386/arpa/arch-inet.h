#ifndef _ARPA__ARCH_INET_H
#define _ARPA__ARCH_INET_H

#include <stdint.h>

static inline uint32_t htonl(uint32_t hostlong)
{
    return __builtin_bswap32(hostlong);
}

static inline uint16_t htons(uint16_t hostshort)
{
    return ((hostshort & 0xff00) >> 8) | ((hostshort & 0x00ff) << 8);
}

static inline uint32_t ntohl(uint32_t netlong)
{
    return __builtin_bswap32(netlong);
}

static inline uint16_t ntohs(uint16_t netshort)
{
    return ((netshort & 0xff00) >> 8) | ((netshort & 0x00ff) << 8);
}

#endif
