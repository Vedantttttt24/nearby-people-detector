# Nearby People Detector

A C program for Linux that estimates how many people are nearby using
Bluetooth and Wi-Fi. It was written as the solution to the Embedded Systems
Intern assessment "Use Bluetooth and Wi-Fi to Detect Nearby People".

Author: Vedant Khairkar, Electronics and Telecommunication Engineering,
Vidyalankar Institute of Technology.

## How it works

1. Wi-Fi: `nmcli` lists the access points that can be heard. The number of
   different access points is reported.
2. Bluetooth: `bluetoothctl` discovers devices in 3 rounds of 10 seconds.
   A device that is seen in at least 2 different rounds is called stable.
3. The estimated number of nearby people is the number of stable Bluetooth
   devices. A confidence level (Low / Medium / High) is printed too.

Details are in the `docs` folder.

## Requirements

- Linux (tested on Ubuntu)
- gcc and make
- BlueZ (`bluetoothctl`), NetworkManager (`nmcli`) and `timeout` (coreutils)
- a Bluetooth adapter that is switched on, and a Wi-Fi adapter

```
sudo apt install build-essential bluez network-manager
```

## Build

```
make
```

This creates `build/people_detector`. `make clean` removes it.

## Run

```
make run
```

or

```
./build/people_detector
```

The scan takes about 30 seconds. The important lines of the result look like
this (values from one of my tests):

```
Wi-Fi access points         : 14
Stable Bluetooth devices    : 7
Estimated nearby people     : 7
```

## Files

```
src/main.c                        the program
Makefile                          build instructions
docs/ARCHITECTURE.md              system architecture
docs/METHODOLOGY.md               detection methodology
docs/ASSUMPTIONS_LIMITATIONS.md   assumptions and limitations
docs/VALIDATION.md                validation approach and results
```
