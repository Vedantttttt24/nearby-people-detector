# 3. Detection Methodology

## Idea

Most people carry a phone, and many also carry earbuds, a watch or a laptop.
These devices have Bluetooth switched on. If the devices that are close by are
counted, the number can be used as a guess for the number of people.

## Bluetooth

1. Bluetooth discovery is run 3 times, 10 seconds each, one after another
   (about 30 seconds in total).
2. In every round each MAC address that appears is stored. A device is counted
   only once per round, because `bluetoothctl` prints several lines for the
   same device (a `[NEW]` line, RSSI changes and so on).
3. For every device the program remembers in how many different rounds it
   was seen.
4. A device seen in 2 or more rounds is called stable. Devices seen only
   once are ignored, since they are often people just walking past, or
   devices that appeared for a moment.

Repeating the scan makes the result less dependent on one lucky or unlucky
scan.

## Wi-Fi

The Wi-Fi scan with `nmcli` lists the access points around the laptop. The
program removes duplicates (the same BSSID can be listed more than once) and
reports the number of different access points.

An access point is a router, not a person. In the real tests the access point
count did not follow the number of people (see the validation document), so
it is shown as information about the wireless environment and is not used in
the people estimate.

## Estimate

```
Estimated nearby people = number of stable Bluetooth devices
```

## Confidence

- Low: no stable device was found.
- High: at least two thirds of all observed Bluetooth devices were stable.
- Medium: otherwise.

The idea is that when most devices are seen again and again the situation is
steady and the number is more trustworthy.
