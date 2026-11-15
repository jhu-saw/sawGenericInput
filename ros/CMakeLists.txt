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
project (saw_generic_input_ros VERSION 1.0.0)

# first test for ROS1
set (ROS1_depends cisst_ros_crtk roscpp roslib)

find_package (catkin QUIET COMPONENTS ${ROS1_depends})

# ROS 1
if (catkin_FOUND)
  set (ROS_VERSION 1)
  catkin_package (CATKIN_DEPENDS ${ROS1_depends})
  include_directories (${catkin_INCLUDE_DIRS})
else (catkin_FOUND)

  # look for ROS2
  find_package (ament_cmake QUIET)
  if (ament_cmake_FOUND)
    set (ROS_VERSION 2)
    set (ROS2_depends
      rclcpp
      std_msgs
      std_srvs
      geometry_msgs
      sensor_msgs
      diagnostic_msgs
      tf2_ros
      tf2_msgs
      cisst_msgs
      crtk_msgs
      cisst_ros_bridge
      cisst_ros_crtk)
    foreach (_dep ${ROS2_depends})
      find_package (${_dep} REQUIRED)
    endforeach ()

  endif (ament_cmake_FOUND)
endif (catkin_FOUND)

if (NOT catkin_FOUND AND NOT ament_cmake_FOUND)
  message (FATAL_ERROR
    "The ROS frontend requires a sourced ROS 1 or ROS 2 environment")
endif ()

# create a list of required cisst libraries
set (REQUIRED_CISST_LIBRARIES
  cisstCommon
  cisstCommonQt
  cisstVector
  cisstOSAbstraction
  cisstMultiTask
  cisstMultiTaskQt
  cisstParameterTypes
  cisstParameterTypesQt)

find_package (cisst 1.5 REQUIRED COMPONENTS ${REQUIRED_CISST_LIBRARIES})

# set paths
cisst_set_output_path ()
set (LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")
set (EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

# sawGenericInput has been compiled within cisst, we should find it automatically
if (NOT TARGET sawGenericInput)
  find_package (sawGenericInput 1.0.0 REQUIRED)
endif ()

# sawGenericInput configuration
include_directories (${sawGenericInput_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})
link_directories (${sawGenericInput_LIBRARY_DIR})

add_executable (generic_input generic_input.cpp)
target_compile_definitions (generic_input PRIVATE
  SAW_GENERIC_INPUT_SHARE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../core/share")

# link against non cisst libraries and saw components
target_link_libraries (generic_input
  ${sawGenericInput_LIBRARIES}
  ${catkin_LIBRARIES}
  ${cisst_LIBRARIES})

install (TARGETS generic_input
  LIBRARY DESTINATION lib/${PROJECT_NAME}
  RUNTIME DESTINATION lib/${PROJECT_NAME}
  LIBRARY DESTINATION lib
  INCLUDES DESTINATION include)

if (ROS_VERSION EQUAL 2)
  ament_target_dependencies (generic_input ${ROS2_depends})
  ament_package ()
endif ()
