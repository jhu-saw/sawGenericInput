#
# (C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty.  The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

cmake_minimum_required (VERSION 3.16)
project (sawGenericInput VERSION 1.0.0)

set (REQUIRED_CISST_LIBRARIES
  cisstCommon
  cisstVector
  cisstOSAbstraction
  cisstMultiTask
  cisstParameterTypes)
find_package (cisst 1.5 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})
if (NOT CISST_HAS_JSON)
  message (FATAL_ERROR "sawGenericInput requires cisst to be built with JSON")
endif ()

find_package (PkgConfig REQUIRED)
pkg_check_modules (EVDEV REQUIRED IMPORTED_TARGET libevdev)

find_package (jsoncpp CONFIG QUIET)
if (TARGET JsonCpp::JsonCpp)
  set (_json_target JsonCpp::JsonCpp)
elseif (TARGET jsoncpp_lib)
  set (_json_target jsoncpp_lib)
else ()
  pkg_check_modules (JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)
  set (_json_target PkgConfig::JSONCPP)
endif ()

# Use the SAW output convention, while keeping standalone builds out of the
# cisst installation when a workspace setup script has been sourced.
cisst_set_output_path ()
set (LIBRARY_OUTPUT_PATH "${sawGenericInputCore_BINARY_DIR}/lib")
set (EXECUTABLE_OUTPUT_PATH "${sawGenericInputCore_BINARY_DIR}/bin")

set (sawGenericInput_INCLUDE_DIR
  "${sawGenericInput_SOURCE_DIR}/include"
  "${sawGenericInput_BINARY_DIR}/include")
set (sawGenericInput_HEADER_DIR
  "${sawGenericInput_SOURCE_DIR}/include/sawGenericInput")
set (sawGenericInput_LIBRARY_DIR "${LIBRARY_OUTPUT_PATH}")
set (sawGenericInput_LIBRARIES sawGenericInput)
include_directories (${sawGenericInput_INCLUDE_DIR})
cisst_add_config_files (sawGenericInput)

add_library (sawGenericInput SHARED
  code/Configuration.cpp
  code/mtsEvdev.cpp
  ${sawGenericInput_HEADER_DIR}/Configuration.h
  ${sawGenericInput_HEADER_DIR}/mtsEvdev.h
  ${sawGenericInput_HEADER_DIR}/sawGenericInputExport.h)
target_compile_features (sawGenericInput PUBLIC cxx_std_17)
target_include_directories (sawGenericInput PUBLIC
  $<BUILD_INTERFACE:${sawGenericInput_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${sawGenericInput_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries (sawGenericInput PUBLIC
  ${cisst_LIBRARIES}
  PRIVATE
  ${_json_target}
  PkgConfig::EVDEV)
set_target_properties (sawGenericInput PROPERTIES
  VERSION ${sawGenericInput_VERSION}
  SOVERSION 1
  FOLDER "sawGenericInput"
  LIBRARY_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_PATH}"
  RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")
target_compile_options (sawGenericInput PRIVATE -Wall -Wextra -Wpedantic)

install (DIRECTORY "${sawGenericInput_SOURCE_DIR}/include/sawGenericInput"
  "${sawGenericInput_BINARY_DIR}/include/sawGenericInput"
  DESTINATION include
  COMPONENT sawGenericInput-dev)
install (TARGETS sawGenericInput
  COMPONENT sawGenericInput
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
