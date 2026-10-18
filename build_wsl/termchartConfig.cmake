
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was TermchartConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# termchartConfig.cmake —— find_package(termchart) 入口
#
# 分层规则源文件（TermchartModules.cmake）随包安装：
# 下游若要做自己的分层校验，可以用同一份真源。

include("${CMAKE_CURRENT_LIST_DIR}/termchartTargets.cmake")

set_and_check(termchart_INCLUDE_DIR "")
set(termchart_VERSION "0.1.0")
set(termchart_ABI_VERSION "0.1")

check_required_components(termchart)
