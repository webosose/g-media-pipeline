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

#include "wayland_foreign.h"
#include "wayland_surface.h"
#include "wayland_importer.h"
#include "lsm_connector.h"

namespace LSM
{

Connector::Connector(void)
    : isRegistered(false)
{
    foreign  = std::make_shared<Wayland::Foreign>();
    surface  = std::make_shared<Wayland::Surface>();
    importer = std::make_shared<Wayland::Importer>();
}

Connector::~Connector(void)
{
}

bool Connector::registerID(const char *windowID, const char *pipelineID)
{
    if (isRegistered)
        return false;

    bool result = true;
    //This value doesn't have meaning now.
    //It is reserved variable of wayland.
    uint32_t exportedType = WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT;

    result &= foreign->initialize();
    result &= surface->initialize(foreign->getCompositor());
    result &= importer->initialize(foreign->getWebosForeign(), windowID, exportedType);
    foreign->flush();

    isRegistered = true;

    return result;
}

bool Connector::unregisterID(void)
{
    if (!isRegistered)
        return false;

    foreign->flush();

    importer->finalize();
    surface->finalize();
    foreign->finalize();

    isRegistered = false;

    return true;
}

bool Connector::attachPunchThrough(void)
{
    if (!isRegistered)
        return false;

    importer->attachPunchThrough();
    foreign->flush();

    return true;
}

bool Connector::detachPunchThrough(void)
{
    if (!isRegistered)
        return false;

    importer->detachPunchThrough();
    foreign->flush();

    return true;
}

bool Connector::attachSurface(void)
{
    if (!isRegistered)
        return false;

    importer->attachSurface(surface->getSurface());
    foreign->flush();

    return true;
}

bool Connector::detachSurface(void)
{
    if (!isRegistered)
        return false;

    importer->detachSurface(surface->getSurface());
    foreign->flush();

    return true;
}

struct wl_display *Connector::getDisplay(void)
{
    if (!isRegistered)
        return nullptr;

    return foreign->getDisplay();
}

struct wl_surface *Connector::getSurface(void)
{
    if (!isRegistered)
        return nullptr;

    return surface->getSurface();
}

void Connector::getVideoSize(gint &width, gint &height) {
  width = video_width;
  height = video_height;
}

void Connector::setVideoSize(gint width, gint height) {
  video_width = width;
  video_height = height;
}

} // namespace LSM
