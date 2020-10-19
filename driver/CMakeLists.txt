cmake_minimum_required(VERSION 3.13.4)

project(FanucDriver LANGUAGES C VERSION 0.1)

#set_target_properties(fwlib32 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/external/fwlib/libfwlib32-linux-x86.so.1.0.5 )

add_executable(fanuc_driver src/main.c src/data.c ../external/cJSON/cJSON.c)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  #  target_compile_options(fanuc_driver PRIVATE -pthread)
  #  set_target_properties(fanuc_driver PROPERTIES COMPILE_FLAGS "-pthread")
else()
  target_compile_options(fanuc_driver PRIVATE -pthread -m32)
  set_target_properties(fanuc_driver PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

#add_library(cjson STATIC external/cJSON/cJSON.c)

add_library(fwlib32 SHARED IMPORTED external/fwlib/fwlib32.h)
# also requires renaming / linking libfwlib32-$platform-$arch.so.$version to libfwlib32.so at runtime
set_target_properties(fwlib32 PROPERTIES
  IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../external/fwlib/libfwlib32-linux-x86.so.1.0.5
)
set(CMAKE_INSTALL_RPATH "${CMAKE_SOURCE_DIR}/../external/fwlib/libfwlib32-linux-x86.so.1.0.5")

#set(RDKAFKA_BUILD_STATIC ON CACHE BOOL "")
#add_subdirectory(external/librdkafka)
#target_link_libraries(fanuc_driver rdkafka)

# add rdkafka here
# add cjson here
target_link_libraries(fanuc_driver rt m fwlib32 pthread rdkafka)

#set(cmake_install_rpath "${CMAKE_INSTALL_PREFIX}/fwlib")

install (TARGETS fanuc_driver DESTINATION /usr/local/bin)
