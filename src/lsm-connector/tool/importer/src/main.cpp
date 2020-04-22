#include <iostream>
#include <cstring>
#include <unistd.h>
#include <assert.h>
#include <lsm_connector.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <glib.h>
#include <glib-unix.h>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#define VERTEX_ARRAY (0)

static guint signal_watch_intr_id;
static constexpr char const *waylandDisplayHandleContextType =
	"GstWaylandDisplayHandleContextType";

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
    EGLSurface eglSurface;
    unsigned int width;
    unsigned int height;
} WaylandEGLSurface;

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
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE};
    ctx_attrib_list[1] = 2; //Client Version.

    eglData->eglContext = eglCreateContext(eglData->eglDisplay, eglData->eglConfig[eglData->configSelect], EGL_NO_CONTEXT, ctx_attrib_list);

    eglSwapInterval(eglData->eglDisplay, 1);

    surface->native     = wl_egl_window_create(surface->wlSurface, surface->width, surface->height);
    surface->eglSurface = eglCreateWindowSurface(eglData->eglDisplay, eglData->currentEglConfig, (EGLNativeWindowType)surface->native, NULL);

    eglMakeCurrent(eglData->eglDisplay, surface->eglSurface, surface->eglSurface, eglData->eglContext);

    const char *pszFragShader = "\
        void main (void)\
        {\
            gl_FragColor = vec4(1.0, 0.0, 0.0, 0.5);\
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

    glViewport(0, 0, surface->width, surface->height);

    glEnableVertexAttribArray(VERTEX_ARRAY);
    glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, glData->vertexStride, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    eglSwapBuffers(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_READ));

    return true;
}

bool testPunchThrough(LSM::Connector &connector)
{
    bool result = false;

    result = connector.attachPunchThrough();
    if (!result) {
        std::cout << "connector.attachPunchThrough error" << std::endl;
        return false;
    }

    for (int i = 0; i < 10; i++) {
        std::cout << "Do something" << std::endl;
        sleep(1);
    }

    result = connector.detachPunchThrough();
    if (!result) {
        std::cout << "connector.detachPunchThrough error" << std::endl;
        return false;
    }

    return true;
}

bool testSurface(LSM::Connector &connector)
{
    WaylandEGLSurface surface;
    EGLData eglData;
    GLData glData;
    bool result = false;

    surface.wlSurface = connector.getSurface();
    surface.width     = 100;
    surface.height    = 100;
    renderInitialize(connector.getDisplay(), &eglData, &surface, &glData);

    result = connector.attachSurface();
    if (!result) {
        std::cout << "connector.attachSurface error" << std::endl;
        return false;
    }

    for (int i = 0; i < 10; i++) {
        rendering(&glData, &surface);
        std::cout << "Do rendering" << std::endl;
        sleep(1);
    }

    result = connector.detachSurface();
    if (!result) {
        std::cout << "connector.detachSurface error" << std::endl;
        return false;
    }

    return true;
}

static gboolean intr_handler(gpointer user_data)
{
  GMainLoop *loop = static_cast<GMainLoop *>(user_data);
  g_main_loop_quit(loop);
  /* remove signal handler */
  signal_watch_intr_id = 0;
  return G_SOURCE_REMOVE;
}

gboolean handleBusCallback (GstBus * bus,
                                             GstMessage * msg,
                                             gpointer data)
{
  GMainLoop *loop = static_cast<GMainLoop*>(data);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err = nullptr;
      gchar *dbg_info = nullptr;

      gst_message_parse_error(msg, &err, &dbg_info);
      std::cout << "ERROR from element " << GST_OBJECT_NAME(msg->src) << ", " << err->message << std::endl;
      std::cout << "Debugging info: " << ((dbg_info) ? dbg_info : "none") << std::endl;

      g_free(dbg_info);
      g_error_free(err);
  	  g_main_loop_quit(loop);
      break;
    }
    case GST_MESSAGE_ASYNC_DONE:
      std::cout << "load Completed" << std::endl;
      break;
	case GST_MESSAGE_EOS:
	  std::cout << "EOS" << std::endl;
	  g_main_loop_quit(loop);
	  break;
    default:
      break;
  }

  return TRUE;
}

