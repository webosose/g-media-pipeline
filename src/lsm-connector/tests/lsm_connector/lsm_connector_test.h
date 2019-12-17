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
#include "lsm_connector.h"
#include "api_call_checker.h"

class LSMConnectorTest1 : public ::testing::Test
{
protected:
    LSM::Connector connector;
};

TEST_F(LSMConnectorTest1, RegisterAndUnregister)
{
    //Arrange
    bool expected          = true;
    const char *windowID   = "TEST_WINDOW_ID";
    const char *pipelineID = "TEST_PIPELINE_ID";

    //Act
    bool actual = connector.registerID(windowID, pipelineID);

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    actual = connector.unregisterID();

    //Assert
    EXPECT_EQ(expected, actual);
}

class LSMConnectorTest2 : public ::testing::Test
{
protected:
    void SetUp(void)
    {
        const char *windowID   = "TEST_WINDOW_ID";
        const char *pipelineID = "TEST_PIPELINE_ID";
        connector.registerID(windowID, pipelineID);
    }

    void TearDown(void)
    {
        connector.unregisterID();
    }

    LSM::Connector connector;
};

TEST_F(LSMConnectorTest2, Punchthrough)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    connector.attachPunchThrough();
    bool actual = isAPICalled("Wayland::Importer::attachPunchThrough");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    connector.detachPunchThrough();
    actual = isAPICalled("Wayland::Importer::detachPunchThrough");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(LSMConnectorTest2, Surface)
{
    //Arrange
    clearCallLog();
    bool expected = true;

    //Act
    connector.attachSurface();
    bool actual = isAPICalled("Wayland::Importer::attachSurface");

    //Assert
    EXPECT_EQ(expected, actual);

    //Act
    connector.detachSurface();
    actual = isAPICalled("Wayland::Importer::detachSurface");

    //Assert
    EXPECT_EQ(expected, actual);
}

TEST_F(LSMConnectorTest2, GetDisplay)
{
    //Arrange

    //Act
    struct wl_display *actual = connector.getDisplay();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}

TEST_F(LSMConnectorTest2, GetSurface)
{
    //Arrange

    //Act
    struct wl_surface *actual = connector.getSurface();

    //Assert
    EXPECT_TRUE(nullptr != actual);
}
