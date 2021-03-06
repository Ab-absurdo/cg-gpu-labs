#include "ImGui/imgui_impl_win32.h"

#include "Renderer.h"

static rendering::Renderer s_g_renderer;

LRESULT CALLBACK windowProc(HWND hwnd, UINT u_msg, WPARAM w_param, LPARAM l_param);

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, PWSTR p_cmd_line, int n_cmd_show) {
    s_g_renderer.init(h_instance, windowProc, n_cmd_show);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        s_g_renderer.render();
    }

    return (int)msg.wParam;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK windowProc(HWND hwnd, UINT u_msg, WPARAM w_param, LPARAM l_param) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, u_msg, w_param, l_param)) {
        return true;
    }

    static POINT s_cursor;
    static bool s_cursor_hidden = false;
    switch (u_msg) {
    case WM_KEYDOWN:
        s_g_renderer.handleKey(w_param, l_param);
        break;

    case WM_LBUTTONDOWN:
        if (ImGui::GetIO().WantCaptureMouse) {
            break;
        }
        while (ShowCursor(false) >= 0);
        s_cursor_hidden = true;
        GetCursorPos(&s_cursor);
        break;

    case WM_MOUSEMOVE:
        if (ImGui::GetIO().WantCaptureMouse) {
            break;
        }
        if (w_param == MK_LBUTTON) {
            POINT current_pos;
            GetCursorPos(&current_pos);
            int dx = current_pos.x - s_cursor.x;
            int dy = current_pos.y - s_cursor.y;
            s_g_renderer.handleMouse(dx, dy);
            SetCursorPos(s_cursor.x, s_cursor.y);
        } else if (s_cursor_hidden) {
            while (ShowCursor(true) <= 0);
            s_cursor_hidden = false;
        }
        break;

    case WM_LBUTTONUP:
        if (ImGui::GetIO().WantCaptureMouse) {
            break;
        }
        SetCursorPos(s_cursor.x, s_cursor.y);
        while (ShowCursor(true) <= 0);
        s_cursor_hidden = false;
        break;

    case WM_SIZE:
        {
            size_t width = LOWORD(l_param);
            size_t height = HIWORD(l_param);
            s_g_renderer.resize(width, height);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, u_msg, w_param, l_param);
    }

    return 0;
}
