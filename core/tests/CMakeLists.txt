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
project (sawGenericInputTests VERSION 1.0.0)
find_package (cisst 1.5 REQUIRED COMPONENTS
  cisstCommon
  cisstVector
  cisstOSAbstraction
  cisstMultiTask
  cisstParameterTypes)
find_package (sawGenericInput 1.0.0 REQUIRED)
include_directories (${sawGenericInput_INCLUDE_DIR})
link_directories (${sawGenericInput_LIBRARY_DIR})
set (_share "${CMAKE_CURRENT_SOURCE_DIR}/../share")
add_executable (sawGenericInput_load_test load_test.cpp)
target_link_libraries (sawGenericInput_load_test PRIVATE ${cisst_LIBRARIES})
add_executable (sawGenericInput_evdev_test sawEvdev_test.cpp)
target_link_libraries (sawGenericInput_evdev_test PRIVATE
  ${sawGenericInput_LIBRARIES}
  ${cisst_LIBRARIES})
add_test (NAME sawGenericInput_evdev
  COMMAND sawGenericInput_evdev_test
  ${_share}/pcsensor-pedals.json)
add_executable (sawGenericInput_frame_test frame_test.cpp)
target_link_libraries (sawGenericInput_frame_test PRIVATE
  ${sawGenericInput_LIBRARIES}
  ${cisst_LIBRARIES})
add_test (NAME sawGenericInput_frames
  COMMAND sawGenericInput_frame_test
  ${_share}/joystick.json)
add_test (NAME sawGenericInput_dynamic_load
  COMMAND sawGenericInput_load_test
  $<TARGET_FILE:sawGenericInput>
  ${_share}/pcsensor-pedals.json)
if (TARGET sawGenericInputConfiguratorSupport)
  add_executable (sawGenericInput_configurator_test configurator_test.cpp)
  target_link_libraries (sawGenericInput_configurator_test PRIVATE
    sawGenericInputConfiguratorSupport)
  add_test (NAME sawGenericInput_configurator
    COMMAND sawGenericInput_configurator_test)
  set_tests_properties (sawGenericInput_configurator PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif ()
