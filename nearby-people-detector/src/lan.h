#ifndef LAN_H
#define LAN_H

#include <stddef.h>

/*
 * Counts other hosts on the Wi-Fi network the computer is connected to.
 * Uses only standard Linux interfaces: getifaddrs() and the kernel files
 * /proc/net/arp and /proc/net/route. No external programs.
 *
 * Hosts are a second view of "how many devices are around". It is a
 * cross-check, not the estimate: TVs and IoT devices are counted, phones not
 * on this network are not.
 */

typedef struct
{
    int available;  /* a Wi-Fi interface with an IPv4 address was found */
    int hosts;      /* other live hosts, gateway excluded */
    int prefix;     /* subnet prefix length, e.g. 24 */
    char iface[32];
    char address[16];
    char gateway[16];
} lan_info;

/*
 * preferred_iface may be NULL (the first interface named wl* is used).
 * The table is only read; no packets are sent.
 */
lan_info lan_scan(const char *preferred_iface);

/*
 * Parses one line of /proc/net/arp. Returns 1 if it is a live host on iface
 * that is not the gateway (gateway may be NULL or empty). Exposed for tests.
 */
int lan_arp_line_counts(const char *line, const char *iface, const char *gateway);

/*
 * Parses one line of /proc/net/route. If it is the default route of iface,
 * writes the gateway as dotted text and returns 1. Exposed for tests.
 */
int lan_route_line_gateway(const char *line, const char *iface, char *out,
                           size_t out_size);

#endif
