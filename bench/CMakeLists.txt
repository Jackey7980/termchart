# bench/CMakeLists.txt —— 性能基线
#
# 每个 bench 都零外部依赖（自带 bench.hpp 计时骨架），且都支持 `--quick`：
# ctest 里只跑 quick 档做冒烟，真正的基线测量由人手动跑（`build/<preset>/bench/<name>`）。
if(NOT TERMCHART_BUILD_BENCH)
    return()
endif()

foreach(bench bench_sugiyama bench_force bench_mermaid_parse)
    add_executable(${bench} ${bench}.cpp)
    target_link_libraries(${bench} PRIVATE termchart)
    termchart_set_warnings(${bench})
    set_target_properties(${bench} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench")
endforeach()

if(TERMCHART_BUILD_TESTS)
    foreach(bench bench_sugiyama bench_force bench_mermaid_parse)
        add_test(NAME ${bench}_quick COMMAND ${bench} --quick)
    endforeach()
endif()
