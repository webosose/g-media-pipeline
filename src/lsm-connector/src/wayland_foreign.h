/******************************************************************************
 *   LCD TV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   Copyright(c) 2018-2019 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work may be reproduced, stored in a
 *   retrieval system, or transmitted by any means without prior written
 *   permission of LG Electronics Inc.
 ******************************************************************************/

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
