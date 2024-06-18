/*
 * Example.cpp (Example_Texturing)
 *
 * Copyright (c) 2015 Lukas Hermanns. All rights reserved.
 * Licensed under the terms of the BSD 3-Clause license (see LICENSE.txt).
 */

#include <ExampleBase.h>
#include <FileUtils.h>
#include <LLGL/Utils/TypeNames.h>
#include <DDSImageReader.h>
#include <stb/stb_image.h>


class Example_Texturing : public ExampleBase
{

    ShaderPipeline              shaderPipeline;
    LLGL::PipelineLayout*       pipelineLayout      = nullptr;
    LLGL::PipelineState*        pipeline            = nullptr;
    LLGL::Buffer*               vertexBuffer        = nullptr;
    LLGL::Buffer*               indexBuffer         = nullptr;
    LLGL::Buffer*               sceneBuffer         = nullptr;
    LLGL::Texture*              colorMaps[2]        = {};
    LLGL::Sampler*              samplers[3]         = {};

    unsigned                    resourceIndex       = 0;

    const char*                 resourceLabels[4]   =
    {
        "compressed (BC1UNorm)",
        "uncompressed (RGBA8UNorm)",
        "uncompressed (RGBA8UNorm), lod bias",
        "uncompressed (RGBA8UNorm), lod bias, nearest filter",
    };

    struct Scene
    {
        Gs::Matrix4f wvpMatrix;
        Gs::Matrix4f wMatrix;
    }
    scene;

public:

    Example_Texturing() :
        ExampleBase { "LLGL Example: Texturing" }
    {
        // Create all graphics objects
        auto vertexFormat = CreateBuffers();
        shaderPipeline = LoadStandardShaderPipeline({ vertexFormat });
        CreatePipelines();
        CreateTextures();
        CreateSamplers();

        // Print some information on the standard output
        LLGL::Log::Printf(
            "press TAB KEY to switch between five different texture samplers\n"
            "texture: %s\r",
            resourceLabels[0]
        );
        ::fflush(stdout);
    }

    LLGL::VertexFormat CreateBuffers()
    {
        // Specify vertex format
        LLGL::VertexFormat vertexFormat;
        vertexFormat.AppendAttribute({ "position", LLGL::Format::RGB32Float });
        vertexFormat.AppendAttribute({ "normal",   LLGL::Format::RGB32Float });
        vertexFormat.AppendAttribute({ "texCoord", LLGL::Format::RG32Float  });

        // Create vertex and index buffers
        vertexBuffer = CreateVertexBuffer(GenerateTexturedCubeVertices(), vertexFormat);
        indexBuffer = CreateIndexBuffer(GenerateTexturedCubeTriangleIndices(), LLGL::Format::R32UInt);

        // Create constant buffer
        sceneBuffer = CreateConstantBuffer(scene);

        return vertexFormat;
    }

    void CreatePipelines()
    {
        // Create pipeline layout
        const bool          hasCombinedSamplers = IsOpenGL();
        const std::uint32_t samplerStateSlot    = (hasCombinedSamplers ? 2u : 3u);
        LLGL::PipelineLayoutDescriptor layoutDesc;
        {
            layoutDesc.bindings =
            {
                LLGL::BindingDescriptor{ "Scene",        LLGL::ResourceType::Buffer,  LLGL::BindFlags::ConstantBuffer, LLGL::StageFlags::VertexStage,   1                },
                LLGL::BindingDescriptor{ "colorMap",     LLGL::ResourceType::Texture, LLGL::BindFlags::Sampled,        LLGL::StageFlags::FragmentStage, 2                },
                LLGL::BindingDescriptor{ "samplerState", LLGL::ResourceType::Sampler, 0,                               LLGL::StageFlags::FragmentStage, samplerStateSlot },
            };
        }
        pipelineLayout = renderer->CreatePipelineLayout(layoutDesc);

        // Create graphics pipeline
        LLGL::GraphicsPipelineDescriptor pipelineDesc;
        {
            pipelineDesc.vertexShader                   = shaderPipeline.vs;
            pipelineDesc.fragmentShader                 = shaderPipeline.ps;
            pipelineDesc.pipelineLayout                 = pipelineLayout;
            pipelineDesc.renderPass                     = swapChain->GetRenderPass();
            pipelineDesc.primitiveTopology              = LLGL::PrimitiveTopology::TriangleList;
            pipelineDesc.depth.testEnabled              = true;
            pipelineDesc.depth.writeEnabled             = true;
            pipelineDesc.rasterizer.multiSampleEnabled  = (GetSampleCount() > 1);
        }
        pipeline = renderer->CreatePipelineState(pipelineDesc);
    }

