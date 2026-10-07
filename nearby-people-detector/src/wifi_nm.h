#ifndef WIFI_NM_H
#define WIFI_NM_H

#include <stddef.h>

/*
 * Wi-Fi environment through libnm (NetworkManager client library).
 * Counts the different access points (BSSID) that the first Wi-Fi device
 * can hear. Access points are routers, not people: this is context only.
 */

typedef struct
{
    int available;      /* a Wi-Fi device was found */
    int access_points;  /* different BSSIDs */
    int scan_requested; /* a fresh scan was asked for */
    int scan_fresh;     /* the scan finished and results are new */
    char iface[32];     /* e.g. wlo1 */
} wifi_nm_info;

/*
 * rescan != 0 asks NetworkManager for a new scan and waits up to 10 s for it.
 * NetworkManager may refuse a scan that follows too soon after another one;
 * then the cached list is used and scan_fresh stays 0.
 * Returns 0 on success, -1 if NetworkManager cannot be reached (err is set).
 */
int wifi_nm_scan(int rescan, wifi_nm_info *info, char *err, size_t err_size);

#endif
