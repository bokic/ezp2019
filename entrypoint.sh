#!/bin/sh

echo "Waiting 1 second for USB devices"
sleep 1

ezp2019
echo "========================================="
ezp2019 is_connected
echo "========================================="

exec "$@"
