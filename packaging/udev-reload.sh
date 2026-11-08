#!/bin/sh
# Make the freshly installed udev rule apply to devices that are already
# plugged in, so the module works without re-plugging.
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || true
    udevadm trigger --subsystem-match=hidraw --action=change || true
fi
exit 0
