#!/bin/sh
# Apply the udev rule to devices that are already plugged in, and start
# cheshub-batteryd (device batteries in Power & Battery). Safe on upgrades
# and in containers without a running systemd.
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || true
    udevadm trigger --subsystem-match=hidraw --action=change || true
fi
# SELinux: make sure the service binary carries the label of a program
# (bin_t), so systemd starts it in the normal service domain.
if command -v selinuxenabled >/dev/null 2>&1 && selinuxenabled && command -v restorecon >/dev/null 2>&1; then
    restorecon -F /usr/libexec/cheshub-batteryd /usr/lib/systemd/system/cheshub-batteryd.service \
        /usr/lib/udev/rules.d/70-cheshub-plasma.rules 2>/dev/null || true
fi
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload || true
    systemctl enable cheshub-batteryd.service >/dev/null 2>&1 || true
    systemctl restart cheshub-batteryd.service || true
fi
exit 0
