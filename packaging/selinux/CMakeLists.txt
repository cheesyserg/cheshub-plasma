# SPDX-License-Identifier: GPL-3.0-or-later
# cheshub-batteryd's SELinux policy module, built and installed the way
# Fedora packages do it: compiled with selinux-policy-devel's Makefile,
# shipped as a bzip2'd .pp in /usr/share/selinux/packages/<type>/ with its
# interface in /usr/share/selinux/devel/include/distributed/, and loaded at
# priority 200 by the package scripts (as %selinux_modules_install does).
set(SELINUX_DEVEL_MAKEFILE "/usr/share/selinux/devel/Makefile" CACHE FILEPATH "selinux-policy-devel Makefile")
set(SELINUX_TYPE "targeted" CACHE STRING "SELinux policy type the module is installed for")
if(NOT EXISTS "${SELINUX_DEVEL_MAKEFILE}")
    message(FATAL_ERROR "BUILD_SELINUX needs selinux-policy-devel (${SELINUX_DEVEL_MAKEFILE} not found)")
endif()
find_program(BZIP2 bzip2 REQUIRED)

# Version of the policy built against, for Requires: selinux-policy >= ...
find_program(RPM_EXECUTABLE rpm)
if(RPM_EXECUTABLE)
    execute_process(COMMAND ${RPM_EXECUTABLE} -q --qf "%{VERSION}" selinux-policy
                    OUTPUT_VARIABLE version RESULT_VARIABLE rc OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(rc EQUAL 0)
        set(SELINUX_POLICY_VERSION "${version}" CACHE INTERNAL "")
    endif()
endif()
if(NOT SELINUX_POLICY_VERSION)
    set(SELINUX_POLICY_VERSION "0" CACHE INTERNAL "")
endif()
message(STATUS "SELinux module built against selinux-policy ${SELINUX_POLICY_VERSION}")

set(module cheshub_batteryd)
set(work ${CMAKE_CURRENT_BINARY_DIR}/module)
set(sources ${module}.te ${module}.fc ${module}.if)
set(copies)
foreach(f ${sources})
    list(APPEND copies ${work}/${f})
endforeach()

add_custom_command(
    OUTPUT ${work}/${module}.pp.bz2
    COMMAND ${CMAKE_COMMAND} -E make_directory ${work}
    COMMAND ${CMAKE_COMMAND} -E copy ${sources} ${work}
    COMMAND make -C ${work} -f ${SELINUX_DEVEL_MAKEFILE} ${module}.pp
    COMMAND ${CMAKE_COMMAND} -E rm -f ${work}/${module}.pp.bz2
    COMMAND ${BZIP2} -9 -k ${work}/${module}.pp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${sources}
    COMMENT "Building SELinux policy module ${module}"
    VERBATIM)
add_custom_target(selinux_module ALL DEPENDS ${work}/${module}.pp.bz2)

install(FILES ${work}/${module}.pp.bz2 DESTINATION ${CMAKE_INSTALL_DATADIR}/selinux/packages/${SELINUX_TYPE})
install(FILES ${module}.if DESTINATION ${CMAKE_INSTALL_DATADIR}/selinux/devel/include/distributed)
