/*
 * Nearby People Detector
 *
 * Estimates how many people are close by from Bluetooth and Wi-Fi, using
 * Linux libraries instead of command-line tools:
 *  - Bluetooth: BlueZ D-Bus API through GDBus (GIO). Devices are counted only
 *    when advertisement events arrive during a round (bt_dbus.c). A device
 *    counts if heard in enough rounds and its mean RSSI is not too weak.
 *  - Wi-Fi: libnm (NetworkManager) for access points; getifaddrs() and
 *    /proc/net/arp for other hosts on the connected network (lan.c). The host
 *    count is a cross-check of the Bluetooth estimate.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bt_dbus.h"
#include "bt_table.h"
#include "estimator.h"
#include "lan.h"
#include "wifi_nm.h"

typedef struct
{
    int rounds;
    int seconds;
    int min_rounds;
    int rssi_min;
    double devices_per_person;
    int use_lan;
    int rescan;
    int include_paired;
    int trace;
    int verbose;
    int csv;
} options;

static void print_usage(const char *program)
{
    printf("Usage: %s [options]\n\n"
           "  --rounds N              Bluetooth rounds (default 3)\n"
           "  --seconds N             seconds per round (default 10)\n"
           "  --min-rounds N          rounds a device must be heard in (default 2)\n"
           "  --rssi-min DBM          ignore devices with a weaker mean RSSI (default -80)\n"
           "  --devices-per-person X  calibration factor (default 1.0)\n"
           "  --include-paired        also count devices paired with this computer\n"
           "  --no-rescan             use NetworkManager's cached access point list\n"
           "  --no-lan                do not count hosts on the Wi-Fi network\n"
           "  --trace                 print every BlueZ event to stderr\n"
           "  --verbose               print the device table\n"
           "  --csv                   print one CSV line only\n"
           "  --help                  show this text\n",
           program);
}

static int parse_long(const char *text, long min, long max, long *out)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' || value < min || value > max)
        return 0;

    *out = value;
    return 1;
}

static int parse_double(const char *text, double min, double max, double *out)
{
    char *end;
    double value;

    errno = 0;
    value = strtod(text, &end);

    if (errno != 0 || end == text || *end != '\0' || value < min || value > max)
        return 0;

    *out = value;
    return 1;
}

/* Returns 0 on success, 1 if --help was shown, -1 on error. */
static int parse_options(int argc, char **argv, options *opt)
{
    opt->rounds = 3;
    opt->seconds = 10;
    opt->min_rounds = 2;
    opt->rssi_min = -80;
    opt->devices_per_person = 1.0;
    opt->use_lan = 1;
    opt->rescan = 1;
    opt->include_paired = 0;
    opt->trace = 0;
    opt->verbose = 0;
    opt->csv = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        long number;

        if (strcmp(arg, "--help") == 0)
        {
            print_usage(argv[0]);
            return 1;
        }
        else if (strcmp(arg, "--include-paired") == 0)
            opt->include_paired = 1;
        else if (strcmp(arg, "--no-rescan") == 0)
            opt->rescan = 0;
        else if (strcmp(arg, "--no-lan") == 0)
            opt->use_lan = 0;
        else if (strcmp(arg, "--trace") == 0)
            opt->trace = 1;
        else if (strcmp(arg, "--verbose") == 0)
            opt->verbose = 1;
        else if (strcmp(arg, "--csv") == 0)
            opt->csv = 1;
        else if (strcmp(arg, "--rounds") == 0 && value != NULL &&
                 parse_long(value, 1, 10, &number))
        {
            opt->rounds = (int)number;
            i++;
        }
        else if (strcmp(arg, "--seconds") == 0 && value != NULL &&
                 parse_long(value, 1, 120, &number))
        {
            opt->seconds = (int)number;
            i++;
        }
        else if (strcmp(arg, "--min-rounds") == 0 && value != NULL &&
                 parse_long(value, 1, 10, &number))
        {
            opt->min_rounds = (int)number;
            i++;
        }
        else if (strcmp(arg, "--rssi-min") == 0 && value != NULL &&
                 parse_long(value, -127, 0, &number))
        {
            opt->rssi_min = (int)number;
            i++;
        }
        else if (strcmp(arg, "--devices-per-person") == 0 && value != NULL &&
                 parse_double(value, 0.1, 10.0, &opt->devices_per_person))
            i++;
        else
        {
            fprintf(stderr, "Invalid or incomplete option: %s\n", arg);
            print_usage(argv[0]);
            return -1;
        }
    }

    if (opt->min_rounds > opt->rounds)
    {
        fprintf(stderr, "--min-rounds cannot be larger than --rounds\n");
        return -1;
    }

    return 0;
}

static const char *device_status(const bt_device *d, const options *opt)
{
    if (d->rounds_seen == 0)
        return "not heard (cached by BlueZ)";
    if (d->paired && !opt->include_paired)
        return "paired (ignored)";
    if (d->rounds_seen < opt->min_rounds)
        return "unstable";
    if (d->has_rssi && d->mean_rssi < (double)opt->rssi_min)
        return "too far";
    return "stable";
}

