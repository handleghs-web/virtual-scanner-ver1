cmake_minimum_required(VERSION 3.15)
project(OMRFolderScanner CXX)

# TWAIN data source ("virtual scanner") that serves the images of a folder like a document feeder.
# Based on the TWAIN Working Group sample data source (Twain_DS_sample01), built without Qt.

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")   # static CRT: no Visual C++ runtime needed for the DS itself

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH win64)
else()
  set(ARCH win32)
endif()

file(GLOB DS_SRC     ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB COMMON_SRC ${CMAKE_SOURCE_DIR}/common/*.cpp)

add_library(OMRFolderScanner SHARED ${DS_SRC} ${COMMON_SRC} ${CMAKE_SOURCE_DIR}/src/TWAINDS.def)

target_include_directories(OMRFolderScanner PRIVATE
  ${CMAKE_SOURCE_DIR}/ext/include ${CMAKE_SOURCE_DIR}/common ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(OMRFolderScanner PRIVATE
  WIN32 _WINDOWS STRICT _USRDLL _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)

target_link_libraries(OMRFolderScanner PRIVATE
  ${CMAKE_SOURCE_DIR}/ext/lib/${ARCH}/FreeImage.lib user32)

set_target_properties(OMRFolderScanner PROPERTIES PREFIX "" SUFFIX ".ds" OUTPUT_NAME "OMRFolderScanner")

# FreeImage.dll must sit next to the .ds file
add_custom_command(TARGET OMRFolderScanner POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/ext/bin/${ARCH}/FreeImage.dll $<TARGET_FILE_DIR:OMRFolderScanner>)
