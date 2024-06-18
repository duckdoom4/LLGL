/*
 * Example.cpp (Example_Mapping)
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include <ExampleBase.h>
#include <stdio.h>


// Use source textures instead for additional copy indirections
#define ENABLE_INTERMEDIATE_TEXTURES 0


/*
 * Texture and buffer mapping example
 * ----------------------------------
 * This is a visually unimpressive example that only demonstrates how to copy data between buffers and textures.
 * You'll see horizontal stripes of red, green, and blue across the window.
 * By pressing the Tab key, you can modify the content in a seemingly unorganized manner.
 * By pressing the Backspace key, you can reset the content to its initial state.
 */
class Example_Mapping : public ExampleBase
{

    const std::uint64_t     contentBufferSize   = 4 * 512; // Format = RGBA8UNorm
    const LLGL::Extent3D    dstTextureSize      = { 64, 64, 1 };
    const LLGL::Extent3D    srcTexture0Size     = { 64, 64, 1 }; // 64 * 4 = 256 = Proper row alignment (especially for D3D12)
    const LLGL::Extent3D    srcTexture1Size     = { 50, 20, 1 }; // 50 * 4 = 200 = Improper row alignment

    ShaderPipeline          shaderPipeline;
    LLGL::PipelineLayout*   pipelineLayout      = nullptr;
    LLGL::PipelineState*    pipeline            = nullptr;
    LLGL::Buffer*           vertexBuffer        = nullptr;

    LLGL::Buffer*           contentBuffer       = nullptr;  // Content buffer which is copied into the textures
    #if ENABLE_INTERMEDIATE_TEXTURES
    LLGL::Texture*          srcTextures[2]      = {};       // Source textures for copy operations
    #endif
    LLGL::Texture*          dstTextures[2]      = {};       // Destination textures for display

    LLGL::Sampler*          samplerState        = nullptr;
    LLGL::ResourceHeap*     resourceHeap        = nullptr;

    int                     dstTextureIndex     = 0;        // Index into the 'dstTextures' array

public:

    Example_Mapping() :
        ExampleBase { "LLGL Example: Mapping" }
    {
        // Create all graphics objects
        auto vertexFormat = CreateBuffers();
        shaderPipeline = LoadStandardShaderPipeline({ vertexFormat });
        CreatePipelines();
        CreateContentBuffer();
        CreateSourceTextures();
        CreateDestinationTexture();
        CreateResourceHeap();
        GenerateTextureContent();

        // Print some information on the standard output
        ::printf(
            "press TAB KEY to iterate copy operations on the texture\n"
            "press BACKSPACE KEY to reset the texture\n"
        );
    }

private:

    LLGL::VertexFormat CreateBuffers()
    {
        // Specify vertex format
        LLGL::VertexFormat vertexFormat;
        vertexFormat.AppendAttribute({ "position", LLGL::Format::RG32Float });
        vertexFormat.AppendAttribute({ "texCoord", LLGL::Format::RG32Float });

        // Define vertex buffer data
        struct Vertex
        {
            float position[2];
            float texCoord[2];
        };

        const float s = 1;//0.5f;

        const Vertex vertices[] =
        {
            { { -s,  s }, { 0, 0 } },
            { { -s, -s }, { 0, 1 } },
            { {  s,  s }, { 1, 0 } },
            { {  s, -s }, { 1, 1 } },
        };

        // Create vertex buffer
        vertexBuffer = CreateVertexBuffer(vertices, vertexFormat);

        // Read vertex buffer back to CPU memory for validation
        Vertex readbackVertices[4] = {};
        renderer->ReadBuffer(*vertexBuffer, 0, readbackVertices, sizeof(readbackVertices));

        auto MatchReadbackVerticesPosition = [&readbackVertices, &vertices](int v, int c)
        {
            if (readbackVertices[v].position[c] != vertices[v].position[c])
            {
                ::fprintf(
                    stderr, "Readback data mismatch: Expected vertices[%d].position[%d] to be %f, but got %f\n",
                    v, c, vertices[v].position[c], readbackVertices[v].position[c]
                );
            }
        };

        auto MatchReadbackVerticesTexCoord = [&readbackVertices, &vertices](int v, int c)
        {
            if (readbackVertices[v].texCoord[c] != vertices[v].texCoord[c])
            {
                ::fprintf(
                    stderr, "Readback data mismatch: Expected vertices[%d].texCoord[%d] to be %f, but got %f\n",
                    v, c, vertices[v].texCoord[c], readbackVertices[v].texCoord[c]
                );
            }
        };

        for (int i = 0; i < 8; ++i)
        {
            MatchReadbackVerticesPosition(i / 2, i % 2);
            MatchReadbackVerticesTexCoord(i / 2, i % 2);
        }

        return vertexFormat;
    }

