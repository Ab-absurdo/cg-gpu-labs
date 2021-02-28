#ifndef UNICODE
#define UNICODE
#endif 

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>       // D3D interface
#include "d3d11_1.h"
#include <dxgi.h>        // DirectX driver interface
#include <d3dcompiler.h> // shader compiler
#include <directxmath.h>
#include <directxcolors.h>

#include "../renderdoc_app.h"

#include <assert.h>

#include <string>

#define DEBUG_LAYER

#pragma comment( lib, "user32" )          // link against the win32 library
#pragma comment( lib, "d3d11.lib" )       // direct3D library
#pragma comment( lib, "dxgi.lib" )        // directx graphics interface
#pragma comment( lib, "d3dcompiler.lib" ) // shader compiler
#pragma comment( lib, "dxguid.lib") 

using namespace DirectX;

enum Keys
{
    W_KEY = 87,
    A_KEY = 65,
    S_KEY = 83,
    D_KEY = 68
};

struct SimpleVertex
{
    XMFLOAT3 _Pos;
    XMFLOAT4 _Col;
};

struct ConstantBuffer
{
    XMMATRIX _World;
    XMMATRIX _View;
    XMMATRIX _Projection;
    XMMATRIX _Translation;
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
        XMVECTOR tangent = XMVector4Normalize(XMVector3Cross(_dir, _up));
        _pos = XMVectorAdd(_pos, XMVectorScale(tangent, dt));
    }

    void rotateHorisontal(float angle)
    {
        XMVECTOR rotation_quaternion = XMQuaternionRotationAxis(_up, angle);
        _dir = XMVector3Rotate(_dir, rotation_quaternion);
    }

    void rotateVertical(float angle)
    {
        XMVECTOR tangent = -XMVector4Normalize(XMVector3Cross(_dir, _up));
        angle = min(angle, XM_PIDIV2 - _vertical_angle);
        angle = max(angle, -XM_PIDIV2 - _vertical_angle);
        _vertical_angle += angle;
        XMVECTOR rotation_quaternion = XMQuaternionRotationAxis(tangent, angle);
        _dir = XMVector3Rotate(_dir, rotation_quaternion);
    }

private:
    XMVECTOR _pos, _dir, _up;
    float _vertical_angle = 0.0f;
};

ID3D11Device* device_ptr = NULL;
ID3D11DeviceContext* device_context_ptr = NULL;
IDXGISwapChain* swap_chain_ptr = NULL;
ID3D11RenderTargetView* render_target_view_ptr = NULL;

XMMATRIX World = XMMatrixIdentity();
XMMATRIX View = XMMatrixIdentity();
XMMATRIX Projection = XMMatrixIdentity();
XMMATRIX Translation = XMMatrixIdentity();

