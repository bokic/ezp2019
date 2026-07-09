#!/bin/sh

echo "Waiting 1 second for USB devices"
#WORKAROUND #Without sleep device is called to early and device enters unrecoverable state
sleep 1

ezp2019
echo "========================================="
ezp2019 is_connected
echo "========================================="

exec "$@"
