# Copyright (c) 2018-2019 LG Electronics, Inc.
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

set(LSM_CONNECTOR_LIB lsm-connector)

if (WEBOS_CONFIG_BUILD_TESTS)
    add_subdirectory(tests)
else()
  message(STATUS "Skipping Unit Test")
endif()

include(FindPkgConfig)
include(webOS/webOS)

pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
webos_add_compiler_flags(ALL ${WAYLAND_CLIENT_CFLAGS})

pkg_check_modules(WAYLAND_WEBOS_CLIENT REQUIRED wayland-webos-client)
webos_add_compiler_flags(ALL ${WAYLAND_WEBOS_CLIENT_CFLAGS})

pkg_check_modules(PMLOGLIB QUIET PmLogLib)
if (PMLOGLIB_FOUND)
  set(PMLOG_LIBRARIES PmLogLib)
endif (PMLOGLIB_FOUND)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -std=gnu99 -Wall -fPIE")

include_directories(include)
include_directories(src)

set(SRC_LIST
    ../log/log.cpp
    src/lsm_connector.cpp
    src/wayland_foreign.cpp
    src/wayland_importer.cpp
    src/wayland_surface.cpp
)

set(LSM_CONNECTOR_VERSION_MAJOR 1)
set(LSM_CONNECTOR_VERSION_MINOR 0)
set(LSM_CONNECTOR_VERSION_PATCH 0)
set(LSM_CONNECTOR_VERSION_LIST
    "${LSM_CONNECTOR_VERSION_MAJOR}"
    ".${LSM_CONNECTOR_VERSION_MINOR}"
    ".${LSM_CONNECTOR_VERSION_PATCH}"
    )
string(REPLACE ";" "" LSM_CONNECTOR_VERSION
    ${LSM_CONNECTOR_VERSION_LIST})


add_library(${LSM_CONNECTOR_LIB} SHARED ${SRC_LIST})
target_link_libraries(${LSM_CONNECTOR_LIB}
    ${PMLOG_LIBRARIES}
    ${WAYLAND_CLIENT_LDFLAGS}
    ${WAYLAND_WEBOS_CLIENT_LDFLAGS})

set_target_properties(${LSM_CONNECTOR_LIB} PROPERTIES
    VERSION ${LSM_CONNECTOR_VERSION}
    SOVERSION ${LSM_CONNECTOR_VERSION_MAJOR})

install(TARGETS ${LSM_CONNECTOR_LIB} DESTINATION lib)

add_subdirectory(tool)

