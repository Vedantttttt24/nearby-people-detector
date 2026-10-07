# Assumptions and Limitations

## Assumptions

- On average a nearby person has about one discoverable Bluetooth device.
- The devices are switched on and send advertisements during the scan.
- People stay in the area for most of the scan (about 30 seconds). Anyone present in at least 2 of the 3 rounds can be counted.
- `bluetoothd` and NetworkManager are running, the Bluetooth adapter is not blocked and the user is allowed to use BlueZ discovery over D-Bus.
- The program runs on Linux (developed and tested on Ubuntu).
- For the host count, the laptop is connected to a Wi-Fi network.

## Limitations

- **Devices are not people.** One person can carry a phone, earbuds and a watch, which makes the estimate too high. A person with no Bluetooth device switched on is not found at all, which makes it too low. Both errors in the real tests were one person too high.
- **Events from BlueZ.** A device is counted only when BlueZ sends a signal for it. BlueZ may send few RSSI updates for a device whose signal stays the same, so the program also counts ManufacturerData, ServiceData and TxPower changes. The behaviour can differ between BlueZ versions (the tests used BlueZ 5.85).
- **RSSI is not distance.** A strong device behind a wall can be counted, and a weak device inside the room can be dropped.
- **Random addresses.** Many devices change their Bluetooth address from time to time. Such a device can be counted twice, or can look unstable and be missed.
- **Other devices.** Devices that belong to nobody present, such as a speaker or a TV, can be counted.
- **Paired devices are ignored.** A phone that is paired with the laptop is not counted, even if its owner is in the room.
- **Host count.** Only devices on the connected Wi-Fi network are seen. Phones on mobile data or with Wi-Fi off are not seen, and networks that isolate their clients (common at universities and in public places) show almost no hosts. The ARP table only contains hosts the laptop has recently talked to, so the count can be low. TVs and other non-person devices are counted.
- **Access points are not people.** They are only reported.
- **Dependence on the environment.** Other Bluetooth devices nearby, walls and the position of the laptop change the result.
- **Snapshot.** A scan takes about 30 seconds, so the result is a snapshot and not a live count.
- **Small test set.** The accuracy was measured in 10 tests with 1 to 4 people (see Section 5). Results for larger groups may differ.
