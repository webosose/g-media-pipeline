// Copyright (c) 2018-2021 LG Electronics, Inc.
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

/** @file wayland_foreign.h
 *
 *  @author     Dongheon Kim (dongheon.kim@lge.com)
 *  @version    1.0
 *  @date       2018.09.07
 *  @note
 *  @see
 */

#ifndef _WAYLAND_FOREIGN_H_
#define _WAYLAND_FOREIGN_H_

#include "common.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <wayland-webos-foreign-client-protocol.h>
#include <wayland-webos-shell-client-protocol.h>

namespace Wayland
{
class Foreign
{
public:
    Foreign(void);
    ~Foreign(void);

    bool initialize(void);
    bool initializeImport(void);
    void finalize(void);

    void setCompositor(struct wl_compositor *compositor);
    void setShell(struct wl_shell *shell);
    void setWebosShell(struct wl_webos_shell *webosShell);
    void setWebosForeign(struct wl_webos_foreign *webosForeign);

    struct wl_display *getDisplay(void);
    struct wl_compositor *getCompositor(void);
    struct wl_shell *getShell(void);
    struct wl_webos_shell *getWebosShell(void);
    struct wl_webos_foreign *getWebosForeign(void);

    struct wl_region *createRegion(int32_t x, int32_t y, int32_t width, int32_t height);
    void destroyRegion(struct wl_region *region);

    // Request wayland flush for sync.
    // Wayland protocol is asynchronous, so this API is required.
    bool flush(void);

private:
    DISALLOW_COPY_AND_ASSIGN(Foreign);

    // Values for wayland protocol.
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_webos_shell *webosShell;
    struct wl_webos_foreign *webosForeign;
};

} // namespace Wayland

#endif //_WAYLAND_FOREIGN_H_
