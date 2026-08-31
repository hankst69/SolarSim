#include "GpuSceneRenderer.h"

#include <algorithm>
#include <array>
#include <cstring>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5_webgpu.h>
#elif defined(_WIN32)
// Native Windows surface creation from an HWND, supported by both
// wgpu-native and Dawn's webgpu.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

/// Simple 4x4 matrix stored column-major, matching WGSL's mat4x4<f32>.
struct Mat4 {
    float m[16]{};

    static Mat4 identity()
    {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
};

Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 lookAt(const geo::Vector3& eye, const geo::Vector3& forward, const geo::Vector3& right,
           const geo::Vector3& up)
{
    Mat4 r = Mat4::identity();
    r.m[0] = static_cast<float>(right.x);
    r.m[4] = static_cast<float>(right.y);
    r.m[8] = static_cast<float>(right.z);

    r.m[1] = static_cast<float>(up.x);
    r.m[5] = static_cast<float>(up.y);
    r.m[9] = static_cast<float>(up.z);

    r.m[2] = static_cast<float>(-forward.x);
    r.m[6] = static_cast<float>(-forward.y);
    r.m[10] = static_cast<float>(-forward.z);

    r.m[12] = static_cast<float>(-right.dot(eye));
    r.m[13] = static_cast<float>(-up.dot(eye));
    r.m[14] = static_cast<float>(forward.dot(eye));
    return r;
}

Mat4 perspective(float fovYRad, float aspect, float nearZ, float farZ)
{
    Mat4 r{};
    const float f = 1.0f / std::tan(fovYRad * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = farZ / (nearZ - farZ);
    r.m[11] = -1.0f;
    r.m[14] = (farZ * nearZ) / (nearZ - farZ);
    return r;
}

constexpr double kFieldOfViewDeg = 55.0;
constexpr double kAmbient = 0.18;
constexpr double kSkyFill = 0.12;
constexpr double kSunDiffuse = 0.85;
constexpr float kTerrainColor[3] = {126.0f / 255.0f, 138.0f / 255.0f, 104.0f / 255.0f};

double clamp01(double value)
{
    return std::min(1.0, std::max(0.0, value));
}

const char* kShaderSource = R"WGSL(
struct Uniforms {
    viewProj : mat4x4<f32>,
};
@group(0) @binding(0) var<uniform> uniforms : Uniforms;

struct VertexIn {
    @location(0) position : vec3<f32>,
    @location(1) color : vec3<f32>,
};

struct VertexOut {
    @builtin(position) clipPosition : vec4<f32>,
    @location(0) color : vec3<f32>,
};

@vertex
fn vs_main(in: VertexIn) -> VertexOut {
    var out: VertexOut;
    out.clipPosition = uniforms.viewProj * vec4<f32>(in.position, 1.0);
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color, 1.0);
}
)WGSL";

} // namespace

GpuSceneRenderer::GpuSceneRenderer() = default;

GpuSceneRenderer::~GpuSceneRenderer()
{
    shutdown();
}

void GpuSceneRenderer::shutdown()
{
    if (m_bindGroup) { wgpuBindGroupRelease(m_bindGroup); m_bindGroup = nullptr; }
    if (m_uniformBuffer) { wgpuBufferRelease(m_uniformBuffer); m_uniformBuffer = nullptr; }
    if (m_vertexBuffer) { wgpuBufferRelease(m_vertexBuffer); m_vertexBuffer = nullptr; }
    if (m_pipeline) { wgpuRenderPipelineRelease(m_pipeline); m_pipeline = nullptr; }
    if (m_depthView) { wgpuTextureViewRelease(m_depthView); m_depthView = nullptr; }
    if (m_depthTexture) { wgpuTextureRelease(m_depthTexture); m_depthTexture = nullptr; }
    if (m_surface) { wgpuSurfaceRelease(m_surface); m_surface = nullptr; }
    if (m_queue) { wgpuQueueRelease(m_queue); m_queue = nullptr; }
    if (m_device) { wgpuDeviceRelease(m_device); m_device = nullptr; }
    if (m_adapter) { wgpuAdapterRelease(m_adapter); m_adapter = nullptr; }
    if (m_instance) { wgpuInstanceRelease(m_instance); m_instance = nullptr; }
}

