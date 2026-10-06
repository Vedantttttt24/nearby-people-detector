# 5. Validation Approach

## Method

The program was validated by comparing its estimate with the real number of
people. For each test the real number of people was known, the compiled
program was run directly (`./build/people_detector`), and the estimated
number was written down together with the Wi-Fi access point count. The
error was calculated for each test:

```
absolute error   = |estimated people - actual people|
percentage error = absolute error / actual people x 100
```

The accuracy of one test is 100 % minus its percentage error:

```
accuracy = (1 - absolute error / actual people) x 100
```

The errors are then averaged: the mean absolute error (MAE), the mean
absolute percentage error (MAPE) and the mean accuracy. The code itself is compiled with
`gcc -Wall -Wextra` and builds without warnings.

## Test setup

- Laptop: HP Victus
- Operating system: Ubuntu 26.04 (Live USB)
- Bluetooth hardware: Intel AX211
- Wi-Fi interface: `wlo1`
- Wi-Fi scan: `nmcli`
- Bluetooth scan: `bluetoothctl`, 3 rounds of 10 seconds each (about 30
  seconds in total)
- Monitor mode was not used.

## Results

| Test | Actual people | Wi-Fi access points | Estimated people (stable Bluetooth devices) | Absolute error | Percentage error | Accuracy |
|------|--------------:|--------------------:|--------------------------------------------:|---------------:|-----------------:|---------:|
| 1 | 6 | 14 | 7 | 1 | 16.67 % | 83.33 % |
| 2 | 9 | 19 | 12 | 3 | 33.33 % | 66.67 % |
| 3 | 11 | 16 | 13 | 2 | 18.18 % | 81.82 % |

- Mean actual people: 8.67
- Mean estimated people: 10.67
- Mean absolute error (MAE): 2.00 people
- Mean absolute percentage error (MAPE): 22.73 %
- **Mean accuracy: 77.27 %** (100 % - 22.73 %)

## Discussion

- The program reached an average accuracy of about 77 % in these tests: the
  best test was 83.33 % (6 people, estimate 7) and the weakest was 66.67 %
  (9 people, estimate 12). None of the three estimates was exactly right.
- The estimate follows the real number of people: it was higher for the
  bigger groups (7, 12 and 13 for 6, 9 and 11 people).
- The estimate was higher than the real number in all three tests. A likely
  reason is that some people carry more than one Bluetooth device, such as a
  phone together with earbuds or a watch. The program counts devices, not
  people.
- The Wi-Fi access point count (14, 19 and 16) did not follow the number of
  people. For example, 16 access points were found with 11 people, but 19
  with 9 people. This supports the decision to treat access points as
  information about the environment and not to use them in the estimate.
- Only three tests were done, so these numbers show the general behaviour of
  the program and not a precise accuracy for every situation.
