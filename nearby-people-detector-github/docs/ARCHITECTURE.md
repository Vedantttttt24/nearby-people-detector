# 2. System Architecture

The program is a single C source file, `src/main.c`. It does not talk to the
radio hardware directly. It starts the standard Linux tools `nmcli` and
`bluetoothctl` with `popen()`, reads their text output line by line, and
processes it in C. These tools use the Linux Wi-Fi stack (NetworkManager) and
the Linux Bluetooth stack (BlueZ), so the program needs no extra C libraries.

```
                    +-------------------+
                    |      main.c       |
                    +---------+---------+
                              |
              +---------------+----------------+
              |                                |
              v                                v
       +-------------+                +-----------------+
       |    nmcli    |                |  bluetoothctl   |
       | (Wi-Fi scan)|                | 3 rounds x 10 s |
       +------+------+                +--------+--------+
              |                                |
              v                                v
     count unique access             device list (MAC, rounds seen)
     points (BSSID)                           |
              |                                v
              |                       stable devices
              |                       (seen in >= 2 rounds)
              |                                |
              +---------------+----------------+
                              v
                    result: estimated people,
                    confidence, Wi-Fi AP count
```

## Libraries and tools used

C standard library and POSIX:

- `stdio.h`: `printf`, `snprintf`, `fgets`, `sscanf`, and `popen` / `pclose`
  (POSIX) to run a tool and read its output
- `stdlib.h`, `string.h`: `strcmp`, `strcpy`, `strstr`, `strchr`, `memset`
- `ctype.h`: `isxdigit` for checking MAC addresses

Linux tools called by the program:

- `nmcli` (NetworkManager): Wi-Fi scan
- `bluetoothctl` (BlueZ): Bluetooth discovery
- `timeout` (GNU coreutils): makes sure a Bluetooth scan cannot run forever

Build tools: GCC and GNU Make.

## Functions in main.c

- `is_valid_mac()` checks that a text is a MAC address (`xx:xx:xx:xx:xx:xx`).
- `scan_bluetooth_round()` runs one discovery round. For every device line it
  finds the MAC address and updates the device list. A device is counted only
  once per round, even if `bluetoothctl` prints many lines for it.
- `count_stable_devices()` counts devices seen in 2 or more rounds.
- `count_wifi_access_points()` runs `nmcli` and counts different BSSIDs.
- `confidence_level()` gives Low, Medium or High.
- `main()` runs the scans in order and prints the result.

## Why tools and not libraries

Libraries such as `libbluetooth` or `libnl` could be used directly, but they
need extra development packages and much more code. `nmcli` and
`bluetoothctl` are installed on Ubuntu by default, so the program stays short
and easy to build. The disadvantage is that the program depends on the text
format printed by these tools.
