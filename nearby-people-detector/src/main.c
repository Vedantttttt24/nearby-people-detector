/*
 * Nearby People Detector
 * Estimates how many people are close by using Bluetooth and Wi-Fi.
 *
 * Bluetooth: devices are discovered with bluetoothctl (BlueZ) in several
 *            rounds. A device seen in at least two different rounds is
 *            "stable", and the number of stable devices is the estimate.
 * Wi-Fi:     nearby access points are listed with nmcli (NetworkManager).
 *            They describe the wireless environment and are reported
 *            next to the estimate.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_DEVICES 256
#define LINE_SIZE 512
#define BT_SCAN_SECONDS 10
#define BT_SCAN_ROUNDS 3
#define MIN_ROUNDS_FOR_STABLE 2

typedef struct
{
    char mac[18];
    int rounds_seen;    /* number of different rounds the device appeared in */
    int last_round;     /* last round in which the device was counted */
} BluetoothDevice;

/* Checks the format xx:xx:xx:xx:xx:xx */
static int is_valid_mac(const char *mac)
{
    if (strlen(mac) != 17)
        return 0;

    for (int i = 0; i < 17; i++)
    {
        if ((i + 1) % 3 == 0)
        {
            if (mac[i] != ':')
                return 0;
        }
        else if (!isxdigit((unsigned char)mac[i]))
        {
            return 0;
        }
    }

    return 1;
}

static int find_device(BluetoothDevice devices[], int count, const char *mac)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(devices[i].mac, mac) == 0)
            return i;
    }

    return -1;
}

/*
 * Runs one Bluetooth discovery round and updates the device list.
 * bluetoothctl prints several lines for the same device during a scan
 * ([NEW] line, RSSI changes, ...), so a device is counted only once per round.
 * Returns the new number of devices in the list.
 */
static int scan_bluetooth_round(BluetoothDevice devices[], int count, int round)
{
    FILE *fp;
    char command[128];
    char line[LINE_SIZE];

    snprintf(command, sizeof(command),
             "timeout %d bluetoothctl --timeout %d scan on 2>&1",
             BT_SCAN_SECONDS + 5, BT_SCAN_SECONDS);

    fp = popen(command, "r");

    if (fp == NULL)
        return count;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char mac[18];
        char *device_pos;

        if (strstr(line, "[DEL]") != NULL)
            continue;

        device_pos = strstr(line, "Device ");

        if (device_pos == NULL)
            continue;

        if (sscanf(device_pos, "Device %17s", mac) != 1)
            continue;

        if (!is_valid_mac(mac))
            continue;

        int index = find_device(devices, count, mac);

        if (index >= 0)
        {
            if (devices[index].last_round != round)
            {
                devices[index].rounds_seen++;
                devices[index].last_round = round;
            }
        }
        else if (count < MAX_DEVICES)
        {
            strcpy(devices[count].mac, mac);
            devices[count].rounds_seen = 1;
            devices[count].last_round = round;
            count++;
        }
    }

    pclose(fp);

    return count;
}

static int count_stable_devices(BluetoothDevice devices[], int count)
{
    int stable = 0;

    for (int i = 0; i < count; i++)
    {
        if (devices[i].rounds_seen >= MIN_ROUNDS_FOR_STABLE)
            stable++;
    }

    return stable;
}

/* Counts the different Wi-Fi access points (BSSIDs) listed by nmcli. */
static int count_wifi_access_points(void)
{
    FILE *fp;
    char line[LINE_SIZE];
    char seen[MAX_DEVICES][64];
    int count = 0;

    fp = popen("nmcli -t -f BSSID dev wifi list 2>/dev/null", "r");

    if (fp == NULL)
        return 0;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *newline = strchr(line, '\n');

        if (newline != NULL)
            *newline = '\0';

        if (strlen(line) == 0 || strlen(line) >= 64)
            continue;

        int duplicate = 0;

        for (int i = 0; i < count; i++)
        {
            if (strcmp(seen[i], line) == 0)
            {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate && count < MAX_DEVICES)
        {
            strcpy(seen[count], line);
            count++;
        }
    }

    pclose(fp);

    return count;
}

/*
 * Confidence depends on how many of the observed Bluetooth devices were
 * stable. If most devices appear in several rounds the result is more
 * reliable than when most devices only appeared once.
 */
static const char *confidence_level(int stable_devices, int total_devices)
{
    if (stable_devices == 0)
        return "Low";

    if (total_devices > 0 && stable_devices >= (total_devices * 2) / 3)
        return "High";

    return "Medium";
}

int main(void)
{
    BluetoothDevice devices[MAX_DEVICES];
    int device_count = 0;
    int wifi_access_points;
    int stable_devices;
    int estimated_people;

    memset(devices, 0, sizeof(devices));

    printf("\n");
    printf("==============================================\n");
    printf("          NEARBY PEOPLE DETECTOR\n");
    printf("          Bluetooth + Wi-Fi\n");
    printf("==============================================\n\n");

    printf("[1] Scanning Wi-Fi...\n");

    wifi_access_points = count_wifi_access_points();

    printf("    Wi-Fi access points detected: %d\n\n", wifi_access_points);

    printf("[2] Scanning Bluetooth...\n");
    printf("    %d rounds x %d seconds\n\n", BT_SCAN_ROUNDS, BT_SCAN_SECONDS);

    for (int round = 1; round <= BT_SCAN_ROUNDS; round++)
    {
        int before = device_count;

        printf("    Bluetooth scan %d/%d...\n", round, BT_SCAN_ROUNDS);

        device_count = scan_bluetooth_round(devices, device_count, round);

        printf("    Unique devices observed so far: %d\n\n", device_count);

        if (device_count == before)
            printf("    No new Bluetooth devices in this round.\n\n");
    }

    stable_devices = count_stable_devices(devices, device_count);
    estimated_people = stable_devices;

    printf("----------------------------------------------\n");
    printf("Wi-Fi access points         : %d\n", wifi_access_points);
    printf("Bluetooth devices observed  : %d\n", device_count);
    printf("Stable Bluetooth devices    : %d\n", stable_devices);
    printf("Estimated nearby people     : %d\n", estimated_people);
    printf("Estimation confidence       : %s\n",
           confidence_level(stable_devices, device_count));
    printf("----------------------------------------------\n\n");

    printf("Notes:\n");
    printf("- A Bluetooth device seen in %d or more rounds counts as stable.\n",
           MIN_ROUNDS_FOR_STABLE);
    printf("- Wi-Fi access points are routers, not people, so they are only\n");
    printf("  reported and not used in the estimate.\n");
    printf("- One person can carry several devices, and some people carry\n");
    printf("  none that can be seen, so the result is an estimate.\n\n");

    return 0;
}
