cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(chat-dock VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

if(WIN32 AND DEFINED OBS_SOURCE_DIR AND DEFINED OBS_IMPORT_LIB_DIR)
  add_library(OBS::libobs SHARED IMPORTED)
  set_target_properties(
    OBS::libobs PROPERTIES IMPORTED_IMPLIB
                           "${OBS_IMPORT_LIB_DIR}/obs.lib")

  if(DEFINED OBS_BIN_DIR)
    set_target_properties(OBS::libobs PROPERTIES IMPORTED_LOCATION
                                                 "${OBS_BIN_DIR}/obs.dll")
  endif()

  add_library(OBS::obs-frontend-api SHARED IMPORTED)
  set_target_properties(
    OBS::obs-frontend-api
    PROPERTIES IMPORTED_IMPLIB
               "${OBS_IMPORT_LIB_DIR}/obs-frontend-api.lib")

  if(DEFINED OBS_BIN_DIR)
    set_target_properties(
      OBS::obs-frontend-api PROPERTIES IMPORTED_LOCATION
                                       "${OBS_BIN_DIR}/obs-frontend-api.dll")
  endif()

  set(OBS_INCLUDE_DIRS
      "${OBS_SOURCE_DIR}/libobs"
      "${OBS_SOURCE_DIR}/libobs/util"
      "${OBS_SOURCE_DIR}/frontend/api")
else()
  find_package(libobs CONFIG REQUIRED)
  find_package(obs-frontend-api CONFIG REQUIRED)
endif()

add_library(
  chat-dock MODULE
  src/obs-browser-panel.cpp
  src/plugin-main.cpp
  src/chat-dock.cpp
  src/detector.cpp)

target_link_libraries(
  chat-dock
  PRIVATE OBS::libobs
          OBS::obs-frontend-api
          Qt6::Widgets
          Qt6::Network)

if(WIN32)
  target_link_libraries(chat-dock PRIVATE winhttp)
endif()

if(OBS_INCLUDE_DIRS)
  target_include_directories(chat-dock PRIVATE ${OBS_INCLUDE_DIRS})
endif()

set_target_properties(chat-dock PROPERTIES PREFIX "")

if(WIN32)
  set_property(TARGET chat-dock PROPERTY MSVC_RUNTIME_LIBRARY
                                         "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(WIN32)
  set(OBS_PLUGIN_ARCH_DIR "64bit")
else()
  set(OBS_PLUGIN_ARCH_DIR "64bit")
endif()

install(
  TARGETS chat-dock
  RUNTIME DESTINATION "chat-dock/bin/${OBS_PLUGIN_ARCH_DIR}"
  LIBRARY DESTINATION "chat-dock/bin/${OBS_PLUGIN_ARCH_DIR}")
