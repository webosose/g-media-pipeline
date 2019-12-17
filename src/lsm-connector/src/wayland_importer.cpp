// Copyright (c) 2018-2019 LG Electronics, Inc.
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

#include "wayland_importer.h"

namespace Wayland
{

Importer::Importer(void) : webosImported(nullptr) {}

Importer::~Importer(void) {}

bool Importer::initialize(struct wl_webos_foreign *foreign, const char *windowID, uint32_t exportedType)
{
    webosImported = wl_webos_foreign_import_element(foreign, windowID, exportedType);
    if (webosImported == nullptr)
        return false;

    return true;
}

void Importer::finalize(void)
{
    if (webosImported) {
        wl_webos_imported_destroy(webosImported);
        webosImported = nullptr;
    }
}

struct wl_webos_imported *Importer::getWebosImported(void) { return webosImported; }

void Importer::attachPunchThrough(void)
{
    if (webosImported) {
        wl_webos_imported_attach_punchthrough(webosImported);
    }
}

void Importer::detachPunchThrough(void)
{
    if (webosImported) {
        wl_webos_imported_detach_punchthrough(webosImported);
    }
}

void Importer::attachSurface(struct wl_surface *surface)
{
    if (webosImported) {
        wl_webos_imported_attach_surface(webosImported, surface);
    }
}

void Importer::detachSurface(struct wl_surface *surface)
{
    if (webosImported) {
        wl_webos_imported_detach_surface(webosImported, surface);
    }
}

} // namespace Wayland
