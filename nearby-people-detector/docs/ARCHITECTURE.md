# System Architecture

The program uses Linux libraries directly instead of calling command-line tools. The counting rules are kept in plain C modules, and the modules that talk to BlueZ and NetworkManager only pass events into them. This way the rules can be tested without Bluetooth hardware.

```
                        main.c  (options, output)
                           |
    +------------+---------+--------------+--------------+
    |            |                        |              |
 bt_dbus.c    wifi_nm.c                lan.c        estimator.c
 (GDBus)      (libnm)               (ARP table)     (people, confidence)
    |
 bt_table.c   (device table and rules)
    |
 BlueZ  (org.bluez on the system D-Bus)
```

| Module | Library or interface | What it does |
|---|---|---|
| `bt_dbus` | GIO / GDBus, BlueZ D-Bus API | Finds the adapter, powers it on, reads which devices are paired, sets the discovery filter, runs the rounds and turns BlueZ signals into table events |
| `bt_table` | none | Keeps the device table: rounds in which a device was heard, mean and best RSSI, paired state. Decides stable, too far and paired |
| `wifi_nm` | libnm | Starts a scan and counts the different access points (BSSID) |
| `lan` | `getifaddrs()`, `/proc/net/arp`, `/proc/net/route` | Finds the Wi-Fi interface and the gateway and counts the other live hosts in the ARP table |
| `estimator` | none | Calculates the estimate, the confidence level and the Wi-Fi cross-check |
| `main` | none | Reads the options, runs the scans in order and prints the result |

## Bluetooth data flow

1. `bt_scanner_new()` connects to the system D-Bus, finds the adapter and the already known devices (with their paired state), powers the adapter on if needed and subscribes to the BlueZ signals `PropertiesChanged` and `InterfacesAdded`.
2. `bt_scanner_run_round()` sets the discovery filter, calls `StartDiscovery`, runs a GLib main loop for the length of the round and calls `StopDiscovery`. Signals that arrive during the round are passed to `bt_table_heard()`.
3. A device counts as heard when BlueZ reports an RSSI, ManufacturerData, ServiceData or TxPower change for it, or creates a new device object. A device that is only in the BlueZ cache sends no signal and is not counted.
4. `bt_table_summarize()` gives the number of stable devices, and `estimate_people()` turns it into the final estimate.

## Error handling

- No system D-Bus, `bluetoothd` not running or no adapter: the program prints a message and stops with exit code 3. It never reports "0 people" because of a missing adapter.
- NetworkManager not reachable or no Wi-Fi device: the access points are shown as unavailable and the estimate is still made from Bluetooth.
- Laptop not connected to Wi-Fi: the host count is unavailable.
- Device table full (256 devices): a warning is printed.

## Why libraries

The BlueZ D-Bus API sends events (new device, RSSI change), so the program knows exactly which devices were heard during a round and can tell them apart from cached ones. libnm gives typed access to the access points, and the ARP table and `getifaddrs()` need no extra packages. Libraries used: GLib/GIO (GDBus), libnm and the C standard library with POSIX.
