# CMake generated Testfile for 
# Source directory: /mnt/c/termchart/bench
# Build directory: /mnt/c/termchart/build_wsl/bench
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[bench_sugiyama_quick]=] "/mnt/c/termchart/build_wsl/bench/bench_sugiyama" "--quick")
set_tests_properties([=[bench_sugiyama_quick]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/bench/CMakeLists.txt;19;add_test;/mnt/c/termchart/bench/CMakeLists.txt;0;")
add_test([=[bench_force_quick]=] "/mnt/c/termchart/build_wsl/bench/bench_force" "--quick")
set_tests_properties([=[bench_force_quick]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/bench/CMakeLists.txt;19;add_test;/mnt/c/termchart/bench/CMakeLists.txt;0;")
add_test([=[bench_mermaid_parse_quick]=] "/mnt/c/termchart/build_wsl/bench/bench_mermaid_parse" "--quick")
set_tests_properties([=[bench_mermaid_parse_quick]=] PROPERTIES  _BACKTRACE_TRIPLES "/mnt/c/termchart/bench/CMakeLists.txt;19;add_test;/mnt/c/termchart/bench/CMakeLists.txt;0;")
