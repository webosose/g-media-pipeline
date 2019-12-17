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
#include "wayland_surface.h"
#include "api_call_checker.h"

class WaylandSurfaceTest : public ::testing::Test
{
protected:
    void SetUp(void)
    {
        compositor = (struct wl_compositor *)wl_registry_bind(registry, 0, &wl_compositor_interface, 0);
    }

    Wayland::Surface surface;
    struct wl_compositor *compositor;
    struct wl_registry *registry;
};

TEST_F(WaylandSurfaceTest, InitializeAndFinalize)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    bool actual = surface.initialize(compositor);

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    surface.finalize();
    actual = isAPICalled("wl_surface_destroy");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(WaylandSurfaceTest, GetSurface)
{
    //Arrange
    clearCallLog();
    surface.initialize(compositor);

    //Act
    struct wl_surface *actual = surface.getSurface();

    //Assert
    EXPECT_TRUE(nullptr != actual);

    surface.finalize();
}
