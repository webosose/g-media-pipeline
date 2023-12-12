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

#include <iostream>
#include <unistd.h>
#include <assert.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "wayland_foreign.h"
#include "wayland_exporter.h"

#define VERTEX_ARRAY (0)

typedef struct {
    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLConfig *eglConfig;
    int configSelect;
    EGLConfig currentEglConfig;
} EGLData;

typedef struct {
    GLuint vbo;
    GLuint fragShader;
    GLuint vertShader;
    GLuint programObject;
    GLuint texture;
    unsigned int vertexStride;
} GLData;

typedef struct {
    struct wl_egl_window *native;
    struct wl_surface *wlSurface;
    struct wl_shell_surface *wlShellSurface;
    struct wl_webos_shell_surface *webosShellSurface;
    EGLSurface eglSurface;
    unsigned int width;
    unsigned int height;
} WaylandEGLSurface;

static void handlePing(void *data, struct wl_shell_surface *shellSurface, uint32_t serial)
{
    wl_shell_surface_pong(shellSurface, serial);
}

static void handleConfigure(void *data, struct wl_shell_surface *shellSurface, uint32_t edges, int32_t width, int32_t height)
{
}

static void handlePopupDone(void *data, struct wl_shell_surface *shellSurface)
{
}

static const struct wl_shell_surface_listener shellSurfaceListener = {
    handlePing,
    handleConfigure,
    handlePopupDone};

bool renderInitialize(struct wl_display *display, EGLData *eglData, WaylandEGLSurface *surface, GLData *glData)
{
    int configs;

    int want_red   = 8;
    int want_green = 8;
    int want_blue  = 8;
    int want_alpha = 8;

    EGLint major_version;
    EGLint minor_version;

    eglData->eglDisplay = eglGetDisplay((EGLNativeDisplayType)display);
    eglInitialize(eglData->eglDisplay, &major_version, &minor_version);
    eglBindAPI(EGL_OPENGL_ES_API);
    eglGetConfigs(eglData->eglDisplay, NULL, 0, &configs);

    eglData->eglConfig = (EGLConfig *)alloca(configs * sizeof(EGLConfig));
    {
        const int NUM_ATTRIBS = 21;
        EGLint *attr          = (EGLint *)malloc(NUM_ATTRIBS * sizeof(EGLint));
        int i                 = 0;

        attr[i++] = EGL_RED_SIZE;
        attr[i++] = want_red;
        attr[i++] = EGL_GREEN_SIZE;
        attr[i++] = want_green;
        attr[i++] = EGL_BLUE_SIZE;
        attr[i++] = want_blue;
        attr[i++] = EGL_ALPHA_SIZE;
        attr[i++] = want_alpha;
        attr[i++] = EGL_DEPTH_SIZE;
        attr[i++] = 24;
        attr[i++] = EGL_STENCIL_SIZE;
        attr[i++] = 0;
        attr[i++] = EGL_SURFACE_TYPE;
        attr[i++] = EGL_WINDOW_BIT;
        attr[i++] = EGL_RENDERABLE_TYPE;
        attr[i++] = EGL_OPENGL_ES2_BIT;

        //multi sample
        attr[i++] = EGL_SAMPLE_BUFFERS;
        attr[i++] = 1;
        attr[i++] = EGL_SAMPLES;
        attr[i++] = 4;

        attr[i++] = EGL_NONE;

        assert(i <= NUM_ATTRIBS);

        if (!eglChooseConfig(eglData->eglDisplay, attr, eglData->eglConfig, configs, &configs) || (configs == 0)) {
            std::cout << "('w')/ eglChooseConfig() failed." << std::endl;
            return false;
        }

        free(attr);
    }

    for (eglData->configSelect = 0; eglData->configSelect < configs; eglData->configSelect++) {
        EGLint red_size, green_size, blue_size, alpha_size, depth_size;

        eglGetConfigAttrib(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_RED_SIZE, &red_size);
        eglGetConfigAttrib(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_GREEN_SIZE, &green_size);
        eglGetConfigAttrib(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_BLUE_SIZE, &blue_size);
        eglGetConfigAttrib(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_ALPHA_SIZE, &alpha_size);
        eglGetConfigAttrib(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_DEPTH_SIZE, &depth_size);

        if ((red_size == want_red) && (green_size == want_green) && (blue_size == want_blue) && (alpha_size == want_alpha)) {
            break;
        }
    }

    if (eglData->configSelect == configs) {
        std::cout << "No suitable configs found." << std::endl;
        return false;
    }

    eglData->currentEglConfig = eglData->eglConfig[eglData->configSelect];

    EGLint ctx_attrib_list[3] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE};
    ctx_attrib_list[1] = 3; //Client Version.

    eglData->eglContext = eglCreateContext(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_NO_CONTEXT, ctx_attrib_list);

    eglSwapInterval(eglData->eglDisplay, 1);

    surface->native     = wl_egl_window_create(surface->wlSurface, surface->width, surface->height);
    surface->eglSurface = eglCreateWindowSurface(eglData->eglDisplay, eglData->currentEglConfig, (EGLNativeWindowType)(surface->native), NULL);

    eglMakeCurrent(eglData->eglDisplay, surface->eglSurface, surface->eglSurface, eglData->eglContext);

    const char *pszFragShader = "\
        void main (void)\
        {\
            gl_FragColor = vec4(0.0, 1.0, 0.0, 0.8);\
        }";

    const char *pszVertShader = "\
        attribute highp vec4    myVertex;\
        void main(void)\
        {\
            gl_Position = myVertex;\
        }";

    GLint bShaderCompiled;

    glData->fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(glData->fragShader, 1, (const char **)&pszFragShader, NULL);
    glCompileShader(glData->fragShader);
    glGetShaderiv(glData->fragShader, GL_COMPILE_STATUS, &bShaderCompiled);
    if (!bShaderCompiled) {
        std::cout << "Frag Shader error" << std::endl;
        return false;
    }

    glData->vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(glData->vertShader, 1, (const char **)&pszVertShader, NULL);
    glCompileShader(glData->vertShader);
    glGetShaderiv(glData->vertShader, GL_COMPILE_STATUS, &bShaderCompiled);
    if (!bShaderCompiled) {
        std::cout << "Vertex Shader error" << std::endl;
        return false;
    }

    glData->programObject = glCreateProgram();

    glAttachShader(glData->programObject, glData->fragShader);
    glAttachShader(glData->programObject, glData->vertShader);

    glBindAttribLocation(glData->programObject, VERTEX_ARRAY, "myVertex");

    glLinkProgram(glData->programObject);

    GLint bLinked;
    glGetProgramiv(glData->programObject, GL_LINK_STATUS, &bLinked);

    if (!bLinked) {
        printf("('w')/ Program Link error\n");
        return 0;
    }

    glUseProgram(glData->programObject);

    GLfloat afVertices[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
    };

    glGenBuffers(1, &glData->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, glData->vbo);
    glData->vertexStride = 3 * sizeof(GLfloat);
    glBufferData(GL_ARRAY_BUFFER, 6 * glData->vertexStride, afVertices, GL_STATIC_DRAW);

    return true;
}

