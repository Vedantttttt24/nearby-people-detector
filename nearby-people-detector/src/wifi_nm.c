#include "wifi_nm.h"

#include <NetworkManager.h>
#include <stdio.h>
#include <string.h>

#define SCAN_WAIT_SECONDS 10

static void set_error(char *err, size_t size, const char *message)
{
    if (err != NULL && size > 0)
        snprintf(err, size, "%s", message);
}

static NMDeviceWifi *first_wifi_device(NMClient *client)
{
    const GPtrArray *devices = nm_client_get_devices(client);

    for (guint i = 0; devices != NULL && i < devices->len; i++)
    {
        NMDevice *device = g_ptr_array_index(devices, i);

        if (NM_IS_DEVICE_WIFI(device))
            return NM_DEVICE_WIFI(device);
    }

    return NULL;
}

typedef struct
{
    int done;
    int ok;
} scan_state;

/* One scan per program run, so the state can live in a static variable. A
 * late callback after a timeout then never writes into freed memory. */
static scan_state scan_result;

static void pump_main_context(void)
{
    while (g_main_context_iteration(NULL, FALSE))
        ;
    g_usleep(50000);
}

static void on_scan_requested(GObject *source, GAsyncResult *result, gpointer data)
{
    GError *error = NULL;
    scan_state *state = data;

    state->ok = nm_device_wifi_request_scan_finish(NM_DEVICE_WIFI(source), result, &error);
    state->done = 1;
    g_clear_error(&error); /* e.g. "scanning not allowed immediately following previous scan" */
}

/*
 * Asks NetworkManager for a new scan and waits until the last-scan time
 * changes. Returns 1 if fresh results arrived.
 */
static int request_fresh_scan(NMDeviceWifi *wifi)
{
    gint64 before = nm_device_wifi_get_last_scan(wifi);
    gint64 end = g_get_monotonic_time() + (gint64)SCAN_WAIT_SECONDS * G_USEC_PER_SEC;
    GCancellable *cancellable = g_cancellable_new();

    memset(&scan_result, 0, sizeof(scan_result));

    nm_device_wifi_request_scan_async(wifi, cancellable, on_scan_requested, &scan_result);

    while (!scan_result.done && g_get_monotonic_time() < end)
        pump_main_context();

    if (!scan_result.done)
    {
        g_cancellable_cancel(cancellable); /* the callback runs with an error */
        pump_main_context();
    }

    g_object_unref(cancellable);

    if (!scan_result.ok)
        return 0;

    while (nm_device_wifi_get_last_scan(wifi) == before &&
           g_get_monotonic_time() < end)
        pump_main_context();

    return nm_device_wifi_get_last_scan(wifi) != before;
}

static int count_unique_bssids(NMDeviceWifi *wifi)
{
    const GPtrArray *aps = nm_device_wifi_get_access_points(wifi);
    GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
    int count;

    for (guint i = 0; aps != NULL && i < aps->len; i++)
    {
        const char *bssid = nm_access_point_get_bssid(g_ptr_array_index(aps, i));

        if (bssid != NULL)
            g_hash_table_add(seen, (gpointer)bssid); /* owned by the AP object */
    }

    count = (int)g_hash_table_size(seen);
    g_hash_table_destroy(seen);

    return count;
}

int wifi_nm_scan(int rescan, wifi_nm_info *info, char *err, size_t err_size)
{
    GError *error = NULL;
    NMClient *client;
    NMDeviceWifi *wifi;

    memset(info, 0, sizeof(*info));

    client = nm_client_new(NULL, &error);

    if (client == NULL)
    {
        set_error(err, err_size,
                  error != NULL ? error->message : "cannot reach NetworkManager");
        g_clear_error(&error);
        return -1;
    }

    wifi = first_wifi_device(client);

    if (wifi == NULL)
    {
        g_object_unref(client);
        return 0; /* reachable, but no Wi-Fi device */
    }

    info->available = 1;
    snprintf(info->iface, sizeof(info->iface), "%s",
             nm_device_get_iface(NM_DEVICE(wifi)));

    if (rescan)
    {
        info->scan_requested = 1;
        info->scan_fresh = request_fresh_scan(wifi);
    }

    info->access_points = count_unique_bssids(wifi);

    g_object_unref(client);

    return 0;
}
