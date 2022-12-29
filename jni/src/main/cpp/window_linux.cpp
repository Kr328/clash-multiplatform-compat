#include "window.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <X11/Xlib.h>

namespace window {
    struct Rectangle {
        ulong left{}, top{}, right{}, bottom{};
    };

    class WindowContext {
    private:
        Window root;

        ulong windowWidth{}, windowHeight{};

        Rectangle windowControlPositions[WindowControl::WINDOW_CONTROL_END]{};
        ulong windowFrameSizes[WindowFrame::WINDOW_FRAME_END]{};

    private:
        static bool isInRect(ulong x, ulong y, const Rectangle &rect) {
            return rect.top < y && y < rect.bottom && rect.left < x && x < rect.right;
        }

    public:
        explicit WindowContext(Window root): root(root) {}

        [[nodiscard]] Window getRootWindow() const {
            return root;
        }

        void updateControlPosition(WindowControl control, ulong left, ulong top, ulong right, ulong bottom) {
            windowControlPositions[control].left = left;
            windowControlPositions[control].top = top;
            windowControlPositions[control].right = right;
            windowControlPositions[control].bottom = bottom;
        }

        void updateFrameSize(WindowFrame frame, ulong size) {
            windowFrameSizes[frame] = size;
        }

        void updateWindowSize(ulong width, ulong height) {
            windowWidth = width;
            windowHeight = height;
        }

        bool isPointInTitleBar(ulong x, ulong y) {
            if (isInRect(x, y, windowControlPositions[CLOSE_BUTTON])) {
                return false;
            }
            if (isInRect(x, y, windowControlPositions[BACK_BUTTON])) {
                return false;
            }

            ulong width = windowWidth;
            ulong height = windowHeight;
            ulong edgeInset = windowFrameSizes[EDGE_INSETS];
            if (x < edgeInset || y < edgeInset || x > (width - edgeInset) || y > (height - edgeInset)) {
                return false;
            }

            if (y < windowFrameSizes[TITLE_BAR]) {
                return true;
            }

            return false;
        }
    };

    static std::mutex windowsLock;
    static std::map<Window, std::shared_ptr<WindowContext>> windows;

    static void delegatedXNextEvent(JNIEnv *env, jclass clazz, jlong _display, jlong _event) {
        auto display = reinterpret_cast<Display *>(_display);
        auto event = reinterpret_cast<XEvent *>(_event);

        while (true) {
            XNextEvent(display, event);

            switch (event->type) {
                case DestroyNotify: {
                    std::lock_guard _lock{windowsLock};

                    windows.erase(event->xdestroywindow.window);

                    return;
                }
                case ConfigureNotify: {
                    std::lock_guard _lock{windowsLock};

                    std::shared_ptr<WindowContext> context = windows[event->xconfigure.window];
                    if (context != nullptr) {
                        context->updateWindowSize(event->xconfigure.width, event->xconfigure.height);
                    }

                    return;
                }
                case ButtonPress: {
                    if (event->xbutton.button == Button1) {
                        std::lock_guard _lock{windowsLock};

                        std::shared_ptr<WindowContext> context = windows[event->xbutton.window];
                        if (context != nullptr && context->isPointInTitleBar(event->xbutton.x, event->xbutton.y)) {
                            XEvent request{};

                            request.xclient.type = ClientMessage;
                            request.xclient.display = display;
                            request.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", True);
                            request.xclient.window = context->getRootWindow();
                            request.xclient.format = 32;
                            request.xclient.data.l[0] = event->xbutton.x_root;
                            request.xclient.data.l[1] = event->xbutton.y_root;
                            request.xclient.data.l[2] = 8; // _NET_WM_MOVERESIZE_MOVE
                            request.xclient.data.l[3] = Button1;
                            request.xclient.data.l[4] = 1; // normal applications

                            XSendEvent(
                                    display,
                                    XDefaultRootWindow(display),
                                    False,
                                    SubstructureNotifyMask | SubstructureRedirectMask,
                                    &request
                            );

                            continue;
                        }
                    } else if (event->xbutton.button == Button3) {
                        std::lock_guard _lock{windowsLock};

                        std::shared_ptr<WindowContext> context = windows[event->xbutton.window];
                        if (context != nullptr && context->isPointInTitleBar(event->xbutton.x, event->xbutton.y)) {
                            continue;
                        }
                    }

                    return;
                }
                case ButtonRelease: {
                    if (event->xbutton.button == Button1) {
                        std::lock_guard _lock{windowsLock};

                        std::shared_ptr<WindowContext> context = windows[event->xbutton.window];
                        if (context != nullptr && context->isPointInTitleBar(event->xbutton.x, event->xbutton.y)) {
                            continue;
                        }
                    } else if (event->xbutton.button == Button3) {
                        std::lock_guard _lock{windowsLock};

                        std::shared_ptr<WindowContext> context = windows[event->xbutton.window];
                        if (context != nullptr && context->isPointInTitleBar(event->xbutton.x, event->xbutton.y)) {
                            XEvent request{};
                            request.xclient.type = ClientMessage;
                            request.xclient.display = display;
                            request.xclient.window = context->getRootWindow();
                            request.xclient.message_type = XInternAtom(display, "_GTK_SHOW_WINDOW_MENU", True);
                            request.xclient.format = 32;
                            request.xclient.data.l[0] = 0,
                            request.xclient.data.l[1] = event->xbutton.x_root;
                            request.xclient.data.l[2] = event->xbutton.y_root;

                            XSendEvent(
                                    display,
                                    XDefaultRootWindow(display),
                                    False,
                                    SubstructureRedirectMask | SubstructureNotifyMask,
                                    &request
                            );

                            continue;
                        }
                    }

                    return;
                }
                default: {
                    return;
                }
            }
        }
    }

