#include <gtest/gtest.h>
#include <iostream>
#include "wayland_importer.h"
#include "api_call_checker.h"

class WaylandImporterTest1 : public ::testing::Test
{
protected:
    Wayland::Importer importer;
};

TEST_F(WaylandImporterTest1, InitializeAndFinalize)
{
    //Arrange
    clearCallLog();
    bool expected = true;
    struct wl_registry *registry;
    struct wl_webos_foreign *foreign = (struct wl_webos_foreign *)wl_registry_bind(registry, 0, &wl_webos_foreign_interface, 0);

    const char *windowID  = "TEST_WINDOWID";
    uint32_t exportedType = WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT;

    //Act
    bool actual = importer.initialize(foreign, windowID, exportedType);

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    importer.finalize();
    actual = isAPICalled("wl_webos_imported_destroy");

    //Assert
    EXPECT_EQ(expected, actual);
}

class WaylandImporterTest2 : public ::testing::Test
{
protected:
    void SetUp(void)
    {
        const char *windowID  = "TEST_WINDOWID";
        uint32_t exportedType = WL_WEBOS_FOREIGN_WEBOS_EXPORTED_TYPE_VIDEO_OBJECT;

        foreign = (struct wl_webos_foreign *)wl_registry_bind(registry, 0, &wl_webos_foreign_interface, 0);
        importer.initialize(foreign, windowID, exportedType);
    }

    void TearDown(void)
    {
        importer.finalize();
        wl_webos_foreign_destroy(foreign);
    }

    Wayland::Importer importer;
    struct wl_registry *registry;
    struct wl_webos_foreign *foreign;
};

TEST_F(WaylandImporterTest2, GetWebosImported)
{
    //Arrange

    //Act
    struct wl_webos_imported *actual = importer.getWebosImported();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(WaylandImporterTest2, Punchthrough)
{
    //Arrange
    clearCallLog();
    bool exported = true;

    //Act
    importer.attachPunchThrough();
    bool actual = isAPICalled("wl_webos_imported_attach_punchthrough");

    //Assert
    EXPECT_EQ(exported, actual);

    //Act
    importer.detachPunchThrough();
    actual = isAPICalled("wl_webos_imported_detach_punchthrough");

    //Assert
    EXPECT_EQ(exported, actual);
}

TEST_F(WaylandImporterTest2, Surface)
{
    //Arrange
    clearCallLog();
    struct wl_compositor *compositor = (struct wl_compositor *)wl_registry_bind(registry, 0, &wl_compositor_interface, 0);
    struct wl_surface *surface       = wl_compositor_create_surface(compositor);
    bool exported                    = true;

    //Act
    importer.attachSurface(surface);
    bool actual = isAPICalled("wl_webos_imported_attach_surface");

    //Assert
    EXPECT_EQ(exported, actual);

    //Act
    importer.detachSurface(surface);
    actual = isAPICalled("wl_webos_imported_detach_surface");

    //Assert
    EXPECT_EQ(exported, actual);

    wl_surface_destroy(surface);
}
