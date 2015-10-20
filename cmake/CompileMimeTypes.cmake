# This file is part of the "x0" project
#   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
#
# x0 is free software: you can redistribute it and/or modify it under
# the terms of the GNU Affero General Public License v3.0.
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Usage:
#  MIMETYPES_GENERATE_CPP(ExportVarCC path/to/your/mime.types)
#
# ExportVarCC is now a variable to a filename that contains the mimetypes map
# in C++ of type unordered_map<string, string>.
#
function(MIMETYPES_GENERATE_CPP SRCS)
  if(NOT ARGN)
    message(SEND_ERROR "Error: MIMETYPES_GENERATE_CPP() called without any arguments")
    return()
  endif(NOT ARGN)

  set(${SRCS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    get_filename_component(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
    get_filename_component(PROJ_ROOT_DIR ${PROJECT_SOURCE_DIR} ABSOLUTE)

    string(LENGTH ${ROOT_DIR} ROOT_DIR_LEN)
    get_filename_component(FIL_DIR ${ABS_FIL} PATH)
    string(LENGTH ${FIL_DIR} FIL_DIR_LEN)
    math(EXPR FIL_DIR_REM "${FIL_DIR_LEN} - ${ROOT_DIR_LEN}")

    string(SUBSTRING ${FIL_DIR} ${ROOT_DIR_LEN} ${FIL_DIR_REM} FIL_DIR_PREFIX)
    set(FIL_WEPREFIX "${FIL_DIR_PREFIX}/${FIL_WE}")

    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WEPREFIX}.mimetypes2cc.cc")

    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WEPREFIX}.mimetypes2cc.cc"
      COMMAND ${PROJ_ROOT_DIR}/mimetypes2cc.sh
      ARGS ${ABS_FIL} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WEPREFIX}.mimetypes2cc.cc" mimetypes2cc
      DEPENDS ${ABS_FIL}
      COMMENT "Running mimetypes2cc on ${FIL}"
      VERBATIM )
  endforeach()

  set_source_files_properties(${${SRCS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
endfunction()


