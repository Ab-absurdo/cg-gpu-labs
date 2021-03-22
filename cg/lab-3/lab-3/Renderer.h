#pragma once

#include <d3d11.h>
#include <d3d11_1.h>

#include <vector>

#include "RenderTexture.h"

#include "Camera.h"
#include "PointLight.h"
#include "WorldBorders.h"

namespace rendering {
    class Renderer {
    public:
        void init(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show);
        void render();
        void handleKey(WPARAM wParam, LPARAM lParam);
        void handleMouse(const POINT& cursor);
        void resize(WPARAM wParam, LPARAM lParam);

        ~Renderer();

    private:
        void initWindow(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show);
        void initResources();
        void initDevice();
        void initShaders();
        void initInputLayout();
        void initScene();

        HWND _hwnd;

        ID3D11Device* _device_ptr = nullptr;
        ID3D11DeviceContext* _device_context_ptr = nullptr;
        IDXGISwapChain* _swap_chain_ptr = nullptr;
        ID3D11RenderTargetView* _render_target_view_ptr = nullptr;

        ID3DBlob* _vs_blob_ptr = nullptr;

        ID3D11VertexShader* _vertex_shader_ptr = nullptr;
        ID3D11PixelShader* _pixel_shader_ptr = nullptr;
        ID3D11VertexShader* _vertex_shader_copy_ptr = nullptr;
        ID3D11PixelShader* _pixel_shader_copy_ptr = nullptr;
        ID3D11PixelShader* _pixel_shader_log_luminance_ptr = nullptr;
        ID3D11PixelShader* _pixel_shader_tone_mapping_ptr = nullptr;

        ID3D11InputLayout* _input_layout_ptr = nullptr;

        DirectX::XMMATRIX _world = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _view = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _projection = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _translation = DirectX::XMMatrixIdentity();

        WorldBorders _borders;
        Camera _camera;
        PointLight _lights[3];

        UINT _vertex_stride;
        UINT _vertex_offset;
        UINT _indices_number;

        float _adapted_log_luminance = 0;

        ID3D11Buffer* _vertex_buffer_ptr = nullptr;
        ID3D11Buffer* _index_buffer_ptr = nullptr;
        ID3D11Buffer* _constant_buffer_ptr = nullptr;

        ID3D11ShaderResourceView* _texture_rv_ptr = nullptr;
        ID3D11SamplerState* _sampler_linear_ptr = nullptr;

        ID3DUserDefinedAnnotation* _p_annotation = nullptr;

        D3D11_VIEWPORT _viewport;

        DX::RenderTexture _render_texture;
        DX::RenderTexture _square_copy;
        std::vector<DX::RenderTexture> _log_luminance_textures;

        ID3D11Texture2D* _average_log_luminance_texture = nullptr;

        ID3D11ShaderResourceView* const _null[128] = { nullptr };
    };
}
