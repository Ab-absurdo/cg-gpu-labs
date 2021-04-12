#include "Renderer.h"

#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <DirectXMath.h>

#include <cassert>
#include <chrono>
#include <string>

#include "DDSTextureLoader/DDSTextureLoader.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Keys.h"
#include "SimpleVertex.h"
#include "Sphere.h"

#define DEBUG_LAYER

namespace {
    ID3DBlob* compileShader(LPCWSTR p_file_name, LPCSTR p_entrypoint, LPCSTR p_target, UINT flags) {
        ID3DBlob* p_code = nullptr;
        ID3DBlob* p_error_msgs = nullptr;
        HRESULT hr = D3DCompileFromFile(p_file_name, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, p_entrypoint, p_target, flags, 0, &p_code, &p_error_msgs);
        if (p_error_msgs) {
            OutputDebugStringA((LPCSTR)p_error_msgs->GetBufferPointer());
        }
        assert(SUCCEEDED(hr));
        return p_code;
    }

    ID3D11VertexShader* createVertexShader(ID3D11Device* p_device, LPCWSTR p_file_name, LPCSTR p_entrypoint, LPCSTR p_target, UINT flags) {
        ID3DBlob* p_code = compileShader(p_file_name, p_entrypoint, p_target, flags);
        ID3D11VertexShader* p_vertex_shader;
        HRESULT hr = p_device->CreateVertexShader(p_code->GetBufferPointer(), p_code->GetBufferSize(), nullptr, &p_vertex_shader);
        assert(SUCCEEDED(hr));
        return p_vertex_shader;
    }

    ID3D11PixelShader* createPixelShader(ID3D11Device* p_device, LPCWSTR p_file_name, LPCSTR p_entrypoint, LPCSTR p_target, UINT flags) {
        ID3DBlob* p_code = compileShader(p_file_name, p_entrypoint, p_target, flags);
        ID3D11PixelShader* p_pixel_shader;
        HRESULT hr = p_device->CreatePixelShader(p_code->GetBufferPointer(), p_code->GetBufferSize(), nullptr, &p_pixel_shader);
        assert(SUCCEEDED(hr));
        return p_pixel_shader;
    }

    ID3D11Buffer* createBuffer(ID3D11Device* p_device, UINT byte_width, UINT bind_flags, const void* p_sys_mem) {
        D3D11_BUFFER_DESC buffer_desc = CD3D11_BUFFER_DESC(byte_width, bind_flags);
        D3D11_SUBRESOURCE_DATA initial_data = { 0 };
        initial_data.pSysMem = p_sys_mem;
        ID3D11Buffer* p_buffer;
        HRESULT hr = p_device->CreateBuffer(&buffer_desc, p_sys_mem ? &initial_data : nullptr, &p_buffer);
        assert(SUCCEEDED(hr));
        return p_buffer;
    }

    void renderTexture(ID3D11DeviceContext* p_device_context, ID3D11RenderTargetView** p_p_render_target_view, D3D11_VIEWPORT& viewport, ID3D11VertexShader* p_vertex_shader, ID3D11PixelShader* p_pixel_shader, ID3D11ShaderResourceView** p_p_shader_resource_view, ID3D11SamplerState** p_p_sampler_state, ID3D11Buffer** p_p_constant_buffer = nullptr, void* p_constant_buffer_data = nullptr) {
        p_device_context->ClearRenderTargetView(*p_p_render_target_view, DirectX::Colors::Black.f);

        p_device_context->RSSetViewports(1, &viewport);

        p_device_context->OMSetRenderTargets(1, p_p_render_target_view, nullptr);

        p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        p_device_context->IASetInputLayout(nullptr);

        p_device_context->VSSetShader(p_vertex_shader, nullptr, 0);
        p_device_context->PSSetShader(p_pixel_shader, nullptr, 0);
        
        if (p_p_constant_buffer && p_constant_buffer_data) {
            p_device_context->UpdateSubresource(*p_p_constant_buffer, 0, nullptr, p_constant_buffer_data, 0, 0);
            p_device_context->PSSetConstantBuffers(0, 1, p_p_constant_buffer);
        }

        p_device_context->PSSetShaderResources(0, 1, p_p_shader_resource_view);
        p_device_context->PSSetSamplers(0, 1, p_p_sampler_state);

        p_device_context->Draw(4, 0);

        const size_t MAX_NUM_SHADER_RESOURCE_VIEWS = 128;
        ID3D11ShaderResourceView* null_shader_resource_views[MAX_NUM_SHADER_RESOURCE_VIEWS] = { nullptr };
        p_device_context->PSSetShaderResources(0, MAX_NUM_SHADER_RESOURCE_VIEWS, null_shader_resource_views);
    }
}