    void CreatePipelines()
    {
        // Create pipeline layout
        LLGL::PipelineLayoutDescriptor layoutDesc;
        {
            layoutDesc.heapBindings =
            {
                LLGL::BindingDescriptor{ LLGL::ResourceType::Texture, LLGL::BindFlags::Sampled, LLGL::StageFlags::FragmentStage, 0 },
                LLGL::BindingDescriptor{ LLGL::ResourceType::Sampler, 0,                        LLGL::StageFlags::FragmentStage, (IsVulkan() || IsMetal() ? 1u : 0u) },
            };
        }
        pipelineLayout = renderer->CreatePipelineLayout(layoutDesc);

        // Create graphics pipeline
        LLGL::GraphicsPipelineDescriptor pipelineDesc;
        {
            pipelineDesc.vertexShader                   = shaderPipeline.vs;
            pipelineDesc.fragmentShader                 = shaderPipeline.ps;
            pipelineDesc.pipelineLayout                 = pipelineLayout;
            pipelineDesc.primitiveTopology              = LLGL::PrimitiveTopology::TriangleStrip;
            pipelineDesc.rasterizer.multiSampleEnabled  = (GetSampleCount() > 1);
        }
        pipeline = renderer->CreatePipelineState(pipelineDesc);
    }

    void CreateContentBuffer()
    {
        // Create content buffer with CPU read/write access but without binding flags since we don't bind it to any pipeline
        LLGL::BufferDescriptor bufferDesc;
        {
            bufferDesc.debugName        = "MyContentBuffer";
            bufferDesc.size             = contentBufferSize;
            bufferDesc.bindFlags        = LLGL::BindFlags::CopySrc | LLGL::BindFlags::CopyDst; // Not used in a graphics or compute shader, only with copy commands
            bufferDesc.cpuAccessFlags   = LLGL::CPUAccessFlags::ReadWrite;
            bufferDesc.miscFlags        = LLGL::MiscFlags::NoInitialData;
        }
        contentBuffer = renderer->CreateBuffer(bufferDesc);
    }

    void CreateSourceTextures()
    {
        #if ENABLE_INTERMEDIATE_TEXTURES

        // Create empty destination texture
        for (int i = 0; i < 2; ++i)
        {
            LLGL::TextureDescriptor texDesc;
            {
                texDesc.bindFlags   = LLGL::BindFlags::CopySrc | LLGL::BindFlags::CopyDst;
                texDesc.miscFlags   = LLGL::MiscFlags::NoInitialData;
                texDesc.extent      = (i == 0 ? srcTexture0Size : srcTexture1Size);
            }
            srcTextures[i] = renderer->CreateTexture(texDesc);
        }

        #endif // /ENABLE_INTERMEDIATE_TEXTURES
    }

    void CreateDestinationTexture()
    {
        // Create empty destination texture
        LLGL::TextureDescriptor texDesc;
        {
            texDesc.bindFlags   = LLGL::BindFlags::Sampled | LLGL::BindFlags::ColorAttachment | LLGL::BindFlags::CopyDst | LLGL::BindFlags::CopySrc;
            texDesc.miscFlags   = LLGL::MiscFlags::NoInitialData;
            texDesc.extent      = dstTextureSize;
            //texDesc.format      = LLGL::Format::R16UNorm;
        }
        for (int i = 0; i < 2; ++i)
            dstTextures[i] = renderer->CreateTexture(texDesc);

        // Assign label to textures (for debugging)
        dstTextures[0]->SetDebugName("MyDestinationTexture[0]");
        dstTextures[1]->SetDebugName("MyDestinationTexture[1]");
    }

    void CreateResourceHeap()
    {
        // Create nearest sampler
        LLGL::SamplerDescriptor samplerDesc;
        {
            samplerDesc.minFilter       = LLGL::SamplerFilter::Nearest;
            samplerDesc.magFilter       = LLGL::SamplerFilter::Nearest;
            samplerDesc.mipMapFilter    = LLGL::SamplerFilter::Nearest;
        }
        samplerState = renderer->CreateSampler(samplerDesc);

        // Create resource heap
        const LLGL::ResourceViewDescriptor resourceViews[] =
        {
            dstTextures[0], samplerState,
            dstTextures[1], samplerState,
        };
        LLGL::ResourceHeapDescriptor resourceHeapDesc;
        {
            resourceHeapDesc.pipelineLayout     = pipelineLayout;
            resourceHeapDesc.numResourceViews   = sizeof(resourceViews) / sizeof(resourceViews[0]);
        }
        resourceHeap = renderer->CreateResourceHeap(resourceHeapDesc, resourceViews);
    }

