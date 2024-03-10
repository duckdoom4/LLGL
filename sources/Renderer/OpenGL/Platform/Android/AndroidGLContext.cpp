/*
 * AndroidGLContext.cpp
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include "AndroidGLContext.h"
#include "AndroidGLCore.h"
#include "../../../CheckedCast.h"
#include "../../../StaticAssertions.h"
#include "../../../../Core/CoreUtils.h"
#include "../../../../Core/Assertion.h"
#include <LLGL/RendererConfiguration.h>
#include <LLGL/Backend/OpenGL/NativeHandle.h>


namespace LLGL
{


/*
 * GLContext class
 */

LLGL_ASSERT_STDLAYOUT_STRUCT( OpenGL::RenderSystemNativeHandle );

std::unique_ptr<GLContext> GLContext::Create(
    const GLPixelFormat&                pixelFormat,
    const RendererConfigurationOpenGL&  profile,
    Surface&                            surface,
    GLContext*                          sharedContext,
    const ArrayView<char>&              /*customNativeHandle*/)
{
    AndroidGLContext* sharedContextEGL = (sharedContext != nullptr ? LLGL_CAST(AndroidGLContext*, sharedContext) : nullptr);
    return MakeUnique<AndroidGLContext>(pixelFormat, profile, surface, sharedContextEGL);
}


/*
 * LinuxGLContext class
 */

AndroidGLContext::AndroidGLContext(
    const GLPixelFormat&                pixelFormat,
    const RendererConfigurationOpenGL&  profile,
    Surface&                            surface,
    AndroidGLContext*                   sharedContext)
:
    display_ { eglGetDisplay(EGL_DEFAULT_DISPLAY) }
{
    CreateContext(pixelFormat, profile, sharedContext);
}

AndroidGLContext::~AndroidGLContext()
{
    DeleteContext();
}

int AndroidGLContext::GetSamples() const
{
    return samples_;
}

bool AndroidGLContext::GetNativeHandle(void* nativeHandle, std::size_t nativeHandleSize) const
{
    if (nativeHandle != nullptr && nativeHandleSize == sizeof(OpenGL::RenderSystemNativeHandle))
    {
        auto* nativeHandleGL = reinterpret_cast<OpenGL::RenderSystemNativeHandle*>(nativeHandle);
        nativeHandleGL->context = context_;
        return true;
    }
    return false;
}


/*
 * ======= Private: =======
 */

bool AndroidGLContext::SetSwapInterval(int interval)
{
    return (eglSwapInterval(display_, interval) == EGL_TRUE);
}

bool AndroidGLContext::SelectConfig(const GLPixelFormat& pixelFormat)
{
    /* Look for a framebuffer configuration; reduce samples if necessary */
    for (samples_ = std::max(1, pixelFormat.samples); samples_ > 0; --samples_)
    {
        /* Initialize framebuffer configuration */
        EGLint attribs[] =
        {
            EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
            EGL_RED_SIZE,       8,
            EGL_GREEN_SIZE,     8,
            EGL_BLUE_SIZE,      8,
            EGL_ALPHA_SIZE,     8,
            EGL_DEPTH_SIZE,     pixelFormat.depthBits,
            EGL_STENCIL_SIZE,   pixelFormat.stencilBits,
            EGL_SAMPLE_BUFFERS, 1,
            EGL_SAMPLES,        samples_,
            EGL_NONE
        };

        if (samples_ <= 1)
        {
            /* Cut off EGL_SAMPLE* entries in case EGL context doesn't support them at all */
            constexpr int sampleBuffersArrayIndex = 14;
            LLGL_ASSERT(attribs[sampleBuffersArrayIndex] == EGL_SAMPLE_BUFFERS);
            attribs[sampleBuffersArrayIndex] = EGL_NONE;
        }

        /* Choose configuration */
        EGLint numConfigs = 0;
        EGLBoolean success = eglChooseConfig(display_, attribs, &config_, 1, &numConfigs);

        /* Reduce number of sample if configuration failed */
        if (success == EGL_TRUE && numConfigs > 0)
        {
            SetDefaultColorFormat();
            DeduceDepthStencilFormat(pixelFormat.depthBits, pixelFormat.stencilBits);
            return true;
        }
    }

    /* No suitable configuration found */
    return false;
}

