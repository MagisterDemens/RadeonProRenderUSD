#include "rifContext.h"
#include "rifError.h"
#include "RadeonProRender_CL.h"
#include "RadeonProRender_GL.h"
#include "RadeonImageFilters_cl.h"
#include "RadeonImageFilters_gl.h"
#include "RadeonImageFilters_metal.h"

#include "rprcpp/rprContext.h"
#include "rprcpp/rprFramebufferGL.h"

#include <vector>
#include <cassert>
#include <stdexcept>

PXR_NAMESPACE_OPEN_SCOPE

namespace rif {

namespace {

class ContextOpenCL final : public Context {
public:
    explicit ContextOpenCL(rpr_context rprContext, std::string const& modelPath);
    ~ContextOpenCL() override = default;

    std::unique_ptr<Image> CreateImage(rpr::FrameBuffer* rprFrameBuffer) override;
private:
    const rif_backend_api_type rifBackendApiType = RIF_BACKEND_API_OPENCL;
};

class ContextMetal final : public Context {
public:
    explicit ContextMetal(rpr_context rprContext, std::string const& modelPath);
    ~ContextMetal() override = default;

    std::unique_ptr<Image> CreateImage(rpr::FrameBuffer* rprFrameBuffer) override;
private:
    const rif_backend_api_type rifBackendApiType = RIF_BACKEND_API_METAL;
};

class ContextCPU final : public Context {
public:
    explicit ContextCPU(rpr_context rprContext, std::string const& modelPath);
    ~ContextCPU() override = default;

    std::unique_ptr<Image> CreateImage(rpr::FrameBuffer* rprFrameBuffer) override;

    void UpdateInputImage(rpr::FrameBuffer* rprFrameBuffer, rif_image image) override;
private:
    const rif_backend_api_type rifBackendApiType = RIF_BACKEND_API_OPENCL;
};

std::vector<rpr_char> GetRprCachePath(rpr_context rprContext) {
    size_t length;
    rpr_status status = rprContextGetInfo(rprContext, RPR_CONTEXT_CACHE_PATH, sizeof(size_t), nullptr, &length);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to get cache path", rprContext));
    }

    std::vector<rpr_char> path(length);
    status = rprContextGetInfo(rprContext, RPR_CONTEXT_CACHE_PATH, path.size(), &path[0], nullptr);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to get cache path", rprContext));
    }

    return path;
}

rif_image_desc GetRifImageDesc(rpr::FrameBuffer* rprFrameBuffer) {
    auto rprDesc = rprFrameBuffer->GetDesc();

    rif_image_desc imageDesc = {};
    imageDesc.image_width = rprDesc.fb_width;
    imageDesc.image_height = rprDesc.fb_height;
    imageDesc.image_depth = 1;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_components = 4;
    imageDesc.type = RIF_COMPONENT_TYPE_FLOAT32;

    return imageDesc;
}

ContextOpenCL::ContextOpenCL(rpr_context rprContext, std::string const& modelPath)
    : Context(modelPath) {
    int deviceCount = 0;
    RIF_ERROR_CHECK_THROW(rifGetDeviceCount(rifBackendApiType, &deviceCount), "Failed to query device count");

    assert(deviceCount != 0);
    if (0 == deviceCount)
        throw rif::Error("No compatible devices.");

    rpr_cl_context clContext;
    rpr_status status = rprContextGetInfo(rprContext, RPR_CL_CONTEXT, sizeof(rpr_cl_context), &clContext, nullptr);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to query CL context", rprContext));
    }

    rpr_cl_device clDevice;
    status = rprContextGetInfo(rprContext, RPR_CL_DEVICE, sizeof(rpr_cl_device), &clDevice, nullptr);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to query CL device", rprContext));
    }

    rpr_cl_command_queue clCommandQueue;
    status = rprContextGetInfo(rprContext, RPR_CL_COMMAND_QUEUE, sizeof(rpr_cl_command_queue), &clCommandQueue, nullptr);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to query CL command queue", rprContext));
    }

    std::vector<rpr_char> path = GetRprCachePath(rprContext);
    #ifndef __APPLE__
    RIF_ERROR_CHECK_THROW(rifCreateContextFromOpenClContext(RIF_API_VERSION, clContext, clDevice, clCommandQueue, path.data(), &m_context), "Failed to create RIF context")
    #endif
}