    void GenerateTextureContent()
    {
        // Map content buffer for writing
        if (void* dst = renderer->MapBuffer(*contentBuffer, LLGL::CPUAccess::WriteDiscard))
        {
            // Write some initial data
            auto dstColors = reinterpret_cast<LLGL::ColorRGBAub*>(dst);
            for (int i = 0; i < 128; ++i)
            {
                dstColors[i] = LLGL::ColorRGBAub{ 0xD0, 0x50, 0x20, 0xFF }; // Red
            }
            renderer->UnmapBuffer(*contentBuffer);
        }

        // Encode copy commands
        commands->Begin();
        {
            // Fill up content buffer (Note: swap endian)
            commands->FillBuffer(*contentBuffer, /*Offset:*/ 128 * 4, /*Value:*/ 0xFF50D040, /*Size:*/ 128 * 4); // Green
            commands->FillBuffer(*contentBuffer, /*Offset:*/ 256 * 4, /*Value:*/ 0xFFD05050, /*Size:*/ 256 * 4); // Blue

            #if ENABLE_INTERMEDIATE_TEXTURES

            // Copy buffer to source textures
            /*commands->CopyTextureFromBuffer(
                *srcTextures[0],
            );*/

            // Copy source textures to destination texture

            #else // ENABLE_INTERMEDIATE_TEXTURES

            // Mix up data in content buffer
            #if 0
            commands->CopyBuffer(
                /*Destination:*/        *contentBuffer,
                /*DestinationOffset:*/  32 * 3 * 4,
                /*Source:*/             *contentBuffer,
                /*SourceOffset:*/       32 * 8 * 4,
                /*Size:*/               64 * 4
            );
            #endif

            // Copy content buffer to destination texture
            for (int y = 0; y < static_cast<int>(dstTextureSize.y); y += 8)
            {
                commands->CopyTextureFromBuffer(
                    *dstTextures[0],
                    LLGL::TextureRegion
                    {
                        LLGL::Offset3D{ 0, y, 0 },
                        LLGL::Extent3D{ 64, 8, 1 }
                    },
                    *contentBuffer,
                    0
                );
            }

            #endif // /ENABLE_INTERMEDIATE_TEXTURES

            // Duplicate destination texture context
            commands->CopyTexture(*dstTextures[1], {}, *dstTextures[0], {}, dstTextureSize);
        }
        commands->End();
        commandQueue->Submit(*commands);
    }

    void ModifyTextureContent()
    {
        // Encode copy commands
        commands->Begin();
        {
            // Modify texture by copying data between the two alternating destination textures
            commands->CopyTexture(
                /*Destination:*/            *dstTextures[(dstTextureIndex + 1) % 2],
                /*DestinationLocation:*/    LLGL::TextureLocation{ LLGL::Offset3D{ 8, 8, 0 } },
                /*Source:*/                 *dstTextures[dstTextureIndex],
                /*SourceLocation:*/         LLGL::TextureLocation{ LLGL::Offset3D{ 12, 10, 0 } },
                /*Size:*/                   LLGL::Extent3D{ 32, 32, 1 }
            );

            // Store single pixel of texture back to content buffer to map texture memory to CPU space
            commands->CopyBufferFromTexture(
                *contentBuffer,
                0,
                *dstTextures[(dstTextureIndex + 1) % 2],
                LLGL::TextureRegion
                {
                    LLGL::Offset3D{ 8, 8, 0 },
                    LLGL::Extent3D{ 1, 1, 1 }
                }
            );
        }
        commands->End();
        commandQueue->Submit(*commands);

        // Map content buffer for reading
        if (const void* src = renderer->MapBuffer(*contentBuffer, LLGL::CPUAccess::ReadOnly))
        {
            auto srcColors = reinterpret_cast<const LLGL::ColorRGBAub*>(src);
            {
                const LLGL::ColorRGBAub srcColor0 = srcColors[0];
                ::printf(
                    "Left-top color in destination texture: (#%02X, #%02X, #%02X)\r",
                    static_cast<int>(srcColor0.r), static_cast<int>(srcColor0.g), static_cast<int>(srcColor0.b)
                );
                ::fflush(stdout);
            }
            renderer->UnmapBuffer(*contentBuffer);
        }

        // Move to next destination texture for display
        dstTextureIndex = (dstTextureIndex + 1) % 2;
    }

private:

    void OnDrawFrame() override
    {
        // Examine user input
        if (input.KeyDown(LLGL::Key::Tab))
            ModifyTextureContent();
        if (input.KeyDown(LLGL::Key::Back))
            GenerateTextureContent();

        // Draw scene
        commands->Begin();
        {
            // Set vertex buffer
            commands->SetVertexBuffer(*vertexBuffer);

            commands->BeginRenderPass(*swapChain);
            {
                // Clear color buffer and set viewport
                commands->Clear(LLGL::ClearFlags::Color);
                commands->SetViewport(swapChain->GetResolution());

                // Set graphics pipeline and vertex buffer
                commands->SetPipelineState(*pipeline);
                commands->SetResourceHeap(*resourceHeap, dstTextureIndex);

                // Draw fullscreen quad
                commands->Draw(4, 0);
            }
            commands->EndRenderPass();
        }
        commands->End();
        commandQueue->Submit(*commands);
    }

};

LLGL_IMPLEMENT_EXAMPLE(Example_Mapping);