Camera camera;

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
        L"Computer Graphics: lab2",     // Window text
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
    swap_chain_descr.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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

    ID3D11InputLayout* input_layout_ptr = NULL;
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
      { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      /*
      { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      */
    };
    hr = device_ptr->CreateInputLayout(
        inputElementDesc,
        ARRAYSIZE(inputElementDesc),
        vs_blob_ptr->GetBufferPointer(),
        vs_blob_ptr->GetBufferSize(),
        &input_layout_ptr);
    assert(SUCCEEDED(hr));

    SimpleVertex vertices[] =
    {
        {XMFLOAT3(0.0f, 0.0f, 0.0f), (XMFLOAT4)Colors::Red},
        {XMFLOAT3(0.0f, 0.0f, 1.0f), (XMFLOAT4)Colors::Green},
        {XMFLOAT3(1.0f, 0.0f, 0.0f), (XMFLOAT4)Colors::Blue},
        {XMFLOAT3(0.5f, 0.5f, 0.5f), (XMFLOAT4)Colors::White},
    };

    WORD indices[] =
    {
        0, 1, 3,
        1, 2, 3,
        2, 0, 3,
        2, 1, 0
    };

    UINT vertices_number = sizeof(vertices) / sizeof(vertices[0]);
    UINT indices_number = sizeof(indices) / sizeof(indices[0]);
    UINT vertex_stride = sizeof(SimpleVertex);
    UINT vertex_offset = 0;

    // Initialize the world matrix
    World = XMMatrixRotationY(XM_PI * -0.3);
    World *= XMMatrixRotationX(XM_PI * -0.1);

    // Initialize the camera
    XMVECTOR pos = XMVectorSet(0.0f, 1.0f, -2.0f, 0.0f);
    XMVECTOR dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    camera = Camera(pos, dir);

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
        constant_buff_descr.ByteWidth = sizeof(ConstantBuffer);;
        constant_buff_descr.Usage = D3D11_USAGE_DEFAULT;
        constant_buff_descr.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constant_buff_descr.CPUAccessFlags = 0;
        HRESULT hr = device_ptr->CreateBuffer(
            &constant_buff_descr,
            nullptr,
            &constant_buffer_ptr);
        assert(SUCCEEDED(hr));
    }

    ID3DUserDefinedAnnotation* pAnnotation = nullptr;
    {
        HRESULT hr_annotation = device_context_ptr->QueryInterface(__uuidof(pAnnotation), reinterpret_cast<void**>(&pAnnotation));
        assert(SUCCEEDED(hr_annotation));
    }

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

            pAnnotation->BeginEvent(L"Rendering start");

            /* clear the back buffer to cornflower blue for the new frame */
            float background_colour[4] = { 0x64 / 255.0f, 0x95 / 255.0f, 0xED / 255.0f, 1.0f };
            device_context_ptr->ClearRenderTargetView(render_target_view_ptr, background_colour);

            /**** Rasteriser state - set viewport area *****/
            RECT winRect;
            GetClientRect(hwnd, &winRect);
            LONG width = winRect.right - winRect.left;
            LONG height = winRect.bottom - winRect.top;
            D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (FLOAT)(width), (FLOAT)(height), 0.0f, 1.0f }; device_context_ptr->RSSetViewports(1, &viewport);

            /**** Output Merger *****/
            device_context_ptr->OMSetRenderTargets(1, &render_target_view_ptr, NULL);

            /***** Input Assembler (map how the vertex shader inputs should be read from vertex buffer) ******/
            device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            device_context_ptr->IASetInputLayout(input_layout_ptr);
            device_context_ptr->IASetVertexBuffers(0, 1, &vertex_buffer_ptr, &vertex_stride, &vertex_offset);
            device_context_ptr->IASetIndexBuffer(index_buffer_ptr, DXGI_FORMAT_R16_UINT, 0);

            pAnnotation->EndEvent();

            pAnnotation->BeginEvent(L"Setting up camera view");
            View = camera.getViewMatrix();

            pAnnotation->EndEvent();

            pAnnotation->BeginEvent(L"Setting up projection");
            // Setup projection	
            float near_z = 0.01f, far_z = 10.0f;
            Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, near_z, far_z);
            
            pAnnotation->EndEvent();



            //
            // Update variables
            //
            ConstantBuffer cb;
            cb._World = XMMatrixTranspose(World);
            cb._View = XMMatrixTranspose(View);
            cb._Projection = XMMatrixTranspose(Projection);
            cb._Translation = XMMatrixTranspose(Translation);
            device_context_ptr->UpdateSubresource(constant_buffer_ptr, 0, nullptr, &cb, 0, 0);

            /*** set vertex shader to use and pixel shader to use, and constant buffers for each ***/
            device_context_ptr->VSSetShader(vertex_shader_ptr, NULL, 0);
            device_context_ptr->VSSetConstantBuffers(0, 1, &constant_buffer_ptr);
            device_context_ptr->PSSetShader(pixel_shader_ptr, NULL, 0);

            pAnnotation->BeginEvent(L"Draw");

            /*** draw the vertex buffer with the shaders ****/
            device_context_ptr->DrawIndexed(indices_number, 0, 0);

            pAnnotation->EndEvent();

            /**** swap the back and front buffers (show the frame we just drew) ****/
            swap_chain_ptr->Present(1, 0);
        } // end of frame
    } // end of main loop

    device_context_ptr->ClearState();

    constant_buffer_ptr->Release();
    vertex_buffer_ptr->Release();
    index_buffer_ptr->Release();
    input_layout_ptr->Release();
    vertex_shader_ptr->Release();
    pixel_shader_ptr->Release();
    render_target_view_ptr->Release();
    swap_chain_ptr->Release();
    device_context_ptr->Release();
    device_ptr->Release();

    pAnnotation->Release();

    return 0;
}

LRESULT keyhandler(WPARAM wParam, LPARAM lParam)
{
    // LPARAM mask_0_15 = 65535, mask_30 = 1 << 30;
    // The number of times the keystroke is autorepeated as a result of the user holding down the key.
    // LPARAM b0_15 = lParam & mask_0_15; 
    // The value is 1 if the key is down before the message is sent, or it is zero if the key is up.
    // LPARAM b30 = (lParam & mask_30) != 0;
    switch (wParam)
    {
    case W_KEY:
        camera.moveNormal(0.1f);
        break;
    case S_KEY:
        camera.moveNormal(-0.1f);
        break;
    case A_KEY:
        camera.moveTangent(0.1f);
        break;
    case D_KEY:
        camera.moveTangent(-0.1f);
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
        while (ShowCursor(false) > 0);
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
            while (ShowCursor(true) < 0);
            cursor_hidden = false;
        }
        return 0;

    case WM_LBUTTONUP:
        SetCursorPos(cursor.x, cursor.y);
        while (ShowCursor(true) < 0);
        cursor_hidden = false;
        return 0;

    case WM_SIZE:
        if (swap_chain_ptr)
        {
            FLOAT width = LOWORD(lParam);
            FLOAT height = HIWORD(lParam);

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
            D3D11_VIEWPORT vp;
            vp.Width = width;
            vp.Height = height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            device_context_ptr->RSSetViewports(1, &vp);
        }
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
