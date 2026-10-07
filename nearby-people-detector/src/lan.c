#include "lan.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#define LINE_SIZE 256
#define ARP_FLAG_COMPLETE 0x2u
#define ROUTE_FLAG_GATEWAY 0x2u

int lan_arp_line_counts(const char *line, const char *iface, const char *gateway)
{
    char ip[16];
    char hw[18];
    char device[32];
    unsigned int flags = 0;

    /* /proc/net/arp: IP  HWtype  Flags  HWaddress  Mask  Device */
    if (sscanf(line, "%15s %*s %x %17s %*s %31s", ip, &flags, hw, device) != 4)
        return 0;

    if (!(flags & ARP_FLAG_COMPLETE))
        return 0;

    if (strcmp(hw, "00:00:00:00:00:00") == 0)
        return 0;

    if (iface != NULL && iface[0] != '\0' && strcmp(device, iface) != 0)
        return 0;

    if (gateway != NULL && gateway[0] != '\0' && strcmp(ip, gateway) == 0)
        return 0;

    return 1;
}

int lan_route_line_gateway(const char *line, const char *iface, char *out,
                           size_t out_size)
{
    char name[32];
    unsigned int destination = 0;
    unsigned int gateway = 0;
    unsigned int flags = 0;
    struct in_addr address;

    /* /proc/net/route: Iface Destination Gateway Flags ... (hex, raw order) */
    if (sscanf(line, "%31s %x %x %x", name, &destination, &gateway, &flags) != 4)
        return 0;

    if (destination != 0 || !(flags & ROUTE_FLAG_GATEWAY))
        return 0;

    if (iface != NULL && strcmp(name, iface) != 0)
        return 0;

    address.s_addr = (in_addr_t)gateway;

    return inet_ntop(AF_INET, &address, out, (socklen_t)out_size) != NULL;
}

static int prefix_length(const struct in_addr *mask)
{
    unsigned int value = ntohl(mask->s_addr);
    int bits = 0;

    for (; value != 0; value <<= 1)
        bits += (value & 0x80000000u) ? 1 : 0;

    return bits;
}

/* First IPv4 address of an UP interface; named preferred or starting with wl. */
static int find_interface(const char *preferred, lan_info *info,
                          struct in_addr *address)
{
    struct ifaddrs *list = NULL;
    int found = 0;

    if (getifaddrs(&list) != 0)
        return 0;

    for (struct ifaddrs *ifa = list; ifa != NULL && !found; ifa = ifa->ifa_next)
    {
        const struct sockaddr_in *ip;
        const struct sockaddr_in *mask;

        if (ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL ||
            ifa->ifa_addr->sa_family != AF_INET || !(ifa->ifa_flags & IFF_UP))
            continue;

        if (preferred != NULL && preferred[0] != '\0')
        {
            if (strcmp(ifa->ifa_name, preferred) != 0)
                continue;
        }
        else if (strncmp(ifa->ifa_name, "wl", 2) != 0)
        {
            continue;
        }

        ip = (const struct sockaddr_in *)(const void *)ifa->ifa_addr;
        mask = (const struct sockaddr_in *)(const void *)ifa->ifa_netmask;

        snprintf(info->iface, sizeof(info->iface), "%s", ifa->ifa_name);
        inet_ntop(AF_INET, &ip->sin_addr, info->address, sizeof(info->address));
        info->prefix = prefix_length(&mask->sin_addr);
        *address = ip->sin_addr;
        found = 1;
    }

    freeifaddrs(list);

    return found;
}

static void find_gateway(lan_info *info)
{
    FILE *fp = fopen("/proc/net/route", "r");
    char line[LINE_SIZE];

    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (lan_route_line_gateway(line, info->iface, info->gateway,
                                   sizeof(info->gateway)))
            break;
    }

    fclose(fp);
}

lan_info lan_scan(const char *preferred_iface)
{
    lan_info info;
    struct in_addr own;
    FILE *fp;
    char line[LINE_SIZE];

    memset(&info, 0, sizeof(info));

    if (!find_interface(preferred_iface, &info, &own))
        return info;

    info.available = 1;

    find_gateway(&info);

    fp = fopen("/proc/net/arp", "r");
    if (fp == NULL)
        return info;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (lan_arp_line_counts(line, info.iface, info.gateway))
            info.hosts++;
    }

    fclose(fp);

    return info;
}