bool GpuSceneRenderer::initialize(void* nativeWindowHandle, std::uint32_t widthPx,
                                  std::uint32_t heightPx)
{
    m_width = std::max<std::uint32_t>(1, widthPx);
    m_height = std::max<std::uint32_t>(1, heightPx);

    WGPUInstanceDescriptor instanceDesc{};
    m_instance = wgpuCreateInstance(&instanceDesc);
    if (!m_instance) {
        return false;
    }

#if defined(__EMSCRIPTEN__)
    // The browser exposes the WebGPU device directly; the canvas is picked up
    // via the "#canvas" selector configured in the Emscripten shell.
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "#canvas";
    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvasDesc);
    m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
#else
    // Native platforms create the surface from the OS window handle that the
    // Qt widget hands us (HWND on Windows). The exact descriptor struct
    // depends on the native WebGPU implementation used (wgpu-native / Dawn);
    // both accept a WGPUSurfaceDescriptorFromWindowsHWND chained struct.
#if defined(_WIN32)
    WGPUSurfaceDescriptorFromWindowsHWND hwndDesc{};
    hwndDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
    hwndDesc.hinstance = GetModuleHandle(nullptr);
    hwndDesc.hwnd = nativeWindowHandle;
    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&hwndDesc);
    m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
#else
    // Linux (X11/xcb) and macOS (Metal layer) surface creation would go here;
    // left unimplemented for now, see README for the follow-up plan.
    m_surface = nullptr;
    (void)nativeWindowHandle;
#endif
#endif

    if (!m_surface) {
        shutdown();
        return false;
    }

    struct AdapterResult {
        WGPUAdapter adapter{nullptr};
        bool done{false};
    } adapterResult;

    WGPURequestAdapterOptions adapterOptions{};
    adapterOptions.compatibleSurface = m_surface;
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

    wgpuInstanceRequestAdapter(
        m_instance, &adapterOptions,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* /*message*/,
           void* userdata) {
            auto* result = static_cast<AdapterResult*>(userdata);
            if (status == WGPURequestAdapterStatus_Success) {
                result->adapter = adapter;
            }
            result->done = true;
        },
        &adapterResult);

    // wgpu-native/Dawn resolve this callback synchronously on native
    // platforms; on Emscripten the event loop drives it, so initialize()
    // there should be called from within an async context (see main.cpp).
    if (!adapterResult.done || !adapterResult.adapter) {
        shutdown();
        return false;
    }
    m_adapter = adapterResult.adapter;

    struct DeviceResult {
        WGPUDevice device{nullptr};
        bool done{false};
    } deviceResult;

    WGPUDeviceDescriptor deviceDesc{};
    wgpuAdapterRequestDevice(
        m_adapter, &deviceDesc,
        [](WGPURequestDeviceStatus status, WGPUDevice device, char const* /*message*/,
           void* userdata) {
            auto* result = static_cast<DeviceResult*>(userdata);
            if (status == WGPURequestDeviceStatus_Success) {
                result->device = device;
            }
            result->done = true;
        },
        &deviceResult);

    if (!deviceResult.done || !deviceResult.device) {
        shutdown();
        return false;
    }
    m_device = deviceResult.device;
    m_queue = wgpuDeviceGetQueue(m_device);

    m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;

    WGPUSurfaceConfiguration surfaceConfig{};
    surfaceConfig.device = m_device;
    surfaceConfig.format = m_surfaceFormat;
    surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
    surfaceConfig.width = m_width;
    surfaceConfig.height = m_height;
    surfaceConfig.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(m_surface, &surfaceConfig);

    ensurePipeline();
    return true;
}

void GpuSceneRenderer::resize(std::uint32_t widthPx, std::uint32_t heightPx)
{
    m_width = std::max<std::uint32_t>(1, widthPx);
    m_height = std::max<std::uint32_t>(1, heightPx);
    if (!m_surface || !m_device) {
        return;
    }

    WGPUSurfaceConfiguration surfaceConfig{};
    surfaceConfig.device = m_device;
    surfaceConfig.format = m_surfaceFormat;
    surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
    surfaceConfig.width = m_width;
    surfaceConfig.height = m_height;
    surfaceConfig.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(m_surface, &surfaceConfig);

    if (m_depthView) { wgpuTextureViewRelease(m_depthView); m_depthView = nullptr; }
    if (m_depthTexture) { wgpuTextureRelease(m_depthTexture); m_depthTexture = nullptr; }
}