GstBusSyncReply handleSyncBusCallback(GstBus * bus,
                                                       GstMessage * msg,
                                                       gpointer data)
{
  // This handler will be invoked synchronously, don't process any application
  // message handling here
  LSM::Connector *connector = static_cast<LSM::Connector*>(data);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_NEED_CONTEXT:{
      const gchar *type = nullptr;
      gst_message_parse_context_type(msg, &type);
      if (g_strcmp0 (type, waylandDisplayHandleContextType) != 0) {
        break;
      }
	  std::cout << "Set a wayland display handle :" << connector->getDisplay() << std::endl;\
      GstContext *context = gst_context_new(waylandDisplayHandleContextType, TRUE);
      gst_structure_set(gst_context_writable_structure (context),
          "handle", G_TYPE_POINTER, connector->getDisplay(), nullptr);
      gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
      goto drop;
    }
    case GST_MESSAGE_ELEMENT:{
      if (!gst_is_video_overlay_prepare_window_handle_message(msg)) {
	  	break;
      }
	  std::cout << "Set a wayland window handle :" << connector->getSurface() << std::endl;
      gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY (msg->src),
          (guintptr)(connector->getSurface()));
      goto drop;
    }
    default:
      break;
  }

  return GST_BUS_PASS;

drop:
  gst_message_unref(msg);
  return GST_BUS_DROP;
}

static void pad_added_handler (GstElement *src, GstPad *new_pad, GstElement *sink)
{
	GstPad *sink_pad = gst_element_get_static_pad (sink, "sink"); // video(h264parse) parser
	
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
    std::cout << "Received new pad '" << GST_PAD_NAME (new_pad)
		<< "' from '" << GST_ELEMENT_NAME (src) << "'" << std::endl;

    new_pad_caps = gst_pad_get_current_caps (new_pad);
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (new_pad_struct);

	if (g_str_has_prefix (new_pad_type, "video/x-h264")) {
		ret = gst_pad_link (new_pad, sink_pad);
		if (GST_PAD_LINK_FAILED (ret))
			std::cout << "pad link failed!" << std::endl;
		else
			std::cout << "pad link succeeded!" << std::endl;
	}
	else
		std::cout << "type : '" << new_pad_type << "', is ignored" << std::endl;

exit:
    if (new_pad_caps != NULL) {
	    gst_caps_unref (new_pad_caps);
	    gst_object_unref (sink_pad);
    }
}


bool testSurfaceWithGST(LSM::Connector &connector, int display_mode)
{
	GstElement *pipeline, *source, *demuxer, *parser, *decoder, *sink;
	GMainLoop *loop = nullptr;

	bool usePlaybin = true;
	bool result = false;

    result = connector.attachSurface();

    if (!result) {
        std::cout << "connector.attachSurface error" << std::endl;
        return false;
    }

	if (usePlaybin)
		pipeline = gst_element_factory_make("playbin", "playbin");
	else
		pipeline = gst_pipeline_new(nullptr);

	if (!pipeline) {
		std::cout << "Cannot create pipeline!" << std::endl;
		return false;
	}

	auto *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message", G_CALLBACK(handleBusCallback), loop);
	gst_bus_set_sync_handler(bus, handleSyncBusCallback, &connector, nullptr);
	gst_object_unref(bus);

	if (!usePlaybin) {
		source = gst_element_factory_make("souphttpsrc", nullptr);
		if (!source) {
			std::cout << "failed to create source element" << std::endl;
	  	    return false;
		}
		g_object_set(G_OBJECT(source), "location", "http://10.178.84.247/mp4/bunny.mp4", nullptr);
		demuxer = gst_element_factory_make("qtdemux", nullptr);
		if (!demuxer) {
			std::cout << "failed to create demuxer element" << std::endl;
			gst_object_unref(GST_OBJECT(pipeline));
	  	    return false;
		}

		parser = gst_element_factory_make("h264parse", nullptr);
		if (!parser) {
			std::cout << "failed to create parser element" << std::endl;
			gst_object_unref(GST_OBJECT(pipeline));
			return false;
		}

		decoder = gst_element_factory_make("mfxh264dec", nullptr);
		if (!decoder) {
			std::cout << "failed to create decoder element" << std::endl;
			gst_object_unref(GST_OBJECT(pipeline));
			return false;
		}
    }
#ifdef PLATFORM_QUALCOMM_SA8155
	sink = gst_element_factory_make("waylandsink", nullptr);
#else
	sink = gst_element_factory_make("mfxsink", nullptr);
#endif

	if (!sink) {
		std::cout << "failed to create sink element" << std::endl;
		gst_object_unref(GST_OBJECT(pipeline));
		return false;
	}

#ifndef PLATFORM_QUALCOMM_SA8155
	g_object_set(G_OBJECT(sink), "display", display_mode, nullptr); // 2:wayland, 3:egl	
	g_object_set(G_OBJECT(sink), "full-color-range", true, nullptr);
    g_object_set(G_OBJECT(sink), "fullscreen", false, nullptr);
#endif

	if (usePlaybin) {
		g_object_set(G_OBJECT(pipeline), "uri", "http://10.178.84.247/mp4/bunny.mp4",
			"video-sink", sink,
			NULL);
	}
    else {
		gst_bin_add_many(GST_BIN(pipeline), source, demuxer, parser, decoder, sink, nullptr);

		if (!gst_element_link(source, demuxer))
			std::cout << "source and demuxer link failed!" << std::endl;
		if (!gst_element_link_many(parser, decoder, sink, nullptr))
			std::cout << "parser, decoder, and sink link failed!" << std::endl;

		g_signal_connect (demuxer, "pad-added", G_CALLBACK (pad_added_handler), parser);
    }

	result = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (result == GST_STATE_CHANGE_FAILURE) {
		std::cout <<  "Unable to set the pipeline to the playing state" << std::endl;
		return exit;
	}

	loop = g_main_loop_new (NULL, FALSE);

    signal_watch_intr_id =
        g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, loop);

    g_main_loop_run (loop);