    void LoadUncompressedTexture(const std::string& filename)
    {
        // Load image data from file (using STBI library, see http://nothings.org/stb_image.h)
        int texWidth = 0, texHeight = 0, texComponents = 0;

        const std::string path = FindResourcePath(filename);

        unsigned char* imageBuffer = stbi_load(path.c_str(), &texWidth, &texHeight, &texComponents, 0);
        if (!imageBuffer)
            throw std::runtime_error("failed to load image from file: " + path);

        // Initialize source image descriptor to upload image data onto hardware texture
        LLGL::ImageView imageView;
        {
            // Set image color format
            imageView.format    = (texComponents == 4 ? LLGL::ImageFormat::RGBA : LLGL::ImageFormat::RGB);

            // Set image data type (unsigned char = 8-bit unsigned integer)
            imageView.dataType  = LLGL::DataType::UInt8;

            // Set image buffer source for texture initial data
            imageView.data      = imageBuffer;

            // Set image buffer size
            imageView.dataSize  = static_cast<std::size_t>(texWidth*texHeight*texComponents);
        }

        // Upload image data onto hardware texture and stop the time
        timer.Start();
        {
            // Create texture
            LLGL::TextureDescriptor texDesc;
            {
                // Texture type: 2D
                texDesc.type        = LLGL::TextureType::Texture2D;

                // Texture hardware format: RGBA with normalized 8-bit unsigned char type
                texDesc.format      = LLGL::Format::BGRA8UNorm;//RGBA8UNorm; //BGRA8UNorm

                // Texture size
                texDesc.extent      = { static_cast<std::uint32_t>(texWidth), static_cast<std::uint32_t>(texHeight), 1u };

                // Generate all MIP-map levels for this texture
                texDesc.miscFlags   = LLGL::MiscFlags::GenerateMips;
            }
            colorMaps[1] = renderer->CreateTexture(texDesc, &imageView);
        }
        double texCreationTime = static_cast<double>(timer.Stop()) / static_cast<double>(timer.GetFrequency());
        LLGL::Log::Printf("texture creation time: %f ms\n", texCreationTime * 1000.0);

        // Release image data
        stbi_image_free(imageBuffer);
    }

    void LoadCompressedTexture(const std::string& filename)
    {
        // Load DDS image
        DDSImageReader imageReader;
        imageReader.LoadFromFile(filename);

        // Create hardware texture with compressed format
        LLGL::TextureDescriptor texDesc = imageReader.GetTextureDesc();
        colorMaps[0] = renderer->CreateTexture(texDesc);

        // For compressed textures, we have to write each MIP-map manually - no automatic MIP-map generation available
        const auto& formatDesc = LLGL::GetFormatAttribs(texDesc.format);

        for (std::uint32_t mipLevel = 0; mipLevel < texDesc.mipLevels; ++mipLevel)
        {
            // Determine texture region for next MIP-map level
            LLGL::TextureRegion region;
            {
                region.extent                   = colorMaps[0]->GetMipExtent(mipLevel);
                region.subresource.baseMipLevel = mipLevel;
                region.subresource.numMipLevels = 1;
            }

            // MIP-maps of block compression must be a multiple of the block size, so we cannot go smaller
            if (region.extent.x  >= formatDesc.blockWidth &&
                region.extent.y >= formatDesc.blockHeight)
            {
                // Write image data into MIP-map
                renderer->WriteTexture(*colorMaps[0], region, imageReader.GetImageView(mipLevel));
            }
        }
    }

