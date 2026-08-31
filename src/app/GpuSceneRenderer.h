#pragma once

#include "geolib/CameraPosition.h"
#include "geolib/DateTimeUtc.h"
#include "geolib/SunLight.h"
#include "geolib/TerrainModel.h"

#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>

/// Hardware accelerated renderer for the terrain scene, built on the WebGPU
/// C API (webgpu.h). The same code path works with a native WebGPU
/// implementation (wgpu-native or Dawn) on desktop platforms and with the
/// browser's WebGPU implementation when compiled to WebAssembly via
/// Emscripten, so the acceleration is independent of the underlying OS
/// graphics API (Vulkan/Metal/D3D12 natively, or the browser's own backend).
///
/// The renderer keeps the CPU side sun/shadow lighting model of SunLight (a
/// full GPU shadow map is out of scope for now) but bakes the resulting
/// per-vertex shade into a vertex buffer and lets the GPU do the projection,
/// rasterization and depth testing, which is dramatically faster than the
/// painter's-algorithm software rasterizer for larger meshes.
class GpuSceneRenderer {
public:
    GpuSceneRenderer();
    ~GpuSceneRenderer();

    GpuSceneRenderer(const GpuSceneRenderer&) = delete;
    GpuSceneRenderer& operator=(const GpuSceneRenderer&) = delete;

    /// Initializes the WebGPU instance/adapter/device and creates a surface
    /// for the given native window handle. Returns false if WebGPU is not
    /// available (e.g. no compatible adapter). On Emscripten, nativeWindow is
    /// ignored and the canvas configured via the environment is used instead.
    bool initialize(void* nativeWindowHandle, std::uint32_t widthPx, std::uint32_t heightPx);

    void resize(std::uint32_t widthPx, std::uint32_t heightPx);

    void setTerrain(std::shared_ptr<const geo::TerrainModel> terrain);
    void setDateTime(const geo::DateTimeUtc& utc);
    void setCamera(const geo::CameraPosition& camera);

    bool isValid() const { return m_device != nullptr; }

    /// Uploads the current scene mesh/lighting (if dirty) and renders one
    /// frame to the surface.
    void renderFrame();

private:
    struct Vertex {
        float position[3];
        float color[3];
    };

    void shutdown();
    void rebuildLight();
    void rebuildGeometry();
    void ensurePipeline();
    void uploadGeometryIfDirty();

    WGPUInstance m_instance{nullptr};
    WGPUAdapter m_adapter{nullptr};
    WGPUDevice m_device{nullptr};
    WGPUQueue m_queue{nullptr};
    WGPUSurface m_surface{nullptr};
    WGPUTextureFormat m_surfaceFormat{WGPUTextureFormat_Undefined};
    WGPURenderPipeline m_pipeline{nullptr};
    WGPUBuffer m_vertexBuffer{nullptr};
    WGPUBuffer m_uniformBuffer{nullptr};
    WGPUBindGroup m_bindGroup{nullptr};
    WGPUTexture m_depthTexture{nullptr};
    WGPUTextureView m_depthView{nullptr};

    std::uint32_t m_width{1};
    std::uint32_t m_height{1};
    bool m_geometryDirty{true};
    std::uint32_t m_vertexCount{0};

    std::shared_ptr<const geo::TerrainModel> m_terrain;
    std::unique_ptr<geo::SunLight> m_light;
    std::unique_ptr<geo::CameraPosition> m_camera;
    geo::DateTimeUtc m_utc;
};
