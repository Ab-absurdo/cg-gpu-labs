#pragma once

#include <d3d11.h>
#include <d3d11_1.h>

#include <vector>

#include "RenderTexture/RenderTexture.h"

#include "ConstantBuffer.h"
#include "Camera.h"
#include "PointLight.h"
#include "RenderModes.h"
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
        void initShaders();
        void initInputLayout();
        void initImGui();
        void initScene();

        void createCubeMap(UINT size, ID3D11PixelShader* p_pixel_shader, ID3D11ShaderResourceView** p_p_smrv_src, ID3D11ShaderResourceView** p_p_smrv_dst);

        void resizeResources(size_t width, size_t height);

        HWND _hwnd;

        ID3D11Device* _p_device = nullptr;
        ID3D11DeviceContext* _p_device_context = nullptr;
        IDXGISwapChain* _p_swap_chain = nullptr;
        ID3D11RenderTargetView* _p_render_target_view = nullptr;

        ID3D11Texture2D* _p_depth_stencil = nullptr;
        ID3D11DepthStencilView* _p_depth_stencil_view = nullptr;
        ID3D11DepthStencilState* _p_ds_less_equal = nullptr;

        ID3DBlob* _p_vs_blob = nullptr;

        ID3D11VertexShader* _p_vertex_shader = nullptr;

        ID3D11PixelShader* _p_pixel_shader_lambert = nullptr;
        ID3D11PixelShader* _p_pixel_shader_pbr = nullptr;
        ID3D11PixelShader* _p_pixel_shader_ndf = nullptr;
        ID3D11PixelShader* _p_pixel_shader_geometry = nullptr;
        ID3D11PixelShader* _p_pixel_shader_fresnel = nullptr;

        ID3D11VertexShader* _p_vertex_shader_copy = nullptr;
        ID3D11PixelShader* _p_pixel_shader_cube_map = nullptr;
        ID3D11PixelShader* _p_pixel_shader_irradiance_map = nullptr;
        ID3D11PixelShader* _p_pixel_shader_copy = nullptr;
        ID3D11PixelShader* _p_pixel_shader_log_luminance = nullptr;
        ID3D11PixelShader* _p_pixel_shader_tone_mapping = nullptr;

        ID3D11VertexShader* _p_skymap_vs = nullptr;
        ID3D11PixelShader* _p_skymap_ps = nullptr;

        ID3D11InputLayout* _p_input_layout = nullptr;

        static const size_t _s_RENDER_MODES_NUMBER = 4;
        const char* _render_modes[_s_RENDER_MODES_NUMBER] = { "PBR", "NDF", "Geometry", "Fresnel" };
        RenderModes _render_mode = RenderModes::PBR;


        DirectX::XMMATRIX _world = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _view = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _projection = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _translation = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _sphere_world = DirectX::XMMatrixIdentity();

        WorldBorders _borders;
        Camera _camera;
        PointLight _lights[N_LIGHTS];
        float _exposure_scale;
        float _sphere_color_rgb[4];
        DirectX::XMFLOAT4 _sphere_color_srgb;
        float _roughness;
        float _metalness;


        UINT _vertex_stride;
        UINT _vertex_offset;
        UINT _indices_number;
        UINT _env_indices_number;

        float _adapted_log_luminance = 0.0f;

        ID3D11Buffer* _p_vertex_buffer = nullptr;
        ID3D11Buffer* _p_index_buffer = nullptr;
        ID3D11Buffer* _p_sphere_index_buffer = nullptr;
        ID3D11Buffer* _p_sphere_vert_buffer = nullptr;
        ID3D11Buffer* _p_geometry_cbuffer = nullptr;
        ID3D11Buffer* _p_sprops_cbuffer = nullptr;
        ID3D11Buffer* _p_lights_cbuffer = nullptr;
        ID3D11Buffer* _p_adaptation_cbuffer = nullptr;

        ID3D11SamplerState* _p_sampler_linear = nullptr;

        ID3DUserDefinedAnnotation* _p_annotation = nullptr;

        D3D11_VIEWPORT _viewport;

        DX::RenderTexture _render_texture;
        DX::RenderTexture _square_copy;
        std::vector<DX::RenderTexture> _log_luminance_textures;

        ID3D11Texture2D* _average_log_luminance_texture = nullptr;

        ID3D11ShaderResourceView* _p_smrv_sky = nullptr;
        ID3D11ShaderResourceView* _p_smrv_irradiance = nullptr;

        static const size_t _s_MAX_NUM_SHADER_RESOURCE_VIEWS = 128;
        ID3D11ShaderResourceView* const _null_shader_resource_views[_s_MAX_NUM_SHADER_RESOURCE_VIEWS] = { nullptr };
    };
}
