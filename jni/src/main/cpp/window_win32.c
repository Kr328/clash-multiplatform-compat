#include "window.h"

#include <stdatomic.h>
#include <dwmapi.h>
#include <winuser.h>
#include <windowsx.h>

#define KEY_AWT_WINDOW_PROCEDURE "awt-window-procedure"
#define KEY_WINDOW_CONTEXT "compat-window-context"

static UINT (*GetDpiForWindow)(HWND handle);
static int (*GetSystemMetricsForDpi)(int nIndex, UINT dpi);

typedef struct {
    HWND root;

    atomic_int referenceCount;

    RECT windowRectNow;
    RECT windowControlPositions[WINDOW_CONTROL_END];
    LONG windowFrameSizes[WINDOW_FRAME_END];
} WindowContext;

static BOOL isPointInRect(int x, int y, RECT *rect) {
    return rect->top < y && y < rect->bottom && rect->left < x && x < rect->right;
}

static LRESULT hitNonClient(WindowContext *context, int x, int y) {
    if (isPointInRect(x, y, &context->windowControlPositions[CLOSE_BUTTON])) {
        return HTCLIENT;
    }
    if (isPointInRect(x, y, &context->windowControlPositions[BACK_BUTTON])) {
        return HTCLIENT;
    }

    int width = context->windowRectNow.right - context->windowRectNow.left;
    int height = context->windowRectNow.bottom - context->windowRectNow.top;
    int edgeInset = context->windowFrameSizes[EDGE_INSETS];
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

    if (y < context->windowFrameSizes[TITLE_BAR]) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

static LRESULT delegateWindowProcedure(
        HWND handle,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
) {
    WNDPROC awtProc = GetPropA(handle, KEY_AWT_WINDOW_PROCEDURE);
    WindowContext *context = GetPropA(handle, KEY_WINDOW_CONTEXT);

    switch (message) {
        case WM_NCHITTEST: {
            int x = GET_X_LPARAM(lParam) - context->windowRectNow.left;
            int y = GET_Y_LPARAM(lParam) - context->windowRectNow.top;

            LRESULT hit = hitNonClient(context, x, y);
            if (handle == context->root) {
                return hit;
            }

            if (hit != HTCLIENT) {
                return HTTRANSPARENT;
            }

            return hit;
        }
        case WM_NCCALCSIZE: {
            if (handle == context->root && wParam) {
                NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *) lParam;

                if (IsZoomed(handle)) {
                    int padding;
                    if (GetDpiForWindow && GetSystemMetricsForDpi) {
                        padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, GetDpiForWindow(handle));
                    } else {
                        padding = GetSystemMetrics(SM_CXPADDEDBORDER);
                    }

                    params->rgrc[0].top += padding;
                }

                return 0;
            }

            break;
        }
        case WM_DESTROY: {
            SetPropA(handle, KEY_WINDOW_CONTEXT, NULL);
            SetWindowLongPtrA(handle, GWLP_WNDPROC, (LONG_PTR) awtProc);

            if (!(--context->referenceCount)) {
                free(context);
            }

            break;
        }
        case WM_SIZE:
        case WM_MOVE: {
            if (handle == context->root) {
                GetWindowRect(handle, &context->windowRectNow);

                if (IsZoomed(handle)) {
                    int padding;
                    if (GetDpiForWindow && GetSystemMetricsForDpi) {
                        padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, GetDpiForWindow(handle));
                    } else {
                        padding = GetSystemMetrics(SM_CXPADDEDBORDER);
                    }

                    context->windowRectNow.top += padding;
                }
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
                HMENU menu = GetSystemMenu(context->root, FALSE);

                TrackPopupMenu(menu, 0, x, y, 0, context->root, NULL);

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
            return DefWindowProcA(context->root, message, wParam, lParam);
        }
        default: {
            break;
        }
    }

    return CallWindowProcA(awtProc, handle, message, wParam, lParam);
}

static BOOL __attribute__((stdcall)) attachToWindow(HWND handle, LPARAM lparam) {
    if (GetPropA(handle, KEY_AWT_WINDOW_PROCEDURE) || GetPropA(handle, KEY_WINDOW_CONTEXT)) {
        return TRUE;
    }

    WindowContext *context = (WindowContext *) lparam;

    context->referenceCount++;

    SetPropA(handle, KEY_WINDOW_CONTEXT, context);
    SetPropA(handle, KEY_AWT_WINDOW_PROCEDURE, (HANDLE) GetWindowLongPtrA(handle, GWLP_WNDPROC));

    SetWindowLongPtrA(handle, GWLP_WNDPROC, (LONG_PTR) &delegateWindowProcedure);

    EnumChildWindows(handle, &attachToWindow, (LPARAM) context);

    SetWindowPos(handle, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    return TRUE;
}

void windowInit(JNIEnv *env) {
    HANDLE user32 = GetModuleHandleA("libuser.dll");

    GetDpiForWindow = (UINT (*)(HWND)) GetProcAddress(user32, "GetDpiForWindow");
    GetSystemMetricsForDpi = (int (*)(int, UINT)) GetProcAddress(user32, "GetSystemMetricsForDpi");
}

void windowSetWindowBorderless(void *handle) {
    WindowContext *context = malloc(sizeof(*context));

    context->root = handle;
    context->referenceCount = 0;

    SetRectEmpty(&context->windowRectNow);

    for (int i = 0; i < WINDOW_CONTROL_END; i++) {
        RECT *rect = &context->windowControlPositions[i];
        rect->left = -1;
        rect->top = -1;
        rect->right = -1;
        rect->bottom = -1;
    }

    const MARGINS margins = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(handle, &margins);

    SetWindowLongA(handle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

    attachToWindow(handle, (LPARAM) context);
}

void windowSetWindowFrameSize(void *handle, enum WindowFrame frame, int size) {
    WindowContext *context = GetPropA(handle, KEY_WINDOW_CONTEXT);
    if (context != NULL) {
        context->windowFrameSizes[frame] = size;
    }
}

void windowSetWindowControlPosition(void *handle, enum WindowControl control, int left, int top, int right, int bottom) {
    WindowContext *context = GetPropA(handle, KEY_WINDOW_CONTEXT);
    if (context != NULL) {
        RECT *rect = &context->windowControlPositions[control];

        rect->left = left;
        rect->top = top;
        rect->right = right;
        rect->bottom = bottom;
    }
}