cmake_minimum_required(VERSION 3.13.4)

project(FanucDriver LANGUAGES C VERSION 0.1)

add_library(fwlib32 SHARED IMPORTED external/fwlib/fwlib32.h)
target_include_directories(fanuc_driver PRIVATE ${CMAKE_SOURCE_DIR}/../)
set_target_properties(fwlib32 PROPERTIES
  IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../libfwlib32-linux-x86.so.1.0.5
)
#set(CMAKE_INSTALL_RPATH "${CMAKE_SOURCE_DIR}/../libfwlib32-linux-x86.so.1.0.5")

#add_library(fwlib32 SHARED IMPORTED external/fwlib/fwlib32.h)
#set_target_properties(fwlib32 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/external/fwlib/libfwlib32-linux-x86.so.1.0.5 )

add_library(cjson STATIC external/cJSON/cJSON.c)

add_executable(fanuc_driver src/main.c src/data.c)

if( "$ENV{ARCH}" STREQUAL "x64" )
  target_compile_options(fanuc_driver PRIVATE -pthread -m32)
  set_target_properties(fanuc_driver PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
else()
  target_compile_options(fanuc_driver PRIVATE -pthread)
  set_target_properties(fanuc_driver PROPERTIES COMPILE_FLAGS "-pthread")
endif()


target_link_libraries(fanuc_driver rt m fwlib32 cjson pthread)

set(RDKAFKA_BUILD_STATIC ON CACHE BOOL "")
add_subdirectory(external/librdkafka)
target_link_libraries(fanuc_driver rdkafka)

#set(cmake_install_rpath "${CMAKE_INSTALL_PREFIX}/fwlib")

install (TARGETS fanuc_driver DESTINATION /usr/local/bin)
