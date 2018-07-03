# Copyright (c) 2018 LG Electronics, Inc.
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

find_package(PkgConfig REQUIRED)

pkg_check_modules(PMLOGLIB QUIET PmLogLib)
if (PMLOGLIB_FOUND)
  set(PMLOG_LIBRARIES PmLogLib)
endif (PMLOGLIB_FOUND)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(.)

include_directories(./base)

include_directories(./service)

include_directories(./log)

include_directories(./mediaresource)

include_directories(./player)

add_subdirectory(player)

add_subdirectory(mediaplayerclient)

find_package(Threads REQUIRED)

add_executable(g-media-pipeline main.cpp)
target_link_libraries(g-media-pipeline gmp-player ${PMLOG_LIBRARIES})
install(TARGETS g-media-pipeline DESTINATION sbin)
