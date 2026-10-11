# TermchartAbiVersion.cmake —— ABI 版本与 soname
#
# 约定（ABI_POLICY.md）：只承诺 PIMPL 门面的稳定性；内部结构不承诺。
# soname 跟随 major.minor：minor 之内不破坏，major 之间允许破坏。

set(TERMCHART_ABI_VERSION "${termchart_VERSION_MAJOR}.${termchart_VERSION_MINOR}")
set(TERMCHART_SOVERSION "${termchart_VERSION_MAJOR}")
message(STATUS "termchart ABI version: ${TERMCHART_ABI_VERSION} (soname ${TERMCHART_SOVERSION})")
