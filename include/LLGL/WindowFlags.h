/*
 * WindowFlags.h
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#ifndef LLGL_WINDOW_FLAGS_H
#define LLGL_WINDOW_FLAGS_H


#include <LLGL/Types.h>
#include <LLGL/Constants.h>
#include <LLGL/Container/Strings.h>
#include <cstdint>


namespace LLGL
{


/**
\brief Window creation flags.
\see WindowDescriptor::flags
*/
struct WindowFlags
{
    enum
    {
        //! Specifies whether the window is visible at creation time.
        Visible                 = (1 << 0),

        //! Specifies whether the window is borderless. This is required for a fullscreen swap-chain.
        Borderless              = (1 << 1),

        /**
        \brief Specifies whether the window can be resized.
        \remarks For every window representing the surface for a SwapChain which has been resized,
        the video mode of that SwapChain must be updated with the resolution of the surface's content size.
        This can be done by resizing the swap-chain buffers to the new resolution before the respective swap-chain is bound to a render pass,
        or it can be handled by a window event listener inside a custom \c OnResize callback:
        \code
        // Alternative 1
        class MyEventListener : public LLGL::Window::EventListener {
            void OnResize(LLGL::Window& sender, const LLGL::Extent2D& clientAreaSize) override {
                mySwapChain->ResizeBuffers(clientAreaSize);
            }
        };
        myWindow->AddEventListener(std::make_shared<MyEventListener>());

        // Alternative 2
        mySwapChain->ResizeBuffers(myWindow->GetSize());
        myCmdBuffer->BeginRenderPass(*mySwapChain);
        \endcode
        \note Not updating the swap-chain on a resized window is undefined behavior.
        \see Surface::GetSize
        \see Window::EventListener::OnResize
        */
        Resizable               = (1 << 2),

        /**
        \brief Specifies whether the window is centered within the desktop screen at creation time.
        \remarks If this is specifies, the \c position field of the WindowDescriptor will be ignored.
        */
        Centered                = (1 << 3),

        /**
        \brief Specifies whether the window allows that files can be draged-and-droped onto the window.
        \note Only supported on: MS/Windows.
        */
        AcceptDropFiles         = (1 << 4),

        /**
        \brief Specifies not to multiply the window size by the backing scale factor.
        \remarks This is to control whether to transform the size from window coordinates into screen resolution coordinates.
        \note Only supported on: macOS and iOS.
        \see Display::GetScale
        */
        DisableSizeScaling      = (1 << 6),
    };
};

/**
\brief Window descriptor structure.
\see Window::Create
*/
struct WindowDescriptor
{
    //! Window title in UTF-8 encoding.
    std::string      title;

    //! Window position (relative to the client area).
    Offset2D        position;

    /**
    \brief Specifies the content size (in window coordinates) of the window.
    \remarks The content size does not including the frame and caption dimensions.
    \see Display::GetScale
    \see WindowFlags::DisableSizeScaling
    */
    Extent2D        size;

    /**
    \brief Specifies the window creation flags. This can be a bitwise OR combination of the WindowFlags entries.
    \see WindowFlags
    */
    long            flags               = 0;

    /**
    \brief Window context handle.
    \remarks If used, this must be cast from a platform specific structure:
    \code
    #include <LLGL/Platform/NativeHandle.h>
    //...
    LLGL::NativeHandle myParentWindowHandle;
    myParentWindow->GetNativeHandle(&myParentWindowHandle, sizeof(myParentWindowHandle));
    windowDesc.windowContext        = &myParentWindowHandle;
    \endcode
    */
    const void*     parentWnd       = nullptr;
};


} // /namespace LLGL


#endif



// ================================================================================
