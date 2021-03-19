#ifndef UNICODE
#define UNICODE
#endif 

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>       // D3D interface
#include "d3d11_1.h"
#include <dxgi.h>        // DirectX driver interface
#include <d3dcompiler.h> // shader compiler
#include <directxmath.h>
#include <directxcolors.h>

#include "DDSTextureLoader.h"
#include "RenderTexture.h"
#include "../renderdoc_app.h"

#include <assert.h>

#include <chrono>
#include <string>
#include <vector>

#define DEBUG_LAYER

#pragma comment( lib, "user32" )          // link against the win32 library
#pragma comment( lib, "d3d11.lib" )       // direct3D library
#pragma comment( lib, "dxgi.lib" )        // directx graphics interface
#pragma comment( lib, "d3dcompiler.lib" ) // shader compiler
#pragma comment( lib, "dxguid.lib") 

using namespace DX;
using namespace DirectX;
using namespace std;

enum Keys
{
    W_KEY = 87,
    A_KEY = 65,
    S_KEY = 83,
    D_KEY = 68,
    _1_KEY = 49,
    _2_KEY = 50,
    _3_KEY = 51,
};

struct WorldBorders
{
    float x_min, x_max;
    float y_min, y_max;
    float z_min, z_max;
};

struct SimpleVertex
{
    XMFLOAT3 _Pos;
    XMFLOAT3 _Nor;
    XMFLOAT2 _Tex;
};

struct ConstantBuffer
{
    XMMATRIX _World;
    XMMATRIX _View;
    XMMATRIX _Projection;

    XMFLOAT4 _LightPos[3];
    XMFLOAT4 _LightColor[3];
    XMFLOAT4 _LightAttenuation[3];
    float _LightIntensity[9];

    float _AdaptedLogLuminance;
};

class Camera
{
public:
    Camera()
    {
        _pos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        _dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        _up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }

    Camera(XMVECTOR pos, XMVECTOR dir) : _pos(pos)
    {
        _dir = XMVector4Normalize(dir);
        _up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }

    XMMATRIX getViewMatrix()
    {
        XMVECTOR focus = XMVectorAdd(_pos, _dir);
        return XMMatrixLookAtLH(_pos, focus, _up);
    }

    void move(float dx = 0.0f, float dy = 0.0f, float dz = 0.0f)
    {
        _pos = XMVectorSet(XMVectorGetX(_pos) + dx, XMVectorGetY(_pos) + dy, XMVectorGetZ(_pos) + dz, 0.0f);
    }

    void moveNormal(float dn)
    {
        _pos = XMVectorAdd(_pos, XMVectorScale(_dir, dn));
    }

    void moveTangent(float dt)
    {
        XMVECTOR tangent = getTangentVector();
        _pos = XMVectorAdd(_pos, XMVectorScale(tangent, dt));
    }

    void rotateHorisontal(float angle)
    {
        XMVECTOR rotation_quaternion = XMQuaternionRotationAxis(_up, angle);
        _dir = XMVector3Rotate(_dir, rotation_quaternion);
    }

    void rotateVertical(float angle)
    {
        XMVECTOR tangent = getTangentVector();
        angle = min(angle, XM_PIDIV2 - _vertical_angle);
        angle = max(angle, -XM_PIDIV2 - _vertical_angle);
        _vertical_angle += angle;
        XMVECTOR rotation_quaternion = XMQuaternionRotationAxis(-tangent, angle);
        _dir = XMVector3Rotate(_dir, rotation_quaternion);
    }

    void positionClip(WorldBorders b)
    {
        if (XMVectorGetX(_pos) > b.x_max)
            _pos = XMVectorSetX(_pos, b.x_max);
        if (XMVectorGetX(_pos) < b.x_min)
            _pos = XMVectorSetX(_pos, b.x_min);
        if (XMVectorGetY(_pos) > b.y_max)
            _pos = XMVectorSetY(_pos, b.y_max);
        if (XMVectorGetY(_pos) < b.y_min)
            _pos = XMVectorSetY(_pos, b.y_min);
        if (XMVectorGetZ(_pos) > b.z_max)
            _pos = XMVectorSetZ(_pos, b.z_max);
        if (XMVectorGetZ(_pos) < b.z_min)
            _pos = XMVectorSetZ(_pos, b.z_min);
    }

private:
    XMVECTOR getTangentVector()
    {
        return XMVector4Normalize(XMVector3Cross(_dir, _up));
    }