static EGLint GetGLESVersionNo(EGLint major, EGLint minor)
{
    return (major*100 + minor*10);
}

static EGLint GetGLESMajorVersion(EGLint version)
{
    return (version/100);
}

static EGLint GetGLESMinorVersion(EGLint version)
{
    return ((version/10)%10);
}

static bool IsSupportedGLESVersion(EGLint version)
{
    return (version == 320 || version == 310 || version == 300 || version == 200);
}

void AndroidGLContext::CreateContext(
    const GLPixelFormat&                pixelFormat,
    const RendererConfigurationOpenGL&  profile,
    AndroidGLContext*                   sharedContext)
{
    /* Flush previous error code */
    (void)eglGetError();

    /* Initialize EGL display connection (ignore major/minor output parameters) */
    if (!eglInitialize(display_, nullptr, nullptr))
        LLGL_TRAP("eglInitialize failed (%s)", EGLErrorToString());

    /* Select EGL context configuration for pixel format */
    if (!SelectConfig(pixelFormat))
    {
        LLGL_TRAP(
            "eglChooseConfig [colorBits = %d, depthBits = %d, stencilBits = %d, samples = %d] failed (%s)",
            pixelFormat.colorBits, pixelFormat.depthBits, pixelFormat.stencilBits, pixelFormat.samples,
            EGLErrorToString()
        );
    }

    /* Set up EGL profile attributes */
    EGLint major = 3, minor = 0;

    if (!(profile.majorVersion == 0 && profile.minorVersion == 0))
    {
        major = profile.majorVersion;
        minor = profile.minorVersion;
        if (!IsSupportedGLESVersion(GetGLESVersionNo(major, minor)))
            LLGL_TRAP("cannot create GLES contex for version %d.%d; supported versions are 3.2, 3.1, 3.0, and 2.0", major, minor);
    }

    /* Create EGL context with optional shared EGL context */
    EGLContext sharedEGLContext = (sharedContext != nullptr ? sharedContext->context_ : EGL_NO_CONTEXT);

    while ((context_ = CreateEGLContextForESVersion(major, minor, sharedEGLContext)) == EGL_NO_CONTEXT)
    {
        if (major == 3)
        {
            /* Try next lower version: GLES 3.2, 3.1, 3.0 */
            if (minor > 0 && minor <= 2)
                --minor;
            else
                --major;
        }
        else
        {
            /* Creating context for GLES 2.0 failed and no lower version is supported by LLGL */
            break;
        }
    }

    if (context_ == EGL_NO_CONTEXT)
        LLGL_TRAP("eglCreateContext failed (%s)", EGLErrorToString());
}

void AndroidGLContext::DeleteContext()
{
    eglDestroyContext(display_, context_);
}

EGLContext AndroidGLContext::CreateEGLContextForESVersion(EGLint major, EGLint minor, EGLContext sharedEGLContext)
{
    EGLint contextAttribs[] =
    {
        EGL_CONTEXT_MAJOR_VERSION,          major,
        EGL_CONTEXT_MINOR_VERSION,          minor,
        #ifdef LLGL_DEBUG
        EGL_CONTEXT_OPENGL_DEBUG,           EGL_TRUE,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS,   EGL_TRUE,
        #endif
        EGL_NONE
    };

    EGLContext context = eglCreateContext(display_, config_, sharedEGLContext, contextAttribs);

    #ifdef LLGL_DEBUG
    if (context == EGL_NO_CONTEXT)
    {
        /* If context creation failed with debug mode, try same version again but without the debug context */
        contextAttribs[4] = EGL_NONE;
        context = eglCreateContext(display_, config_, sharedEGLContext, contextAttribs);
    }
    #endif

    return context;
}


} // /namespace LLGL



// ================================================================================
