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

find_package (cisst 1.5 REQUIRED COMPONENTS
  cisstCommon
  cisstCommonQt
  cisstVector
  cisstNumerical
  cisstOSAbstraction
  cisstMultiTask
  cisstMultiTaskQt
  cisstParameterTypes
  cisstParameterTypesQt)
add_executable (sawGenericInputQtExample mainQt.cpp)
target_compile_definitions (sawGenericInputQtExample PRIVATE
  SAW_GENERIC_INPUT_SHARE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../share")
target_link_libraries (sawGenericInputQtExample PRIVATE
  sawGenericInput
  ${QT_LIBRARIES}
  ${cisst_LIBRARIES})
install (TARGETS sawGenericInputQtExample RUNTIME DESTINATION bin)
