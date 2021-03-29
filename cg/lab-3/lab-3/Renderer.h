#pragma once

#include <d3d11.h>
#include <d3d11_1.h>

#include <vector>

#include "RenderTexture.h"

#include "Camera.h"
#include "Geometry.h"
#include "PointLight.h"
#include "WorldBorders.h"

namespace rendering {
    class Renderer {
    public:
        void init(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show);
        void render();
        void handleKey(WPARAM wParam, LPARAM lParam);
        void handleMouse(int dx, int dy);
        void resize(size_t width, size_t height);

        ~Renderer();

    private:
        void initWindow(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show);
        void initResources();
        void initDevice();
        void initDepthStencil();
        void initShaders();
        void initInputLayout();
        void initScene();

        void resizeResources(size_t width, size_t height);

        HWND _hwnd;

        ID3D11Device* _p_device = nullptr;
        ID3D11DeviceContext* _p_device_context = nullptr;
        IDXGISwapChain* _p_swap_chain = nullptr;
        ID3D11RenderTargetView* _p_render_target_view = nullptr;

        ID3D11Texture2D* _p_depth_stencil = nullptr;
        ID3D11DepthStencilState* _p_depth_stencil_state = nullptr;
        ID3D11DepthStencilView* _p_depth_stencil_view = nullptr;

        ID3DBlob* _p_vs_blob = nullptr;

        ID3D11VertexShader* _p_vertex_shader = nullptr;
        ID3D11PixelShader* _p_pixel_shader = nullptr;
        ID3D11VertexShader* _p_vertex_shader_copy = nullptr;
        ID3D11PixelShader* _p_pixel_shader_copy = nullptr;
        ID3D11PixelShader* _p_pixel_shader_log_luminance = nullptr;
        ID3D11PixelShader* _p_pixel_shader_tone_mapping = nullptr;

        ID3D11InputLayout* _p_input_layout = nullptr;

        DirectX::XMMATRIX _world = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _view = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _projection = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _translation = DirectX::XMMatrixIdentity();

        WorldBorders _borders;
        Camera _camera;
        PointLight _lights[3];
        DirectX::XMFLOAT4 _ambient_light;

        UINT _vertex_stride;
        UINT _vertex_offset;
        UINT _indices_number;

        float _adapted_log_luminance = 0;

        ID3D11Buffer* _p_vertex_buffer = nullptr;
        ID3D11Buffer* _p_index_buffer = nullptr;
        ID3D11Buffer* _p_constant_buffer = nullptr;

        ID3D11SamplerState* _p_sampler_linear = nullptr;

        ID3DUserDefinedAnnotation* _p_annotation = nullptr;

        D3D11_VIEWPORT _viewport;

        DX::RenderTexture _render_texture;
        DX::RenderTexture _square_copy;
        std::vector<DX::RenderTexture> _log_luminance_textures;

        ID3D11Texture2D* _average_log_luminance_texture = nullptr;

        static const size_t _s_MAX_NUM_SHADER_RESOURCE_VIEWS = 128;
        ID3D11ShaderResourceView* const _null_shader_resource_views[_s_MAX_NUM_SHADER_RESOURCE_VIEWS] = { nullptr };
    };
}