namespace rendering {
    void Renderer::init(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show) {
        initWindow(h_instance, window_proc, n_cmd_show);
        initResources();
        initImGui();
        initScene();
    }

    void Renderer::initWindow(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show) {
        const wchar_t CLASS_NAME[] = L"Sample Window Class";

        WNDCLASS wc = {};
        wc.lpfnWndProc = window_proc;
        wc.hInstance = h_instance;
        wc.lpszClassName = CLASS_NAME;

        RegisterClass(&wc);

        _hwnd = CreateWindowEx(0, CLASS_NAME, L"Computer Graphics: lab3", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, h_instance, nullptr);
        assert(!(_hwnd == nullptr));

        ShowWindow(_hwnd, n_cmd_show);
    }

    void Renderer::initResources() {
        initDevice();
        initShaders();
        initInputLayout();
    }

    void Renderer::initDevice() {
        IDXGIFactory* p_factory;
        HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&p_factory));
        assert(SUCCEEDED(hr));

        IDXGIAdapter* best_p_adapter;
        p_factory->EnumAdapters(0, &best_p_adapter);
        DXGI_ADAPTER_DESC best_p_adapter_desc;
        best_p_adapter->GetDesc(&best_p_adapter_desc);
        IDXGIAdapter* p_adapter;
        for (UINT i = 1; p_factory->EnumAdapters(i, &p_adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC p_adapter_desc;
            p_adapter->GetDesc(&p_adapter_desc);
            if (p_adapter_desc.DedicatedVideoMemory > best_p_adapter_desc.DedicatedVideoMemory) {
                best_p_adapter = p_adapter;
                best_p_adapter_desc = p_adapter_desc;
            }
        }

        DXGI_SWAP_CHAIN_DESC swap_chain_desc = { 0 };
        swap_chain_desc.BufferDesc.RefreshRate.Numerator = 0;
        swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
        swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.OutputWindow = _hwnd;
        swap_chain_desc.Windowed = true;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        D3D_FEATURE_LEVEL feature_level;
        UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(DEBUG_LAYER)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        hr = D3D11CreateDeviceAndSwapChain(best_p_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &swap_chain_desc, &_p_swap_chain, &_p_device, &feature_level, &_p_device_context);
        assert(SUCCEEDED(hr));

        ID3D11Texture2D* p_framebuffer;
        hr = _p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)(&p_framebuffer));
        assert(SUCCEEDED(hr));

        hr = _p_device->CreateRenderTargetView(p_framebuffer, 0, &_p_render_target_view);
        assert(SUCCEEDED(hr));
        p_framebuffer->Release();

        D3D11_DEPTH_STENCIL_DESC dss_desc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
        dss_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        _p_device->CreateDepthStencilState(&dss_desc, &_p_ds_less_equal);
    }

    void Renderer::initShaders() {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        _p_vs_blob = compileShader(L"../../lab-3/shaders.hlsl", "vsMain", "vs_5_0", flags);

        _p_vertex_shader = createVertexShader(_p_device, L"../../lab-3/shaders.hlsl", "vsMain", "vs_5_0", flags);

        _p_pixel_shader_lambert = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psLambert", "ps_5_0", flags);
        _p_pixel_shader_pbr = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psPBR", "ps_5_0", flags);
        _p_pixel_shader_ndf = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psNDF", "ps_5_0", flags);
        _p_pixel_shader_geometry = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psGeometry", "ps_5_0", flags);
        _p_pixel_shader_fresnel = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psFresnel", "ps_5_0", flags);

        _p_vertex_shader_copy = createVertexShader(_p_device, L"../../lab-3/shaders.hlsl", "vsCopyMain", "vs_5_0", flags);
        _p_pixel_shader_copy = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psCopyMain", "ps_5_0", flags);

        _p_pixel_shader_log_luminance = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psLogLuminanceMain", "ps_5_0", flags);

        _p_pixel_shader_tone_mapping = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psToneMappingMain", "ps_5_0", flags);

        _p_skymap_vs = createVertexShader(_p_device, L"../../lab-3/shaders.hlsl", "vsSkymap", "vs_5_0", flags);
        _p_skymap_ps = createPixelShader(_p_device, L"../../lab-3/shaders.hlsl", "psSkymap", "ps_5_0", flags);
    }

    void Renderer::initInputLayout() {
        D3D11_INPUT_ELEMENT_DESC input_element_desc[] = {
          { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
          { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
          /*{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },*/
        };

        HRESULT hr = _p_device->CreateInputLayout(input_element_desc, ARRAYSIZE(input_element_desc), _p_vs_blob->GetBufferPointer(), _p_vs_blob->GetBufferSize(), &_p_input_layout);
        assert(SUCCEEDED(hr));
    }

    void Renderer::resizeResources(size_t width, size_t height) {
        _viewport = { 0.0f, 0.0f, (FLOAT)(width), (FLOAT)(height), 0.0f, 1.0f };

        if (_p_depth_stencil) {
            _p_depth_stencil->Release();
        }

        CD3D11_TEXTURE2D_DESC depth_desc(DXGI_FORMAT_D24_UNORM_S8_UINT, (UINT)width, (UINT)height, 1, 1, D3D11_BIND_DEPTH_STENCIL);
        HRESULT hr = _p_device->CreateTexture2D(&depth_desc, nullptr, &_p_depth_stencil);
        assert(SUCCEEDED(hr));

        if (_p_depth_stencil_view) {
            _p_depth_stencil_view->Release();
        }

        CD3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc(D3D11_DSV_DIMENSION_TEXTURE2D, depth_desc.Format);
        hr = _p_device->CreateDepthStencilView(_p_depth_stencil, &depth_stencil_view_desc, &_p_depth_stencil_view);
        assert(SUCCEEDED(hr));

        // Setup projection
        float near_z = 0.01f, far_z = 100.0f;
        _projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, width / (FLOAT)height, near_z, far_z);

        _render_texture.SetDevice(_p_device);
        _render_texture.SizeResources(width, height);

        size_t n = (size_t)log2((double)min(width, height));
        _square_copy.SetDevice(_p_device);
        _square_copy.SizeResources(1i64 << n, 1i64 << n);

        _log_luminance_textures.resize(n + 1);
        for (size_t i = 0; i <= n; ++i) {
            _log_luminance_textures[i].SetDevice(_p_device);
            _log_luminance_textures[i].SizeResources(1i64 << (n - i), 1i64 << (n - i));
        }
    }

    void Renderer::initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(_hwnd);
        ImGui_ImplDX11_Init(_p_device, _p_device_context);
    }

    void Renderer::initScene() {
        _borders._min = { -20.0f, -10.0f, -20.0f };
        _borders._max = { 20.0f, 10.0f, 20.0f };

        Sphere sphere(1.0f, 30, 30, true, true);
        auto& vertices = sphere.getVertices();
        auto& indices = sphere.getIndices();

        _indices_number = (UINT)indices.size();
        _vertex_stride = sizeof(SimpleVertex);
        _vertex_offset = 0;

        const float SPHERE_COLOR_RGB[4] = { 0.2f, 0.0f, 0.0f, 1.0f };
        memcpy(_sphere_color_rgb, SPHERE_COLOR_RGB, sizeof(float) * 4);
        _roughness = 0.3f;
        _metalness = 0.2f;

        DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.0f, 1.5f, -3.0f, 0.0f);
        DirectX::XMVECTOR dir = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        _camera = Camera(pos, dir);

        _ambient_light = (DirectX::XMFLOAT4)DirectX::Colors::Black;

        _lights[0]._pos = { 0.0f, 3.0f, -2.0f, 0.0f };
        _lights[0]._color = (DirectX::XMFLOAT4)DirectX::Colors::White;

        _p_vertex_buffer = createBuffer(_p_device, sizeof(SimpleVertex) * (UINT)vertices.size(), D3D11_BIND_VERTEX_BUFFER, vertices.data());

        _p_index_buffer = createBuffer(_p_device, sizeof(unsigned) * _indices_number, D3D11_BIND_INDEX_BUFFER, indices.data());

       
        _p_geometry_cbuffer = createBuffer(_p_device, sizeof(GeometryOperatorsCB), D3D11_BIND_CONSTANT_BUFFER, nullptr);
        _p_sprops_cbuffer = createBuffer(_p_device, sizeof(SurfacePropsCB), D3D11_BIND_CONSTANT_BUFFER, nullptr);
        _p_lights_cbuffer = createBuffer(_p_device, sizeof(LightsCB), D3D11_BIND_CONSTANT_BUFFER, nullptr);
        _p_adaptation_cbuffer = createBuffer(_p_device, sizeof(AdaptationCB), D3D11_BIND_CONSTANT_BUFFER, nullptr);
       

        D3D11_SAMPLER_DESC samp_desc;
        ZeroMemory(&samp_desc, sizeof(samp_desc));
        samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samp_desc.MinLOD = 0;
        samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
        HRESULT hr = _p_device->CreateSamplerState(&samp_desc, &_p_sampler_linear);
        assert(SUCCEEDED(hr));

        {
            HRESULT hr_annotation = _p_device_context->QueryInterface(__uuidof(_p_annotation), reinterpret_cast<void**>(&_p_annotation));
            assert(SUCCEEDED(hr_annotation));
        }

        RECT winRect;
        GetClientRect(_hwnd, &winRect);
        LONG width = winRect.right - winRect.left;
        LONG height = winRect.bottom - winRect.top;
        resizeResources(width, height);

        CD3D11_TEXTURE2D_DESC average_log_luminance_texture_desc(DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1);
        average_log_luminance_texture_desc.MipLevels = 1;
        average_log_luminance_texture_desc.BindFlags = 0;
        average_log_luminance_texture_desc.Usage = D3D11_USAGE_STAGING;
        average_log_luminance_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        hr = _p_device->CreateTexture2D(&average_log_luminance_texture_desc, nullptr, &_average_log_luminance_texture);
        assert(SUCCEEDED(hr));

        Sphere environment(1.0f, 10, 10, false, true);

        auto& env_verts = environment.getVertices();
        _p_sphere_vert_buffer = createBuffer(_p_device, sizeof(SimpleVertex) * (UINT)env_verts.size(), D3D11_BIND_VERTEX_BUFFER, env_verts.data());

        auto& env_indices = environment.getIndices();
        _env_indices_number = (UINT)env_indices.size();
        _p_sphere_index_buffer = createBuffer(_p_device, sizeof(unsigned) * _env_indices_number, D3D11_BIND_INDEX_BUFFER, env_indices.data());

        ID3D11Texture2D* p_sm_texture = nullptr;
        hr = DirectX::CreateDDSTextureFromFileEx(_p_device, L"../../lab-3/skymap.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_TEXTURECUBE, true, (ID3D11Resource**)(&p_sm_texture), &_p_smrv);
        assert(SUCCEEDED(hr));
        p_sm_texture->Release();
    }

    void Renderer::render() {
        auto start = std::chrono::high_resolution_clock::now();

        auto render_texture_render_target_view = _render_texture.GetRenderTargetView();
        auto render_target_view = _render_mode == RenderModes::PBR ? render_texture_render_target_view : _p_render_target_view;

        ID3D11PixelShader* p_pixel_shader = nullptr;
        switch (_render_mode) {
        case RenderModes::PBR:
            p_pixel_shader = _p_pixel_shader_pbr;
            break;
        case RenderModes::NDF:
            p_pixel_shader = _p_pixel_shader_ndf;
            break;
        case RenderModes::GEOMETRY:
            p_pixel_shader = _p_pixel_shader_geometry;
            break;
        case RenderModes::FRESNEL:
            p_pixel_shader = _p_pixel_shader_fresnel;
            break;
        }

        {
            _p_annotation->BeginEvent(L"Rendering start");

            DirectX::XMVECTORF32 background_color = { 0.05f, 0.05f, 0.1f, 1.0f };
            _p_device_context->ClearRenderTargetView(render_target_view, background_color.f);
            _p_device_context->ClearDepthStencilView(_p_depth_stencil_view, D3D11_CLEAR_DEPTH, 1.0f, 0);

            _p_device_context->RSSetViewports(1, &_viewport);

            _p_device_context->OMSetRenderTargets(1, &render_target_view, _p_depth_stencil_view);
            _p_device_context->OMSetDepthStencilState(_p_ds_less_equal, 0);

            _p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            _p_device_context->IASetInputLayout(_p_input_layout);
            _p_device_context->IASetVertexBuffers(0, 1, &_p_vertex_buffer, &_vertex_stride, &_vertex_offset);
            _p_device_context->IASetIndexBuffer(_p_index_buffer, DXGI_FORMAT_R32_UINT, 0);

            _p_annotation->EndEvent();

            _p_annotation->BeginEvent(L"Setting up camera view");
            _view = _camera.getViewMatrix();
            DirectX::XMFLOAT4 camera_pos;
            DirectX::XMStoreFloat4(&camera_pos, _camera.getPosition());
            _p_annotation->EndEvent();

            GeometryOperatorsCB geometry_cbuffer;
            geometry_cbuffer._world = DirectX::XMMatrixTranspose(_world);
            geometry_cbuffer._world_normals = DirectX::XMMatrixInverse(nullptr, _world);
            geometry_cbuffer._view = DirectX::XMMatrixTranspose(_view);
            geometry_cbuffer._projection = DirectX::XMMatrixTranspose(_projection);
            geometry_cbuffer._camera_pos = camera_pos;
          
            _p_device_context->UpdateSubresource(_p_geometry_cbuffer, 0, nullptr, &geometry_cbuffer, 0, 0);

            float sphere_color_srgb[4];
            for (size_t i = 0; i < 4; ++i) {
                sphere_color_srgb[i] = powf(_sphere_color_rgb[i], 2.2f);
            }
            _sphere_color_srgb = DirectX::XMFLOAT4(sphere_color_srgb);
            SurfacePropsCB sprops_cbuffer = { _sphere_color_srgb, _roughness, _metalness};
            _p_device_context->UpdateSubresource(_p_sprops_cbuffer, 0, nullptr, &sprops_cbuffer, 0, 0);

            LightsCB lights_cbuffer;
            lights_cbuffer._ambient_light = _ambient_light;
            for (int i = 0; i < N_LIGHTS; i++)
            {
                lights_cbuffer._light_pos[i] = _lights[i]._pos;
                lights_cbuffer._light_color[i] = _lights[i]._color;
                DirectX::XMFLOAT4 att(_lights[i]._const_att, _lights[i]._lin_att, _lights[i]._exp_att, 0.0f);
                lights_cbuffer._light_attenuation[i] = att;
                lights_cbuffer._light_intensity[4 * i] = _lights[i].getIntensity();
            }
            _p_device_context->UpdateSubresource(_p_lights_cbuffer, 0, nullptr, &lights_cbuffer, 0, 0);

            _p_device_context->VSSetShader(_p_vertex_shader, nullptr, 0);
            _p_device_context->VSSetConstantBuffers(0, 1, &_p_geometry_cbuffer);
            _p_device_context->PSSetShader(p_pixel_shader, nullptr, 0);
            _p_device_context->PSSetConstantBuffers(0, 1, &_p_geometry_cbuffer);
            _p_device_context->PSSetConstantBuffers(1, 1, &_p_sprops_cbuffer);
            _p_device_context->PSSetConstantBuffers(2, 1, &_p_lights_cbuffer);
            _p_device_context->PSSetConstantBuffers(3, 1, &_p_adaptation_cbuffer);

            _p_annotation->BeginEvent(L"Draw");
            _p_device_context->DrawIndexed(_indices_number, 0, 0);
            _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);
            _p_annotation->EndEvent();

            _p_device_context->IASetVertexBuffers(0, 1, &_p_sphere_vert_buffer, &_vertex_stride, &_vertex_offset);
            _p_device_context->IASetIndexBuffer(_p_sphere_index_buffer, DXGI_FORMAT_R32_UINT, 0);

            auto scale = DirectX::XMMatrixScaling(5, 5, 5);
            auto translation = DirectX::XMMatrixTranslation(camera_pos.x, camera_pos.y, camera_pos.z);
            _sphere_world = scale * translation;

            geometry_cbuffer._world = DirectX::XMMatrixTranspose(_sphere_world);

            _p_device_context->UpdateSubresource(_p_geometry_cbuffer, 0, nullptr, &geometry_cbuffer, 0, 0);

            _p_device_context->VSSetShader(_p_skymap_vs, nullptr, 0);
            _p_device_context->VSSetConstantBuffers(0, 1, &_p_geometry_cbuffer);
            _p_device_context->PSSetShader(_p_skymap_ps, nullptr, 0);

            _p_device_context->PSSetShaderResources(0, 1, &_p_smrv);
            _p_device_context->PSSetSamplers(0, 1, &_p_sampler_linear);

            _p_device_context->DrawIndexed(_env_indices_number, 0, 0);

            _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);
        }

        if (_render_mode == RenderModes::PBR) {
            {
                auto render_texture_shader_resource_view = _render_texture.GetShaderResourceView();
                auto square_copy_render_target_view = _square_copy.GetRenderTargetView();

                size_t n = _log_luminance_textures.size() - 1;
                D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << n), FLOAT(1 << n), 0, 1 };

                renderTexture(_p_device_context, &square_copy_render_target_view, vp, _p_vertex_shader_copy, _p_pixel_shader_copy, &render_texture_shader_resource_view, &_p_sampler_linear);
            }

            {
                auto square_copy_shader_resource_view = _square_copy.GetShaderResourceView();
                auto first_log_luminance_texture_render_target_view = _log_luminance_textures.front().GetRenderTargetView();

                size_t n = _log_luminance_textures.size() - 1;
                D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << n), FLOAT(1 << n), 0, 1 };

                renderTexture(_p_device_context, &first_log_luminance_texture_render_target_view, vp, _p_vertex_shader_copy, _p_pixel_shader_log_luminance, &square_copy_shader_resource_view, &_p_sampler_linear);
            }

            {
                size_t n = _log_luminance_textures.size() - 1;

                for (size_t i = 1; i <= n; ++i) {
                    auto previous_log_luminance_texture_shader_resource_view = _log_luminance_textures[i - 1].GetShaderResourceView();
                    auto next_log_luminance_texture_render_target_view = _log_luminance_textures[i].GetRenderTargetView();

                    D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << (n - i)), FLOAT(1 << (n - i)), 0, 1 };

                    renderTexture(_p_device_context, &next_log_luminance_texture_render_target_view, vp, _p_vertex_shader_copy, _p_pixel_shader_copy, &previous_log_luminance_texture_shader_resource_view, &_p_sampler_linear);
                }
            }

            {
                D3D11_MAPPED_SUBRESOURCE average_log_luminance_mapped_subresource;
                _p_device_context->CopyResource(_average_log_luminance_texture, _log_luminance_textures.back().GetRenderTarget());
                _p_device_context->Map(_average_log_luminance_texture, 0, D3D11_MAP_READ, 0, &average_log_luminance_mapped_subresource);
                float average_log_luminance = ((float*)average_log_luminance_mapped_subresource.pData)[0];
                _p_device_context->Unmap(_average_log_luminance_texture, 0);

                auto end = std::chrono::high_resolution_clock::now();
                float delta_t = std::chrono::duration<float>(end - start).count();
                float s = 1;
                _adapted_log_luminance += (average_log_luminance - _adapted_log_luminance) * (1 - expf(-delta_t / s));

                AdaptationCB adaptation_cbuffer;
                adaptation_cbuffer._adapted_log_luminance = _adapted_log_luminance;


                auto render_texture_shader_resource_view = _render_texture.GetShaderResourceView();

                renderTexture(_p_device_context, &_p_render_target_view, _viewport, _p_vertex_shader_copy,
                    _p_pixel_shader_tone_mapping, &render_texture_shader_resource_view, &_p_sampler_linear, &_p_adaptation_cbuffer, &adaptation_cbuffer);
            }
        }

        {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Scene parameters");
            ImGui::Text("Scene");
            ImGui::ListBox("Render mode", (int*)(&_render_mode), _render_modes, _s_RENDER_MODES_NUMBER);
            ImGui::Text("Object");
            ImGui::SliderFloat("Roughness", &_roughness, 0, 1);
            ImGui::SliderFloat("Metalness", &_metalness, 0, 1);
            ImGui::ColorEdit3("Metal F0", _sphere_color_rgb);
            ImGui::End();

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            _p_swap_chain->Present(1, 0);
        }
    }

    void Renderer::handleKey(WPARAM wParam, LPARAM lParam) {
        LPARAM mask_0_15 = 65535, mask_30 = 1 << 30;
        LPARAM b0_15 = lParam & mask_0_15; // The number of times the keystroke is autorepeated as a result of the user holding down the key.
        LPARAM b30 = (lParam & mask_30) != 0; // The value is 1 if the key is down before the message is sent, or it is zero if the key is up.
        float step = 0.1f;
        switch (wParam) {
        case (WPARAM)Keys::W_KEY:
            _camera.moveNormal(step);
            _camera.positionClip(_borders);
            break;
        case (WPARAM)Keys::S_KEY:
            _camera.moveNormal(-step);
            _camera.positionClip(_borders);
            break;
        case (WPARAM)Keys::A_KEY:
            _camera.moveTangent(step);
            _camera.positionClip(_borders);
            break;
        case (WPARAM)Keys::D_KEY:
            _camera.moveTangent(-step);
            _camera.positionClip(_borders);
            break;
        case (WPARAM)Keys::_1_KEY:
            if (!b30) {
                _lights[0].changeIntensity();
            }
            break;
        case (WPARAM)Keys::_2_KEY:
            if (!b30) {
                if(N_LIGHTS > 1)
                    _lights[1].changeIntensity();
            }
            break;
        case (WPARAM)Keys::_3_KEY:
            if (!b30) {
                if (N_LIGHTS > 2)
                    _lights[2].changeIntensity();
            }
            break;
        default:
            break;
        }
    }

    void Renderer::handleMouse(int dx, int dy) {
        float mouse_sence = 5e-3f;
        _camera.rotateHorisontal(dx * mouse_sence);
        _camera.rotateVertical(dy * mouse_sence);
    }

    void Renderer::resize(size_t width, size_t height) {
        if (_p_swap_chain) {
            const size_t REASONABLE_DEFAULT_MIN_SIZE = 8;
            width = max(REASONABLE_DEFAULT_MIN_SIZE, width);
            height = max(REASONABLE_DEFAULT_MIN_SIZE, height);

            _p_device_context->OMSetRenderTargets(0, 0, 0);

            _p_render_target_view->Release();

            _p_device_context->Flush();

            HRESULT hr = _p_swap_chain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);
            assert(SUCCEEDED(hr));

            std::string texture_name = "Texture";

            ID3D11Texture2D* p_buffer;

            hr = _p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&p_buffer);
            assert(SUCCEEDED(hr));

            p_buffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)texture_name.size(), texture_name.c_str());

            hr = _p_device->CreateRenderTargetView(p_buffer, nullptr, &_p_render_target_view);
            assert(SUCCEEDED(hr));

            p_buffer->Release();

            _p_device_context->OMSetRenderTargets(1, &_p_render_target_view, nullptr);

            resizeResources(width, height);
        }
    }

    Renderer::~Renderer() {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        _p_device_context->ClearState();

        _p_geometry_cbuffer->Release();
        _p_sprops_cbuffer->Release();
        _p_lights_cbuffer->Release();
        _p_adaptation_cbuffer->Release();
        _p_vertex_buffer->Release();
        _p_index_buffer->Release();
        _p_sphere_vert_buffer->Release();
        _p_sphere_index_buffer->Release();

        _p_input_layout->Release();

        _p_sampler_linear->Release();

        _average_log_luminance_texture->Release();

        _p_smrv->Release();

        _p_vertex_shader->Release();
        _p_vertex_shader_copy->Release();
        _p_skymap_vs->Release();

        _p_pixel_shader_lambert->Release();
        _p_pixel_shader_pbr->Release();
        _p_pixel_shader_ndf->Release();
        _p_pixel_shader_geometry->Release();
        _p_pixel_shader_fresnel->Release();

        _p_pixel_shader_copy->Release();
        _p_pixel_shader_log_luminance->Release();
        _p_pixel_shader_tone_mapping->Release();
        _p_skymap_ps->Release();

        _p_depth_stencil->Release();
        _p_depth_stencil_view->Release();
        _p_ds_less_equal->Release();

        _p_render_target_view->Release();
        _p_swap_chain->Release();
        _p_device_context->Release();
        _p_device->Release();

        _p_annotation->Release();
    }
}
