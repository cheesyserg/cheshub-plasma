#!/bin/sh
# $1 is 0 on removal, 1 on upgrade.
if [ "$1" = "0" ] && [ -d /run/systemd/system ]; then
    systemctl disable --now cheshub-batteryd.service || true
fi
exit 0