exit:
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(pipeline));

    if (signal_watch_intr_id > 0)
        g_source_remove (signal_watch_intr_id);

    g_main_loop_unref(loop);
    result = connector.detachSurface();

    if (!result) {
        std::cout << "connector.detachSurface error" << std::endl;
        return false;
    }

    return result;
}


enum DisplayMode { PunchThrough, Surface, SurfaceWAYLAND, SurfaceEGL };

int main(int argc, char *argv[])
{
    DisplayMode mode;

    if (argc != 3) {
        std::cout << "Please input with windowID and command" << std::endl;
        std::cout << "./importer $windowID PunchThrough" << std::endl;
        std::cout << "./importer $windowID Surface" << std::endl;
        std::cout << "./importer $windowID SurfaceWithGST" << std::endl;
        return -1;
    }

	gst_init(&argc, &argv);

    if (!strcmp(argv[2], "PunchThrough")) {
        mode = PunchThrough;
    } else if (!strcmp(argv[2], "Surface")) {
        mode = Surface;
    } else if (!strcmp(argv[2], "SurfaceWAYLAND")) {
        mode = SurfaceWAYLAND;
    } else if (!strcmp(argv[2], "SurfaceEGL")) {
        mode = SurfaceEGL;
    } else {
        std::cout << "Command is not supported" << std::endl;
        return -1;
    }

    LSM::Connector connector;
    const char *windowID = argv[1];
    bool result          = false;

    result = connector.registerID(windowID, nullptr);
    if (!result) {
        std::cout << "connector.registerID error : " << windowID << std::endl;
        return 0;
    } else {
        std::cout << "connector.registerID sucess : " << windowID << std::endl;
    }

    switch (mode) {
        case PunchThrough:
            std::cout << "testPunchThrough" << std::endl;
            result = testPunchThrough(connector);
            break;
        case Surface:
            std::cout << "testSurface" << std::endl;
            result = testSurface(connector);
            break;
        case SurfaceWAYLAND:
            std::cout << "testSurfaceWAYLAND" << std::endl;
            result = testSurfaceWithGST(connector, 2); // 2:wayland
            break;
		case SurfaceEGL:
			std::cout << "testSurfaceEGL" << std::endl;
			result = testSurfaceWithGST(connector, 3); // 3:egl
			break;
    }
    if (!result) {
        std::cout << "error occured..." << std::endl;
        return -1;
    }

    result = connector.unregisterID();
    if (!result) {
        std::cout << "connector.unregisterID error : " << windowID << std::endl;
        return 0;
    } else {
        std::cout << "connector.unregisterID sucess : " << windowID << std::endl;
    }

    return 0;
}
