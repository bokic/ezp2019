#!/bin/sh

set -e

# Write diagnostic message to stderr instead of stdout so we don't corrupt redirected output
echo "Waiting 1 second for USB devices..." >&2

# WORKAROUND: Without sleep, the device is called too early and can enter an unrecoverable state
sleep 1

# If the first argument is an ezp2019 subcommand, prepend the binary name
case "$1" in
    is_connected|connected_ic|read_ic|write_ic|verify_ic|erase_ic)
        set -- ezp2019 "$@"
        ;;
esac

exec "$@"