    XMVECTOR _pos, _dir, _up;
    float _vertical_angle = 0.0f;
};

class PointLight
{
public:
    PointLight() 
    { 
        _pos = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        _color = (XMFLOAT4)Colors::White;
    }

    float getIntensity() { return _intensities[_current_index];  }
    void changeIntensity() { _current_index = (_current_index + 1) % 3; }

    XMFLOAT4 _pos;
    XMFLOAT4 _color;
    float _const_att = 0.1f, _lin_att = 1.0f, _exp_att = 1.0f;

private:
    float _intensities[3] = { 1.0f, 10.0f, 100.0f };
    size_t _current_index = 0;
};

ID3D11Device* device_ptr = NULL;
ID3D11DeviceContext* device_context_ptr = NULL;
IDXGISwapChain* swap_chain_ptr = NULL;
ID3D11RenderTargetView* render_target_view_ptr = NULL;
ID3D11ShaderResourceView* texture_rv_ptr = NULL;
ID3D11SamplerState* sampler_linear_ptr = NULL;

RenderTexture render_texture;
RenderTexture square_copy;
vector<RenderTexture> log_luminance_textures;

D3D11_VIEWPORT viewport;

XMMATRIX World = XMMatrixIdentity();
XMMATRIX View = XMMatrixIdentity();
XMMATRIX Projection = XMMatrixIdentity();
XMMATRIX Translation = XMMatrixIdentity();

WorldBorders borders = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
Camera camera;
PointLight lights[3];

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window.

    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Computer Graphics: lab3",     // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC swap_chain_descr = { 0 };
    swap_chain_descr.BufferDesc.RefreshRate.Numerator = 0;
    swap_chain_descr.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_descr.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_descr.SampleDesc.Count = 1;
    swap_chain_descr.SampleDesc.Quality = 0;
    swap_chain_descr.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_descr.BufferCount = 2;
    swap_chain_descr.OutputWindow = hwnd;
    swap_chain_descr.Windowed = true;
    swap_chain_descr.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL feature_level;
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined( DEBUG_LAYER )
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        flags,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &swap_chain_descr,
        &swap_chain_ptr,
        &device_ptr,
        &feature_level,
        &device_context_ptr);
    assert(S_OK == hr && swap_chain_ptr && device_ptr && device_context_ptr);

    ID3D11Texture2D* framebuffer;
    hr = swap_chain_ptr->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&framebuffer);
    assert(SUCCEEDED(hr));

    hr = device_ptr->CreateRenderTargetView(framebuffer, 0, &render_target_view_ptr);
    assert(SUCCEEDED(hr));
    framebuffer->Release();

    flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; // add more debug output
