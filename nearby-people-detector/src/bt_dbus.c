#include "bt_dbus.h"

#include <gio/gio.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define BLUEZ_NAME "org.bluez"
#define ADAPTER_IFACE "org.bluez.Adapter1"
#define DEVICE_IFACE "org.bluez.Device1"
#define PROPS_IFACE "org.freedesktop.DBus.Properties"
#define OBJMAN_IFACE "org.freedesktop.DBus.ObjectManager"
#define CALL_TIMEOUT_MS 5000

struct bt_scanner
{
    GDBusConnection *connection;
    bt_table *table;
    char adapter_path[64];
    guint sub_changed;
    guint sub_added;
    GMainLoop *loop;
    int round;
    int active; /* events are recorded only while a round is running */
    int trace;
};

static void set_error(char *err, size_t size, const char *format, ...)
{
    va_list args;

    if (err == NULL || size == 0)
        return;

    va_start(args, format);
    vsnprintf(err, size, format, args);
    va_end(args);
}

static GVariant *bluez_call(bt_scanner *scanner, const char *path,
                            const char *iface, const char *method,
                            GVariant *parameters, const GVariantType *reply_type,
                            GError **error)
{
    return g_dbus_connection_call_sync(
        scanner->connection, BLUEZ_NAME, path, iface, method, parameters,
        reply_type, G_DBUS_CALL_FLAGS_NONE, CALL_TIMEOUT_MS, NULL, error);
}

static int path_on_adapter(const bt_scanner *scanner, const char *path)
{
    size_t length = strlen(scanner->adapter_path);

    return strncmp(path, scanner->adapter_path, length) == 0 &&
           path[length] == '/';
}

static int has_key(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);

    if (value == NULL)
        return 0;

    g_variant_unref(value);
    return 1;
}

/* Reads Paired and AddressType from a Device1 property dictionary. */
static void read_device_info(bt_scanner *scanner, const char *mac,
                             GVariant *properties)
{
    gboolean paired = FALSE;
    const gchar *address_type = NULL;

    g_variant_lookup(properties, "Paired", "b", &paired);
    g_variant_lookup(properties, "AddressType", "&s", &address_type);

    bt_table_set_info(scanner->table, mac, paired,
                      address_type != NULL &&
                          strcmp(address_type, "random") == 0);
}

/*
 * PropertiesChanged(s interface, a{sv} changed, as invalidated) on a Device1.
 * During discovery BlueZ emits it when an advertisement changes the RSSI,
 * manufacturer data, service data or TX power of a device.
 */
static void on_properties_changed(GDBusConnection *connection,
                                  const gchar *sender, const gchar *path,
                                  const gchar *interface, const gchar *signal,
                                  GVariant *parameters, gpointer user_data)
{
    bt_scanner *scanner = user_data;
    const gchar *changed_interface = NULL;
    GVariant *changed = NULL;
    GVariant *invalidated = NULL;
    char mac[18];
    gint16 rssi = 0;
    gboolean paired = FALSE;
    int has_rssi;
    int activity;

    (void)connection;
    (void)sender;
    (void)interface;
    (void)signal;

    if (!path_on_adapter(scanner, path) || !bt_mac_from_path(path, mac, sizeof(mac)))
        return;

    g_variant_get(parameters, "(&s@a{sv}@as)", &changed_interface, &changed,
                  &invalidated);

    has_rssi = g_variant_lookup(changed, "RSSI", "n", &rssi);
    activity = has_rssi || has_key(changed, "ManufacturerData") ||
               has_key(changed, "ServiceData") || has_key(changed, "TxPower");

    if (g_variant_lookup(changed, "Paired", "b", &paired))
        bt_table_set_info(scanner->table, mac, paired, 0);

    if (scanner->trace)
        fprintf(stderr, "[trace] changed %s%s%s\n", mac,
                has_rssi ? " RSSI" : "", activity ? " (heard)" : "");

    if (scanner->active && activity)
        bt_table_heard(scanner->table, mac, has_rssi, (int)rssi, scanner->round);

    g_variant_unref(changed);
    g_variant_unref(invalidated);
}

/*
 * InterfacesAdded(o path, a{sa{sv}} interfaces): BlueZ creates a Device1 object
 * the first time it hears a new device.
 */
