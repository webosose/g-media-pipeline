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

#include <gtest/gtest.h>
#include <iostream>
#include "wayland_foreign.h"
#include "api_call_checker.h"

class WaylandForeignTest1 : public ::testing::Test
{
protected:
    Wayland::Foreign foreign;
};

TEST_F(WaylandForeignTest1, InitializeAndFinalize)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    bool actual = foreign.initialize();

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    foreign.finalize();
    actual = isAPICalled("wl_webos_foreign_destroy");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = isAPICalled("wl_webos_shell_destroy");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = isAPICalled("wl_shell_destroy");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = isAPICalled("wl_compositor_destroy");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = isAPICalled("wl_display_flush");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = isAPICalled("wl_display_disconnect");

    //Assert
    EXPECT_EQ(expected, actual);
}

class WaylandForeignTest2 : public ::testing::Test
{
protected:
    void SetUp(void)
    {
        foreign.initialize();
    }

    void TearDown(void)
    {
        foreign.finalize();
    }

    Wayland::Foreign foreign;
};

TEST_F(WaylandForeignTest2, GetDisplay)
{
    //Arrange

    //Act
    struct wl_display *actual = foreign.getDisplay();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(WaylandForeignTest2, GetCompositor)
{
    //Arrange

    //Act
    struct wl_compositor *actual = foreign.getCompositor();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(WaylandForeignTest2, GetShell)
{
    //Arrange

    //Act
    struct wl_shell *actual = foreign.getShell();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(WaylandForeignTest2, GetWebosShell)
{
    //Arrange

    //Act
    struct wl_webos_shell *actual = foreign.getWebosShell();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(WaylandForeignTest2, GetWebosForeign)
{
    //Arrange

    //Act
    struct wl_webos_foreign *actual = foreign.getWebosForeign();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(WaylandForeignTest2, Region)
{
    //Arrange
    clearCallLog();
    int32_t x         = 0;
    int32_t y         = 0;
    int32_t width     = 512;
    int32_t height    = 512;
    bool expectedCall = true;

    //Act
    struct wl_region *actual = foreign.createRegion(x, y, width, height);

    //Assert
    EXPECT_TRUE(nullptr != actual);

    //Act
    foreign.destroyRegion(actual);
    bool actualCall = isAPICalled("wl_region_destroy");

    //Assert
    EXPECT_EQ(expectedCall, actualCall);
}

TEST_F(WaylandForeignTest2, Flush)
{
    //Arrange
    bool expected = true;

    //Act
    bool actual = foreign.flush();

    //Assert
    EXPECT_EQ(expected, actual);
}
