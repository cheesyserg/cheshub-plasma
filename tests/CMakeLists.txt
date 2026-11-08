# SPDX-License-Identifier: GPL-3.0-or-later
foreach(name tkgm012 b75 reportdescriptor)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE cheshub_core Qt6::Test)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