static void print_device_table(const bt_table *table, const options *opt)
{
    printf("\n    %-18s %6s %7s %9s %8s %-4s  %s\n", "Device", "Rounds",
           "Events", "Mean dBm", "Best dBm", "Addr", "Status");

    for (int i = 0; i < table->count; i++)
    {
        const bt_device *d = &table->devices[i];
        const char *kind = d->random_addr ? "rand" : "pub";

        if (d->has_rssi)
            printf("    %-18s %6d %7u %9.1f %8d %-4s  %s\n", d->mac,
                   d->rounds_seen, d->observations, d->mean_rssi, d->best_rssi,
                   kind, device_status(d, opt));
        else
            printf("    %-18s %6d %7u %9s %8s %-4s  %s\n", d->mac,
                   d->rounds_seen, d->observations, "-", "-", kind,
                   device_status(d, opt));
    }
}

int main(int argc, char **argv)
{
    static bt_table table;
    options opt;
    bt_scanner *scanner;
    wifi_nm_info nm;
    lan_info lan;
    bt_summary summary;
    estimate_input input;
    estimate_result result;
    char err[256] = "";
    int status;

    status = parse_options(argc, argv, &opt);
    if (status != 0)
        return status > 0 ? 0 : 2;

    bt_table_init(&table);

    scanner = bt_scanner_new(&table, opt.trace, err, sizeof(err));
    if (scanner == NULL)
    {
        fprintf(stderr, "Bluetooth: %s\n", err);
        return 3;
    }

    if (!opt.csv)
    {
        printf("\n==============================================\n");
        printf(" NEARBY PEOPLE DETECTOR\n");
        printf(" Bluetooth (BlueZ D-Bus) + Wi-Fi (libnm)\n");
        printf("==============================================\n\n");
        printf("[1] Wi-Fi...\n");
    }

    if (wifi_nm_scan(opt.rescan, &nm, err, sizeof(err)) != 0)
    {
        fprintf(stderr, "Wi-Fi: NetworkManager not available: %s\n", err);
        memset(&nm, 0, sizeof(nm));
        nm.access_points = -1;
    }

    memset(&lan, 0, sizeof(lan));
    if (opt.use_lan)
        lan = lan_scan(nm.available ? nm.iface : NULL);

    if (!opt.csv)
    {
        if (nm.access_points >= 0 && nm.available)
            printf("    Access points heard : %d%s\n", nm.access_points,
                   nm.scan_requested && !nm.scan_fresh ? " (cached list)" : "");
        else
            printf("    Access points heard : unavailable\n");

        if (lan.available)
            printf("    Other hosts on %s : %d\n", lan.iface, lan.hosts);
        else if (opt.use_lan)
            printf("    Not connected to a Wi-Fi network: host count unavailable\n");

        printf("\n[2] Bluetooth on %s (%d rounds x %d s)...\n\n",
               bt_scanner_adapter(scanner), opt.rounds, opt.seconds);
    }

    for (int round = 1; round <= opt.rounds; round++)
    {
        if (!opt.csv)
            printf("    Round %d/%d...\n", round, opt.rounds);

        if (bt_scanner_run_round(scanner, round, (unsigned)opt.seconds, err,
                                 sizeof(err)) != 0)
        {
            fprintf(stderr, "Bluetooth: %s\n", err);
            bt_scanner_free(scanner);
            return 3;
        }
    }

    bt_scanner_free(scanner);

    summary = bt_table_summarize(&table, opt.min_rounds, opt.rssi_min,
                                 opt.include_paired);

    input.stable_bt = summary.stable;
    input.observed_bt = summary.observed;
    input.lan_available = lan.available;
    input.lan_hosts = lan.hosts;
    input.devices_per_person = opt.devices_per_person;
    result = estimate_people(&input);

    if (opt.csv)
    {
        printf("wifi_aps,lan_hosts,bt_observed,bt_stable,bt_too_far,estimated,confidence\n");
        printf("%d,%d,%d,%d,%d,%d,%s\n", nm.available ? nm.access_points : -1,
               lan.available ? lan.hosts : -1, summary.observed, summary.stable,
               summary.too_far, result.people, result.confidence);
        return 0;
    }

    if (opt.verbose)
        print_device_table(&table, &opt);

    printf("\n----------------------------------------------\n");
    if (nm.available)
        printf("Wi-Fi access points       : %d\n", nm.access_points);
    if (lan.available)
        printf("Other hosts on Wi-Fi net  : %d\n", lan.hosts);
    printf("Bluetooth devices heard   : %d\n", summary.observed);
    printf("Paired (ignored)          : %d\n", summary.paired_ignored);
    printf("Ignored as too far        : %d\n", summary.too_far);
    printf("Stable Bluetooth devices  : %d\n", summary.stable);
    printf("Estimated nearby people   : %d\n", result.people);
    printf("Estimation confidence     : %s\n", result.confidence);
    printf("----------------------------------------------\n\n");

    if (table.overflow > 0)
        printf("Warning: %d devices did not fit in the table.\n", table.overflow);
    if (result.lan_disagrees)
        printf("Note: Wi-Fi host count and Bluetooth estimate differ by more than 3x.\n");

    printf("Notes:\n");
    printf("- A device counts if heard in %d or more rounds and its mean RSSI is\n"
           "  not below %d dBm. Devices only cached by BlueZ are not counted.\n",
           opt.min_rounds, opt.rssi_min);
    printf("- Access points are routers, not people: reported only.\n");
    printf("- One person can carry several devices and some carry none, so\n");
    printf("  this is an estimate (devices per person factor: %.2f).\n\n",
           opt.devices_per_person);

    return 0;
}