static void on_interfaces_added(GDBusConnection *connection,
                                const gchar *sender, const gchar *path_unused,
                                const gchar *interface, const gchar *signal,
                                GVariant *parameters, gpointer user_data)
{
    bt_scanner *scanner = user_data;
    const gchar *path = NULL;
    GVariant *interfaces = NULL;
    GVariant *properties;
    char mac[18];
    gint16 rssi = 0;
    int has_rssi;

    (void)connection;
    (void)sender;
    (void)path_unused;
    (void)interface;
    (void)signal;

    g_variant_get(parameters, "(&o@a{sa{sv}})", &path, &interfaces);

    properties =
        g_variant_lookup_value(interfaces, DEVICE_IFACE, G_VARIANT_TYPE_VARDICT);

    if (properties != NULL && path_on_adapter(scanner, path) &&
        bt_mac_from_path(path, mac, sizeof(mac)))
    {
        has_rssi = g_variant_lookup(properties, "RSSI", "n", &rssi);

        read_device_info(scanner, mac, properties);

        if (scanner->trace)
            fprintf(stderr, "[trace] added   %s%s\n", mac,
                    has_rssi ? " RSSI" : "");

        if (scanner->active)
            bt_table_heard(scanner->table, mac, has_rssi, (int)rssi,
                           scanner->round);
    }

    if (properties != NULL)
        g_variant_unref(properties);

    g_variant_unref(interfaces);
}

/*
 * Finds the first adapter and reads the pairing state of known devices.
 * Returns 1 if an adapter was found. *was_powered tells if it was on.
 */
