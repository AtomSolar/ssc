# Install script for directory: C:/SAM-Development/sam/ssc

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/sam_simulation_core")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/SAM-Development/sam/ssc/build_msvc/splinter/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/shared/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/nlopt/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/lpsolve/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/solarpilot/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/tcs/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/ssc/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/sdktool/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/tcsconsole/cmake_install.cmake")
  include("C:/SAM-Development/sam/ssc/build_msvc/test/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/SAM-Development/sam/ssc/build_msvc/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