std::unique_ptr<Image> ContextOpenCL::CreateImage(rpr::FrameBuffer* rprFrameBuffer) {
    if (!rprFrameBuffer) {
        return nullptr;
    }

    rif_image rifImage = nullptr;

    #ifndef __APPLE__
    if (auto rprFrameBufferGL = dynamic_cast<rpr::FrameBufferGL*>(rprFrameBuffer)) {
        RIF_ERROR_CHECK_THROW(rifContextCreateImageFromOpenGlTexture(m_context, GL_TEXTURE_2D, 0, rprFrameBufferGL->GetGL(), &rifImage), "Failed to create RIF image from OpenGL texture")
    } else {
        rpr_cl_mem clMem = rprFrameBuffer->GetCLMem();
        assert(clMem);
        if (!clMem) {
            RIF_THROW_ERROR_MSG("Failed to get rpr framebuffer cl_mem");
        }

        auto rifImageDesc = GetRifImageDesc(rprFrameBuffer);
        RIF_ERROR_CHECK_THROW(rifContextCreateImageFromOpenClMemory(m_context, &rifImageDesc, clMem, false, &rifImage), "Failed to create RIF image from OpenCL memory");

    }
    #endif

    return std::unique_ptr<Image>(new Image(rifImage));
}

ContextCPU::ContextCPU(rpr_context rprContext, std::string const& modelPath)
    : Context(modelPath) {
    int deviceCount = 0;
    RIF_ERROR_CHECK_THROW(rifGetDeviceCount(rifBackendApiType, &deviceCount), "Failed to query device count");

    assert(deviceCount != 0);
    if (0 == deviceCount)
        RIF_THROW_ERROR_MSG("No compatible devices");

    std::vector<rpr_char> path = GetRprCachePath(rprContext);

    RIF_ERROR_CHECK_THROW(rifCreateContext(RIF_API_VERSION, rifBackendApiType, 0, path.data(), &m_context), "Failed to create RIF context")
}

std::unique_ptr<Image> ContextCPU::CreateImage(rpr::FrameBuffer* rprFrameBuffer) {
    if (!rprFrameBuffer) {
        return nullptr;
    }

    return Context::CreateImage(GetRifImageDesc(rprFrameBuffer));
}

void ContextCPU::UpdateInputImage(rpr::FrameBuffer* rprFrameBuffer, rif_image image) {
    if (!rprFrameBuffer || !image) {
        return;
    }

    // data have to be acquired from RPR framebuffers and moved to filter inputs

    size_t sizeInBytes = 0;
    size_t retSize = 0;

    // verify image size
    RIF_ERROR_CHECK_THROW(rifImageGetInfo(image, RIF_IMAGE_DATA_SIZEBYTE, sizeof(size_t), (void*)& sizeInBytes, &retSize), "Failed to get RIF image info");

    size_t fbSize;
    rpr_status status = rprFrameBufferGetInfo(rprFrameBuffer->GetHandle(), RPR_FRAMEBUFFER_DATA, 0, NULL, &fbSize);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to query RPR_FRAMEBUFFER_DATA"));
    }

    assert(sizeInBytes == fbSize);

    if (sizeInBytes != fbSize)
        RIF_THROW_ERROR_MSG("Failed to match RIF image and frame buffer sizes");

    // resolve framebuffer data to rif image
    void* imageData = nullptr;
    RIF_ERROR_CHECK_THROW(rifImageMap(image, RIF_IMAGE_MAP_WRITE, &imageData), "Failed to map RIF image");

    auto rprStatus = rprFrameBufferGetInfo(rprFrameBuffer->GetHandle(), RPR_FRAMEBUFFER_DATA, fbSize, imageData, NULL);
    assert(RPR_SUCCESS == rprStatus);

    // try to unmap at first, then raise a possible error

    RIF_ERROR_CHECK_THROW(rifImageUnmap(image, imageData), "Failed to unmap RIF image");

    if (RPR_SUCCESS != rprStatus)
        RIF_THROW_ERROR_MSG("Failed to get data from RPR frame buffer");
}