    void CreateTextures()
    {
        LoadCompressedTexture("Crate-DXT1-MipMapped.dds");
        LoadUncompressedTexture("Crate.jpg");
    }

    void CreateSamplers()
    {
        // Create 1st sampler state with default settings
        LLGL::SamplerDescriptor anisotropySamplerDesc;
        {
            anisotropySamplerDesc.maxAnisotropy = 8;
        }
        samplers[0] = renderer->CreateSampler(anisotropySamplerDesc);

        // Create 2nd sampler state with MIP-map bias
        LLGL::SamplerDescriptor lodSamplerDesc;
        {
            lodSamplerDesc.mipMapLODBias        = 3;
        }
        samplers[1] = renderer->CreateSampler(lodSamplerDesc);

        // Create 2nd sampler state with MIP-map bias
        LLGL::SamplerDescriptor nearestSamplerDesc;
        {
            nearestSamplerDesc.minFilter        = LLGL::SamplerFilter::Nearest;
            nearestSamplerDesc.magFilter        = LLGL::SamplerFilter::Nearest;
            nearestSamplerDesc.minLOD           = 4;
            nearestSamplerDesc.maxLOD           = 4;
        }
        samplers[2] = renderer->CreateSampler(nearestSamplerDesc);
    }

private:

    void OnDrawFrame() override
    {
        // Examine user input
        if (input.KeyDown(LLGL::Key::Tab))
        {
            // Switch to next resource we want to present
            if (input.KeyPressed(LLGL::Key::Shift))
                resourceIndex = ((resourceIndex - 1) % 4 + 4) % 4;
            else
                resourceIndex = (resourceIndex + 1) % 4;

            const std::string spaces(30, ' ');
            LLGL::Log::Printf(
                "texture: %s%s\r",
                resourceLabels[resourceIndex],
                spaces.c_str()
            );
            ::fflush(stdout);
        }

        // Update scene constants
        static float rotation = Gs::Deg2Rad(-20.0f);
        if (input.KeyPressed(LLGL::Key::LButton) || input.KeyPressed(LLGL::Key::RButton))
            rotation += static_cast<float>(input.GetMouseMotion().x)*0.005f;

        scene.wMatrix.LoadIdentity();
        Gs::Translate(scene.wMatrix, Gs::Vector3f{ 0, 0, 5 });
        Gs::RotateFree(scene.wMatrix, Gs::Vector3f{ 0, 1, 0 }, rotation);

        scene.wvpMatrix = projection;
        scene.wvpMatrix *= scene.wMatrix;

        // Set render target
        commands->Begin();
        {
            // Update scene constant buffer
            commands->UpdateBuffer(*sceneBuffer, 0, &scene, sizeof(scene));

            // Set vertex buffer
            commands->SetVertexBuffer(*vertexBuffer);
            commands->SetIndexBuffer(*indexBuffer);

            commands->BeginRenderPass(*swapChain);
            {
                // Clear color and depth buffers
                commands->Clear(LLGL::ClearFlags::ColorDepth, backgroundColor);
                commands->SetViewport(swapChain->GetResolution());

                // Bind graphics PSO
                commands->SetPipelineState(*pipeline);

                commands->SetResource(0, *sceneBuffer);
                commands->SetResource(1, *colorMaps[resourceIndex == 0 ? 0 : 1]);
                commands->SetResource(2, *samplers[resourceIndex == 0 ? 0 : resourceIndex - 1]);

                commands->DrawIndexed(36, 0);
            }
            commands->EndRenderPass();
        }
        commands->End();
        commandQueue->Submit(*commands);
    }

};

LLGL_IMPLEMENT_EXAMPLE(Example_Texturing);