#endif
    ID3DBlob* vs_blob_ptr = NULL, * ps_blob_ptr = NULL, * error_blob = NULL;
    ID3DBlob* vs_copy_blob_ptr = NULL, * ps_copy_blob_ptr = NULL, * copy_error_blob = NULL;
    ID3DBlob* ps_log_luminance_blob_ptr = NULL, * log_luminance_error_blob = NULL;
    ID3DBlob* ps_tone_mapping_blob_ptr = NULL, * tone_mapping_error_blob = NULL;

    // COMPILE VERTEX SHADER
    hr = D3DCompileFromFile(
        L"../../lab-2/shaders.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "vs_main",
        "vs_5_0",
        flags,
        0,
        &vs_blob_ptr,
        &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            OutputDebugStringA((char*)error_blob->GetBufferPointer());
            error_blob->Release();
        }
        if (vs_blob_ptr) { vs_blob_ptr->Release(); }
        assert(false);
    }
    // COMPILE PIXEL SHADER
    hr = D3DCompileFromFile(
        L"../../lab-2/shaders.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "ps_main",
        "ps_5_0",
        flags,
        0,
        &ps_blob_ptr,
        &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            OutputDebugStringA((char*)error_blob->GetBufferPointer());
            error_blob->Release();
        }
        if (ps_blob_ptr) { ps_blob_ptr->Release(); }
        assert(false);
    }

    // COMPILE VERTEX SHADER COPY
    hr = D3DCompileFromFile(
        L"../../lab-2/shaders.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "vs_copy_main",
        "vs_5_0",
        flags,
        0,
        &vs_copy_blob_ptr,
        &copy_error_blob);
    if (FAILED(hr)) {
        if (copy_error_blob) {
            OutputDebugStringA((char*)copy_error_blob->GetBufferPointer());
            copy_error_blob->Release();
        }
        if (vs_copy_blob_ptr) { vs_copy_blob_ptr->Release(); }
        assert(false);
    }
    // COMPILE PIXEL SHADER COPY
    hr = D3DCompileFromFile(
        L"../../lab-2/shaders.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "ps_copy_main",
        "ps_5_0",
        flags,
        0,
        &ps_copy_blob_ptr,
        &copy_error_blob);
    if (FAILED(hr)) {
        if (copy_error_blob) {
            OutputDebugStringA((char*)copy_error_blob->GetBufferPointer());
            copy_error_blob->Release();
        }
        if (ps_copy_blob_ptr) { ps_copy_blob_ptr->Release(); }
        assert(false);
    }

    // COMPILE PIXEL SHADER LOGARITHM OF LUMINANCE
    hr = D3DCompileFromFile(
        L"../../lab-2/shaders.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "ps_log_luminance_main",
        "ps_5_0",
        flags,
        0,
        &ps_log_luminance_blob_ptr,
        &log_luminance_error_blob);
    if (FAILED(hr)) {
        if (log_luminance_error_blob) {
            OutputDebugStringA((char*)log_luminance_error_blob->GetBufferPointer());
            log_luminance_error_blob->Release();
        }
        if (ps_log_luminance_blob_ptr) { ps_log_luminance_blob_ptr->Release(); }
        assert(false);
    }

    // COMPILE PIXEL SHADER TONE MAPPING
    hr = D3DCompileFromFile(
        L"../../lab-2/shaders.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "ps_tone_mapping_main",
        "ps_5_0",
        flags,
        0,
        &ps_tone_mapping_blob_ptr,
        &tone_mapping_error_blob);
    if (FAILED(hr)) {
        if (tone_mapping_error_blob) {
            OutputDebugStringA((char*)tone_mapping_error_blob->GetBufferPointer());
            tone_mapping_error_blob->Release();
        }
        if (ps_tone_mapping_blob_ptr) { ps_tone_mapping_blob_ptr->Release(); }
        assert(false);
    }

    ID3D11VertexShader* vertex_shader_ptr = NULL;
    ID3D11PixelShader* pixel_shader_ptr = NULL;

    hr = device_ptr->CreateVertexShader(
        vs_blob_ptr->GetBufferPointer(),
        vs_blob_ptr->GetBufferSize(),
        NULL,
        &vertex_shader_ptr);
    assert(SUCCEEDED(hr));

    hr = device_ptr->CreatePixelShader(
        ps_blob_ptr->GetBufferPointer(),
        ps_blob_ptr->GetBufferSize(),
        NULL,
        &pixel_shader_ptr);
    assert(SUCCEEDED(hr));

    ID3D11VertexShader* vertex_shader_copy_ptr = NULL;
    ID3D11PixelShader* pixel_shader_copy_ptr = NULL;

    hr = device_ptr->CreateVertexShader(
        vs_copy_blob_ptr->GetBufferPointer(),
        vs_copy_blob_ptr->GetBufferSize(),
        NULL,
        &vertex_shader_copy_ptr);
    assert(SUCCEEDED(hr));

    hr = device_ptr->CreatePixelShader(
        ps_copy_blob_ptr->GetBufferPointer(),
        ps_copy_blob_ptr->GetBufferSize(),
        NULL,
        &pixel_shader_copy_ptr);
    assert(SUCCEEDED(hr));

    ID3D11PixelShader* pixel_shader_log_luminance_ptr = NULL;

    hr = device_ptr->CreatePixelShader(
        ps_log_luminance_blob_ptr->GetBufferPointer(),
        ps_log_luminance_blob_ptr->GetBufferSize(),
        NULL,
        &pixel_shader_log_luminance_ptr);
    assert(SUCCEEDED(hr));

    ID3D11PixelShader* pixel_shader_tone_mapping_ptr = NULL;

    hr = device_ptr->CreatePixelShader(
        ps_tone_mapping_blob_ptr->GetBufferPointer(),
        ps_tone_mapping_blob_ptr->GetBufferSize(),
        NULL,
        &pixel_shader_tone_mapping_ptr);
    assert(SUCCEEDED(hr));

    CD3D11_TEXTURE2D_DESC average_log_luminance_texture_desc(DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1);
    average_log_luminance_texture_desc.MipLevels = 1;
    average_log_luminance_texture_desc.BindFlags = 0;
    average_log_luminance_texture_desc.Usage = D3D11_USAGE_STAGING;
    average_log_luminance_texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D* average_log_luminance_texture;
    hr = device_ptr->CreateTexture2D(&average_log_luminance_texture_desc, NULL, &average_log_luminance_texture);
    assert(SUCCEEDED(hr));

    ID3D11InputLayout* input_layout_ptr = NULL;
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
      { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      /*{ "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },*/
      { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device_ptr->CreateInputLayout(
        inputElementDesc,
        ARRAYSIZE(inputElementDesc),
        vs_blob_ptr->GetBufferPointer(),
        vs_blob_ptr->GetBufferSize(),
        &input_layout_ptr);
    assert(SUCCEEDED(hr));


    // setup geometry
    borders.x_min = -20.0f;
    borders.x_max = 20.0f;
    borders.y_min = 0.2f;
    borders.y_max = 10.0f;
    borders.z_min = -20.0f;
    borders.z_max = 20.0f;
    SimpleVertex vertices[] =

    {
        {XMFLOAT3(borders.x_min, 0.0f, borders.z_min), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f)},
        {XMFLOAT3(borders.x_max, 0.0f, borders.z_min), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(10.0f, 0.0f)},
        {XMFLOAT3(borders.x_max, 0.0f, borders.z_max), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(10.0f, 10.0f)},
        {XMFLOAT3(borders.x_min, 0.0f, borders.z_max), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 10.0f)},
    };

    WORD indices[] =
    {
        3, 1, 0,
        2, 1, 3,
    };

    UINT vertices_number = sizeof(vertices) / sizeof(vertices[0]);
    UINT indices_number = sizeof(indices) / sizeof(indices[0]);
    UINT vertex_stride = sizeof(SimpleVertex);
    UINT vertex_offset = 0;

    // setup camera
    XMVECTOR pos = XMVectorSet(0.0f, 1.0f, -2.0f, 0.0f);
    XMVECTOR dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    camera = Camera(pos, dir);

    // setup light sources
    size_t light_sources_number = 3;
    float r = 2.0f, h = 5.0f;
    for (int i = 0; i < light_sources_number; i++)
        lights[i]._pos = XMFLOAT4(r * sinf(i *XM_2PI / light_sources_number), h, r * cosf(i * XM_2PI / light_sources_number), 0.0f);
    lights[0]._color = (XMFLOAT4)Colors::Red;
    lights[1]._color = (XMFLOAT4)Colors::Lime;
    lights[2]._color = (XMFLOAT4)Colors::Blue;
  

    ID3D11Buffer* vertex_buffer_ptr = NULL;
    {   /*** load mesh data into vertex buffer **/
        D3D11_BUFFER_DESC vertex_buff_descr = {};
        vertex_buff_descr.ByteWidth = sizeof(vertices);
        vertex_buff_descr.Usage = D3D11_USAGE_DEFAULT;
        vertex_buff_descr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sr_data = { 0 };
        sr_data.pSysMem = vertices;
        HRESULT hr = device_ptr->CreateBuffer(
            &vertex_buff_descr,
            &sr_data,
            &vertex_buffer_ptr);
        assert(SUCCEEDED(hr));
    }
    ID3D11Buffer* index_buffer_ptr = NULL;
    {
        D3D11_BUFFER_DESC index_buff_descr = {};
        index_buff_descr.ByteWidth = sizeof(indices);
        index_buff_descr.Usage = D3D11_USAGE_DEFAULT;
        index_buff_descr.BindFlags = D3D11_BIND_INDEX_BUFFER;
        index_buff_descr.CPUAccessFlags = 0;
        D3D11_SUBRESOURCE_DATA sr_data = { 0 };
        sr_data.pSysMem = indices;
        HRESULT hr = device_ptr->CreateBuffer(
            &index_buff_descr,
            &sr_data,
            &index_buffer_ptr);
        assert(SUCCEEDED(hr));
    }

    // Create the constant buffer
    ID3D11Buffer* constant_buffer_ptr = NULL;
    {
        D3D11_BUFFER_DESC constant_buff_descr = {};
        constant_buff_descr.ByteWidth = sizeof(ConstantBuffer);
        constant_buff_descr.Usage = D3D11_USAGE_DEFAULT;
        constant_buff_descr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constant_buff_descr.CPUAccessFlags = 0;
        HRESULT hr = device_ptr->CreateBuffer(
            &constant_buff_descr,
            nullptr,
            &constant_buffer_ptr);
        assert(SUCCEEDED(hr));
    }

    // Load the Texture
    hr = CreateDDSTextureFromFileEx(device_ptr,
        nullptr,
        L"../../lab-2/seafloor.dds",
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        true,
        nullptr,
        &texture_rv_ptr);
    assert(SUCCEEDED(hr));

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_ptr->CreateSamplerState(&sampDesc, &sampler_linear_ptr);
    assert(SUCCEEDED(hr));

    ID3DUserDefinedAnnotation* pAnnotation = nullptr;
    {
        HRESULT hr_annotation = device_context_ptr->QueryInterface(__uuidof(pAnnotation), reinterpret_cast<void**>(&pAnnotation));
        assert(SUCCEEDED(hr_annotation));
    }

    RECT winRect;
    GetClientRect(hwnd, &winRect);
    LONG width = winRect.right - winRect.left;
    LONG height = winRect.bottom - winRect.top;
    viewport = { 0.0f, 0.0f, (FLOAT)(width), (FLOAT)(height), 0.0f, 1.0f };

    // Setup projection
    float near_z = 0.01f, far_z = 100.0f;
    Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, near_z, far_z);

    render_texture.SetDevice(device_ptr);
    render_texture.SetWindow(winRect);

    size_t n = (size_t)log2(min(width, height));
    square_copy.SetDevice(device_ptr);
    square_copy.SizeResources(1i64 << n, 1i64 << n);

    log_luminance_textures.resize(n + 1);
    for (size_t i = 0; i <= n; ++i) {
        log_luminance_textures[i].SetDevice(device_ptr);
        log_luminance_textures[i].SizeResources(1i64 << (n - i), 1i64 << (n - i));
    }

    ID3D11ShaderResourceView* const null[128] = { NULL };

    float adapted_log_luminance = 0;

    // Run the message loop.

    MSG msg = {};
    bool should_close = false;
    while (!should_close) {
        /**** handle user input and other window events ****/
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (msg.message == WM_QUIT) { break; }

        {   /*** RENDER A FRAME ***/

            auto start = std::chrono::high_resolution_clock::now();

            {
                pAnnotation->BeginEvent(L"Rendering start");

                /* clear the back buffer to black for the new frame */
                auto render_texture_render_target_view = render_texture.GetRenderTargetView();
                device_context_ptr->ClearRenderTargetView(render_texture_render_target_view, Colors::Black.f);

                /**** Rasteriser state - set viewport area *****/
                device_context_ptr->RSSetViewports(1, &viewport);

                /**** Output Merger *****/
                device_context_ptr->OMSetRenderTargets(1, &render_texture_render_target_view, NULL);

                /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
                device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                device_context_ptr->IASetInputLayout(input_layout_ptr);
                device_context_ptr->IASetVertexBuffers(0, 1, &vertex_buffer_ptr, &vertex_stride, &vertex_offset);
                device_context_ptr->IASetIndexBuffer(index_buffer_ptr, DXGI_FORMAT_R16_UINT, 0);

                pAnnotation->EndEvent();

                pAnnotation->BeginEvent(L"Setting up camera view");
                View = camera.getViewMatrix();

                pAnnotation->EndEvent();


                //
                // Update variables
                //
                ConstantBuffer cb;
                cb._World = XMMatrixTranspose(World);
                cb._View = XMMatrixTranspose(View);
                cb._Projection = XMMatrixTranspose(Projection);
                for (int i = 0; i < 3; i++)
                {
                    cb._LightPos[i] = lights[i]._pos;
                    cb._LightColor[i] = lights[i]._color;
                    XMFLOAT4 att(lights[i]._const_att, lights[i]._lin_att, lights[i]._exp_att, 0.0f);
                    cb._LightAttenuation[i] = att;
                    cb._LightIntensity[4 * i] = lights[i].getIntensity();
                }
                device_context_ptr->UpdateSubresource(constant_buffer_ptr, 0, nullptr, &cb, 0, 0);

                /*** set vertex shader to use and pixel shader to use, and constant buffers for each ***/
                device_context_ptr->VSSetShader(vertex_shader_ptr, NULL, 0);
                device_context_ptr->VSSetConstantBuffers(0, 1, &constant_buffer_ptr);
                device_context_ptr->PSSetShader(pixel_shader_ptr, NULL, 0);
                device_context_ptr->PSSetConstantBuffers(0, 1, &constant_buffer_ptr);
                device_context_ptr->PSSetShaderResources(0, 1, &texture_rv_ptr);
                device_context_ptr->PSSetSamplers(0, 1, &sampler_linear_ptr);

                pAnnotation->BeginEvent(L"Draw");

                /*** draw the vertex buffer with the shaders ****/
                device_context_ptr->DrawIndexed(indices_number, 0, 0);
                device_context_ptr->PSSetShaderResources(0, 128, null);

                pAnnotation->EndEvent();
            }

            auto render_texture_shader_resource_view = render_texture.GetShaderResourceView();

            auto square_copy_render_target_view = square_copy.GetRenderTargetView();

            {
                device_context_ptr->ClearRenderTargetView(square_copy_render_target_view, Colors::Black.f);

                /**** Rasteriser state - set viewport area *****/
                size_t n = log_luminance_textures.size() - 1;
                D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << n), FLOAT(1 << n), 0, 1 };
                device_context_ptr->RSSetViewports(1, &vp);

                /**** Output Merger *****/
                device_context_ptr->OMSetRenderTargets(1, &square_copy_render_target_view, NULL);

                /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
                device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                device_context_ptr->IASetInputLayout(nullptr);

                /*** set vertex shader to use and pixel shader to use, and constant buffers for each ***/
                device_context_ptr->VSSetShader(vertex_shader_copy_ptr, NULL, 0);
                device_context_ptr->PSSetShader(pixel_shader_copy_ptr, NULL, 0);
                device_context_ptr->PSSetShaderResources(0, 1, &render_texture_shader_resource_view);
                device_context_ptr->PSSetSamplers(0, 1, &sampler_linear_ptr);

                device_context_ptr->Draw(4, 0);
                device_context_ptr->PSSetShaderResources(0, 128, null);
            }

            auto square_copy_shader_resource_view = square_copy.GetShaderResourceView();
            auto first_log_luminance_texture_render_target_view = log_luminance_textures.front().GetRenderTargetView();

            {
                device_context_ptr->ClearRenderTargetView(first_log_luminance_texture_render_target_view, Colors::Black.f);

                /**** Rasteriser state - set viewport area *****/
                size_t n = log_luminance_textures.size() - 1;
                D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << n), FLOAT(1 << n), 0, 1 };
                device_context_ptr->RSSetViewports(1, &vp);

                /**** Output Merger *****/
                device_context_ptr->OMSetRenderTargets(1, &first_log_luminance_texture_render_target_view, NULL);

                /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
                device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                device_context_ptr->IASetInputLayout(nullptr);

                /*** set vertex shader to use and pixel shader to use, and constant buffers for each ***/
                device_context_ptr->VSSetShader(vertex_shader_copy_ptr, NULL, 0);
                device_context_ptr->PSSetShader(pixel_shader_log_luminance_ptr, NULL, 0);
                device_context_ptr->PSSetShaderResources(0, 1, &square_copy_shader_resource_view);
                device_context_ptr->PSSetSamplers(0, 1, &sampler_linear_ptr);

                device_context_ptr->Draw(4, 0);
                device_context_ptr->PSSetShaderResources(0, 128, null);
            }

            {
                size_t n = log_luminance_textures.size() - 1;
                for (size_t i = 1; i <= n; ++i) {
                    auto previous_log_luminance_texture_shader_resource_view = log_luminance_textures[i - 1].GetShaderResourceView();
                    auto next_log_luminance_texture_render_target_view = log_luminance_textures[i].GetRenderTargetView();

                    device_context_ptr->ClearRenderTargetView(next_log_luminance_texture_render_target_view, Colors::Black.f);

                    /**** Rasteriser state - set viewport area *****/
                    D3D11_VIEWPORT vp = { 0, 0, FLOAT(1 << (n - i)), FLOAT(1 << (n - i)), 0, 1 };
                    device_context_ptr->RSSetViewports(1, &vp);

                    /**** Output Merger *****/
                    device_context_ptr->OMSetRenderTargets(1, &next_log_luminance_texture_render_target_view, NULL);

                    /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
                    device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                    device_context_ptr->IASetInputLayout(nullptr);

                    /*** set vertex shader to use and pixel shader to use, and constant buffers for each ***/
                    device_context_ptr->VSSetShader(vertex_shader_copy_ptr, NULL, 0);
                    device_context_ptr->PSSetShader(pixel_shader_copy_ptr, NULL, 0);
                    device_context_ptr->PSSetShaderResources(0, 1, &previous_log_luminance_texture_shader_resource_view);
                    device_context_ptr->PSSetSamplers(0, 1, &sampler_linear_ptr);

                    device_context_ptr->Draw(4, 0);
                    device_context_ptr->PSSetShaderResources(0, 128, null);
                }
            }

            {
                device_context_ptr->ClearRenderTargetView(render_target_view_ptr, Colors::Black.f);

                /**** Rasteriser state - set viewport area *****/
                device_context_ptr->RSSetViewports(1, &viewport);

                /**** Output Merger *****/
                device_context_ptr->OMSetRenderTargets(1, &render_target_view_ptr, NULL);

                /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
                device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                device_context_ptr->IASetInputLayout(nullptr);

                D3D11_MAPPED_SUBRESOURCE average_log_luminance_mapped_subresource;
                device_context_ptr->CopyResource(average_log_luminance_texture, log_luminance_textures.back().GetRenderTarget());
                device_context_ptr->Map(average_log_luminance_texture, 0, D3D11_MAP_READ, 0, &average_log_luminance_mapped_subresource);
                float average_log_luminance = ((float*)average_log_luminance_mapped_subresource.pData)[0];
                device_context_ptr->Unmap(average_log_luminance_texture, 0);

                auto end = std::chrono::high_resolution_clock::now();
                float delta_t = std::chrono::duration<float>(end - start).count();
                float s = 1;
                adapted_log_luminance += (average_log_luminance - adapted_log_luminance) * (1 - expf(-delta_t / s));

                ConstantBuffer cb;
                cb._AdaptedLogLuminance = adapted_log_luminance;

                device_context_ptr->UpdateSubresource(constant_buffer_ptr, 0, nullptr, &cb, 0, 0);

                /*** set vertex shader to use and pixel shader to use, and constant buffers for each ***/
                device_context_ptr->VSSetShader(vertex_shader_copy_ptr, NULL, 0);
                device_context_ptr->PSSetShader(pixel_shader_tone_mapping_ptr, NULL, 0);
                device_context_ptr->PSSetConstantBuffers(0, 1, &constant_buffer_ptr);
                device_context_ptr->PSSetShaderResources(0, 1, &render_texture_shader_resource_view);
                device_context_ptr->PSSetSamplers(0, 1, &sampler_linear_ptr);

                device_context_ptr->Draw(4, 0);
                device_context_ptr->PSSetShaderResources(0, 128, null);

                /**** swap the back and front buffers (show the frame we just drew) ****/
                swap_chain_ptr->Present(1, 0);
            }
        } // end of frame
    } // end of main loop

    device_context_ptr->ClearState();

    constant_buffer_ptr->Release();
    vertex_buffer_ptr->Release();
    index_buffer_ptr->Release();
    input_layout_ptr->Release();
    texture_rv_ptr->Release();
    sampler_linear_ptr->Release();
    vertex_shader_ptr->Release();
    vertex_shader_copy_ptr->Release();
    pixel_shader_ptr->Release();
    pixel_shader_copy_ptr->Release();
    pixel_shader_log_luminance_ptr->Release();
    pixel_shader_tone_mapping_ptr->Release();
    render_target_view_ptr->Release();
    swap_chain_ptr->Release();
    device_context_ptr->Release();
    device_ptr->Release();
    average_log_luminance_texture->Release();

    pAnnotation->Release();

    return 0;
}

