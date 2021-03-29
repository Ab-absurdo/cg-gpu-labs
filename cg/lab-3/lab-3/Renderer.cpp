#include "Renderer.h"

#include <d3dcompiler.h>
#include <DirectXColors.h>

#include <cassert>
#include <chrono>
#include <string>

#include "DDSTextureLoader.h"

#include "ConstantBuffer.h"
#include "Keys.h"
#include "SimpleVertex.h"

#define DEBUG_LAYER

namespace rendering {
    void Renderer::init(HINSTANCE h_instance, WNDPROC window_proc, int n_cmd_show) {
        initWindow(h_instance, window_proc, n_cmd_show);
        initResources();
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
        initDepthStencil();
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
    }

    void Renderer::initDepthStencil() {
        RECT rc;
        GetClientRect(_hwnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        // Create depth stencil texture
        D3D11_TEXTURE2D_DESC depth_desc;
        ZeroMemory(&depth_desc, sizeof(depth_desc));
        depth_desc.Width = width;
        depth_desc.Height = height;
        depth_desc.MipLevels = 1;
        depth_desc.ArraySize = 1;
        depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_desc.SampleDesc.Count = 1;
        depth_desc.SampleDesc.Quality = 0;
        depth_desc.Usage = D3D11_USAGE_DEFAULT;
        depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depth_desc.CPUAccessFlags = 0;
        depth_desc.MiscFlags = 0;
        HRESULT hr = _p_device->CreateTexture2D(&depth_desc, nullptr, &_p_depth_stencil);
        assert(SUCCEEDED(hr));

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
        ZeroMemory(&descDSV, sizeof(descDSV));
        descDSV.Format = depth_desc.Format;
        descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = _p_device->CreateDepthStencilView(_p_depth_stencil, &descDSV, &_p_depth_stencil_view);
        assert(SUCCEEDED(hr));

        _p_device_context->OMSetRenderTargets(1, &_p_render_target_view, _p_depth_stencil_view);
    }

    void Renderer::initShaders() {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ID3DBlob* p_error_blob = nullptr;
        HRESULT hr = D3DCompileFromFile(L"../../lab-3/shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vsMain", "vs_5_0", flags, 0, &_p_vs_blob, &p_error_blob);
        if (FAILED(hr)) {
            if (p_error_blob) {
                OutputDebugStringA((char*)p_error_blob->GetBufferPointer());
                p_error_blob->Release();
            }
            if (_p_vs_blob) {
                _p_vs_blob->Release();
            }
            assert(false);
        }

        ID3DBlob* p_ps_blob = nullptr;
        hr = D3DCompileFromFile(L"../../lab-3/shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psMain", "ps_5_0", flags, 0, &p_ps_blob, &p_error_blob);
        if (FAILED(hr)) {
            if (p_error_blob) {
                OutputDebugStringA((char*)p_error_blob->GetBufferPointer());
                p_error_blob->Release();
            }
            if (p_ps_blob) {
                p_ps_blob->Release();
            }
            assert(false);
        }

        ID3DBlob* p_vs_copy_blob = nullptr, * p_ps_copy_blob = nullptr, * p_copy_error_blob = nullptr;
        hr = D3DCompileFromFile(L"../../lab-3/shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vsCopyMain", "vs_5_0", flags, 0, &p_vs_copy_blob, &p_copy_error_blob);
        if (FAILED(hr)) {
            if (p_copy_error_blob) {
                OutputDebugStringA((char*)p_copy_error_blob->GetBufferPointer());
                p_copy_error_blob->Release();
            }
            if (p_vs_copy_blob) {
                p_vs_copy_blob->Release();
            }
            assert(false);
        }

        hr = D3DCompileFromFile(L"../../lab-3/shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psCopyMain", "ps_5_0", flags, 0, &p_ps_copy_blob, &p_copy_error_blob);
        if (FAILED(hr)) {
            if (p_copy_error_blob) {
                OutputDebugStringA((char*)p_copy_error_blob->GetBufferPointer());
                p_copy_error_blob->Release();
            }
            if (p_ps_copy_blob) {
                p_ps_copy_blob->Release();
            }
            assert(false);
        }

        ID3DBlob* p_ps_log_luminance_blob = nullptr, * p_ps_log_luminance_error_blob = nullptr;
        hr = D3DCompileFromFile(L"../../lab-3/shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psLogLuminanceMain", "ps_5_0", flags, 0, &p_ps_log_luminance_blob, &p_ps_log_luminance_error_blob);
        if (FAILED(hr)) {
            if (p_ps_log_luminance_error_blob) {
                OutputDebugStringA((char*)p_ps_log_luminance_error_blob->GetBufferPointer());
                p_ps_log_luminance_error_blob->Release();
            }
            if (p_ps_log_luminance_blob) {
                p_ps_log_luminance_blob->Release();
            }
            assert(false);
        }

        ID3DBlob* p_ps_tone_mapping_blob = nullptr, * p_ps_tone_mapping_error_blob = nullptr;
        hr = D3DCompileFromFile(L"../../lab-3/shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psToneMappingMain", "ps_5_0", flags, 0, &p_ps_tone_mapping_blob, &p_ps_tone_mapping_error_blob);
        if (FAILED(hr)) {
            if (p_ps_tone_mapping_error_blob) {
                OutputDebugStringA((char*)p_ps_tone_mapping_error_blob->GetBufferPointer());
                p_ps_tone_mapping_error_blob->Release();
            }
            if (p_ps_tone_mapping_blob) {
                p_ps_tone_mapping_blob->Release();
            }
            assert(false);
        }

        hr = _p_device->CreateVertexShader(_p_vs_blob->GetBufferPointer(), _p_vs_blob->GetBufferSize(), nullptr, &_p_vertex_shader);
        assert(SUCCEEDED(hr));

        hr = _p_device->CreatePixelShader(p_ps_blob->GetBufferPointer(), p_ps_blob->GetBufferSize(), nullptr, &_p_pixel_shader);
        assert(SUCCEEDED(hr));

        hr = _p_device->CreateVertexShader(p_vs_copy_blob->GetBufferPointer(), p_vs_copy_blob->GetBufferSize(), nullptr, &_p_vertex_shader_copy);
        assert(SUCCEEDED(hr));

        hr = _p_device->CreatePixelShader(p_ps_copy_blob->GetBufferPointer(), p_ps_copy_blob->GetBufferSize(), nullptr, &_p_pixel_shader_copy);
        assert(SUCCEEDED(hr));

        hr = _p_device->CreatePixelShader(p_ps_log_luminance_blob->GetBufferPointer(), p_ps_log_luminance_blob->GetBufferSize(), nullptr, &_p_pixel_shader_log_luminance);
        assert(SUCCEEDED(hr));

        hr = _p_device->CreatePixelShader(p_ps_tone_mapping_blob->GetBufferPointer(), p_ps_tone_mapping_blob->GetBufferSize(), nullptr, &_p_pixel_shader_tone_mapping);
        assert(SUCCEEDED(hr));
    }

    void Renderer::initInputLayout() {
        D3D11_INPUT_ELEMENT_DESC input_element_desc[] = {
          { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
          /*{ "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },*/
          { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
          { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        HRESULT hr = _p_device->CreateInputLayout(input_element_desc, ARRAYSIZE(input_element_desc), _p_vs_blob->GetBufferPointer(), _p_vs_blob->GetBufferSize(), &_p_input_layout);
        assert(SUCCEEDED(hr));
    }

    void Renderer::initScene() {
        _borders._min = { -20.0f, -10.0f, -20.0f };
        _borders._max = { 20.0f, 10.0f, 20.0f };

        Sphere sphere(1.0f, 30, 30, true);
        const SimpleVertex *vertices = sphere.getVertices();
        const WORD* indices = sphere.getIndices();

        _indices_number = sphere._n_indices;
        _vertex_stride = sizeof(SimpleVertex);
        _vertex_offset = 0;

        DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.0f, 1.0f, -2.0f, 0.0f);
        DirectX::XMVECTOR dir = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        _camera = Camera(pos, dir);

        _ambient_light = DirectX::XMFLOAT4(0.1, 0.1, 0.1, 0);

        size_t light_sources_number = 3;
        float r = 2.0f, h = 5.0f;
        _lights[0]._pos = { 0.0f, -h, r, 0.0f };
        for (int i = 1; i < light_sources_number; i++) {
            _lights[i]._pos = { r * sinf(i * DirectX::XM_2PI / light_sources_number), h, r * cosf(i * DirectX::XM_2PI / light_sources_number), 0.0f };
        }
        _lights[0]._color = (DirectX::XMFLOAT4)DirectX::Colors::Red;
        _lights[1]._color = (DirectX::XMFLOAT4)DirectX::Colors::Lime;
        _lights[2]._color = (DirectX::XMFLOAT4)DirectX::Colors::Blue;

        {
            D3D11_BUFFER_DESC vertex_buff_descr = {};
            vertex_buff_descr.ByteWidth = sizeof(SimpleVertex) * sphere._n_vertices;
            vertex_buff_descr.Usage = D3D11_USAGE_DEFAULT;
            vertex_buff_descr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA sr_data = { 0 };
            sr_data.pSysMem = vertices;
            HRESULT hr = _p_device->CreateBuffer(&vertex_buff_descr, &sr_data, &_p_vertex_buffer);
            assert(SUCCEEDED(hr));
        }

        {
            D3D11_BUFFER_DESC index_buff_descr = {};
            index_buff_descr.ByteWidth = sizeof(WORD) * _indices_number;
            index_buff_descr.Usage = D3D11_USAGE_DEFAULT;
            index_buff_descr.BindFlags = D3D11_BIND_INDEX_BUFFER;
            index_buff_descr.CPUAccessFlags = 0;
            D3D11_SUBRESOURCE_DATA sr_data = { 0 };
            sr_data.pSysMem = indices;
            HRESULT hr = _p_device->CreateBuffer(&index_buff_descr, &sr_data, &_p_index_buffer);
            assert(SUCCEEDED(hr));
        }

        {
            D3D11_BUFFER_DESC constant_buff_descr = {};
            constant_buff_descr.ByteWidth = sizeof(ConstantBuffer);
            constant_buff_descr.Usage = D3D11_USAGE_DEFAULT;
            constant_buff_descr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            constant_buff_descr.CPUAccessFlags = 0;
            HRESULT hr = _p_device->CreateBuffer(&constant_buff_descr, nullptr, &_p_constant_buffer);
            assert(SUCCEEDED(hr));
        }

        HRESULT hr = DirectX::CreateDDSTextureFromFileEx(_p_device, nullptr, L"../../lab-3/seafloor.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, true, nullptr, &_p_texture_rv);
        assert(SUCCEEDED(hr));

        D3D11_SAMPLER_DESC samp_desc;
        ZeroMemory(&samp_desc, sizeof(samp_desc));
        samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samp_desc.MinLOD = 0;
        samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = _p_device->CreateSamplerState(&samp_desc, &_p_sampler_linear);
        assert(SUCCEEDED(hr));

        {
            HRESULT hr_annotation = _p_device_context->QueryInterface(__uuidof(_p_annotation), reinterpret_cast<void**>(&_p_annotation));
            assert(SUCCEEDED(hr_annotation));
        }

        RECT winRect;
        GetClientRect(_hwnd, &winRect);
        LONG width = winRect.right - winRect.left;
        LONG height = winRect.bottom - winRect.top;
        _viewport = { 0.0f, 0.0f, (FLOAT)(width), (FLOAT)(height), 0.0f, 1.0f };

        // Setup projection
        float near_z = 0.01f, far_z = 100.0f;
        _projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, width / (FLOAT)height, near_z, far_z);

        _render_texture.SetDevice(_p_device);
        _render_texture.SetWindow(winRect);

        size_t n = (size_t)log2(min(width, height));
        _square_copy.SetDevice(_p_device);
        _square_copy.SizeResources(1i64 << n, 1i64 << n);

        _log_luminance_textures.resize(n + 1);
        for (size_t i = 0; i <= n; ++i) {
            _log_luminance_textures[i].SetDevice(_p_device);
            _log_luminance_textures[i].SizeResources(1i64 << (n - i), 1i64 << (n - i));
        }

        CD3D11_TEXTURE2D_DESC average_log_luminance_texture_desc(DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1);
        average_log_luminance_texture_desc.MipLevels = 1;
        average_log_luminance_texture_desc.BindFlags = 0;
        average_log_luminance_texture_desc.Usage = D3D11_USAGE_STAGING;
        average_log_luminance_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        hr = _p_device->CreateTexture2D(&average_log_luminance_texture_desc, nullptr, &_average_log_luminance_texture);
        assert(SUCCEEDED(hr));
    }

    void Renderer::render() {
        auto start = std::chrono::high_resolution_clock::now();

        {
            _p_annotation->BeginEvent(L"Rendering start");

            auto render_texture_render_target_view = _render_texture.GetRenderTargetView();
            _p_device_context->ClearRenderTargetView(render_texture_render_target_view, DirectX::Colors::Black.f);

            _p_device_context->RSSetViewports(1, &_viewport);

            _p_device_context->OMSetRenderTargets(1, &render_texture_render_target_view, nullptr);

            _p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            _p_device_context->IASetInputLayout(_p_input_layout);
            _p_device_context->IASetVertexBuffers(0, 1, &_p_vertex_buffer, &_vertex_stride, &_vertex_offset);
            _p_device_context->IASetIndexBuffer(_p_index_buffer, DXGI_FORMAT_R16_UINT, 0);

            _p_annotation->EndEvent();

            _p_annotation->BeginEvent(L"Setting up camera view");
            _view = _camera.getViewMatrix();
            _p_annotation->EndEvent();

            ConstantBuffer cb;
            cb._world = DirectX::XMMatrixTranspose(_world);
            cb._view = DirectX::XMMatrixTranspose(_view);
            cb._projection = DirectX::XMMatrixTranspose(_projection);
            cb._ambient_light = _ambient_light;
            for (int i = 0; i < 3; i++)
            {
                cb._light_pos[i] = _lights[i]._pos;
                cb._light_color[i] = _lights[i]._color;
                DirectX::XMFLOAT4 att(_lights[i]._const_att, _lights[i]._lin_att, _lights[i]._exp_att, 0.0f);
                cb._light_attenuation[i] = att;
                cb._light_intensity[4 * i] = _lights[i].getIntensity();
            }
            _p_device_context->UpdateSubresource(_p_constant_buffer, 0, nullptr, &cb, 0, 0);

            _p_device_context->VSSetShader(_p_vertex_shader, nullptr, 0);
            _p_device_context->VSSetConstantBuffers(0, 1, &_p_constant_buffer);
            _p_device_context->PSSetShader(_p_pixel_shader, nullptr, 0);
            _p_device_context->PSSetConstantBuffers(0, 1, &_p_constant_buffer);
            _p_device_context->PSSetShaderResources(0, 1, &_p_texture_rv);
            _p_device_context->PSSetSamplers(0, 1, &_p_sampler_linear);

            _p_annotation->BeginEvent(L"Draw");
            _p_device_context->DrawIndexed(_indices_number, 0, 0);
            _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);
            _p_annotation->EndEvent();
        }

        auto render_texture_shader_resource_view = _render_texture.GetShaderResourceView();
        auto square_copy_render_target_view = _square_copy.GetRenderTargetView();

        {
            _p_device_context->ClearRenderTargetView(square_copy_render_target_view, DirectX::Colors::Black.f);

            size_t n = _log_luminance_textures.size() - 1;
            D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << n), FLOAT(1 << n), 0, 1 };
           _p_device_context->RSSetViewports(1, &vp);

            _p_device_context->OMSetRenderTargets(1, &square_copy_render_target_view, nullptr);

            _p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            _p_device_context->IASetInputLayout(nullptr);

            _p_device_context->VSSetShader(_p_vertex_shader_copy, nullptr, 0);
            _p_device_context->PSSetShader(_p_pixel_shader_copy, nullptr, 0);
            _p_device_context->PSSetShaderResources(0, 1, &render_texture_shader_resource_view);
            _p_device_context->PSSetSamplers(0, 1, &_p_sampler_linear);

            _p_device_context->Draw(4, 0);
            _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);
        }

        auto square_copy_shader_resource_view = _square_copy.GetShaderResourceView();
        auto first_log_luminance_texture_render_target_view = _log_luminance_textures.front().GetRenderTargetView();

        {
            _p_device_context->ClearRenderTargetView(first_log_luminance_texture_render_target_view, DirectX::Colors::Black.f);

            size_t n = _log_luminance_textures.size() - 1;
            D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << n), FLOAT(1 << n), 0, 1 };
            _p_device_context->RSSetViewports(1, &vp);

