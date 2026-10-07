/*
 * Unit tests for the pure C modules. No Bluetooth/Wi-Fi hardware, no GLib.
 * Run with: make test
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "bt_table.h"
#include "estimator.h"
#include "lan.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        checks++;                                                            \
        if (!(cond))                                                         \
        {                                                                    \
            failures++;                                                      \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
        }                                                                    \
    } while (0)

static void test_mac_from_path(void)
{
    char mac[18];

    CHECK(bt_mac_from_path("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF", mac, sizeof(mac)));
    CHECK(strcmp(mac, "AA:BB:CC:DD:EE:FF") == 0);

    CHECK(bt_mac_from_path("/org/bluez/hci0/dev_aa_bb_cc_dd_ee_01", mac, sizeof(mac)));
    CHECK(strcmp(mac, "AA:BB:CC:DD:EE:01") == 0);

    /* sub-objects of a device still belong to that device */
    CHECK(bt_mac_from_path("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF/service000a", mac, sizeof(mac)));

    CHECK(!bt_mac_from_path("/org/bluez/hci0", mac, sizeof(mac)));
    CHECK(!bt_mac_from_path("/org/bluez/hci0/dev_AA_BB_CC_DD_EE", mac, sizeof(mac)));
    CHECK(!bt_mac_from_path("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_GG", mac, sizeof(mac)));
    CHECK(!bt_mac_from_path("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FFX", mac, sizeof(mac)));
    CHECK(!bt_mac_from_path("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF", mac, 10));
}

static void test_table_rounds_and_rssi(void)
{
    static bt_table t;
    bt_summary s;

    bt_table_init(&t);

    /* A: heard in rounds 1 and 2, close: stable. Many events count once per round. */
    bt_table_heard(&t, "AA:00:00:00:00:01", 1, -60, 1);
    bt_table_heard(&t, "AA:00:00:00:00:01", 1, -62, 1);
    bt_table_heard(&t, "AA:00:00:00:00:01", 1, -58, 2);

    /* B: only known (cached): set_info alone must never count as heard */
    bt_table_set_info(&t, "BB:00:00:00:00:02", 0, 0);

    /* C: heard in rounds 1 and 3 but very weak: too far */
    bt_table_heard(&t, "CC:00:00:00:00:03", 1, -90, 1);
    bt_table_heard(&t, "CC:00:00:00:00:03", 1, -88, 3);

    /* D: heard once only: unstable */
    bt_table_heard(&t, "DD:00:00:00:00:04", 1, -40, 2);

    /* E: heard in 2 rounds without RSSI (only manufacturer data): cannot be filtered */
    bt_table_heard(&t, "EE:00:00:00:00:05", 0, 0, 1);
    bt_table_heard(&t, "EE:00:00:00:00:05", 0, 0, 2);

    s = bt_table_summarize(&t, 2, -80, 0);
    CHECK(s.observed == 4);  /* A, C, D, E; B never heard */
    CHECK(s.stable == 2);    /* A, E */
    CHECK(s.too_far == 1);   /* C */
    CHECK(s.paired_ignored == 0);

    CHECK(t.devices[0].rounds_seen == 2);
    CHECK(t.devices[0].observations == 3);
    CHECK(t.devices[0].rssi_samples == 3);
    CHECK(t.devices[0].best_rssi == -58);
    CHECK(t.devices[0].mean_rssi > -60.1 && t.devices[0].mean_rssi < -59.9);

    /* a weaker RSSI limit lets C through */
    s = bt_table_summarize(&t, 2, -95, 0);
    CHECK(s.stable == 3);
    CHECK(s.too_far == 0);

    /* rounds below 1 are ignored */
    bt_table_heard(&t, "FF:00:00:00:00:06", 1, -50, 0);
    CHECK(t.count == 5);
}

static void test_table_paired_and_overflow(void)
{
    static bt_table t;
    bt_summary s;
    char mac[18];

    bt_table_init(&t);

    bt_table_set_info(&t, "AA:00:00:00:00:01", 1, 0); /* paired with this computer */
    bt_table_heard(&t, "AA:00:00:00:00:01", 1, -50, 1);
    bt_table_heard(&t, "AA:00:00:00:00:01", 1, -50, 2);

    s = bt_table_summarize(&t, 2, -80, 0);
    CHECK(s.stable == 0);
    CHECK(s.paired_ignored == 1);

    s = bt_table_summarize(&t, 2, -80, 1);
    CHECK(s.stable == 1);

    /* table full: extra devices are counted as overflow, nothing crashes */
    bt_table_init(&t);
    for (int i = 0; i < BT_MAX_DEVICES + 5; i++)
    {
        snprintf(mac, sizeof(mac), "00:00:00:00:%02X:%02X", i / 256, i % 256);
        bt_table_heard(&t, mac, 1, -50, 1);
    }
    CHECK(t.count == BT_MAX_DEVICES);
    CHECK(t.overflow == 5);
}

