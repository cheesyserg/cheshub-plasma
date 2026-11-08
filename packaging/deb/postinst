#!/bin/sh
# Apply the udev rule to devices that are already plugged in, and start
# cheshub-batteryd (device batteries in Power & Battery). Safe on upgrades
# and in containers without a running systemd.
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules || true
    udevadm trigger --subsystem-match=hidraw --action=change || true
fi
# SELinux policy module for cheshub-batteryd (Fedora builds ship it), installed
# the way %selinux_modules_install does: priority 200, then load the policy
# if SELinux is enabled.
MODULE=/usr/share/selinux/packages/targeted/cheshub_batteryd.pp.bz2
if [ -e "$MODULE" ] && command -v semodule >/dev/null 2>&1; then
    semodule -n -s targeted -X 200 -i "$MODULE" || true
    if command -v selinuxenabled >/dev/null 2>&1 && selinuxenabled; then
        load_policy || true
    fi
fi
# Relabel the installed files (as %selinux_relabel_post does), so the service
# binary gets cheshub_batteryd_exec_t and systemd starts it in its domain.
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
