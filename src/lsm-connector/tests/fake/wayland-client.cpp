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

#include <cstdio>
#include "wayland-client.h"
#include "api_call_checker.h"

struct wl_display {
    int data;
};

struct wl_compositor {
    int data;
};

struct wl_shell {
    int data;
};

struct wl_webos_shell {
    int data;
};

struct wl_interface {
    int data;
};

struct wl_registry {
    int data;
};

struct wl_webos_surface_group_compositor {
    int data;
};

struct wl_surface {
    int data;
};

struct wl_region {
    int data;
};

struct wl_shell_surface {
    int data;
};

struct wl_webos_shell_surface {
    int data;
};

struct wl_egl_window {
    int data;
};

struct wl_webos_surface_group {
    int data;
};

struct wl_webos_exported {
    int data;
};

struct wl_webos_foreign {
    int data;
};

struct wl_webos_imported {
    int data;
};

const struct wl_interface wl_compositor_interface = {.data = 0};
const struct wl_interface wl_shell_interface = {.data = 0};
const struct wl_interface wl_webos_shell_interface = {.data = 0};
const struct wl_interface wl_webos_surface_group_compositor_interface = {.data = 0};

const struct wl_interface wl_webos_foreign_interface = {.data = 0};
const struct wl_interface wl_webos_exported_interface = {.data = 0};
const struct wl_interface wl_webos_imported_interface = {.data = 0};

static struct wl_display _valid_display;
static struct wl_registry _valid_registry;
static struct wl_compositor _valid_compositor;
static struct wl_shell _valid_shell;
static struct wl_region _valid_region;
static struct wl_webos_shell _valid_webos_shell;
static struct wl_webos_surface_group_compositor _valid_webos_surface_group_compositor;
static struct wl_surface _valid_surface;
static struct wl_shell_surface _valid_shell_surface;
static struct wl_webos_shell_surface _valid_webos_shell_surface;
static struct wl_egl_window _valid_egl_window;
static struct wl_webos_surface_group _valid_webos_surface_group;
static struct wl_webos_exported _valid_webos_exported;
static struct wl_webos_foreign _valid_webos_foreign;
static struct wl_webos_imported _valid_webos_imported;

const struct wl_registry_listener *_listener;
void *_data;

void *wl_registry_bind(struct wl_registry *wl_registry, uint32_t name, const struct wl_interface *interface, uint32_t version)
{
    callAPI("wl_registry_bind");

    if (interface == &wl_compositor_interface) {
        return &_valid_compositor;
    } else if (interface == &wl_shell_interface) {
        return &_valid_shell;
    } else if (interface == &wl_webos_shell_interface) {
        return &_valid_webos_shell;
    } else if (interface == &wl_webos_surface_group_compositor_interface) {
        return &_valid_webos_surface_group_compositor;
    } else if (interface == &wl_webos_foreign_interface) {
        return &_valid_webos_foreign;
    }

    return nullptr;
}

struct wl_display *wl_display_connect(const char *name)
{
    callAPI("wl_display_connect");

    return &_valid_display;
}

struct wl_registry *wl_display_get_registry(struct wl_display *wl_display)
{
    callAPI("wl_display_get_registry");

    if (&_valid_display != wl_display)
        return nullptr;
    return &_valid_registry;
}

int wl_registry_add_listener(struct wl_registry *wl_registry, const struct wl_registry_listener *listener, void *data)
{
    callAPI("wl_registry_add_listener");

    if (&_valid_registry != wl_registry)
        return 0;

    _listener = listener;
    _data     = data;

    return 1;
}

int wl_display_dispatch(struct wl_display *display)
{
    callAPI("wl_display_dispatch");

    if (&_valid_display != display)
        return 0;

    _listener->global(_data, &_valid_registry, 0, "wl_compositor", 0);
    _listener->global(_data, &_valid_registry, 0, "wl_shell", 0);
    _listener->global(_data, &_valid_registry, 0, "wl_webos_shell", 0);
    _listener->global(_data, &_valid_registry, 0, "wl_webos_surface_group_compositor", 0);
    _listener->global(_data, &_valid_registry, 0, "wl_webos_foreign", 0);

    return 1;
}

