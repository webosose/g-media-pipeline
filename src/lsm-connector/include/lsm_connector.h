/******************************************************************************
 *   LCD TV LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 *   Copyright(c) 2018-2019 by LG Electronics Inc.
 *
 *   All rights reserved. No part of this work may be reproduced, stored in a
 *   retrieval system, or transmitted by any means without prior written
 *   permission of LG Electronics Inc.
 ******************************************************************************/

/** @file lsm_connector.h
 *
 *  @author     Dongheon Kim (dongheon.kim@lge.com)
 *  @version    1.0
 *  @date       2018.09.07
 *  @note
 *  @see
 */

#ifndef _LSM_CONNECTOR_H_
#define _LSM_CONNECTOR_H_

#include <glib.h>
#include <memory>

struct wl_display;
struct wl_surface;

namespace Wayland
{
class Foreign;
class Importer;
class Surface;
}

namespace LSM
{

class Connector
{
public:
    Connector(void);
    ~Connector(void);

    bool registerID(const char *windowID, const char *pipelineID);
    bool unregisterID(void);

    bool attachPunchThrough(void);
    bool detachPunchThrough(void);

    bool attachSurface(void);
    bool detachSurface(void);

    struct wl_display *getDisplay(void);
    struct wl_surface *getSurface(void);

    void getVideoSize(gint &width, gint &height);
    void setVideoSize(gint width, gint height);

private:
    //Disallow copy and assign
    Connector(const Connector &);
    void operator=(const Connector &);

    gint video_width = 0;
    gint video_height = 0;

    std::shared_ptr<Wayland::Foreign> foreign;
    std::shared_ptr<Wayland::Surface> surface;
    std::shared_ptr<Wayland::Importer> importer;

    bool isRegistered;
};

} //namespace LSM

#endif //_LSM_CONNECTOR_H_