bool rendering(GLData *glData, WaylandEGLSurface *surface)
{
    //Update
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(100, 100, 600, 200);

    glEnableVertexAttribArray(VERTEX_ARRAY);
    glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, glData->vertexStride, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_READ));

    return true;
}

int main(int argc, char *argv[])
{
    if (argc != 1 && argc != 5) {
        std::cout << "Please input correct agruments! (nothing or x y w h)" << std::endl;
        return -1;
    }

    Wayland::Foreign foreign;
    Wayland::Exporter exporter;

    WaylandEGLSurface surface;
    EGLData eglData;
    GLData glData;

    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int exportWidth  = 1920;
    unsigned int exportHeight = 1080;
    uint32_t exported_type    = WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT;

    if (argc == 5) {
      x = std::stoi(std::string(argv[1]));
      y = std::stoi(std::string(argv[2]));
      exportWidth = std::stoi(std::string(argv[3]));
      exportHeight = std::stoi(std::string(argv[4]));
    }

    foreign.initialize();

    struct wl_compositor *compositor = foreign.getCompositor();
    if(NULL == compositor)
    {
        std::cout << "compositor instance is NULL, probably because HDMI display is not connected." << std::endl;
        std::cout << "Please rerun the app, after connecting HDMI display." << std::endl;
        foreign.finalize();
        return 0;
    }

    surface.wlSurface      = wl_compositor_create_surface(compositor);
    surface.wlShellSurface = wl_shell_get_shell_surface(foreign.getShell(), surface.wlSurface);
    wl_shell_surface_add_listener(surface.wlShellSurface, &shellSurfaceListener, NULL);
//    surface.webosShellSurface = wl_webos_shell_get_shell_surface(foreign.getWebosShell(), surface.wlSurface);
//    wl_webos_shell_surface_set_property(surface.webosShellSurface, "_WEBOS_WINDOW_TYPE", "_WEBOS_WINDOW_TYPE_OVERLAY");

    surface.width  = 1920;
    surface.height = 1080;

    struct wl_region *region_src  = foreign.createRegion(0, 0, 1920, 1080); // don't care
    struct wl_region *region_dst  = foreign.createRegion(x, y, exportWidth, exportHeight);

    bool result = exporter.initialize(foreign.getDisplay(), foreign.getWebosForeign(), surface.wlSurface, exported_type);
    if (result == false) {
        std::cout << "exporter.initialize error" << std::endl;
        return 0;
    } else {
        std::cout << "exporter.initialize success" << std::endl;
    }

    exporter.setRegion(region_src, region_dst);
    foreign.flush();

    std::cout << "exported window ID is : " << exporter.getWindowID() << std::endl;

    renderInitialize(foreign.getDisplay(), &eglData, &surface, &glData);

    while (1) {
        rendering(&glData, &surface);
        eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_READ));
        sleep(1);
    }

    return 0;
}
