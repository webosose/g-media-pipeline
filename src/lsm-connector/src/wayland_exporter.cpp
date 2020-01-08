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

#include "wayland_exporter.h"

void handlerWindowIDAssigned(void *data, struct wl_webos_exported *wlWebosExported, const char *windowID,
                             uint32_t exportedType)
{
    auto exporter = (Wayland::Exporter *)data;
    exporter->setWindowID(windowID);
}

static const struct wl_webos_exported_listener exportedListener = {handlerWindowIDAssigned};

namespace Wayland
{

Exporter::Exporter() : webosExported(nullptr) {}

Exporter::~Exporter() {}

bool Exporter::initialize(struct wl_display *display, struct wl_webos_foreign *foreign, struct wl_surface *surface,
                          uint32_t exportedType)
{
    webosExported = wl_webos_foreign_export_element(foreign, surface, exportedType);
    if (webosExported == nullptr)
        return false;

    wl_webos_exported_add_listener(webosExported, &exportedListener, this);

    wl_display_dispatch(display);

    return true;
}

void Exporter::finalize(void)
{
    if (webosExported) {
        wl_webos_exported_destroy(webosExported);
        webosExported = nullptr;
    }
}

void Exporter::setWindowID(const char *windowID) { this->windowID = windowID; }

const char *Exporter::getWindowID(void) { return windowID.c_str(); }

struct wl_webos_exported *Exporter::getWebosExported(void) { return webosExported; }

void Exporter::setRegion(struct wl_region *sourceRegion, struct wl_region *destinationRegion)
{
    wl_webos_exported_set_exported_window(webosExported, sourceRegion, destinationRegion);
}

} // namespace Wayland
