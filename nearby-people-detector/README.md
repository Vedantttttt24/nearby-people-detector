# Nearby People Detector

Embedded Systems Intern assessment: Use Bluetooth and Wi-Fi to Detect Nearby People.
A C program for Linux that estimates how many people are nearby.

Author: Vedant Khairkar, Department of Electronics and Telecommunication Engineering, Vidyalankar Institute of Technology.
GitHub repository: https://github.com/Vedantttttt24/nearby-people-detector

## Summary

The program estimates the number of nearby people mainly from Bluetooth. It listens to Bluetooth advertisements through the BlueZ D-Bus API in three rounds of 10 seconds. A device is counted when it was heard in at least two rounds and its mean signal strength (RSSI) is not weaker than -80 dBm. Devices paired with the laptop are ignored.

Wi-Fi is read in two ways: the number of access points through libnm, and the number of other hosts on the connected network through `/proc/net/arp`. These Wi-Fi numbers are not turned into people. The host count is only used to check the Bluetooth estimate and can lower the confidence level.

In 10 real tests with 1 to 4 people the estimate was exactly right 8 times (80 %), and 2 times it was one person too high. The mean absolute error was 0.20 people. The details are in Section 5.

## How it works

1. **Bluetooth** (main signal): the program talks to BlueZ over D-Bus using GDBus (GIO). It subscribes to BlueZ signals, starts discovery and counts a device in a round only if an advertisement for it arrives during that round. Devices that BlueZ only has in its cache are not counted.
2. **Wi-Fi** (second signal): libnm gives the number of access points around the laptop. The number of other hosts on the Wi-Fi network is read from the kernel ARP table and is used as a cross-check.
3. **Estimate:** estimated people = stable Bluetooth devices / devices-per-person factor (default 1.0). A confidence level (Low, Medium or High) is printed with the result.

## Requirements

- Ubuntu (or another Linux with BlueZ and NetworkManager), `gcc`, `make`, `pkg-config`
- BlueZ (`bluetoothd`) and NetworkManager running, a Bluetooth adapter that is not blocked, and a Wi-Fi adapter

```
sudo apt install build-essential pkg-config libglib2.0-dev libnm-dev bluez network-manager
```

## Build, test and run

```
make                    # builds build/people_detector (no warnings with -Wall -Wextra -Wpedantic)
make test               # unit tests, no hardware needed
make run                # one scan, about 30 seconds
./build/people_detector --help
```

| Option | Meaning |
|---|---|
| `--rounds N` | number of Bluetooth rounds (default 3) |
| `--seconds N` | seconds per round (default 10) |
| `--min-rounds N` | rounds a device must be heard in (default 2) |
| `--rssi-min DBM` | ignore devices with a weaker mean RSSI (default -80) |
| `--devices-per-person X` | devices per person factor (default 1.0) |
| `--include-paired` | also count devices paired with this computer |
| `--no-rescan` | use the cached access point list of NetworkManager |
| `--no-lan` | do not count hosts on the Wi-Fi network |
| `--verbose` | print the table of all devices |
| `--trace` | print every BlueZ event to stderr |
| `--csv` | print the result as one CSV line |

## Files

```
src/main.c          options and output
src/bt_dbus.[ch]    BlueZ D-Bus scanner (GDBus): adapter, signals, rounds
src/bt_table.[ch]   device table and counting rules
src/wifi_nm.[ch]    access points through libnm
src/lan.[ch]        hosts on the Wi-Fi network (ARP table)
src/estimator.[ch]  people estimate and confidence level
tests/              unit tests and a mock BlueZ service
scripts/metrics.sh  error metrics from a CSV file
docs/               architecture, methodology, assumptions, validation
results/results.csv the 10 real test results
```
