# SPDX-License-Identifier: GPL-3.0-or-later
foreach(name tkgm012 b75 reportdescriptor)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE cheshub_core Qt6::Test)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

if(BUILD_BATTERYD)
    add_executable(test_batteryd test_batteryd.cpp)
    target_link_libraries(test_batteryd PRIVATE cheshub_batteryd_lib Qt6::Test)
    add_test(NAME batteryd COMMAND test_batteryd)
endif()
