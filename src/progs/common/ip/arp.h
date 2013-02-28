#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#include "interfaces.h"


uint64_t arp_lookup(struct interface *i, uint32_t ip);
void arp_incoming(struct interface *i);

#endif