void GpuSceneRenderer::ensurePipeline()
{
    if (m_pipeline || !m_device) {
        return;
    }

    WGPUShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = kShaderSource;
    WGPUShaderModuleDescriptor shaderDesc{};
    shaderDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgslDesc);
    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(m_device, &shaderDesc);

    WGPUBufferBindingLayout bufferLayout{};
    bufferLayout.type = WGPUBufferBindingType_Uniform;
    bufferLayout.minBindingSize = sizeof(Mat4);

    WGPUBindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Vertex;
    bglEntry.buffer = bufferLayout;

    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bglDesc);

    WGPUPipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &layoutDesc);

    std::array<WGPUVertexAttribute, 2> attributes{};
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[0].offset = offsetof(Vertex, position);
    attributes[0].shaderLocation = 0;
    attributes[1].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = offsetof(Vertex, color);
    attributes[1].shaderLocation = 1;

    WGPUVertexBufferLayout vertexLayout{};
    vertexLayout.arrayStride = sizeof(Vertex);
    vertexLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount = static_cast<uint32_t>(attributes.size());
    vertexLayout.attributes = attributes.data();

    WGPUColorTargetState colorTarget{};
    colorTarget.format = m_surfaceFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState{};
    fragmentState.module = shader;
    fragmentState.entryPoint = "fs_main";
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPUDepthStencilState depthStencil{};
    depthStencil.format = WGPUTextureFormat_Depth24Plus;
    depthStencil.depthWriteEnabled = true;
    depthStencil.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = shader;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexLayout;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    pipelineDesc.depthStencil = &depthStencil;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;
    pipelineDesc.fragment = &fragmentState;

    m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);

    WGPUBufferDescriptor uniformDesc{};
    uniformDesc.size = sizeof(Mat4);
    uniformDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    m_uniformBuffer = wgpuDeviceCreateBuffer(m_device, &uniformDesc);

    WGPUBindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = m_uniformBuffer;
    bgEntry.size = sizeof(Mat4);

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    m_bindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);

    wgpuShaderModuleRelease(shader);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);
}

void GpuSceneRenderer::setTerrain(std::shared_ptr<const geo::TerrainModel> terrain)
{
    m_terrain = std::move(terrain);
    rebuildLight();
    rebuildGeometry();
}

void GpuSceneRenderer::setDateTime(const geo::DateTimeUtc& utc)
{
    m_utc = utc;
    rebuildLight();
    rebuildGeometry();
}

void GpuSceneRenderer::setCamera(const geo::CameraPosition& camera)
{
    m_camera = std::make_unique<geo::CameraPosition>(camera);
}

void GpuSceneRenderer::rebuildLight()
{
    if (!m_terrain) {
        m_light.reset();
        return;
    }
    m_light = std::make_unique<geo::SunLight>(*m_terrain, m_utc);
}

void GpuSceneRenderer::rebuildGeometry()
{
    m_geometryDirty = true;
}

void GpuSceneRenderer::uploadGeometryIfDirty()
{
    if (!m_geometryDirty || !m_terrain || !m_device) {
        return;
    }
    m_geometryDirty = false;

    const geo::TriangleMesh& mesh = m_terrain->sceneMesh();
    const std::vector<geo::Vector3>& vertices = mesh.vertices();

    const bool sunUp = m_light && m_light->isAboveHorizon();
    const geo::Vector3 toSun = m_light ? m_light->directionToSun() : geo::Vector3{0.0, 0.0, 1.0};
    const double sunIntensity = m_light ? m_light->intensity() : 0.0;

    std::vector<Vertex> gpuVertices;
    gpuVertices.reserve(mesh.triangles().size() * 3);

    for (const geo::TriangleMesh::Triangle& tri : mesh.triangles()) {
        const geo::Vector3& a = vertices[tri.a];
        const geo::Vector3& b = vertices[tri.b];
        const geo::Vector3& c = vertices[tri.c];

        geo::Vector3 normal = (b - a).cross(c - a).normalized();
        if (normal.z < 0.0) {
            normal = normal * -1.0;
        }

        double shade = kAmbient + kSkyFill * clamp01(normal.z);
        if (sunUp) {
            const double lambert = clamp01(normal.dot(toSun));
            if (lambert > 0.0) {
                const geo::Vector3 centre = (a + b + c) / 3.0;
                if (!m_terrain->isInShadow(centre, toSun)) {
                    shade += kSunDiffuse * lambert * clamp01(sunIntensity + 0.25);
                }
            }
        }
        shade = clamp01(shade);

        const float color[3] = {static_cast<float>(kTerrainColor[0] * shade),
                                static_cast<float>(kTerrainColor[1] * shade),
                                static_cast<float>(kTerrainColor[2] * shade)};

        for (const geo::Vector3* v : {&a, &b, &c}) {
            Vertex vertex{};
            vertex.position[0] = static_cast<float>(v->x);
            vertex.position[1] = static_cast<float>(v->y);
            vertex.position[2] = static_cast<float>(v->z);
            vertex.color[0] = color[0];
            vertex.color[1] = color[1];
            vertex.color[2] = color[2];
            gpuVertices.push_back(vertex);
        }
    }

    m_vertexCount = static_cast<std::uint32_t>(gpuVertices.size());

    if (m_vertexBuffer) {
        wgpuBufferRelease(m_vertexBuffer);
        m_vertexBuffer = nullptr;
    }
    if (m_vertexCount == 0) {
        return;
    }

    const std::uint64_t byteSize = gpuVertices.size() * sizeof(Vertex);
    WGPUBufferDescriptor vbDesc{};
    vbDesc.size = byteSize;
    vbDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    m_vertexBuffer = wgpuDeviceCreateBuffer(m_device, &vbDesc);
    wgpuQueueWriteBuffer(m_queue, m_vertexBuffer, 0, gpuVertices.data(), byteSize);
}

