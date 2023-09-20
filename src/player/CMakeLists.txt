# Copyright (c) 2018-2022 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})

pkg_check_modules(GSTPLAYER gstreamer-player-1.0 REQUIRED)
include_directories(${GSTPLAYER_INCLUDE_DIRS})
link_directories(${GSTPLAYER_LIBRARY_DIRS})

pkg_check_modules(GSTAPP gstreamer-app-1.0 REQUIRED)
include_directories(${GSTAPP_INCLUDE_DIRS})
link_directories(${GSTAPP_LIBRARY_DIRS})

pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)
include_directories(${GSTREAMER_INCLUDE_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})

pkg_check_modules(GSTPBUTIL gstreamer-pbutils-1.0 REQUIRED)
include_directories(${GSTPBUTIL_INCLUDE_DIRS})
link_directories(${GSTPBUTIL_LIBRARY_DIRS})

pkg_check_modules(GST_VIDEO gstreamer-video-1.0 REQUIRED)
include_directories(${GST_VIDEO_INCLUDE_DIRS})
link_directories(${GST_VIDEO_LIBRARY_DIRS})


pkg_check_modules(MRD_PKG QUIET gstreamer-mrd-1.0)
if (MRD_PKG_FOUND)
  add_definitions(-DUSE_MRD)
  message("Found gstreamer-mrd ver.${MRD_PKG_VERSION}")
else ()
  message(STATUS "Failed to find gstreamer-mrd")
endif (MRD_PKG_FOUND)

pkg_check_modules(LUNA luna-service2 REQUIRED)
include_directories(${LUNA_INCLUDE_DIRS})
link_directories(${LUNA_LIBRARY_DIRS})

pkg_check_modules(PBNJSON pbnjson_cpp REQUIRED)
include_directories(${PBNJSON_INCLUDE_DIRS})
link_directories(${PBNJSON_LIBRARY_DIRS})

pkg_check_modules(RESOURCE-CALCULATOR REQUIRED media-resource-calculator)
include_directories(${RESOURCE-CALCULATOR_INCLUDE_DIRS})

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

find_package(Boost 1.54.0 REQUIRED COMPONENTS log)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

pkg_check_modules(PMLOGLIB QUIET PmLogLib)
if (PMLOGLIB_FOUND)
  set(PMLOG_LIBRARIES PmLogLib)
endif (PMLOGLIB_FOUND)

set(G-MEDIA-PIPELINE_HEADERS
    Player.h
    PlayerTypes.h
    ../base/types.h
    ../log/log.h
    ../mediaresource/requestor.h
    ../service/service.h
    ../dsi/DSIGeneratorFactory.h
    ../dsi/DSIGenerator.h
    ../dsi/DSIGeneratorAAC.h
    ../playerfactory/ElementFactory.h
    ../playerfactory/PlayerFactory.h
    ../playerfactory/UriPlayerFactory.h
    ../mediaplayerclient/MediaPlayerClient.h
    ../lunaserviceclient/LunaServiceClient.h
    )

set(G-MEDIA-PIPELINE_SRC
    AbstractPlayer.cpp
    UriPlayer.cpp
    UriPlainPlayer.cpp
    RtpPlayer.cpp
    BufferPlayer.cpp
    BufferPlainPlayer.cpp
    ../log/log.cpp
    ../parser/parser.cpp
    ../parser/composer.cpp
    ../service/service.cpp
    ../util/util.cpp
    ../mediaresource/requestor.cpp
    ../dsi/DSIGeneratorAAC.cpp
    ../dsi/DSIGeneratorFactory.cpp
    ../playerfactory/ElementFactory.cpp
    ../playerfactory/PlayerFactory.cpp
    ../playerfactory/UriPlayerFactory.cpp
    ../mediaplayerclient/MediaPlayerClient.cpp
    ../lunaserviceclient/LunaServiceClient.cpp
    )

set(G-MEDIA-PIPELINE_LIB
    ums_connector
    ${GSTPLAYER_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTPBUTIL_LIBRARIES}
    ${GST_VIDEO_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB2_LIBRARIES}
    ${PBNJSON_LIBRARIES}
    ${PMLOG_LIBRARIES}
    ${MRD_PKG_LIBRARIES}
    ${LUNA_LIBRARIES}
    )

add_library(gmp-player SHARED ${G-MEDIA-PIPELINE_SRC})
set_target_properties (gmp-player PROPERTIES VERSION 1.0 SOVERSION 1)
target_link_libraries(gmp-player ${G-MEDIA-PIPELINE_LIB}
    ${RESOURCE-CALCULATOR_LDFLAGS}
    resource_mgr_client
    resource_mgr_client_c
    lsm-connector
    )

if(${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi3" OR ${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi3-64")
  message(STATUS "Using config & debug file for raspberrypi3")
  install(FILES ./raspberrypi3/gst_debug.conf DESTINATION /etc/g-media-pipeline)
  install(FILES ./raspberrypi3/gst_elements.conf DESTINATION /etc/g-media-pipeline)
elseif(${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi4" OR ${WEBOS_TARGET_MACHINE} STREQUAL "raspberrypi4-64")
  message(STATUS "Using config & debug file for raspberrypi4")
  install(FILES ./raspberrypi4/gst_debug.conf DESTINATION /etc/g-media-pipeline)
  install(FILES ./raspberrypi4/gst_elements.conf DESTINATION /etc/g-media-pipeline)
elseif (${WEBOS_TARGET_MACHINE} STREQUAL "qemux86" OR ${WEBOS_TARGET_MACHINE} STREQUAL "qemux86-64")
  message(STATUS "Using config & debug file for qemux86")
  install(FILES ./qemux86/gst_debug.conf DESTINATION /etc/g-media-pipeline)
  install(FILES ./qemux86/gst_elements.conf DESTINATION /etc/g-media-pipeline)
else()
  message(FATAL_ERROR the "Check WEBOS_TARGET_MACHINE: " ${WEBOS_TARGET_MACHINE})
endif()

install(TARGETS gmp-player DESTINATION lib)
install(FILES ${G-MEDIA-PIPELINE_HEADERS} DESTINATION include/gmp)
