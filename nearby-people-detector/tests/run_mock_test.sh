#!/bin/sh
# End-to-end test of bt_dbus.c against a mock BlueZ on a private D-Bus.
# Needs: dbus-daemon, python3-gi. Usage: tests/run_mock_test.sh   (after make)
set -e
cd "$(dirname "$0")/.."

ADDR=$(dbus-daemon --session --fork --print-address)
PID=$(pgrep -n -x dbus-daemon || true)
trap 'kill $MOCK_PID 2>/dev/null; [ -n "$PID" ] && kill $PID 2>/dev/null' EXIT

python3 tests/mock_bluez.py "$ADDR" 2>/tmp/mock.log &
MOCK_PID=$!
sleep 1

DBUS_SYSTEM_BUS_ADDRESS="$ADDR" ./build/people_detector \
    --rounds 3 --seconds 2 --min-rounds 2 --rssi-min -80 --no-lan --no-rescan --verbose
