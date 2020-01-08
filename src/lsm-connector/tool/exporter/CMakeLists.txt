# Copyright (c) 2018-2020 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

set(BIN_NAME lsm_connector_exporter)

include(FindPkgConfig)

pkg_check_modules(EGL REQUIRED egl)
webos_add_compiler_flags(ALL ${EGL_CFLAGS})

pkg_check_modules(GLESV2 REQUIRED glesv2)
webos_add_compiler_flags(ALL ${GLESV2_CFLAGS})

pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
webos_add_compiler_flags(ALL ${WAYLAND_EGL_CFLAGS})

pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
webos_add_compiler_flags(ALL ${WAYLAND_CLIENT_CFLAGS})

pkg_check_modules(WAYLAND_WEBOS_CLIENT REQUIRED wayland-webos-client)
webos_add_compiler_flags(ALL ${WAYLAND_WEBOS_CLIENT_CFLAGS})

include_directories(../../include)
include_directories(src)
include_directories(${PF_INCLUDE_DIRS})

set(SRC_LIST
    src/main.cpp
)

set(LIB_LIST
    lsm-connector
)

add_executable(${BIN_NAME} ${SRC_LIST})
target_link_libraries(${BIN_NAME} ${LIB_LIST}
    ${EGL_LDFLAGS}
    ${GLESV2_LDFLAGS}
    ${WAYLAND_EGL_LDFLAGS}
    ${WAYLAND_CLIENT_LDFLAGS}
    ${WAYLAND_WEBOS_CLIENT_LDFLAGS})

install(TARGETS ${BIN_NAME} DESTINATION sbin)
