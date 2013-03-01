#ifndef TCP_H
#define TCP_H

#include <compiler.h>
#include <stdint.h>

struct tcphdr
{
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq, ack;
    uint8_t hdrlen;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgentptr;
} cc_packed;

struct tcpfhdr
{
    struct tcppsiphdr
    {
        uint32_t src_ip;
        uint32_t dst_ip;
        uint8_t zero;
        uint8_t prot_id;
        uint16_t tcp_length;
    } cc_packed psiphdr;
    struct tcphdr tcphdr;
} cc_packed;

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

#endif
