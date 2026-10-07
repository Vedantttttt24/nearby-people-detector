#ifndef BT_DBUS_H
#define BT_DBUS_H

#include <stddef.h>

#include "bt_table.h"

/*
 * Bluetooth discovery through the BlueZ D-Bus API (GDBus, from GIO).
 *
 * A device is "heard" in a round only when an event for it arrives while the
 * round is running: an RSSI / ManufacturerData / ServiceData / TxPower change
 * (org.freedesktop.DBus.Properties.PropertiesChanged) or a new device object
 * (org.freedesktop.DBus.ObjectManager.InterfacesAdded). Devices that BlueZ
 * only has in its cache from earlier scans produce no events and are not
 * counted.
 */

typedef struct bt_scanner bt_scanner;

/*
 * Connects to the system bus, finds the first adapter, powers it on, reads
 * which devices are paired, and subscribes to the BlueZ signals.
 * Returns NULL on failure and writes a message into err.
 */
bt_scanner *bt_scanner_new(bt_table *table, int trace, char *err,
                           size_t err_size);

/* Runs discovery for `seconds` seconds as round number `round` (from 1). */
int bt_scanner_run_round(bt_scanner *scanner, int round, unsigned seconds,
                         char *err, size_t err_size);

/* Adapter object path, e.g. "/org/bluez/hci0". */
const char *bt_scanner_adapter(const bt_scanner *scanner);

void bt_scanner_free(bt_scanner *scanner);

#endif
