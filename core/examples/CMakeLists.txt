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
project (sawGenericInputExamples VERSION 1.0.0)

find_package (cisst 1.5 REQUIRED COMPONENTS
  cisstCommon
  cisstVector
  cisstOSAbstraction
  cisstMultiTask
  cisstParameterTypes)
find_package (sawGenericInput 1.0.0 REQUIRED)
include_directories (${sawGenericInput_INCLUDE_DIR})
link_directories (${sawGenericInput_LIBRARY_DIR})

if (sawGenericInput_WITH_QT)
  add_subdirectory (Qt)
endif ()