LRESULT keyhandler(WPARAM wParam, LPARAM lParam)
{
    LPARAM mask_0_15 = 65535, mask_30 = 1 << 30;
    // The number of times the keystroke is autorepeated as a result of the user holding down the key.
    LPARAM b0_15 = lParam & mask_0_15; 
    // The value is 1 if the key is down before the message is sent, or it is zero if the key is up.
    LPARAM b30 = (lParam & mask_30) != 0;
    float step = 0.1f;
    switch (wParam)
    {
    case W_KEY:
        camera.moveNormal(step);
        camera.positionClip(borders);
        break;
    case S_KEY:
        camera.moveNormal(-step);
        camera.positionClip(borders);
        break;
    case A_KEY:
        camera.moveTangent(step);
        camera.positionClip(borders);
        break;
    case D_KEY:
        camera.moveTangent(-step);
        camera.positionClip(borders);
        break;
    case _1_KEY:
        if (!b30)
            lights[0].changeIntensity();
        break;
    case _2_KEY:
        if (!b30)
            lights[1].changeIntensity();
        break;
    case _3_KEY:
        if (!b30)
            lights[2].changeIntensity();
        break;
    default:
        break;
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static POINT cursor;
    static float mouse_sence = 5e-3f;
    static bool cursor_hidden = false;
    switch (uMsg)
    {
    case WM_KEYDOWN:
        return keyhandler(wParam, lParam);

    case WM_LBUTTONDOWN:
        while (ShowCursor(false) >= 0);
        cursor_hidden = true;
        GetCursorPos(&cursor);
        return 0;

    case WM_MOUSEMOVE:
        if(wParam == MK_LBUTTON)
        {
            POINT current_pos;
            int dx, dy;
            GetCursorPos(&current_pos);
            dx = current_pos.x - cursor.x;
            dy = current_pos.y - cursor.y;
            SetCursorPos(cursor.x, cursor.y);
            camera.rotateHorisontal(dx * mouse_sence);
            camera.rotateVertical(dy * mouse_sence);
        }
        else if(cursor_hidden)
        {
            while (ShowCursor(true) <= 0);
            cursor_hidden = false;
        }
        return 0;

    case WM_LBUTTONUP:
        SetCursorPos(cursor.x, cursor.y);
        while (ShowCursor(true) <= 0);
        cursor_hidden = false;
        return 0;

    case WM_SIZE:
        if (swap_chain_ptr)
        {
            size_t width = LOWORD(lParam);
            size_t height = HIWORD(lParam);

            device_context_ptr->OMSetRenderTargets(0, 0, 0);

            // Release all outstanding references to the swap chain's buffers.
            render_target_view_ptr->Release();

            device_context_ptr->Flush();

            HRESULT hr;
            // Preserve the existing buffer count and format.
            // Automatically choose the width and height to match the client rect for HWNDs.
            hr = swap_chain_ptr->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            assert(SUCCEEDED(hr));

            // Get buffer and create a render-target-view.
            std::string textureName = "Texture";

            ID3D11Texture2D* pBuffer;

            hr = swap_chain_ptr->GetBuffer(0, __uuidof(ID3D11Texture2D),
                (void**)&pBuffer);
            assert(SUCCEEDED(hr));

            pBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)textureName.size(), textureName.c_str());

            hr = device_ptr->CreateRenderTargetView(pBuffer, NULL,
                &render_target_view_ptr);
            assert(SUCCEEDED(hr));

            pBuffer->Release();

            device_context_ptr->OMSetRenderTargets(1, &render_target_view_ptr, NULL);

            // Set up the viewport.
            viewport = { 0, 0, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f };

            // Setup projection
            float near_z = 0.01f, far_z = 100.0f;
            Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, (FLOAT)width / (FLOAT)height, near_z, far_z);

            render_texture.SizeResources(width, height);

            size_t n = (size_t)log2((double)min(width, height));
            square_copy.SizeResources(1i64 << n, 1i64 << n);
            log_luminance_textures.resize(n + 1);
            for (size_t i = 0; i <= n; ++i) {
                log_luminance_textures[i].SizeResources(1i64 << (n - i), 1i64 << (n - i));
            }
        }
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
