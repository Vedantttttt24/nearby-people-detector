# Detection Methodology

## Idea

Most people carry a phone, and many also carry earbuds, a watch or a laptop. Their Bluetooth radios send advertisements, and their phones are usually connected to Wi-Fi. If the devices that are close by are counted, the number can be used as an estimate of the number of people.

## Bluetooth (main signal)

1. Discovery runs in 3 rounds of 10 seconds through the BlueZ D-Bus API.
2. **Heard rule.** A device counts in a round only if BlueZ reports something for it during that round: a change of RSSI, ManufacturerData, ServiceData or TxPower, or a new device object. A device that BlueZ keeps in its cache from an earlier scan sends nothing and is not counted.
3. A device is counted once per round, however many events arrive for it.
4. **Stable rule.** A device heard in at least 2 rounds is called stable. Devices heard only once are ignored, because they are often people just walking past.
5. **Distance rule.** The mean RSSI of all samples of a device must be at least -80 dBm (this limit can be changed). Weaker devices are reported as "too far" and are not counted. RSSI only gives a rough idea of distance, so the limit is a setting and not a distance in metres. A device without any RSSI sample cannot be filtered this way.
6. **Paired devices.** Devices paired with the laptop belong to the tester, not to the people in the room, so they are ignored.

## Wi-Fi

- **Access points.** libnm starts a fresh scan and the program counts the different BSSIDs. Access points are routers, not people, so this number is only shown as information about the surroundings.
- **Hosts on the network.** The program reads the ARP table of the Wi-Fi interface and counts the other live hosts, without the gateway. This number is used only to check the Bluetooth estimate. TVs and other devices on the network are counted too, and phones that are not on this network are not.

The Wi-Fi numbers never add people to the estimate.

## Estimate

```
estimated people = round( stable Bluetooth devices / devices-per-person )
```

The factor is 1.0 by default, which means one discoverable device per person.

## Confidence level

- Low: no stable device was found.
- High: at least two thirds of all heard devices are stable.
- Medium: all other cases.

If the number of hosts on the Wi-Fi network and the estimated number of people differ by more than a factor of 3, the level "High" is lowered to "Medium" and a note is printed. A host count of zero is not treated as a disagreement, because people may not use this network.
