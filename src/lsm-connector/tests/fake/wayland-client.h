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

#ifndef _WAYLAND_CLIENT_H_
#define _WAYLAND_CLIENT_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wl_display;
struct wl_compositor;
struct wl_shell;
struct wl_webos_shell;
struct wl_interface;
struct wl_registry;
struct wl_webos_surface_group_compositor;
struct wl_region;
struct wl_surface;
struct wl_webos_exported;
struct wl_webos_foreign;
struct wl_webos_imported;

extern const struct wl_interface wl_compositor_interface;
extern const struct wl_interface wl_shell_interface;
extern const struct wl_interface wl_webos_shell_interface;
extern const struct wl_interface wl_webos_surface_group_compositor_interface;
extern const struct wl_interface wl_webos_foreign_interface;
extern const struct wl_interface wl_webos_exported_interface;
extern const struct wl_interface wl_webos_imported_interface;

enum wl_webos_foreign_webos_exported_type {
    WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT    = 0,
    WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_SUBTITLE_OBJECT = 1,
};

enum wl_webos_imported_surface_alignment {
    WL_WEBOS_IMPORTED_SURFACE_ALIGNMENT_STRETCH = 0,
    WL_WEBOS_IMPORTED_SURFACE_ALIGNMENT_NORTH   = 1,
    WL_WEBOS_IMPORTED_SURFACE_ALIGNMENT_WEST    = 2,
    WL_WEBOS_IMPORTED_SURFACE_ALIGNMENT_SOUTH   = 4,
    WL_WEBOS_IMPORTED_SURFACE_ALIGNMENT_EAST    = 8,
    WL_WEBOS_IMPORTED_SURFACE_ALIGNMENT_CENTER  = 16,
};

struct wl_webos_exported_listener {
    void (*window_id_assigned)(void *data,
                               struct wl_webos_exported *wl_webos_exported,
                               const char *window_id,
                               uint32_t exported_type);
};

struct wl_webos_imported_listener {
    void (*destination_region_changed)(void *data,
                                       struct wl_webos_imported *wl_webos_imported,
                                       uint32_t width,
                                       uint32_t height);
};

struct wl_registry_listener {
    void (*global)(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version);

    void (*global_remove)(void *data, struct wl_registry *wl_registry, uint32_t name);
};

struct wl_shell_surface_listener {
    void (*ping)(void *data, struct wl_shell_surface *wl_shell_surface, uint32_t serial);
    void (*configure)(void *data, struct wl_shell_surface *wl_shell_surface, uint32_t edges, int32_t width, int32_t height);
    void (*popup_done)(void *data, struct wl_shell_surface *wl_shell_surface);
};

void *wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version);
struct wl_display *wl_display_connect(const char *name);
struct wl_registry *wl_display_get_registry(struct wl_display *wl_display);
int wl_registry_add_listener(struct wl_registry *wl_registry, const struct wl_registry_listener *listener, void *data);
int wl_display_dispatch(struct wl_display *display);
void wl_webos_shell_destroy(struct wl_webos_shell *wl_webos_shell);
void wl_shell_destroy(struct wl_shell *wl_shell);
void wl_compositor_destroy(struct wl_compositor *wl_compositor);
void wl_display_disconnect(struct wl_display *display);
int wl_display_flush(struct wl_display *display);
void wl_webos_surface_group_compositor_destroy(struct wl_webos_surface_group_compositor *wl_webos_surface_group_compositor);
struct wl_region *wl_compositor_create_region(struct wl_compositor *wl_compositor);
void wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height);
void wl_region_destroy(struct wl_region *wl_region);

void wl_shell_surface_pong(struct wl_shell_surface *wl_shell_surface, uint32_t serial);
struct wl_surface *wl_compositor_create_surface(struct wl_compositor *wl_compositor);
struct wl_shell_surface *wl_shell_get_shell_surface(struct wl_shell *wl_shell, struct wl_surface *surface);
int wl_shell_surface_add_listener(struct wl_shell_surface *wl_shell_surface, const struct wl_shell_surface_listener *listener, void *data);
void wl_shell_surface_set_title(struct wl_shell_surface *wl_shell_surface, const char *title);
struct wl_webos_shell_surface *wl_webos_shell_get_shell_surface(struct wl_webos_shell *wl_webos_shell, struct wl_surface *surface);
void wl_webos_shell_surface_set_property(struct wl_webos_shell_surface *wl_webos_shell_surface, const char *name, const char *value);
struct wl_egl_window *wl_egl_window_create(struct wl_surface *surface, int width, int height);
void wl_egl_window_destroy(struct wl_egl_window *egl_window);
void wl_webos_shell_surface_destroy(struct wl_webos_shell_surface *wl_webos_shell_surface);
void wl_shell_surface_destroy(struct wl_shell_surface *wl_shell_surface);
void wl_surface_destroy(struct wl_surface *wl_surface);
void wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y);
void wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height);
void wl_surface_commit(struct wl_surface *wl_surface);

struct wl_webos_surface_group *wl_webos_surface_group_compositor_get_surface_group(struct wl_webos_surface_group_compositor *wl_webos_surface_group_compositor, const char *name);
void wl_webos_surface_group_attach(struct wl_webos_surface_group *wl_webos_surface_group, struct wl_surface *surface, const char *layer_name);
void wl_webos_surface_group_detach(struct wl_webos_surface_group *wl_webos_surface_group, struct wl_surface *surface);
void wl_webos_surface_group_destroy(struct wl_webos_surface_group *wl_webos_surface_group);

void wl_webos_foreign_destroy(struct wl_webos_foreign *wl_webos_foreign);
struct wl_webos_exported *wl_webos_foreign_export_element(struct wl_webos_foreign *wl_webos_foreign, struct wl_surface *surface, uint32_t exported_type);
struct wl_webos_imported *wl_webos_foreign_import_element(struct wl_webos_foreign *wl_webos_foreign, const char *window_id, uint32_t exported_type);
int wl_webos_exported_add_listener(struct wl_webos_exported *wl_webos_exported, const struct wl_webos_exported_listener *listener, void *data);
void wl_webos_exported_destroy(struct wl_webos_exported *wl_webos_exported);
void wl_webos_exported_set_exported_window(struct wl_webos_exported *wl_webos_exported, struct wl_region *source_region, struct wl_region *destination_region);
int wl_webos_imported_add_listener(struct wl_webos_imported *wl_webos_imported, const struct wl_webos_imported_listener *listener, void *data);
void wl_webos_imported_set_surface_alignment(struct wl_webos_imported *wl_webos_imported, uint32_t format);
void wl_webos_imported_destroy(struct wl_webos_imported *wl_webos_imported);
void wl_webos_imported_attach_punchthrough(struct wl_webos_imported *wl_webos_imported);
void wl_webos_imported_detach_punchthrough(struct wl_webos_imported *wl_webos_imported);
void wl_webos_imported_attach_surface(struct wl_webos_imported *wl_webos_imported, struct wl_surface *surface);
void wl_webos_imported_detach_surface(struct wl_webos_imported *wl_webos_imported, struct wl_surface *surface);

#ifdef __cplusplus
}
#endif

#endif //_WAYLAND_CLIENT_H_