void wl_webos_shell_destroy(struct wl_webos_shell *wl_webos_shell)
{
    callAPI("wl_webos_shell_destroy");
}

void wl_shell_destroy(struct wl_shell *wl_shell)
{
    callAPI("wl_shell_destroy");
}

void wl_compositor_destroy(struct wl_compositor *wl_compositor)
{
    callAPI("wl_compositor_destroy");
}

void wl_display_disconnect(struct wl_display *display)
{
    callAPI("wl_display_disconnect");
}

int wl_display_flush(struct wl_display *display)
{
    callAPI("wl_display_flush");

    if (&_valid_display != display)
        return 0;

    return 1;
}

void wl_webos_surface_group_compositor_destroy(struct wl_webos_surface_group_compositor *wl_webos_surface_group_compositor)
{
    callAPI("wl_webos_surface_group_compositor_destroy");
}

struct wl_region *wl_compositor_create_region(struct wl_compositor *wl_compositor)
{
    callAPI("wl_compositor_create_region");

    if (&_valid_compositor != wl_compositor)
        return nullptr;

    return &_valid_region;
}

void wl_region_add(struct wl_region *wl_region, int32_t x, int32_t y, int32_t width, int32_t height)
{
    callAPI("wl_region_add");
}

void wl_region_destroy(struct wl_region *wl_region)
{
    callAPI("wl_region_destroy");
}

void wl_shell_surface_pong(struct wl_shell_surface *wl_shell_surface, uint32_t serial)
{
    callAPI("wl_shell_surface_pong");
}

struct wl_surface *wl_compositor_create_surface(struct wl_compositor *wl_compositor)
{
    callAPI("wl_compositor_create_surface");

    if (&_valid_compositor != wl_compositor)
        return nullptr;

    return &_valid_surface;
}

struct wl_shell_surface *wl_shell_get_shell_surface(struct wl_shell *wl_shell, struct wl_surface *surface)
{
    callAPI("wl_shell_get_shell_surface");

    if (&_valid_shell != wl_shell || &_valid_surface != surface)
        return nullptr;

    return &_valid_shell_surface;
}

int wl_shell_surface_add_listener(struct wl_shell_surface *wl_shell_surface, const struct wl_shell_surface_listener *listener, void *data)
{
    callAPI("wl_shell_surface_add_listener");

    if (&_valid_shell_surface != wl_shell_surface)
        return 0;

    return 1;
}

void wl_shell_surface_set_title(struct wl_shell_surface *wl_shell_surface, const char *title)
{
    callAPI("wl_shell_surface_set_title");
}

struct wl_webos_shell_surface *wl_webos_shell_get_shell_surface(struct wl_webos_shell *wl_webos_shell, struct wl_surface *surface)
{
    callAPI("wl_webos_shell_get_shell_surface");

    if (&_valid_webos_shell != wl_webos_shell || &_valid_surface != surface)
        return nullptr;

    return &_valid_webos_shell_surface;
}

void wl_webos_shell_surface_set_property(struct wl_webos_shell_surface *wl_webos_shell_surface, const char *name, const char *value)
{
    callAPI("wl_webos_shell_surface_set_property");
}

struct wl_egl_window *wl_egl_window_create(struct wl_surface *surface, int width, int height)
{
    callAPI("wl_egl_window_create");

    if (&_valid_surface != surface)
        return nullptr;

    return &_valid_egl_window;
}

void wl_egl_window_destroy(struct wl_egl_window *egl_window)
{
    callAPI("wl_egl_window_destroy");
}

void wl_webos_shell_surface_destroy(struct wl_webos_shell_surface *wl_webos_shell_surface)
{
    callAPI("wl_webos_shell_surface_destroy");
}

void wl_shell_surface_destroy(struct wl_shell_surface *wl_shell_surface)
{
    callAPI("wl_shell_surface_destroy");
}

void wl_surface_attach(struct wl_surface *wl_surface, struct wl_buffer *buffer, int32_t x, int32_t y)
{
    callAPI("wl_surface_attach");
}

void wl_surface_damage(struct wl_surface *wl_surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
    callAPI("wl_surface_damage");
}

