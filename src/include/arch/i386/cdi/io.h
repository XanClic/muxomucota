#ifndef _CDI_IO_H_
#define _CDI_IO_H_

#include <drivers/ports.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint8_t  cdi_inb(uint16_t port) { return in8(port);  }
static inline uint16_t cdi_inw(uint16_t port) { return in16(port); }
static inline uint32_t cdi_inl(uint16_t port) { return in32(port); }

static inline void cdi_outb(uint16_t port, uint8_t data)  { out8(port, data);  }
static inline void cdi_outw(uint16_t port, uint16_t data) { out16(port, data); }
static inline void cdi_outl(uint16_t port, uint32_t data) { out32(port, data); }

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