            _p_device_context->OMSetRenderTargets(1, &first_log_luminance_texture_render_target_view, nullptr);

            _p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            _p_device_context->IASetInputLayout(nullptr);

            _p_device_context->VSSetShader(_p_vertex_shader_copy, nullptr, 0);
            _p_device_context->PSSetShader(_p_pixel_shader_log_luminance, nullptr, 0);
            _p_device_context->PSSetShaderResources(0, 1, &square_copy_shader_resource_view);
            _p_device_context->PSSetSamplers(0, 1, &_p_sampler_linear);

            _p_device_context->Draw(4, 0);
            _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);
        }

        {
            size_t n = _log_luminance_textures.size() - 1;
            for (size_t i = 1; i <= n; ++i) {
                auto previous_log_luminance_texture_shader_resource_view = _log_luminance_textures[i - 1].GetShaderResourceView();
                auto next_log_luminance_texture_render_target_view = _log_luminance_textures[i].GetRenderTargetView();

                _p_device_context->ClearRenderTargetView(next_log_luminance_texture_render_target_view, DirectX::Colors::Black.f);

                D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << (n - i)), FLOAT(1 << (n - i)), 0, 1 };
                _p_device_context->RSSetViewports(1, &vp);

                _p_device_context->OMSetRenderTargets(1, &next_log_luminance_texture_render_target_view, nullptr);

                _p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                _p_device_context->IASetInputLayout(nullptr);

                _p_device_context->VSSetShader(_p_vertex_shader_copy, nullptr, 0);
                _p_device_context->PSSetShader(_p_pixel_shader_copy, nullptr, 0);
                _p_device_context->PSSetShaderResources(0, 1, &previous_log_luminance_texture_shader_resource_view);
                _p_device_context->PSSetSamplers(0, 1, &_p_sampler_linear);

                _p_device_context->Draw(4, 0);
                _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);
            }
        }

        {
            _p_device_context->ClearRenderTargetView(_p_render_target_view, DirectX::Colors::Black.f);

            _p_device_context->RSSetViewports(1, &_viewport);

            _p_device_context->OMSetRenderTargets(1, &_p_render_target_view, nullptr);

            _p_device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            _p_device_context->IASetInputLayout(nullptr);

            D3D11_MAPPED_SUBRESOURCE average_log_luminance_mapped_subresource;
            _p_device_context->CopyResource(_average_log_luminance_texture, _log_luminance_textures.back().GetRenderTarget());
            _p_device_context->Map(_average_log_luminance_texture, 0, D3D11_MAP_READ, 0, &average_log_luminance_mapped_subresource);
            float average_log_luminance = ((float*)average_log_luminance_mapped_subresource.pData)[0];
            _p_device_context->Unmap(_average_log_luminance_texture, 0);

            auto end = std::chrono::high_resolution_clock::now();
            float delta_t = std::chrono::duration<float>(end - start).count();
            float s = 1;
            _adapted_log_luminance += (average_log_luminance - _adapted_log_luminance) * (1 - expf(-delta_t / s));

            ConstantBuffer cb;
            cb._adapted_log_luminance = _adapted_log_luminance;

            _p_device_context->UpdateSubresource(_p_constant_buffer, 0, nullptr, &cb, 0, 0);

            _p_device_context->VSSetShader(_p_vertex_shader_copy, nullptr, 0);
            _p_device_context->PSSetShader(_p_pixel_shader_tone_mapping, nullptr, 0);
            _p_device_context->PSSetConstantBuffers(0, 1, &_p_constant_buffer);
            _p_device_context->PSSetShaderResources(0, 1, &render_texture_shader_resource_view);
            _p_device_context->PSSetSamplers(0, 1, &_p_sampler_linear);

            _p_device_context->Draw(4, 0);
            _p_device_context->PSSetShaderResources(0, _s_MAX_NUM_SHADER_RESOURCE_VIEWS, _null_shader_resource_views);

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
                _lights[1].changeIntensity();
            }
            break;
        case (WPARAM)Keys::_3_KEY:
            if (!b30) {
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

            _viewport = { 0, 0, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f };

            float near_z = 0.01f, far_z = 100.0f;
            _projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, (FLOAT)width / (FLOAT)height, near_z, far_z);

            _render_texture.SizeResources(width, height);

            size_t n = (size_t)log2((double)min(width, height));
            _square_copy.SizeResources(1i64 << n, 1i64 << n);
            _log_luminance_textures.resize(n + 1);
            for (size_t i = 0; i <= n; ++i) {
                _log_luminance_textures[i].SetDevice(_p_device);
                _log_luminance_textures[i].SizeResources(1i64 << (n - i), 1i64 << (n - i));
            }
        }
    }

    Renderer::~Renderer() {
        _p_device_context->ClearState();

        _p_constant_buffer->Release();
        _p_vertex_buffer->Release();
        _p_index_buffer->Release();

        _p_input_layout->Release();

        _p_texture_rv->Release();
        _p_sampler_linear->Release();

        _average_log_luminance_texture->Release();

        _p_vertex_shader->Release();
        _p_vertex_shader_copy->Release();
        _p_pixel_shader->Release();
        _p_pixel_shader_copy->Release();
        _p_pixel_shader_log_luminance->Release();
        _p_pixel_shader_tone_mapping->Release();

        _p_depth_stencil->Release();
        _p_depth_stencil_view->Release();

        _p_render_target_view->Release();
        _p_swap_chain->Release();
        _p_device_context->Release();
        _p_device->Release();

        _p_annotation->Release();
    }
}
