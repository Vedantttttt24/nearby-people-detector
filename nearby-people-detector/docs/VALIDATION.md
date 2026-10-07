# Validation Approach

## Method

The estimate of the program was compared with the real number of people. For each test the number of people in the room was counted by hand, the program was started, and the estimated number was written down. Then the error was calculated:

```
absolute error   = |estimated - actual|
percentage error = absolute error / actual x 100
accuracy         = 100 % - percentage error
```

The errors of all tests were combined into the mean absolute error (MAE), the mean absolute percentage error (MAPE), the mean accuracy and the exact match rate (the share of tests in which the estimate was exactly right). The script `scripts/metrics.sh` calculates these values from `results/results.csv`.

The program was also checked in other ways:

- **Unit tests** (`make test`, 52 checks) cover the counting rules, the parsing of the MAC address, ARP and route lines, and the estimate and confidence level. They need no hardware.
- **Mock BlueZ test** (`tests/run_mock_test.sh`) runs the real program against a mock BlueZ service on a private D-Bus. The mock contains a stable device, an unstable device, a cached device that sends no signal, a weak device, a paired device and a device without RSSI. The program classified all of them correctly. The same run showed no errors with AddressSanitizer and UBSan.
- **Compiler check:** the code builds with `gcc -Wall -Wextra -Wpedantic` without warnings.

## Test setup

| Item | Value |
|---|---|
| Laptop | HP Victus |
| Operating system | Ubuntu 26.04.1 LTS |
| Bluetooth hardware | Intel AX211 |
| Wi-Fi interface | wlo1 |
| BlueZ version | 5.85 |
| Command | `./build/people_detector --rounds 3 --seconds 10 --min-rounds 2 --verbose` |
| Settings | default RSSI limit -80 dBm, devices per person 1.0 |
| Actual number of people | counted by hand during each test |

## Results

| Test | Actual people | Estimated people | Absolute error | Percentage error | Accuracy |
|---|---|---|---|---|---|
| 1 | 4 | 4 | 0 | 0 % | 100 % |
| 2 | 3 | 3 | 0 | 0 % | 100 % |
| 3 | 3 | 4 | 1 | 33.33 % | 66.67 % |
| 4 | 1 | 1 | 0 | 0 % | 100 % |
| 5 | 2 | 2 | 0 | 0 % | 100 % |
| 6 | 2 | 2 | 0 | 0 % | 100 % |
| 7 | 3 | 3 | 0 | 0 % | 100 % |
| 8 | 2 | 2 | 0 | 0 % | 100 % |
| 9 | 1 | 1 | 0 | 0 % | 100 % |
| 10 | 2 | 3 | 1 | 50.00 % | 50.00 % |

- Mean actual people: 2.30
- Mean estimated people: 2.50
- Exact match rate: 80 % (8 of 10 tests)
- Mean absolute error (MAE): 0.20 people
- Mean absolute percentage error (MAPE): 8.33 %
- Mean accuracy: 91.67 % (100 % - 8.33 %)

## Discussion

- The estimate was exactly right in 8 of the 10 tests and one person too high in the other two. No estimate was off by more than one person, and none was too low.
- A likely reason for the two errors is that some people carry more than one Bluetooth device, for example a phone together with earbuds or a watch. The program counts devices, not people. This was not checked for the two tests.
- The estimated number of devices per person is 1.09 (25 estimated devices for 23 people), so the default factor of 1.0 was kept.
- With small groups one wrong device already gives a large percentage error: one extra device with 2 people is an error of 50 %.
- Only 10 tests with 1 to 4 people were done, so the numbers show how the program behaves in general and not a precise accuracy for every situation.