    bool install(JNIEnv *env) {
        jclass wrapper = env->FindClass("sun/awt/X11/XlibWrapper");

        JNINativeMethod nextEvent = {
                .name = const_cast<char*>("XNextEvent"),
                .signature = const_cast<char*>("(JJ)V"),
                .fnPtr = reinterpret_cast<void *>(&delegatedXNextEvent),
        };

        if (env->RegisterNatives(wrapper, &nextEvent, 1) != JNI_OK) {
            return false;
        }

        return true;
    }

    static void storeWindow(Display *display, Window window, const std::shared_ptr<WindowContext> &context) {
        if (windows.find(window) != windows.end()) {
            return;
        }

        windows[window] = context;

        Window root = 0;
        Window parent = 0;
        Window *children = nullptr;
        uint32_t childrenLength = 0;

        XQueryTree(display, window, &root, &parent, &children, &childrenLength);
        for (uint32_t i = 0; i < childrenLength; i++) {
            storeWindow(display, children[i], context);
        }

        XFree(children);
    }

    void setWindowBorderless(void *handle) {
        Display *display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            fprintf(stderr, "Unable to open display\n");
            abort();
        }

        std::lock_guard _lock{windowsLock};

        auto window = reinterpret_cast<Window>(handle);

        std::shared_ptr<WindowContext> context = std::make_shared<WindowContext>(window);

        storeWindow(display, window, context);

        XCloseDisplay(display);
    }

    void setWindowControlPosition(
            void *handle,
            WindowControl control,
            int left,
            int top,
            int right,
            int bottom
    ) {
        std::lock_guard _lock{windowsLock};

        auto window = reinterpret_cast<Window>(handle);

        std::shared_ptr<WindowContext> context = windows[window];
        if (context != nullptr) {
            context->updateControlPosition(control, left, top, right, bottom);
        }
    }

    void setWindowFrameSize(void *handle, WindowFrame frame, int size) {
        std::lock_guard _lock{windowsLock};

        auto window = reinterpret_cast<Window>(handle);

        std::shared_ptr<WindowContext> context = windows[window];
        if (context != nullptr) {
            context->updateFrameSize(frame, size);
        }
    }
}
