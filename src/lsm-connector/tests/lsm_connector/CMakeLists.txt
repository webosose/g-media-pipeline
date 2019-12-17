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

include_directories(../../include)
include_directories(../fake)
include_directories(../../src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -o2 -g -std=c++11")

set(BIN_NAME gtest_lsm-connector_test)
set(SRC_LIST
    gtest_TVDEVTC-3009_lsm_connector.cpp
    ../../src/lsm_connector.cpp
    ../fake/wayland_foreign.cpp
    ../fake/wayland_importer.cpp
    ../fake/wayland_surface.cpp
    ../fake/wayland-client.cpp
    ../fake/api_call_checker.cpp
)

add_executable(${BIN_NAME} ${SRC_LIST})
target_link_libraries(${BIN_NAME} rt dl m stdc++ ${WEBOS_GTEST_LIBRARIES})

install(TARGETS ${BIN_NAME} DESTINATION ${WEBOS_INSTALL_TESTSDIR}/starfish-media-pipeline/lsm-connector PERMISSIONS OWNER_EXECUTE OWNER_READ)

if(COVERAGE_TEST)
    string(REPLACE "${CMAKE_SOURCE_DIR}/" "" RELATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${BIN_NAME} PROPERTIES LINK_FLAGS -coverage)
    set_target_properties(${BIN_NAME} PROPERTIES COMPILE_FLAGS "-g -O0 -coverage")
    set_property(TARGET ${BIN_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS " -fprofile-dir=/home/gcov/${PROJECT_NAME}/${RELATIVE_DIR}")
endif()
