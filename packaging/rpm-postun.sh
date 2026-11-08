#!/bin/sh
# $1 is 0 on removal, 1 on upgrade. Remove the SELinux policy module after the
# package's files are gone, as %selinux_modules_uninstall does.
if [ "$1" = "0" ] && command -v semodule >/dev/null 2>&1; then
    semodule -n -X 200 -s targeted -r cheshub_batteryd >/dev/null 2>&1 || true
    if command -v selinuxenabled >/dev/null 2>&1 && selinuxenabled; then
        load_policy || true
    fi
fi
exit 0
