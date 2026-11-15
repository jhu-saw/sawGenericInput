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
project (sawGenericInputShare VERSION 1.0.0)
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION share/sawGenericInput
  COMPONENT sawGenericInput-Share
  PATTERN CMakeLists.txt EXCLUDE)
install (FILES ../../README.md
  DESTINATION share/sawGenericInput
  COMPONENT sawGenericInput-Share)
