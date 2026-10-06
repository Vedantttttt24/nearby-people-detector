# 4. Assumptions and Limitations

## Assumptions

- On average a nearby person has about one Bluetooth device that can be
  discovered.
- The Bluetooth devices are switched on and discoverable during the scan.
- The people stay in the area for the whole scan (about 30 seconds). Anyone
  who stays for at least two of the three rounds is counted.
- The laptop has a working Bluetooth adapter and Wi-Fi adapter, and the
  tools `bluetoothctl`, `nmcli` and `timeout` are installed.
- The program is run on Linux. It was tested on Ubuntu.

## Limitations

- **Devices are not people.** One person can carry a phone, earbuds, a watch
  and a laptop, which makes the estimate too high. A person with no
  Bluetooth device switched on is not found at all, which makes it too low.
  In the real tests the estimate was higher than the real number every time.
- **No distance check.** The program does not look at signal strength, so a
  device in the next room can be counted if the laptop can still discover it.
- **Random addresses.** Many Bluetooth devices change their address from time
  to time. A device that changes its address during the scan can appear as
  two devices.
- **Other devices.** Devices that belong to nobody present (for example a
  speaker, a TV or devices already paired with the laptop) can be counted.
- **Wi-Fi is not used for the count.** The Wi-Fi scan only finds access
  points. It does not find the phones of people, so it does not change the
  estimate. Finding phones by Wi-Fi would need monitor mode and the capture of
  probe requests, which was not part of this implementation.
- **Depends on the tools.** The program reads the text printed by
  `bluetoothctl` and `nmcli`. If the format of that text changes in another
  version, the parsing may need to be adjusted.
- **Time.** A scan needs about 30 seconds, so the result is a snapshot, not
  a live count.
- **Small test.** The accuracy was measured with only three tests (see the
  validation document).

## Possible improvements

- Use the signal strength (RSSI) to ignore devices that are far away.
- Calibrate a "devices per person" factor, for example from reference counts.
- Capture Wi-Fi probe requests in monitor mode so that phones can be counted.
- Use the BlueZ and Linux wireless libraries directly instead of the tools.
