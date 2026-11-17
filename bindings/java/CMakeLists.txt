# bindings/java/CMakeLists.txt —— Java 高层出图桥（JNI glue 包 termchart_capi）
#
# 由顶层在 TERMCHART_BUILD_JAVA=ON（且 CAPI=ON）时 add_subdirectory。产出共享库
# termchart_jni（Java System.loadLibrary("termchart_jni") 加载）+ 无头 JVM 冒烟。
# 依赖 JDK；缺 JDK 时配置期 fail-loud（显式开启就该报错，不静默跳过）。
find_package(JNI REQUIRED)
find_package(Java COMPONENTS Development REQUIRED)

message(STATUS "termchart Java 桥: JNI=${JNI_INCLUDE_DIRS} javac=${Java_JAVAC_EXECUTABLE}")

set(TC_JAVA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(TC_JNI_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/jni_generated")
set(TC_JAVA_CLASS_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")

# 1) javac -h：编译 Java 类并生成 JNI 头（Termchart.h），避免手写 mangled 签名。
file(MAKE_DIRECTORY "${TC_JNI_GEN_DIR}" "${TC_JAVA_CLASS_DIR}")
add_custom_command(
    OUTPUT "${TC_JNI_GEN_DIR}/Termchart.h"
           "${TC_JAVA_CLASS_DIR}/Termchart.class"
           "${TC_JAVA_CLASS_DIR}/TermchartException.class"
           "${TC_JAVA_CLASS_DIR}/TermchartSmoke.class"
    COMMAND ${Java_JAVAC_EXECUTABLE} -encoding UTF-8
            -h "${TC_JNI_GEN_DIR}" -d "${TC_JAVA_CLASS_DIR}"
            "${TC_JAVA_SRC_DIR}/Termchart.java"
            "${TC_JAVA_SRC_DIR}/TermchartException.java"
            "${TC_JAVA_SRC_DIR}/TermchartSmoke.java"
    DEPENDS "${TC_JAVA_SRC_DIR}/Termchart.java"
            "${TC_JAVA_SRC_DIR}/TermchartException.java"
            "${TC_JAVA_SRC_DIR}/TermchartSmoke.java"
    COMMENT "javac -h Termchart.java（生成 JNI 头 + class）"
    VERBATIM)

add_custom_target(termchart_java_classes ALL
    DEPENDS "${TC_JNI_GEN_DIR}/Termchart.h")

# 2) JNI 共享库。Windows 去 lib 前缀（System.loadLibrary 只查 termchart_jni.dll）。
# 直接把 capi.cpp 编进来 + 链静态 termchart：单 DLL 自包含（与 gui_jni 同模式），
# 不在运行期依赖 libtermchart_capi.dll（否则 JVM 的 java.library.path 还得带上 capi 目录）。
add_library(termchart_jni SHARED
    src/termchart_jni.cpp
    "${CMAKE_SOURCE_DIR}/libs/capi/src/capi.cpp"
    "${TC_JNI_GEN_DIR}/Termchart.h")
add_dependencies(termchart_jni termchart_java_classes)
set_target_properties(termchart_jni PROPERTIES OUTPUT_NAME termchart_jni)
if(WIN32)
    set_target_properties(termchart_jni PROPERTIES PREFIX "")
endif()
target_include_directories(termchart_jni PRIVATE
    "${TC_JNI_GEN_DIR}"
    "${CMAKE_SOURCE_DIR}/libs/capi/include"
    ${JNI_INCLUDE_DIRS})
target_link_libraries(termchart_jni PRIVATE termchart)

# MinGW 静态链运行时，DLL 自包含（libstdc++-6.dll 不再成为 JVM 加载的拦路依赖）。
if(WIN32)
    target_link_options(termchart_jni PRIVATE -static-libgcc -static-libstdc++)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_jni PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# 3) JVM 冒烟：plot/bar/kinds/错误通道全链路（无 AWT/Swing）。
# sanitizer 构建跳过（JVM 无法加载插桩 native 库，同 gui_jni 的既定处理）。
set(_java_sanitized OFF)
if(TERMCHART_ENABLE_ASAN OR TERMCHART_ENABLE_UBSAN OR TERMCHART_ENABLE_TSAN)
    set(_java_sanitized ON)
endif()
string(FIND "${CMAKE_CXX_FLAGS}" "-fsanitize=" _java_san_flag_pos)
if(NOT _java_san_flag_pos EQUAL -1)
    set(_java_sanitized ON)
endif()
if(TERMCHART_BUILD_TESTS AND NOT _java_sanitized)
    add_test(NAME java_smoke
        COMMAND ${Java_JAVA_EXECUTABLE}
                -Djava.library.path=$<TARGET_FILE_DIR:termchart_jni>
                -cp "${TC_JAVA_CLASS_DIR}" TermchartSmoke)
    set_tests_properties(java_smoke PROPERTIES
        REQUIRED_FILES "$<TARGET_FILE:termchart_jni>"
        TIMEOUT 30)
endif()

install(TARGETS termchart_jni LIBRARY DESTINATION lib RUNTIME DESTINATION .)
install(FILES "${TC_JAVA_CLASS_DIR}/Termchart.class"
              "${TC_JAVA_CLASS_DIR}/TermchartException.class"
        DESTINATION java)