rpr_int GpuDeviceIdUsed(rpr_creation_flags contextFlags) {

#define GPU(x) RPR_CREATION_FLAGS_ENABLE_GPU##x

    std::vector<rpr_int> gpu_ids;
    gpu_ids.reserve(16);
    gpu_ids.push_back(GPU(0));
    gpu_ids.push_back(GPU(1));
    gpu_ids.push_back(GPU(2));
    gpu_ids.push_back(GPU(3));
    gpu_ids.push_back(GPU(4));
    gpu_ids.push_back(GPU(5));
    gpu_ids.push_back(GPU(6));
    gpu_ids.push_back(GPU(7));
    gpu_ids.push_back(GPU(8));
    gpu_ids.push_back(GPU(9));
    gpu_ids.push_back(GPU(10));
    gpu_ids.push_back(GPU(11));
    gpu_ids.push_back(GPU(12));
    gpu_ids.push_back(GPU(13));
    gpu_ids.push_back(GPU(14));
    gpu_ids.push_back(GPU(15));

#undef GPU

    for (rpr_int i = 0; i < gpu_ids.size(); i++ )
    {
        if ((contextFlags & gpu_ids[i]) != 0)
            return i;
    }

    return -1;
}

ContextMetal::ContextMetal(rpr_context rprContext, std::string const& modelPath)
    : Context(modelPath) {
    int deviceCount = 0;
    RIF_ERROR_CHECK_THROW(rifGetDeviceCount(rifBackendApiType, &deviceCount), "Failed to query device count");

    assert(deviceCount != 0);
    if (0 == deviceCount)
        RIF_THROW_ERROR_MSG("No compatible devices");

    rpr_creation_flags contextFlags = 0;
    rpr_status status = rprContextGetInfo(rprContext, RPR_CONTEXT_CREATION_FLAGS, sizeof(rpr_creation_flags), &contextFlags, nullptr);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to query RPR context creation flags"));
    }

    std::vector<rpr_char> path = GetRprCachePath(rprContext);

    // we find the active gpu from the rpr contextFlags and then use that to create the rif context
    RIF_ERROR_CHECK_THROW(rifCreateContext(RIF_API_VERSION, rifBackendApiType, GpuDeviceIdUsed(contextFlags), path.data(), &m_context), "Failed to create RIF context");
}

std::unique_ptr<Image> ContextMetal::CreateImage(rpr::FrameBuffer* rprFrameBuffer) {
    if (!rprFrameBuffer) {
        return nullptr;
    }

    rif_image rifImage = nullptr;
    rpr_cl_mem clMem = rprFrameBuffer->GetCLMem();
    assert(clMem);
    if (!clMem)
        RIF_THROW_ERROR_MSG("Failed to get frame buffer cl_mem");

    rpr_image_format framebufferFormat;
    rpr_status status = rprFrameBufferGetInfo(rprFrameBuffer->GetHandle(), RPR_FRAMEBUFFER_FORMAT, sizeof(framebufferFormat), &framebufferFormat, nullptr);
    if (status != RPR_SUCCESS) {
        throw rif::Error(RPR_GET_ERROR_MESSAGE(status, "Failed to get framebuffer format"));
    }

    int bytesPerComponent = 1;
    if (framebufferFormat.type == RPR_COMPONENT_TYPE_FLOAT32) {
        bytesPerComponent = 4;
    } else if (framebufferFormat.type == RPR_COMPONENT_TYPE_FLOAT16) {
        bytesPerComponent = 2;
    }
    auto desc = GetRifImageDesc(rprFrameBuffer);
    rif_longlong size = desc.image_width * desc.image_height * framebufferFormat.num_components * bytesPerComponent;

    RIF_ERROR_CHECK_THROW(rifContextCreateImageFromMetalMemory(m_context, &desc, clMem, size, &rifImage), "Failed to create RIF image from metal memory");

    return std::unique_ptr<Image>(new Image(rifImage));
}

