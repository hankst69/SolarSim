#pragma once

#include "SceneViewBase.h"

#include "geolib/TerrainModel.h"

#include <QWidget>

#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>
#include <vector>

/// Hardware accelerated view of the terrain scene, built on the WebGPU C API
/// (webgpu.h). The same code path works with a native WebGPU implementation
/// (wgpu-native or Dawn) on desktop platforms and with the browser's WebGPU
/// implementation when compiled to WebAssembly via Emscripten, so the
/// acceleration is independent of the underlying OS graphics API
/// (Vulkan/Metal/D3D12 natively, or the browser's own backend).
///
/// Like SceneViewGL, this is a drop-in replacement for SceneViewCPU: it is a
/// plain QWidget that renders into its own native window via a WebGPU
/// surface, rather than going through Qt's painter or OpenGL integration.
///
/// The renderer keeps the CPU side sun lighting model of SunLight (a full GPU
/// shadow map is out of scope for now) but bakes the resulting per-vertex
/// shade into a vertex buffer and lets the GPU do the projection,
/// rasterization and depth testing, which is dramatically faster than the
/// painter's-algorithm software rasterizer for larger meshes. The terrain
/// cast shadow raycast (TerrainModel::isInShadow()) is by far the most
/// expensive part of shading a frame, so it is only evaluated when
/// setShadowsEnabled(true) is set; by default a cheap sun-facing preview
/// shading is used instead, keeping date/time changes interactive.
class SceneViewWEBGPU : public QWidget, public SceneViewBase {
    Q_OBJECT

public:
    explicit SceneViewWEBGPU(QWidget* parent = nullptr);
    ~SceneViewWEBGPU() override;

    SceneViewWEBGPU(const SceneViewWEBGPU&) = delete;
    SceneViewWEBGPU& operator=(const SceneViewWEBGPU&) = delete;

    bool isValid() const { return m_device != nullptr; }

signals:
    void cameraChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void onTerrainChanged() override;
    void onDateTimeChanged() override;
    void onCameraChanged() override;
    void onShadowsEnabledChanged() override;
    void emitCameraChanged() override { emit cameraChanged(); }

private:
    struct Vertex {
        float position[3];
        float color[3];
    };

    /// Initializes the WebGPU instance/adapter/device and creates a surface
    /// for this widget's native window. Returns false if WebGPU is not
    /// available (e.g. no compatible adapter). On Emscripten, the canvas
    /// configured via the environment is used instead of a native handle.
    bool initialize(std::uint32_t widthPx, std::uint32_t heightPx);
    void resizeSurface(std::uint32_t widthPx, std::uint32_t heightPx);

    /// Uploads the current scene mesh/lighting (if dirty) and renders one
    /// frame to the surface.
    void renderFrame();

    void shutdown();
    void rebuildGeometry();
    void rebuildSunOverlay();
    void ensurePipeline();
    void uploadGeometryIfDirty();
    void uploadSunOverlayIfDirty();

    WGPUInstance m_instance{nullptr};
    WGPUAdapter m_adapter{nullptr};
    WGPUDevice m_device{nullptr};
    WGPUQueue m_queue{nullptr};
    WGPUSurface m_surface{nullptr};
    WGPUTextureFormat m_surfaceFormat{WGPUTextureFormat_Undefined};
    WGPURenderPipeline m_pipeline{nullptr};
    WGPURenderPipeline m_overlayPipeline{nullptr};
    WGPUBuffer m_vertexBuffer{nullptr};
    WGPUBuffer m_overlayVertexBuffer{nullptr};
    WGPUBuffer m_uniformBuffer{nullptr};
    WGPUBindGroup m_bindGroup{nullptr};
    WGPUTexture m_depthTexture{nullptr};
    WGPUTextureView m_depthView{nullptr};

    std::uint32_t m_width{1};
    std::uint32_t m_height{1};
    bool m_geometryDirty{true};
    bool m_overlayDirty{true};
    bool m_gpuInitialized{false};
    std::uint32_t m_vertexCount{0};
    std::uint32_t m_overlayVertexCount{0};
};

