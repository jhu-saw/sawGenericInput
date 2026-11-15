# (C) Copyright 2026 Johns Hopkins University (JHU), All Rights Reserved.
#
# --- begin cisst license - do not edit ---
#
# This software is provided "as is" under an open source license, with
# no warranty. The complete license can be found in license.txt and
# http://www.cisst.org/cisst/license.txt.
#
# --- end cisst license ---

find_package (cisst 1.5 REQUIRED COMPONENTS
  cisstCommon
  cisstCommonQt
  cisstVector
  cisstMultiTask
  cisstParameterTypes)
if (CISST_QT_VERSION STREQUAL "6")
  find_package (Qt6 REQUIRED COMPONENTS Widgets)
  set (_widgets Qt6::Widgets)
else ()
  find_package (Qt5 REQUIRED COMPONENTS Widgets)
  set (_widgets Qt5::Widgets)
endif ()
find_package (PkgConfig REQUIRED)
pkg_check_modules (EVDEV_CONFIGURATOR REQUIRED IMPORTED_TARGET libevdev)
add_library (sawGenericInputConfiguratorSupport STATIC
  CaptureModel.cpp
  ConfigWindow.cpp)
target_compile_features (sawGenericInputConfiguratorSupport PUBLIC cxx_std_17)
target_include_directories (sawGenericInputConfiguratorSupport PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries (sawGenericInputConfiguratorSupport PUBLIC
  sawGenericInput
  ${_widgets}
  PRIVATE
  PkgConfig::EVDEV_CONFIGURATOR)
target_compile_options (sawGenericInputConfiguratorSupport PRIVATE
  -Wall
  -Wextra
  -Wpedantic)
add_executable (sawGenericInputConfigQt main.cpp)
target_link_libraries (sawGenericInputConfigQt PRIVATE
  sawGenericInputConfiguratorSupport)
set_target_properties (sawGenericInputConfigQt PROPERTIES
  FOLDER sawGenericInput)
install (TARGETS sawGenericInputConfigQt
  COMPONENT sawGenericInput-Applications
  RUNTIME DESTINATION bin)
