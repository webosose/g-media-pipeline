set(BIN_NAME lsm_connector_exporter)

include(FindPkgConfig)

pkg_check_modules(EGL REQUIRED egl)
webos_add_compiler_flags(ALL ${EGL_CFLAGS})

pkg_check_modules(GLESV2 REQUIRED glesv2)
webos_add_compiler_flags(ALL ${GLESV2_CFLAGS})

pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
webos_add_compiler_flags(ALL ${WAYLAND_EGL_CFLAGS})

pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
webos_add_compiler_flags(ALL ${WAYLAND_CLIENT_CFLAGS})

pkg_check_modules(WAYLAND_WEBOS_CLIENT REQUIRED wayland-webos-client)
webos_add_compiler_flags(ALL ${WAYLAND_WEBOS_CLIENT_CFLAGS})

include_directories(src)
include_directories(../../src)

set(SRC_LIST
    src/main.cpp
    ../../src/wayland_foreign.cpp
    src/wayland_exporter.cpp
)

add_executable(${BIN_NAME} ${SRC_LIST})
target_link_libraries(${BIN_NAME}
    ${EGL_LDFLAGS}
    ${GLESV2_LDFLAGS}
    ${WAYLAND_EGL_LDFLAGS}
    ${WAYLAND_CLIENT_LDFLAGS}
    ${WAYLAND_WEBOS_CLIENT_LDFLAGS})

install(TARGETS ${BIN_NAME} DESTINATION sbin)
