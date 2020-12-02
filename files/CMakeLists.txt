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

configure_file(sysbus/g-media-pipeline.manifest.json.in sysbus/g-media-pipeline.manifest.json @ONLY)
configure_file(sysbus/com.webos.g-media-pipeline.role.json.in sysbus/com.webos.g-media-pipeline.role.json @ONLY)
configure_file(sysbus/com.webos.g-media-pipeline.service.in sysbus/com.webos.g-media-pipeline.service @ONLY)
configure_file(sysbus/com.webos.g-media-pipeline.api.json sysbus/com.webos.g-media-pipeline.api.json @ONLY)
configure_file(sysbus/com.webos.g-media-pipeline.perm.json sysbus/com.webos.g-media-pipeline.perm.json @ONLY)
configure_file(sysbus/com.webos.g-media-pipeline.groups.json sysbus/com.webos.g-media-pipeline.groups.json @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sysbus/g-media-pipeline.manifest.json
    DESTINATION share/luna-service2/manifests.d/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sysbus/com.webos.g-media-pipeline.role.json
    DESTINATION share/luna-service2/roles.d/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sysbus/com.webos.g-media-pipeline.service
    DESTINATION share/luna-service2/services.d/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sysbus/com.webos.g-media-pipeline.api.json
    DESTINATION share/luna-service2/api-permissions.d/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sysbus/com.webos.g-media-pipeline.perm.json
    DESTINATION share/luna-service2/client-permissions.d/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sysbus/com.webos.g-media-pipeline.groups.json
    DESTINATION share/luna-service2/groups.d/)
