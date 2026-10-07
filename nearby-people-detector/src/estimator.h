#ifndef ESTIMATOR_H
#define ESTIMATOR_H

typedef struct
{
    int stable_bt;   /* stable Bluetooth devices */
    int observed_bt; /* all Bluetooth devices heard at least once */
    int lan_available;
    int lan_hosts;   /* other hosts on the Wi-Fi network */
    double devices_per_person; /* calibration factor, 1.0 = one per person */
} estimate_input;

typedef struct
{
    int people;
    const char *confidence; /* "Low", "Medium" or "High" */
    int lan_disagrees;      /* Wi-Fi hosts and Bluetooth differ by more than 3x */
} estimate_result;

/*
 * Confidence from stability only: Low if nothing is stable, High if at least
 * two thirds of the observed devices are stable, otherwise Medium.
 * Integer arithmetic without rounding: stable * 3 >= observed * 2.
 */
const char *estimate_stability_confidence(int stable, int observed);

estimate_result estimate_people(const estimate_input *input);

#endif
