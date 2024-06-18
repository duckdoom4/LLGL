/*
 * Display.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_DISPLAY_H
#define LLGL_DISPLAY_H


#include <LLGL/Interface.h>
#include <LLGL/DisplayFlags.h>
#include <LLGL/Container/Strings.h>
#include <vector>
#include <span>


namespace LLGL
{


/**
\brief Display interface to query the attributes of all connected displays/monitors.
\remarks Here is an example to print the attributes of all displays:
\code
for (Display* const * myDisplayList = LLGL::Display::GetList(); Display* myDisplay = *myDisplayList; ++myDisplayList) {
    LLGL::Offset2D    myDisplayOffset = myDisplay->GetOffset();
    LLGL::DisplayMode myDisplayMode   = myDisplay->GetDisplayMode();
    printf("Display: \"%s\"\n", myDisplay->GetDeviceName().c_str());
    printf("|-Primary = %s\n", myDisplay->IsPrimary() ? "yes" : "no");
    printf("|-X       = %d\n", myDisplayOffset.x);
    printf("|-Y       = %d\n", myDisplayOffset.y);
    printf("|-Width   = %u\n", myDisplayMode.resolution.width);
    printf("|-Height  = %u\n", myDisplayMode.resolution.height);
    printf("`-Hz      = %u\n", myDisplayMode.refreshRate);
}
\endcode
*/
class LLGL_EXPORT Display : public Interface
{

        LLGL_DECLARE_INTERFACE( InterfaceID::Display );

    public:
        /**
        \brief Returns a null-terminated array of all displays.
        \return A span of all displays.
        \remarks This function always checks for updates in the display list.
        */
        static std::span<Display* const> GetList();

        /**
        \brief Returns the primary display or null if no display can be found.
        \see Get
        */
        static Display* GetPrimary();

    public:

        //! Returns true if this is the primary display, as configured by the host system.
        virtual bool IsPrimary() const = 0;

        //! Returns the device name of this display in UTF-8 encoding. This may also be empty, if the platform does not support display names.
        virtual UTF8String GetDeviceName() const = 0;

        /**
        \brief Returns the 2D offset relative to the primary display.
        \remarks This can be used to position your windows accordingly to your displays.
        \see Window::SetPosition
        */
        virtual Offset2D GetOffset() const = 0;

        /**
        \brief Returns the scale factor for this display.
        \remarks This value is used to convert between screen resolution coordinates (see SwapChainDescriptor::resolution) and window coordinates (see WindowDescriptor::size).
        \remarks For high resolution displays, this may have a value of 3 or 2. Otherwise, 1 is the most common value if no extra scaling is necessariy.
        \see SwapChainDescriptor::resolution
        \see WindowDescriptor::size
        */
        virtual float GetScale() const = 0;

        /**
        \brief Resets the display mode to its default value depending on the host system configuration.
        \see SetDisplayMode
        */
        virtual bool ResetDisplayMode() = 0;

        /**
        \brief Sets the new display mode for this display.
        \param[in] displayMode Specifies the new display mode.
        \return True on success, otherwise the specified display mode is not supported by this display and the function has no effect.
        \see GetDisplayMode
        */
        virtual bool SetDisplayMode(const DisplayMode& displayMode) = 0;

        /**
        \brief Returns the current display mode of this display.
        \see SetDisplayMode
        */
        virtual DisplayMode GetDisplayMode() const = 0;

        /**
        \brief Returns a list of all modes that are supported by this display.
        \remarks This list is always sorted in the following manner:
        The first sorting criterion is the number of pixels (resolution width times resolution height) in ascending order,
        and the second sorting criterion is the refresh rate in ascending order.
        To get only the currently active display mode, use GetDisplayMode.
        \see GetDisplayMode
        */
        virtual std::vector<DisplayMode> GetSupportedDisplayModes() const = 0;

    protected:

        /**
        \brief Sorts the specified list of display modes as described in the GetSupportedDisplayModes function, and removes duplicate entries.
        \see GetSupportedDisplayModes
        */
        static void FinalizeDisplayModes(std::vector<DisplayMode>& displayMode);

};


} // /namespace LLGL


#endif



// ================================================================================
