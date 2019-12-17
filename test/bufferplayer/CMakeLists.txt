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

message(STATUS "BUILDING test/bufferplayer")

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src
                   ${CMAKE_SOURCE_DIR}/src/base
                   ${CMAKE_SOURCE_DIR}/src/service
                   ${CMAKE_SOURCE_DIR}/src/log
                   ${CMAKE_SOURCE_DIR}/src/lsm-connector/include
                   ${CMAKE_SOURCE_DIR}/src/mediaplayerclient
                   ${CMAKE_SOURCE_DIR}/src/player
                   ${CMAKE_SOURCE_DIR}/src/dsi
                   )

set(TESTNAME "bufferplayer_test")
set(SRC_LIST BufferPlayerTest.cpp StreamSource.cpp)
add_executable (${TESTNAME} ${SRC_LIST})
#confirming link language here avoids linker confusion and prevents errors seen previously
set_target_properties(${TESTNAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${TESTNAME}
                      ${GLIB2_LIBRARIES}
                      ${GSTPLAYER_LIBRARIES}
                      ${GSTREAMER_LIBRARIES}
                      ${PMLOG_LIBRARIES}
                      gmp-player
                      lsm-connector
                      )
install(TARGETS ${TESTNAME} DESTINATION sbin)