void wl_surface_commit(struct wl_surface *wl_surface)
{
    callAPI("wl_surface_commit");
}

void wl_surface_destroy(struct wl_surface *wl_surface)
{
    callAPI("wl_surface_destroy");
}

struct wl_webos_surface_group *wl_webos_surface_group_compositor_get_surface_group(struct wl_webos_surface_group_compositor *wl_webos_surface_group_compositor, const char *name)
{
    callAPI("wl_webos_surface_group_compositor_get_surface_group");

    if (&_valid_webos_surface_group_compositor != wl_webos_surface_group_compositor)
        return nullptr;

    return &_valid_webos_surface_group;
}

void wl_webos_surface_group_attach(struct wl_webos_surface_group *wl_webos_surface_group, struct wl_surface *surface, const char *layer_name)
{
    callAPI("wl_webos_surface_group_attach");
}

void wl_webos_surface_group_detach(struct wl_webos_surface_group *wl_webos_surface_group, struct wl_surface *surface)
{
    callAPI("wl_webos_surface_group_detach");
}

void wl_webos_surface_group_destroy(struct wl_webos_surface_group *wl_webos_surface_group)
{
    callAPI("wl_webos_surface_group_destroy");
}

void wl_webos_foreign_destroy(struct wl_webos_foreign *wl_webos_foreign)
{
    callAPI("wl_webos_foreign_destroy");
}

struct wl_webos_exported *wl_webos_foreign_export_element(struct wl_webos_foreign *wl_webos_foreign, struct wl_surface *surface, uint32_t exported_type)
{
    callAPI("wl_webos_foreign_export_element");

    if (&_valid_webos_foreign != wl_webos_foreign)
        return nullptr;

    return &_valid_webos_exported;
}

struct wl_webos_imported *wl_webos_foreign_import_element(struct wl_webos_foreign *wl_webos_foreign, const char *window_id, uint32_t exported_type)
{
    callAPI("wl_webos_foreign_import_element");

    if (&_valid_webos_foreign != wl_webos_foreign)
        return nullptr;

    return &_valid_webos_imported;
}

int wl_webos_exported_add_listener(struct wl_webos_exported *wl_webos_exported, const struct wl_webos_exported_listener *listener, void *data)
{
    callAPI("wl_webos_exported_add_listener");

    if (&_valid_webos_exported != wl_webos_exported)
        return 0;

    return 1;
}

void wl_webos_exported_destroy(struct wl_webos_exported *wl_webos_exported)
{
    callAPI("wl_webos_exported_destroy");
}

void wl_webos_exported_set_exported_window(struct wl_webos_exported *wl_webos_exported, struct wl_region *source_region, struct wl_region *destination_region)
{
    callAPI("wl_webos_exported_set_exported_window");
}

int wl_webos_imported_add_listener(struct wl_webos_imported *wl_webos_imported, const struct wl_webos_imported_listener *listener, void *data)
{
    callAPI("wl_webos_imported_add_listener");

    if (&_valid_webos_imported != wl_webos_imported)
        return 0;

    return 1;
}

void wl_webos_imported_set_surface_alignment(struct wl_webos_imported *wl_webos_imported, uint32_t format)
{
    callAPI("wl_webos_imported_set_surface_alignment");
}

void wl_webos_imported_destroy(struct wl_webos_imported *wl_webos_imported)
{
    callAPI("wl_webos_imported_destroy");
}

void wl_webos_imported_attach_punchthrough(struct wl_webos_imported *wl_webos_imported)
{
    callAPI("wl_webos_imported_attach_punchthrough");
}

void wl_webos_imported_detach_punchthrough(struct wl_webos_imported *wl_webos_imported)
{
    callAPI("wl_webos_imported_detach_punchthrough");
}

void wl_webos_imported_attach_surface(struct wl_webos_imported *wl_webos_imported, struct wl_surface *surface)
{
    callAPI("wl_webos_imported_attach_surface");
}

void wl_webos_imported_detach_surface(struct wl_webos_imported *wl_webos_imported, struct wl_surface *surface)
{
    callAPI("wl_webos_imported_detach_surface");
}