bool HasGpuContext(rpr_creation_flags contextFlags) {

#define GPU(x) RPR_CREATION_FLAGS_ENABLE_GPU##x

    rpr_creation_flags gpuMask = GPU(0) | GPU(1) | GPU(2) | GPU(3) | GPU(4) | GPU(5) | GPU(6) | GPU(7) |
        GPU(8) | GPU(9) | GPU(10) | GPU(11) | GPU(12) | GPU(13) | GPU(14) | GPU(15);

#undef GPU

    return (contextFlags & gpuMask) != 0;
}

} // namespace anonymous

std::unique_ptr<Context> Context::Create(rpr::Context* rprContext, std::string const& modelPath) {
    if (!rprContext) {
        return nullptr;
    }

    rpr_creation_flags contextFlags = 0;
    if (RPR_ERROR_CHECK(rprContextGetInfo(rprContext->GetHandle(), RPR_CONTEXT_CREATION_FLAGS, sizeof(rpr_creation_flags), &contextFlags, nullptr), "Failed to query RPR context creation flags")) {
        return nullptr;
    }

    try {
        std::unique_ptr<Context> rifContext;
        if (contextFlags & RPR_CREATION_FLAGS_ENABLE_METAL) {
            rifContext.reset(new ContextMetal(rprContext->GetHandle(), modelPath));
        } else if (HasGpuContext(contextFlags) && rprContext->GetActivePluginType() != rpr::PluginType::HYBRID) {
            rifContext.reset(new ContextOpenCL(rprContext->GetHandle(), modelPath));
        } else {
            rifContext.reset(new ContextCPU(rprContext->GetHandle(), modelPath));
        }

        RIF_ERROR_CHECK_THROW(rifContextCreateCommandQueue(rifContext->m_context, &rifContext->m_commandQueue), "Failed to create RIF command queue");

        return rifContext;
    } catch (rif::Error const& e) {
        TF_RUNTIME_ERROR("Failed to create RIF context. RIF error: %s", e.what());
    }

    return nullptr;
}

Context::Context(std::string const& modelPath)
    : m_modelPath(modelPath) {

}

Context::~Context() {
    if (m_commandQueue) {
        rifObjectDelete(m_commandQueue);    
    }
    if (m_context) {
        rifObjectDelete(m_context);    
    }
}

std::unique_ptr<Image> Context::CreateImage(rif_image_desc const& desc) {
    rif_image rifImage = nullptr;
    RIF_ERROR_CHECK_THROW(rifContextCreateImage(m_context, &desc, nullptr, &rifImage), "Failed to create RIF image");
    return std::unique_ptr<Image>(new Image(rifImage));
}

void Context::UpdateInputImage(rpr::FrameBuffer* rprFrameBuffer, rif_image image) {
    // no-op
}

void Context::AttachFilter(rif_image_filter filter, rif_image inputImage, rif_image outputImage) {
    RIF_ERROR_CHECK_THROW(rifCommandQueueAttachImageFilter(m_commandQueue, filter, inputImage, outputImage), "Failed to attach image filter to queue");
    ++m_numAttachedFilters;
}

void Context::DetachFilter(rif_image_filter filter) {
    auto rifStatus = rifCommandQueueDetachImageFilter(m_commandQueue, filter);
    if (rifStatus == RIF_ERROR_INVALID_PARAMETER) {
        // Ignore if filter was not attached before
        return;
    }

    RIF_ERROR_CHECK_THROW(rifStatus, "Failed to detach image filter from queue");
    --m_numAttachedFilters;
}

rif_image_filter Context::CreateImageFilter(rif_image_filter_type type) {
    rif_image_filter outFilter = nullptr;
    RIF_ERROR_CHECK_THROW(rifContextCreateImageFilter(m_context, type, &outFilter), "Failed to create image filter");
    return outFilter;
}

void Context::ExecuteCommandQueue() {
    if (!m_numAttachedFilters) {
        return;
    }

    RIF_ERROR_CHECK_THROW(rifContextExecuteCommandQueue(m_context, m_commandQueue, nullptr, nullptr, nullptr), "Failed to execute command queue");
    RIF_ERROR_CHECK_THROW(rifSyncronizeQueue(m_commandQueue), "Failed to synchronize command queue");
}

} // namespace rif

PXR_NAMESPACE_CLOSE_SCOPE
