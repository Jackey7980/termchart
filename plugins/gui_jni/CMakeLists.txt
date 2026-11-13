# plugins/gui_jni/CMakeLists.txt —— JNI GUI 后端（可选插件，Take 2 句柄注入）
#
# 由顶层无条件 add_subdirectory（插件本身即可选单元）。产出共享库
# termchart_gui_jni（Java System.loadLibrary("termchart_gui_jni") 加载）+ Java 冒烟。
# 依赖 JDK（jni.h）；缺 JDK 时本插件自行跳过，不拖累核心零依赖构建。
#
# Android：把 src/jni_backend.cpp + example/jni_demo.cpp 直接编进 NDK 的 CMake，
# 不需要本文件的桌面共享库目标。
find_package(JNI QUIET)
if(NOT JNI_FOUND)
    message(STATUS "termchart GUI JNI 插件: 未找到 JDK/jni.h，跳过")
    return()
endif()

message(STATUS "termchart GUI JNI 后端: ${JNI_INCLUDE_DIRS}")

add_library(termchart_gui_jni SHARED src/jni_backend.cpp example/jni_demo.cpp)
set_target_properties(termchart_gui_jni PROPERTIES OUTPUT_NAME termchart_gui_jni)
# Windows：MinGW 默认加 "lib" 前缀，而 System.loadLibrary 只查 termchart_gui_jni.dll，
# 必须去掉前缀，否则 UnsatisfiedLinkError。
if(WIN32)
    set_target_properties(termchart_gui_jni PROPERTIES PREFIX "")
endif()
target_include_directories(termchart_gui_jni PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${JNI_INCLUDE_DIRS})
target_link_libraries(termchart_gui_jni PUBLIC termchart_gui)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(termchart_gui_jni PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
# 共享库在 Unix 上需要位置无关代码（Windows/MinGW 默认可链接静态库进 DLL）。
if(UNIX AND NOT APPLE)
    set_target_properties(termchart_gui_jni PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Java 冒烟：编译 java/TermchartEmbed.java 并跑一次（无 AWT/Swing，纯内存验证）。
find_package(Java COMPONENTS Development)
if(Java_FOUND)
    set(TERMCHART_JNI_CLASSES_DIR "${CMAKE_BINARY_DIR}/jni_classes")
    add_custom_command(
        OUTPUT "${TERMCHART_JNI_CLASSES_DIR}/TermchartEmbed.class"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${TERMCHART_JNI_CLASSES_DIR}"
        # 源文件是 UTF-8（含中文注释）；不显式指定时 Windows runner 用
        # windows-1252 解码，javac 直接报 "unmappable character"。
        COMMAND ${Java_JAVAC_EXECUTABLE} -encoding UTF-8 -d "${TERMCHART_JNI_CLASSES_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/java/TermchartEmbed.java"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/java/TermchartEmbed.java"
        COMMENT "javac TermchartEmbed.java")
    add_custom_target(termchart_jni_java ALL
        DEPENDS "${TERMCHART_JNI_CLASSES_DIR}/TermchartEmbed.class")

    # JVM 无法加载 sanitizer 插桩的 native 库（TSan 实测报
    # "libtsan.so.2: cannot allocate memory in static TLS block"）。
    # jni_smoke 是宿主集成冒烟，不是库逻辑测试，所有 sanitizer 构建都跳过它；
    # native 库本身仍照常编译，正常构建 / CI 其它 job 仍跑该用例。
    set(_jni_sanitized OFF)
    if(TERMCHART_ENABLE_ASAN OR TERMCHART_ENABLE_UBSAN OR TERMCHART_ENABLE_TSAN)
        set(_jni_sanitized ON)
    endif()
    string(FIND "${CMAKE_CXX_FLAGS}" "-fsanitize=" _jni_san_flag_pos)
    if(NOT _jni_san_flag_pos EQUAL -1)
        set(_jni_sanitized ON)
    endif()
    if(TERMCHART_BUILD_TESTS AND NOT _jni_sanitized)
        add_test(NAME jni_smoke
            COMMAND ${Java_JAVA_EXECUTABLE}
                    -Djava.library.path=$<TARGET_FILE_DIR:termchart_gui_jni>
                    -cp "${TERMCHART_JNI_CLASSES_DIR}" TermchartEmbed)
        set_tests_properties(jni_smoke PROPERTIES
            REQUIRED_FILES "$<TARGET_FILE:termchart_gui_jni>"
            TIMEOUT 30)
    endif()
else()
    message(STATUS "termchart GUI JNI: 未找到 Java 运行时，跳过 Java 冒烟（native 库仍构建）")
endif()
