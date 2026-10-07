#include "bt_table.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void bt_table_init(bt_table *table)
{
    memset(table, 0, sizeof(*table));
}

static bt_device *find_or_add(bt_table *table, const char *mac)
{
    bt_device *device;

    for (int i = 0; i < table->count; i++)
    {
        if (strcmp(table->devices[i].mac, mac) == 0)
            return &table->devices[i];
    }

    if (table->count >= BT_MAX_DEVICES)
    {
        table->overflow++;
        return NULL;
    }

    device = &table->devices[table->count++];
    memset(device, 0, sizeof(*device));
    snprintf(device->mac, sizeof(device->mac), "%s", mac);

    return device;
}

void bt_table_heard(bt_table *table, const char *mac, int has_rssi,
                    int rssi_dbm, int round)
{
    bt_device *device;

    if (round < 1)
        return;

    device = find_or_add(table, mac);
    if (device == NULL)
        return;

    if (device->last_round != round)
    {
        device->last_round = round;
        device->rounds_seen++;
    }

    device->observations++;

    if (!has_rssi)
        return;

    device->rssi_samples++;

    if (!device->has_rssi)
    {
        device->has_rssi = 1;
        device->best_rssi = rssi_dbm;
        device->mean_rssi = rssi_dbm;
        return;
    }

    if (rssi_dbm > device->best_rssi)
        device->best_rssi = rssi_dbm;

    /* running mean over all RSSI samples */
    device->mean_rssi +=
        ((double)rssi_dbm - device->mean_rssi) / (double)device->rssi_samples;
}

void bt_table_set_info(bt_table *table, const char *mac, int paired,
                       int random_addr)
{
    bt_device *device = find_or_add(table, mac);

    if (device == NULL)
        return;

    device->paired = paired ? 1 : 0;
    device->random_addr = random_addr ? 1 : 0;
}

bt_summary bt_table_summarize(const bt_table *table, int min_rounds,
                              int rssi_min, int include_paired)
{
    bt_summary summary = {0, 0, 0, 0};

    for (int i = 0; i < table->count; i++)
    {
        const bt_device *device = &table->devices[i];

        if (device->rounds_seen < 1)
            continue;

        if (device->paired && !include_paired)
        {
            summary.paired_ignored++;
            continue;
        }

        summary.observed++;

        if (device->rounds_seen < min_rounds)
            continue;

        if (device->has_rssi && device->mean_rssi < (double)rssi_min)
            summary.too_far++;
        else
            summary.stable++;
    }

    return summary;
}

int bt_mac_from_path(const char *path, char *mac, size_t size)
{
    const char *p = strstr(path, "/dev_");

    if (p == NULL || size < 18)
        return 0;

    p += strlen("/dev_");

    if (strlen(p) < 17)
        return 0;

    for (int i = 0; i < 17; i++)
    {
        if ((i + 1) % 3 == 0)
        {
            if (p[i] != '_')
                return 0;
            mac[i] = ':';
        }
        else
        {
            if (!isxdigit((unsigned char)p[i]))
                return 0;
            mac[i] = (char)toupper((unsigned char)p[i]);
        }
    }

    if (p[17] != '\0' && p[17] != '/')
        return 0;

    mac[17] = '\0';

    return 1;
}