static void test_lan_arp(void)
{
    CHECK(lan_arp_line_counts("192.168.1.20      0x1         0x2         aa:bb:cc:dd:ee:01     *        wlo1\n", "wlo1", "192.168.1.1"));
    /* gateway not counted */
    CHECK(!lan_arp_line_counts("192.168.1.1       0x1         0x2         aa:bb:cc:dd:ee:02     *        wlo1\n", "wlo1", "192.168.1.1"));
    /* incomplete entry (flags 0x0) and all-zero address */
    CHECK(!lan_arp_line_counts("192.168.1.21      0x1         0x0         00:00:00:00:00:00     *        wlo1\n", "wlo1", "192.168.1.1"));
    CHECK(!lan_arp_line_counts("192.168.1.22      0x1         0x2         00:00:00:00:00:00     *        wlo1\n", "wlo1", NULL));
    /* other interface */
    CHECK(!lan_arp_line_counts("10.0.0.5          0x1         0x2         aa:bb:cc:dd:ee:03     *        eth0\n", "wlo1", NULL));
    /* header line and empty line */
    CHECK(!lan_arp_line_counts("IP address       HW type     Flags       HW address            Mask     Device\n", "wlo1", NULL));
    CHECK(!lan_arp_line_counts("\n", "wlo1", NULL));
    /* no interface filter */
    CHECK(lan_arp_line_counts("10.0.0.5          0x1         0x2         aa:bb:cc:dd:ee:03     *        eth0\n", NULL, NULL));
}

static void test_lan_route(void)
{
    char gateway[16];

    /* the kernel prints the address in raw network byte order as a hex number */
    unsigned int raw = 0;
    char line[128];
    struct in_addr a;

    inet_pton(AF_INET, "192.168.1.1", &a);
    raw = (unsigned int)a.s_addr;

    snprintf(line, sizeof(line), "wlo1\t00000000\t%08X\t0003\t0\t0\t600\t00000000\t0\t0\t0\n", raw);

    CHECK(lan_route_line_gateway(line, "wlo1", gateway, sizeof(gateway)));
    CHECK(strcmp(gateway, "192.168.1.1") == 0);

    CHECK(!lan_route_line_gateway(line, "eth0", gateway, sizeof(gateway)));

    /* not a default route (destination is not zero) */
    snprintf(line, sizeof(line), "wlo1\t0001A8C0\t00000000\t0001\t0\t0\t600\t00FFFFFF\t0\t0\t0\n");
    CHECK(!lan_route_line_gateway(line, "wlo1", gateway, sizeof(gateway)));

    CHECK(!lan_route_line_gateway("Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\n", "wlo1", gateway, sizeof(gateway)));
}

static void test_estimator(void)
{
    estimate_input in;
    estimate_result r;

    /* version 1 bug: 6 of 10 is 60 %, must NOT be High */
    CHECK(strcmp(estimate_stability_confidence(6, 10), "Medium") == 0);
    CHECK(strcmp(estimate_stability_confidence(7, 10), "High") == 0);
    CHECK(strcmp(estimate_stability_confidence(2, 3), "High") == 0);
    CHECK(strcmp(estimate_stability_confidence(0, 5), "Low") == 0);

    memset(&in, 0, sizeof(in));
    in.stable_bt = 12;
    in.observed_bt = 14;
    in.devices_per_person = 1.0;
    r = estimate_people(&in);
    CHECK(r.people == 12);
    CHECK(strcmp(r.confidence, "High") == 0);

    in.devices_per_person = 1.2;
    r = estimate_people(&in);
    CHECK(r.people == 10);

    /* Wi-Fi hosts very different from the Bluetooth estimate: High is lowered */
    in.devices_per_person = 1.0;
    in.lan_available = 1;
    in.lan_hosts = 50;
    r = estimate_people(&in);
    CHECK(r.lan_disagrees == 1);
    CHECK(strcmp(r.confidence, "Medium") == 0);

    in.lan_hosts = 10;
    r = estimate_people(&in);
    CHECK(r.lan_disagrees == 0);
    CHECK(strcmp(r.confidence, "High") == 0);

    in.lan_hosts = 0; /* nobody on this network: not a disagreement */
    r = estimate_people(&in);
    CHECK(r.lan_disagrees == 0);
}

int main(void)
{
    test_mac_from_path();
    test_table_rounds_and_rssi();
    test_table_paired_and_overflow();
    test_lan_arp();
    test_lan_route();
    test_estimator();

    printf("%d checks, %d failed\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
