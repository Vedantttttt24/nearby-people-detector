#ifndef BT_TABLE_H
#define BT_TABLE_H

#include <stddef.h>

/*
 * Device table and counting rules. Pure C: no D-Bus, no GLib, no I/O.
 * The D-Bus layer (bt_dbus.c) only feeds events into it, so all rules can be
 * unit tested with fake events.
 */

#define BT_MAX_DEVICES 256

typedef struct
{
    char mac[18];
    int rounds_seen;      /* number of different rounds the device was heard in */
    int last_round;
    unsigned observations; /* number of events (advertisement updates) */
    unsigned rssi_samples; /* number of events that carried an RSSI value */
    int has_rssi;
    int best_rssi;         /* strongest RSSI (dBm) */
    double mean_rssi;      /* mean of all RSSI samples (dBm) */
    int paired;            /* device is paired with this computer */
    int random_addr;       /* AddressType is "random" */
} bt_device;

typedef struct
{
    bt_device devices[BT_MAX_DEVICES];
    int count;
    int overflow; /* devices dropped because the table was full */
} bt_table;

typedef struct
{
    int observed;       /* heard in at least one round (paired excluded) */
    int stable;         /* heard in enough rounds and close enough */
    int too_far;        /* heard in enough rounds but mean RSSI too weak */
    int paired_ignored; /* paired devices that were heard but not counted */
} bt_summary;

void bt_table_init(bt_table *table);

/*
 * Records that a device was heard in `round` (rounds start at 1).
 * has_rssi != 0 means rssi_dbm is valid. A device is counted once per round
 * however many events arrive for it.
 */
void bt_table_heard(bt_table *table, const char *mac, int has_rssi,
                    int rssi_dbm, int round);

/* Records properties that do not mean "heard": pairing state, address type. */
void bt_table_set_info(bt_table *table, const char *mac, int paired,
                       int random_addr);

/*
 * Stable = heard in at least min_rounds rounds. If RSSI samples exist the mean
 * must be at least rssi_min dBm; devices without RSSI cannot be filtered.
 * Paired devices are ignored unless include_paired is set.
 */
bt_summary bt_table_summarize(const bt_table *table, int min_rounds,
                              int rssi_min, int include_paired);

/*
 * Extracts the MAC from a BlueZ object path such as
 * "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF". Returns 1 on success.
 * mac must hold at least 18 bytes.
 */
int bt_mac_from_path(const char *path, char *mac, size_t size);

#endif