static int load_managed_objects(bt_scanner *scanner, int *was_powered,
                                char *err, size_t err_size)
{
    GError *error = NULL;
    GVariant *reply;
    GVariantIter *objects = NULL;
    const gchar *path = NULL;
    GVariant *interfaces = NULL;
    int found = 0;

    *was_powered = 1;

    reply = bluez_call(scanner, "/", OBJMAN_IFACE, "GetManagedObjects", NULL,
                       G_VARIANT_TYPE("(a{oa{sa{sv}}})"), &error);

    if (reply == NULL)
    {
        set_error(err, err_size, "GetManagedObjects failed: %s (is bluetoothd running?)",
                  error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        return 0;
    }

    g_variant_get(reply, "(a{oa{sa{sv}}})", &objects);

    while (g_variant_iter_next(objects, "{&o@a{sa{sv}}}", &path, &interfaces))
    {
        GVariant *adapter =
            g_variant_lookup_value(interfaces, ADAPTER_IFACE, G_VARIANT_TYPE_VARDICT);

        if (adapter != NULL)
        {
            if (!found)
            {
                gboolean powered = TRUE;

                snprintf(scanner->adapter_path, sizeof(scanner->adapter_path),
                         "%s", path);
                g_variant_lookup(adapter, "Powered", "b", &powered);
                *was_powered = powered ? 1 : 0;
                found = 1;
            }
            g_variant_unref(adapter);
        }

        g_variant_unref(interfaces);
    }

    g_variant_iter_free(objects);

    /* second pass, now that the adapter path is known: paired devices */
    g_variant_get(reply, "(a{oa{sa{sv}}})", &objects);

    while (found && g_variant_iter_next(objects, "{&o@a{sa{sv}}}", &path, &interfaces))
    {
        GVariant *device =
            g_variant_lookup_value(interfaces, DEVICE_IFACE, G_VARIANT_TYPE_VARDICT);
        char mac[18];

        if (device != NULL)
        {
            if (path_on_adapter(scanner, path) &&
                bt_mac_from_path(path, mac, sizeof(mac)))
                read_device_info(scanner, mac, device);

            g_variant_unref(device);
        }

        g_variant_unref(interfaces);
    }

    g_variant_iter_free(objects);
    g_variant_unref(reply);

    if (!found)
        set_error(err, err_size, "no Bluetooth adapter found (check: rfkill list)");

    return found;
}

static int power_on(bt_scanner *scanner)
{
    GError *error = NULL;
    GVariant *reply = bluez_call(
        scanner, scanner->adapter_path, PROPS_IFACE, "Set",
        g_variant_new("(ssv)", ADAPTER_IFACE, "Powered", g_variant_new_boolean(TRUE)),
        NULL, &error);

    if (reply == NULL)
    {
        g_clear_error(&error);
        return 0;
    }

    g_variant_unref(reply);
    return 1;
}

bt_scanner *bt_scanner_new(bt_table *table, int trace, char *err,
                           size_t err_size)
{
    bt_scanner *scanner = g_new0(bt_scanner, 1);
    GError *error = NULL;
    int was_powered = 1;

    scanner->table = table;
    scanner->trace = trace;

    scanner->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

    if (scanner->connection == NULL)
    {
        set_error(err, err_size, "cannot connect to the system D-Bus: %s",
                  error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        g_free(scanner);
        return NULL;
    }

    if (!load_managed_objects(scanner, &was_powered, err, err_size))
    {
        bt_scanner_free(scanner);
        return NULL;
    }

    if (!was_powered)
    {
        if (!power_on(scanner))
        {
            set_error(err, err_size,
                      "adapter %s is off and could not be powered on "
                      "(rfkill list ; bluetoothctl power on)",
                      scanner->adapter_path);
            bt_scanner_free(scanner);
            return NULL;
        }

        g_usleep(1000000); /* give the adapter time to power up */
    }

    scanner->sub_changed = g_dbus_connection_signal_subscribe(
        scanner->connection, BLUEZ_NAME, PROPS_IFACE, "PropertiesChanged", NULL,
        DEVICE_IFACE, G_DBUS_SIGNAL_FLAGS_NONE, on_properties_changed, scanner,
        NULL);

    scanner->sub_added = g_dbus_connection_signal_subscribe(
        scanner->connection, BLUEZ_NAME, OBJMAN_IFACE, "InterfacesAdded", NULL,
        NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_interfaces_added, scanner, NULL);

    return scanner;
}

static gboolean on_round_timeout(gpointer data)
{
    bt_scanner *scanner = data;

    g_main_loop_quit(scanner->loop);

    return G_SOURCE_REMOVE;
}

int bt_scanner_run_round(bt_scanner *scanner, int round, unsigned seconds,
                         char *err, size_t err_size)
{
    GError *error = NULL;
    GVariantBuilder filter;
    GVariant *reply;

    /* Transport auto = BR/EDR and LE. DuplicateData keeps RSSI updates coming. */
    g_variant_builder_init(&filter, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&filter, "{sv}", "Transport", g_variant_new_string("auto"));
    g_variant_builder_add(&filter, "{sv}", "DuplicateData", g_variant_new_boolean(TRUE));

    reply = bluez_call(scanner, scanner->adapter_path, ADAPTER_IFACE,
                       "SetDiscoveryFilter", g_variant_new("(a{sv})", &filter),
                       NULL, &error);

    if (reply != NULL)
        g_variant_unref(reply);
    else
    {
        if (scanner->trace)
            fprintf(stderr, "[trace] SetDiscoveryFilter: %s\n", error->message);
        g_clear_error(&error);
    }

    reply = bluez_call(scanner, scanner->adapter_path, ADAPTER_IFACE,
                       "StartDiscovery", NULL, NULL, &error);

    if (reply != NULL)
        g_variant_unref(reply);
    else if (error != NULL && strstr(error->message, "InProgress") != NULL)
        g_clear_error(&error); /* already discovering: not an error */
    else
    {
        set_error(err, err_size, "StartDiscovery failed: %s",
                  error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
        return -1;
    }

    scanner->round = round;
    scanner->active = 1;
    scanner->loop = g_main_loop_new(NULL, FALSE);

    g_timeout_add_seconds(seconds, on_round_timeout, scanner);
    g_main_loop_run(scanner->loop);

    g_main_loop_unref(scanner->loop);
    scanner->loop = NULL;
    scanner->active = 0;

    reply = bluez_call(scanner, scanner->adapter_path, ADAPTER_IFACE,
                       "StopDiscovery", NULL, NULL, &error);

    if (reply != NULL)
        g_variant_unref(reply);
    else
        g_clear_error(&error);

    return 0;
}

const char *bt_scanner_adapter(const bt_scanner *scanner)
{
    return scanner->adapter_path;
}

void bt_scanner_free(bt_scanner *scanner)
{
    if (scanner == NULL)
        return;

    if (scanner->connection != NULL)
    {
        if (scanner->sub_changed != 0)
            g_dbus_connection_signal_unsubscribe(scanner->connection,
                                                 scanner->sub_changed);
        if (scanner->sub_added != 0)
            g_dbus_connection_signal_unsubscribe(scanner->connection,
                                                 scanner->sub_added);
        g_object_unref(scanner->connection);
    }

    g_free(scanner);
}
