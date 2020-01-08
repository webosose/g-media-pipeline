// Copyright (c) 2018-2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#ifndef _WAYLAND_EXPORTER_
#define _WAYLAND_EXPORTER_

#include "common.h"
#include <string>
#include <wayland-webos-foreign-client-protocol.h>

namespace Wayland
{
class Exporter
{
public:
    Exporter(void);
    ~Exporter(void);

    bool initialize(struct wl_display *display, struct wl_webos_foreign *foreign, struct wl_surface *surface,
                    uint32_t exportedType);
    void finalize(void);

    void setWindowID(const char *windowID);
    const char *getWindowID(void);

    struct wl_webos_exported *getWebosExported(void);

    void setRegion(struct wl_region *sourceRegion, struct wl_region *destinationRegion);

private:
    DISALLOW_COPY_AND_ASSIGN(Exporter);

    struct wl_webos_exported *webosExported;
    std::string windowID;
};

} // namespace Wayland

#endif //_WAYLAND_EXPORTER_
