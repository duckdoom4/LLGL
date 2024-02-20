/*
 * LinuxDisplay.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include "LinuxDisplay.h"
#include "../../Core/CoreUtils.h"
#include <X11/extensions/Xrandr.h>


namespace LLGL
{


static std::vector<std::unique_ptr<LinuxDisplay>>   g_displayList;
static std::vector<Display*>                        g_displayRefList;
static Display*                                     g_primaryDisplay;

LinuxSharedX11DisplaySPtr LinuxSharedX11Display::GetShared()
{
    static LinuxSharedX11DisplaySPtr sharedX11Display = std::make_shared<LinuxSharedX11Display>();
    return sharedX11Display;
}

static bool UpdateDisplayList()
{
    LinuxSharedX11DisplaySPtr sharedX11Display = LinuxSharedX11Display::GetShared();

    const int screenCount = ScreenCount(sharedX11Display->GetNative());
    if (screenCount >= 0 && static_cast<std::size_t>(screenCount) != g_displayList.size())
    {
        g_displayList.resize(static_cast<std::size_t>(screenCount));
        for (int i = 0; i < screenCount; ++i)
        {
            g_displayList[i] = MakeUnique<LinuxDisplay>(sharedX11Display, i);
            if (i == DefaultScreen(sharedX11Display->GetNative()))
                g_primaryDisplay = g_displayList[i].get();
        }
        return true;
    }

    return false;
}


/*
 * LinuxSharedX11Display class
 */

LinuxSharedX11Display::LinuxSharedX11Display() :
    native_ { XOpenDisplay(nullptr) }
{
    if (!native_)
        throw std::runtime_error("failed to open connection to X server");
}

LinuxSharedX11Display::~LinuxSharedX11Display()
{
    XCloseDisplay(native_);
}


/*
 * Display class
 */

std::span<Display* const> Display::GetList()
{
    if (UpdateDisplayList() || g_displayList.size() != g_displayRefList.size())
    {
        /* Update reference list */
        g_displayRefList.clear();
        g_displayRefList.reserve(g_displayList.size());
        for (const auto& display : g_displayList)
            g_displayRefList.push_back(display.get());
    }
    return g_displayRefList;
}

Display* Display::GetPrimary()
{
    UpdateDisplayList();
    return g_primaryDisplay;
}

/*
 * LinuxDisplay class
 */

LinuxDisplay::LinuxDisplay(const std::shared_ptr<LinuxSharedX11Display>& sharedX11Display, int screenIndex) :
    sharedX11Display_ { sharedX11Display },
    screen_           { screenIndex      }
{
}

bool LinuxDisplay::IsPrimary() const
{
    return (screen_ == DefaultScreen(GetNative()));
}

UTF8String LinuxDisplay::GetDeviceName() const
{
    return UTF8String{ DisplayString(GetNative()) };
}

Offset2D LinuxDisplay::GetOffset() const
{
    /* Get display offset from position of root window */
    XWindowAttributes attribs = {};
    XGetWindowAttributes(GetNative(), RootWindow(GetNative(), screen_), &attribs);
    return Offset2D
    {
        attribs.x, attribs.y
    };
}

float LinuxDisplay::GetScale() const
{
    return 1.0f; // dummy
}

bool LinuxDisplay::ResetDisplayMode()
{
    //TODO
    return false;
}

bool LinuxDisplay::SetDisplayMode(const DisplayMode& displayMode)
{
    ::Display* dpy = GetNative();
    ::Window rootWnd = RootWindow(dpy, screen_);

    /* Get all screen sizes from X11 extension Xrandr */
    int numSizes = 0;
    XRRScreenSize* scrSizes = XRRSizes(GetNative(), screen_, &numSizes);

    for (int i = 0; i < numSizes; ++i)
    {
        /* Check if specified display mode resolution matches this screen configuration */
        if (displayMode.resolution.width  == static_cast<std::uint32_t>(scrSizes[i].width) &&
            displayMode.resolution.height == static_cast<std::uint32_t>(scrSizes[i].height))
        {
            if (XRRScreenConfiguration* scrCfg = XRRGetScreenInfo(dpy, rootWnd))
            {
                Status status = XRRSetScreenConfig(dpy, scrCfg, rootWnd, i, RR_Rotate_0, 0);
                XRRFreeScreenConfigInfo(scrCfg);
                return (status != 0);
            }
        }
    }

    return false;
}

DisplayMode LinuxDisplay::GetDisplayMode() const
{
    DisplayMode displayMode;

    ::Display* dpy = GetNative();
    if (::Screen* scr = ScreenOfDisplay(dpy, screen_))
    {
        ::Window rootWnd = RootWindow(dpy, screen_);

        /* Get screen resolution from X11 screen */
        displayMode.resolution.width    = static_cast<std::uint32_t>(scr->width);
        displayMode.resolution.height   = static_cast<std::uint32_t>(scr->height);

        /* Get refresh reate from X11 extension Xrandr */
        if (XRRScreenConfiguration* scrCfg = XRRGetScreenInfo(dpy, rootWnd))
        {
            displayMode.refreshRate = static_cast<std::uint32_t>(XRRConfigCurrentRate(scrCfg));
            XRRFreeScreenConfigInfo(scrCfg);
        }
    }

    return displayMode;
}

std::vector<DisplayMode> LinuxDisplay::GetSupportedDisplayModes() const
{
    std::vector<DisplayMode> displayModes;

    DisplayMode displayMode;

    /* Get all screen sizes from X11 extension Xrandr */
    int numSizes = 0;
    XRRScreenSize* scrSizes = XRRSizes(GetNative(), screen_, &numSizes);

    for (int i = 0; i < numSizes; ++i)
    {
        /* Initialize resolution */
        displayMode.resolution.width    = static_cast<std::uint32_t>(scrSizes[i].width);
        displayMode.resolution.height   = static_cast<std::uint32_t>(scrSizes[i].height);

        /* Add one display mode for each rate */
        int numRates = 0;
        short* rates = XRRRates(GetNative(), screen_, i, &numRates);

        for (int j = 0; j < numRates; ++j)
        {
            displayMode.refreshRate = static_cast<std::uint32_t>(rates[j]);
            displayModes.push_back(displayMode);
        }
    }

    /* Sort final display mode list and remove duplciate entries */
    FinalizeDisplayModes(displayModes);

    return displayModes;
}


/*
 * ======= Private: =======
 */

::Display* LinuxDisplay::GetNative() const
{
    return sharedX11Display_->GetNative();
}


} // /namespace LLGL



// ================================================================================
