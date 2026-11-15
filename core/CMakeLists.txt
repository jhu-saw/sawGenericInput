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
if (POLICY CMP0144)
  cmake_policy (SET CMP0144 NEW)
  set (CMAKE_POLICY_DEFAULT_CMP0144 NEW)
endif ()
project (sawGenericInputCore VERSION 1.0.0)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message (FATAL_ERROR "sawGenericInput requires Linux and libevdev")
endif ()

find_package (cisst 1.5 REQUIRED)
cisst_cpack_settings (VENDOR "JHU" MAINTAINER "anton.deguet@jhu.edu")

option (sawGenericInput_WITH_QT
  "Build Qt input tools and the joystick monitor"
  OFF)

# Keep every subproject's build artifacts together, as in other SAW cores.
set (LIBRARY_OUTPUT_PATH "${sawGenericInputCore_BINARY_DIR}/lib")
set (EXECUTABLE_OUTPUT_PATH "${sawGenericInputCore_BINARY_DIR}/bin")
include (CTest)
add_subdirectory (components)

set (sawGenericInput_DIR "${sawGenericInputCore_BINARY_DIR}/components")
add_subdirectory (examples)
add_subdirectory (share)
add_subdirectory (applications)
if (BUILD_TESTING)
  add_subdirectory (tests)
endif ()

include (CPack)
cpack_add_component (sawGenericInput)
cpack_add_component (sawGenericInput-dev DEPENDS sawGenericInput)
cpack_add_component (sawGenericInput-Examples DEPENDS sawGenericInput)
cpack_add_component (sawGenericInput-Share DEPENDS sawGenericInput)

cpack_add_component (sawGenericInput-Applications DEPENDS sawGenericInput)
