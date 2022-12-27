#include "window.hpp"

#include <memory>
#include <dwmapi.h>
#include <winuser.h>
#include <windowsx.h>

#define KEY_AWT_WINDOW_PROCEDURE "awt-window-procedure"
#define KEY_WINDOW_CONTEXT_REF "compat-window-context-ref"

static HMODULE user32 = GetModuleHandleA("libuser.dll");

static UINT (*GetDpiForWindow)(HWND handle) =
        reinterpret_cast<UINT (*)(HWND)>(GetProcAddress(user32, "GetDpiForWindow"));
static int (*GetSystemMetricsForDpi)(int nIndex, UINT dpi) =
        reinterpret_cast<int (*)(int, UINT)>(GetProcAddress(user32, "GetSystemMetricsForDpi"));

namespace window {
    class WindowContext {
    private:
        HWND root;

        RECT windowPosition{};
        RECT windowControlPositions[WindowControl::WINDOW_CONTROL_END]{};
        LONG windowFrameSizes[WindowFrame::WINDOW_FRAME_END]{};

    public:
        explicit WindowContext(HWND root) : root(root) {}

    private:
        static bool isPointInRect(int x, int y, const RECT &rect) {
            return rect.top < y && y < rect.bottom && rect.left < x && x < rect.right;
        }

    public:
        [[nodiscard]] HWND getRoot() const {
            return root;
        }

        void updateWindowPosition(RECT rect) {
            windowPosition = rect;
        }

        void updateControlPosition(WindowControl control, int left, int top, int right, int bottom) {
            RECT &rect = windowControlPositions[control];

            rect.left = left;
            rect.top = top;
            rect.right = right;
            rect.bottom = bottom;
        }

        void updateFrameSize(WindowFrame frame, int size) {
            windowFrameSizes[frame] = size;
        }

        int hit(int x, int y) {
            x = x - windowPosition.left;
            y = y - windowPosition.top;

            if (isPointInRect(x, y, windowControlPositions[CLOSE_BUTTON])) {
                return HTCLIENT;
            }
            if (isPointInRect(x, y, windowControlPositions[BACK_BUTTON])) {
                return HTCLIENT;
            }

            int width = windowPosition.right - windowPosition.left;
            int height = windowPosition.bottom - windowPosition.top;
            int edgeInset = windowFrameSizes[EDGE_INSETS];

            int inLeft = x < edgeInset;
            int inTop = y < edgeInset;
            int inRight = x > (width - edgeInset);
            int inBottom = y > (height - edgeInset);

            if (inTop) {
                if (inLeft) {
                    return HTTOPLEFT;
                }
                if (inRight) {
                    return HTTOPRIGHT;
                }
                return HTTOP;
            }
            if (inBottom) {
                if (inLeft) {
                    return HTBOTTOMLEFT;
                }
                if (inRight) {
                    return HTBOTTOMRIGHT;
                }
                return HTBOTTOM;
            }
            if (inLeft) {
                return HTLEFT;
            }
            if (inRight) {
                return HTRIGHT;
            }

            if (y < windowFrameSizes[TITLE_BAR]) {
                return HTCAPTION;
            }

            return HTCLIENT;
        }
    };

    struct WindowContextRef {
        std::shared_ptr<WindowContext> context;
    };

    static int getCaptionPadding(HWND handle) {
        if (IsZoomed(handle)) {
            if (GetSystemMetricsForDpi && GetDpiForWindow) {
                return GetSystemMetricsForDpi(SM_CXPADDEDBORDER, GetDpiForWindow(handle));
            }

            return GetSystemMetrics(SM_CXPADDEDBORDER);
        }

        return 0;
    }