void GpuSceneRenderer::renderFrame()
{
    if (!isValid() || !m_terrain || !m_camera) {
        return;
    }

    uploadGeometryIfDirty();
    if (m_vertexCount == 0) {
        return;
    }

    if (!m_depthTexture) {
        WGPUTextureDescriptor depthDesc{};
        depthDesc.size = {m_width, m_height, 1};
        depthDesc.format = WGPUTextureFormat_Depth24Plus;
        depthDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthDesc.dimension = WGPUTextureDimension_2D;
        depthDesc.mipLevelCount = 1;
        depthDesc.sampleCount = 1;
        m_depthTexture = wgpuDeviceCreateTexture(m_device, &depthDesc);
        m_depthView = wgpuTextureCreateView(m_depthTexture, nullptr);
    }

    // View/projection matrix, mirroring SceneView's software camera math.
    const geo::Vector3 eye = m_camera->localPosition();
    geo::Vector3 target{0.0, 0.0, 0.0};
    double heightM = 0.0;
    if (m_terrain->heightAt(0.0, 0.0, heightM)) {
        target.z = heightM;
    }
    const geo::Vector3 forward = (target - eye).normalized();
    const geo::Vector3 worldUp{0.0, 0.0, 1.0};
    geo::Vector3 right = forward.cross(worldUp).normalized();
    if (right.length() < 1e-9) {
        right = geo::Vector3{1.0, 0.0, 0.0};
    }
    const geo::Vector3 up = right.cross(forward).normalized();

    const Mat4 view = lookAt(eye, forward, right, up);
    const float aspect = static_cast<float>(m_width) / static_cast<float>(std::max<std::uint32_t>(1, m_height));
    const Mat4 proj = perspective(static_cast<float>(kFieldOfViewDeg * 3.14159265358979 / 180.0),
                                  aspect, 0.5f, 100000.0f);
    const Mat4 viewProj = multiply(proj, view);

    wgpuQueueWriteBuffer(m_queue, m_uniformBuffer, 0, viewProj.m, sizeof(Mat4));

    WGPUSurfaceTexture surfaceTexture{};
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);
    if (!surfaceTexture.texture) {
        return;
    }
    WGPUTextureView frameView = wgpuTextureCreateView(surfaceTexture.texture, nullptr);

    WGPUCommandEncoderDescriptor encoderDesc{};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoderDesc);

    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.view = frameView;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    const bool sunUp = m_light && m_light->isAboveHorizon();
    colorAttachment.clearValue = sunUp ? WGPUColor{0.38, 0.58, 0.84, 1.0}
                                       : WGPUColor{0.06, 0.09, 0.16, 1.0};

    WGPURenderPassDepthStencilAttachment depthAttachment{};
    depthAttachment.view = m_depthView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.0f;

    WGPURenderPassDescriptor passDesc{};
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = &depthAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
    wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, m_bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, m_vertexBuffer, 0,
                                        m_vertexCount * sizeof(Vertex));
    wgpuRenderPassEncoderDraw(pass, m_vertexCount, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmdDesc{};
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, &cmdDesc);
    wgpuQueueSubmit(m_queue, 1, &commands);
    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(frameView);

#if !defined(__EMSCRIPTEN__)
    wgpuSurfacePresent(m_surface);
#endif
}
