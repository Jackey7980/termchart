# CMake generated Testfile for 
# Source directory: /mnt/c/termchart/tests
# Build directory: /mnt/c/termchart/build_wsl/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[termchart_tests]=] "/mnt/c/termchart/build_wsl/tests/termchart_tests")
set_tests_properties([=[termchart_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/tests/CMakeLists.txt;223;add_test;/mnt/c/termchart/tests/CMakeLists.txt;0;")
add_test([=[tests_golden]=] "/mnt/c/termchart/build_wsl/tests/termchart_tests" "--test-case=*golden*")
set_tests_properties([=[tests_golden]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/tests/CMakeLists.txt;226;add_test;/mnt/c/termchart/tests/CMakeLists.txt;0;")
add_test([=[tests_render]=] "/mnt/c/termchart/build_wsl/tests/termchart_tests" "--test-case=*render*")
set_tests_properties([=[tests_render]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/tests/CMakeLists.txt;227;add_test;/mnt/c/termchart/tests/CMakeLists.txt;0;")
add_test([=[tests_core]=] "/mnt/c/termchart/build_wsl/tests/termchart_tests" "--test-case=*core*")
set_tests_properties([=[tests_core]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/tests/CMakeLists.txt;228;add_test;/mnt/c/termchart/tests/CMakeLists.txt;0;")
add_test([=[tests_util]=] "/mnt/c/termchart/build_wsl/tests/termchart_tests" "--test-case=*util*")
set_tests_properties([=[tests_util]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/tests/CMakeLists.txt;229;add_test;/mnt/c/termchart/tests/CMakeLists.txt;0;")