    static LRESULT delegateWindowProcedure(
            HWND handle,
            UINT message,
            WPARAM wParam,
            LPARAM lParam
    ) {
        auto awtProc = reinterpret_cast<WNDPROC>(GetPropA(handle, KEY_AWT_WINDOW_PROCEDURE));
        auto ref = reinterpret_cast<WindowContextRef *>(GetPropA(handle, KEY_WINDOW_CONTEXT_REF));

        switch (message) {
            case WM_NCHITTEST: {
                int area = ref->context->hit(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                if (ref->context->getRoot() == handle) {
                    return area;
                }

                if (area != HTCLIENT) {
                    return HTTRANSPARENT;
                }

                return area;
            }
            case WM_NCCALCSIZE: {
                if (ref->context->getRoot() == handle && wParam) {
                    auto params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);

                    params->rgrc[0].top += getCaptionPadding(handle);

                    return 0;
                }

                break;
            }
            case WM_DESTROY: {
                SetPropA(handle, KEY_WINDOW_CONTEXT_REF, nullptr);
                SetWindowLongPtrA(handle, GWLP_WNDPROC, (LONG_PTR) awtProc);

                delete ref;

                break;
            }
            case WM_SIZE:
            case WM_MOVE: {
                if (ref->context->getRoot() == handle) {
                    RECT rect;

                    GetWindowRect(handle, &rect);

                    rect.top += getCaptionPadding(handle);

                    ref->context->updateWindowPosition(rect);
                }

                break;
            }
            case WM_NCRBUTTONDOWN: {
                if (wParam == HTCAPTION) {
                    return 0;
                }

                break;
            }
            case WM_NCRBUTTONUP: {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);

                if (wParam == HTCAPTION) {
                    HMENU menu = GetSystemMenu(ref->context->getRoot(), FALSE);

                    TrackPopupMenu(menu, 0, x, y, 0, ref->context->getRoot(), nullptr);

                    return 0;
                }

                break;
            }
            case WM_COMMAND: {
                if (wParam & 0xF000) {
                    return SendMessageA(handle, WM_SYSCOMMAND, wParam, lParam);
                }

                break;
            }
            case WM_SYSCOMMAND: {
                return DefWindowProcA(ref->context->getRoot(), message, wParam, lParam);
            }
            default: {
                break;
            }
        }

        return CallWindowProcA(awtProc, handle, message, wParam, lParam);
    }

    [[gnu::stdcall]]
    static BOOL attachToWindow(HWND handle, LPARAM lparam) {
        if (GetPropA(handle, KEY_AWT_WINDOW_PROCEDURE) || GetPropA(handle, KEY_WINDOW_CONTEXT_REF)) {
            return TRUE;
        }

        auto *context = reinterpret_cast<std::shared_ptr<WindowContext> *>(lparam);
        auto *contextRef = new WindowContextRef{*context};

        SetPropA(handle, KEY_WINDOW_CONTEXT_REF, contextRef);
        SetPropA(handle, KEY_AWT_WINDOW_PROCEDURE, reinterpret_cast<HANDLE>(GetWindowLongPtrA(handle, GWLP_WNDPROC)));

        SetWindowLongPtrA(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&delegateWindowProcedure));

        EnumChildWindows(handle, &attachToWindow, lparam);

        SetWindowPos(handle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

        return TRUE;
    }

    void setWindowBorderless(void *handle) {
        std::shared_ptr<WindowContext> context = std::make_shared<WindowContext>(reinterpret_cast<HWND>(handle));

        const MARGINS margins = {0, 0, 0, 1};
        DwmExtendFrameIntoClientArea(reinterpret_cast<HWND>(handle), &margins);

        SetWindowLongA(reinterpret_cast<HWND>(handle), GWL_STYLE, WS_OVERLAPPEDWINDOW);

        attachToWindow(reinterpret_cast<HWND>(handle), reinterpret_cast<LPARAM>(&context));
    }

    void setWindowControlPosition(void *handle, WindowControl control, int left, int top, int right, int bottom) {
        auto ref = reinterpret_cast<WindowContextRef*>(GetPropA(reinterpret_cast<HWND>(handle), KEY_WINDOW_CONTEXT_REF));
        if (ref != nullptr) {
            ref->context->updateControlPosition(control, left, top, right, bottom);
        }
    }

    void setWindowFrameSize(void *handle, WindowFrame frame, int size) {
        auto ref = reinterpret_cast<WindowContextRef*>(GetPropA(reinterpret_cast<HWND>(handle), KEY_WINDOW_CONTEXT_REF));
        if (ref != nullptr) {
            ref->context->updateFrameSize(frame, size);
        }
    }
}
