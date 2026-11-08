# SPDX-License-Identifier: GPL-3.0-or-later
# Guards the cheshub-batteryd unit against settings that break it under
# SELinux (Fedora): with no_new_privs set, systemd cannot move the service
# into its normal domain and opening hidraw/uhid is denied. Any of these
# options sets no_new_privs once CAP_SYS_ADMIN is not kept (systemd.exec).
# Usage: cmake -DUNIT=<file> -P check_unit.cmake
file(READ "${UNIT}" unit)
set(implies_nnp
    NoNewPrivileges SystemCallFilter SystemCallArchitectures RestrictAddressFamilies
    RestrictNamespaces PrivateDevices ProtectKernelTunables ProtectKernelModules
    ProtectKernelLogs ProtectClock MemoryDenyWriteExecute RestrictRealtime
    RestrictSUIDSGID LockPersonality DynamicUser)
foreach(option ${implies_nnp})
    if(unit MATCHES "\n${option}=")
        message(FATAL_ERROR "${option}= sets no_new_privs; SELinux would then deny device access")
    endif()
endforeach()
# A binary under a library directory is labelled as a library by SELinux.
string(REGEX MATCH "\nExecStart=([^\n]*)" _ "${unit}")
if(CMAKE_MATCH_1 MATCHES "/lib64/")
    message(FATAL_ERROR "ExecStart under a lib64 directory: ${CMAKE_MATCH_1}")
endif()
message(STATUS "unit ok: ExecStart=${CMAKE_MATCH_1}")
