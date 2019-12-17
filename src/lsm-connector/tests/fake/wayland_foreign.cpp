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
#include "api_call_checker.h"
#include <cstdio>
#include <cstring>

static void display_handle_global(void *waylandData, struct wl_registry *registry, uint32_t id, const char *interface,
                                  uint32_t version)
{
    auto foreign = (Wayland::Foreign *)waylandData;

    if (strcmp(interface, "wl_compositor") == 0) {
        foreign->setCompositor((struct wl_compositor *)wl_registry_bind(registry, id, &wl_compositor_interface, 1));
    } else if (strcmp(interface, "wl_shell") == 0) {
        foreign->setShell((struct wl_shell *)wl_registry_bind(registry, id, &wl_shell_interface, 1));
    } else if (strcmp(interface, "wl_webos_shell") == 0) {
        foreign->setWebosShell((struct wl_webos_shell *)wl_registry_bind(registry, id, &wl_webos_shell_interface, 1));
    } else if (strcmp(interface, "wl_webos_foreign") == 0) {
        foreign->setWebosForeign(
            (struct wl_webos_foreign *)wl_registry_bind(registry, id, &wl_webos_foreign_interface, 1));
    }
}

static const struct wl_registry_listener registryListener = {display_handle_global};

namespace Wayland
{

Foreign::Foreign(void)
    : display(nullptr), registry(nullptr), compositor(nullptr), shell(nullptr), webosShell(nullptr),
      webosForeign(nullptr)
{
}

Foreign::~Foreign(void) {}

bool Foreign::initialize(void)
{
    display = wl_display_connect(nullptr);
    callAPI("Wayland::Foreign::initialize");

    if (!display) {
        return false;
    }

    registry = wl_display_get_registry(display);
    if (!registry) {
        return false;
    }

    wl_registry_add_listener(registry, &registryListener, this);

    wl_display_dispatch(display);

    return true;
}

void Foreign::finalize(void)
{
    if (webosForeign)
        wl_webos_foreign_destroy(webosForeign);
    if (webosShell)
        wl_webos_shell_destroy(webosShell);
    if (shell)
        wl_shell_destroy(shell);
    if (compositor)
        wl_compositor_destroy(compositor);

    if (display) {
        wl_display_flush(display);
        wl_display_disconnect(display);
    }

    callAPI("Wayland::Foreign::finalize");
}

void Foreign::setCompositor(struct wl_compositor *compositor)
{
    this->compositor = compositor;
    callAPI("Wayland::Foreign::setCompositor");
}

void Foreign::setShell(struct wl_shell *shell)
{
    this->shell = shell;
    callAPI("Wayland::Foreign::setShell");
}

void Foreign::setWebosShell(struct wl_webos_shell *webosShell)
{
    this->webosShell = webosShell;
    callAPI("Wayland::Foreign::setWebosShell");
}

void Foreign::setWebosForeign(struct wl_webos_foreign *webosForeign)
{
    this->webosForeign = webosForeign;
    callAPI("Wayland::Foreign::setWebosForeign");
}

struct wl_display *Foreign::getDisplay(void)
{
    callAPI("Wayland::Foreign::getDisplay");
    return display;
}

struct wl_compositor *Foreign::getCompositor(void)
{
    callAPI("Wayland::Foreign::getCompositor");
    return compositor;
}

struct wl_shell *Foreign::getShell(void)
{
    callAPI("Wayland::Foreign::getShell");
    return shell;
}

struct wl_webos_shell *Foreign::getWebosShell(void)
{
    callAPI("Wayland::Foreign::getWebosShell");
    return webosShell;
}

struct wl_webos_foreign *Foreign::getWebosForeign(void)
{
    callAPI("Wayland::Foreign::getWebosForeign");
    return webosForeign;
}

struct wl_region *Foreign::createRegion(int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct wl_region *region = wl_compositor_create_region(compositor);
    wl_region_add(region, x, y, width, height);
    callAPI("Wayland::Foreign::createRegion");

    return region;
}

void Foreign::destroyRegion(struct wl_region *region)
{
    wl_region_destroy(region);
    callAPI("Wayland::Foreign::destroyRegion");
}

bool Foreign::flush(void)
{
    callAPI("Wayland::Foreign::flush");
    return wl_display_flush(display);
}

} // namespace Wayland
