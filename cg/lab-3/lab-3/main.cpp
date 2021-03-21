#include "Renderer.h"

rendering::Renderer renderer;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT u_msg, WPARAM w_param, LPARAM l_param);

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, PWSTR p_cmd_line, int n_cmd_show)
{
    renderer.init(h_instance, WindowProc, n_cmd_show);

    MSG msg = {};
    bool should_close = false;
    while (!should_close) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT) {
            break;
        }

        renderer.render();
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static POINT cursor;
    static bool cursor_hidden = false;
    switch (uMsg)
    {
    case WM_KEYDOWN:
        renderer.handleKey(wParam, lParam);
        return 0;

    case WM_LBUTTONDOWN:
        while (ShowCursor(false) >= 0);
        cursor_hidden = true;
        GetCursorPos(&cursor);
        return 0;

    case WM_MOUSEMOVE:
        if(wParam == MK_LBUTTON) {
            renderer.handleMouse(cursor);
        } else if(cursor_hidden) {
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
        renderer.resize(wParam, lParam);
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
