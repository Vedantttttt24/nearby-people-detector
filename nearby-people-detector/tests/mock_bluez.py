#!/usr/bin/env python3
"""
Mock BlueZ service for testing bt_dbus.c without Bluetooth hardware.

Run it on a private bus (see tests/run_mock_test.sh). It exports:
  /                        org.freedesktop.DBus.ObjectManager
  /org/bluez/hci0          org.bluez.Adapter1 (StartDiscovery, StopDiscovery, SetDiscoveryFilter)

Scenario (one StartDiscovery call = one round):
  A  heard in rounds 1,2,3, close (-55 dBm)          -> stable
  B  heard in round 1 only                           -> unstable
  C  cached in BlueZ from the start, never heard     -> must NOT be counted
  D  heard in rounds 1,2 but weak (-92 dBm)          -> too far
  P  paired device, heard in every round             -> ignored
  E  heard in rounds 1,2 with ManufacturerData only  -> stable (no RSSI to filter)
"""
import sys

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402

ADAPTER = "/org/bluez/hci0"


def dev_path(mac):
    return ADAPTER + "/dev_" + mac.replace(":", "_")


MAC = {
    "A": "AA:00:00:00:00:01",
    "B": "BB:00:00:00:00:02",
    "C": "CC:00:00:00:00:03",
    "D": "DD:00:00:00:00:04",
    "P": "11:00:00:00:00:05",
    "E": "EE:00:00:00:00:06",
}

OBJMAN_XML = """<node><interface name='org.freedesktop.DBus.ObjectManager'>
<method name='GetManagedObjects'><arg type='a{oa{sa{sv}}}' direction='out'/></method>
<signal name='InterfacesAdded'><arg type='o'/><arg type='a{sa{sv}}'/></signal>
</interface></node>"""

ADAPTER_XML = """<node><interface name='org.bluez.Adapter1'>
<method name='StartDiscovery'/><method name='StopDiscovery'/>
<method name='SetDiscoveryFilter'><arg type='a{sv}' direction='in'/></method>
<property name='Powered' type='b' access='readwrite'/>
</interface></node>"""

state = {"round": 0, "conn": None, "known": {}}


def v(sig, value):
    return GLib.Variant(sig, value)


def device_props(mac, rssi=None, paired=False):
    props = {
        "Address": v("s", mac),
        "AddressType": v("s", "public"),
        "Paired": v("b", paired),
    }
    if rssi is not None:
        props["RSSI"] = v("n", rssi)
    return props


def managed_objects():
    objects = {ADAPTER: {"org.bluez.Adapter1": {"Powered": v("b", True)}}}
    # C is cached from an earlier scan: it exists but will never produce events.
    objects[dev_path(MAC["C"])] = {"org.bluez.Device1": device_props(MAC["C"], rssi=-50)}
    # P is paired with the computer.
    objects[dev_path(MAC["P"])] = {"org.bluez.Device1": device_props(MAC["P"], paired=True)}
    return objects


def emit_added(key, rssi):
    mac = MAC[key]
    state["known"][key] = True
    state["conn"].emit_signal(
        None, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
        v("(oa{sa{sv}})", (dev_path(mac), {"org.bluez.Device1": device_props(mac, rssi)})))
    return False


def emit_changed(key, changed):
    state["conn"].emit_signal(
        None, dev_path(MAC[key]), "org.freedesktop.DBus.Properties", "PropertiesChanged",
        v("(sa{sv}as)", ("org.bluez.Device1", changed, [])))
    return False


def rssi_event(key, rssi):
    """First event of a device is InterfacesAdded, later ones PropertiesChanged."""
    if key == "P":
        return emit_changed(key, {"RSSI": v("n", rssi)})
    if not state["known"].get(key):
        return emit_added(key, rssi)
    return emit_changed(key, {"RSSI": v("n", rssi)})


def schedule_round(n):
    events = []
    events.append(("A", -55))
    events.append(("P", -50))
    if n == 1:
        events.append(("B", -60))
    if n in (1, 2):
        events.append(("D", -92))
    for i, (key, rssi) in enumerate(events):
        GLib.timeout_add(200 + 150 * i, rssi_event, key, rssi)
    if n == 1:
        GLib.timeout_add(900, emit_added, "E", None)  # new device, no RSSI yet
    elif n == 2:
        GLib.timeout_add(900, emit_changed, "E",
                         {"ManufacturerData": v("a{qv}", {0x004C: v("ay", b"\x01\x02")})})


def on_adapter_call(conn, sender, path, iface, method, params, invocation):
    if method == "StartDiscovery":
        state["round"] += 1
        sys.stderr.write("[mock] StartDiscovery -> round %d\n" % state["round"])
        schedule_round(state["round"])
    invocation.return_value(None)


def on_objman_call(conn, sender, path, iface, method, params, invocation):
    invocation.return_value(v("(a{oa{sa{sv}}})", (managed_objects(),)))


def get_prop(conn, sender, path, iface, name):
    return v("b", True)


def main():
    address = sys.argv[1]
    conn = Gio.DBusConnection.new_for_address_sync(
        address,
        Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,
        None, None)
    state["conn"] = conn

    conn.register_object("/", Gio.DBusNodeInfo.new_for_xml(OBJMAN_XML).interfaces[0],
                         on_objman_call, None, None)
    conn.register_object(ADAPTER, Gio.DBusNodeInfo.new_for_xml(ADAPTER_XML).interfaces[0],
                         on_adapter_call, get_prop, None)

    Gio.bus_own_name_on_connection(conn, "org.bluez", Gio.BusNameOwnerFlags.NONE, None, None)
    sys.stderr.write("[mock] ready\n")
    GLib.MainLoop().run()


main()
