#include "window.h"

#include "cleaner.h"

#include <unordered_map.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define LOCK(lock) pthread_mutex_lock(lock); CLEANABLE(pthread_mutex_unlock) pthread_mutex_t *__local_lock = lock

CLEANER_NULLABLE(Display*, XCloseDisplay)
CLEANER_NULLABLE(pthread_mutex_t *, pthread_mutex_unlock)
CLEANER_NULLABLE(Window *, XFree)

typedef struct {
    ulong left, top, right, bottom;
} Rectangle;

typedef struct {
    Window root;

    ulong windowWidth, windowHeight;

    Rectangle windowControlPositions[WINDOW_CONTROL_END];
    uint64_t windowFrameSizes[WINDOW_FRAME_END];
} WindowContext;

static pthread_mutex_t windowsLock = PTHREAD_MUTEX_INITIALIZER;
static unordered_map windows;

static int isPointInRect(ulong x, ulong y, Rectangle *rect) {
    return rect->top < y && y < rect->bottom && rect->left < x && x < rect->right;
}

static int isInTitleBar(WindowContext *context, ulong x, ulong y) {
    if (isPointInRect(x, y, &context->windowControlPositions[CLOSE_BUTTON])) {
        return 0;
    }
    if (isPointInRect(x, y, &context->windowControlPositions[BACK_BUTTON])) {
        return 0;
    }

    ulong width = context->windowWidth;
    ulong height = context->windowHeight;
    ulong edgeInset = context->windowFrameSizes[EDGE_INSETS];
    if (x < edgeInset || y < edgeInset || x > (width - edgeInset) || y > (height - edgeInset)) {
        return 0;
    }

    if (y < context->windowFrameSizes[TITLE_BAR]) {
        return 1;
    }

    return 0;
}

static void delegatedXNextEvent(JNIEnv *env, jclass clazz, jlong _display, jlong _event) {
    Display *display = (Display *) _display;
    XEvent *event = (XEvent *) _event;

    while (1) {
        XNextEvent(display, event);

        switch (event->type) {
            case DestroyNotify: {
                LOCK(&windowsLock);

                WindowContext *hints;
                if (unordered_map_get(&hints, windows, &event->xdestroywindow.window)) {
                    if (hints->root == event->xdestroywindow.window) {
                        free(hints);
                    }

                    unordered_map_remove(windows, &event->xdestroywindow.window);
                }

                return;
            }
            case ConfigureNotify: {
                LOCK(&windowsLock);

                WindowContext *context;
                if (unordered_map_get(&context, windows, &event->xconfigure.window)) {
                    if (context->root == event->xconfigure.window) {
                        context->windowWidth = event->xconfigure.width;
                        context->windowHeight = event->xconfigure.height;
                    }
                }

                return;
            }
            case ButtonPress: {
                if (event->xbutton.button == Button1) {
                    LOCK(&windowsLock);

                    WindowContext *context;
                    if (unordered_map_get(&context, windows, &event->xbutton.window)) {
                        if (isInTitleBar(context, event->xbutton.x, event->xbutton.y)) {
                            XEvent message = {
                                    .xclient = {
                                            .type = ClientMessage,
                                            .message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", True),
                                            .display = display,
                                            .window = context->root,
                                            .format = 32,
                                            .data.l[0] = event->xbutton.x_root,
                                            .data.l[1] = event->xbutton.y_root,
                                            .data.l[2] = 8, // _NET_WM_MOVERESIZE_MOVE
                                            .data.l[3] = Button1,
                                            .data.l[4] = 1 // normal applications
                                    }
                            };

                            XSendEvent(
                                    display,
                                    XDefaultRootWindow(display),
                                    False,
                                    SubstructureNotifyMask | SubstructureRedirectMask,
                                    &message
                            );

                            continue;
                        }
                    }
                } else if (event->xbutton.button == Button3) {
                    LOCK(&windowsLock);

                    WindowContext *context;
                    if (unordered_map_get(&context, windows, &event->xbutton.window)) {
                        if (isInTitleBar(context, event->xbutton.x, event->xbutton.y)) {
                            continue;
                        }
                    }
                }

                return;
            }
            case ButtonRelease: {
                if (event->xbutton.button == Button1) {
                    LOCK(&windowsLock);

                    WindowContext *context;
                    if (unordered_map_get(&context, windows, &event->xbutton.window)) {
                        if (isInTitleBar(context, event->xbutton.x, event->xbutton.y)) {
                            continue;
                        }
                    }
                } else if (event->xbutton.button == Button3) {
                    LOCK(&windowsLock);

                    WindowContext *context;
                    if (unordered_map_get(&context, windows, &event->xbutton.window)) {
                        if (isInTitleBar(context, event->xbutton.x, event->xbutton.y)) {
                            XEvent message = {
                                    .xclient = {
                                            .type = ClientMessage,
                                            .window = context->root,
                                            .message_type = XInternAtom(display, "_GTK_SHOW_WINDOW_MENU", True),
                                            .format = 32,
                                            .data.l[0] = 0,
                                            .data.l[1] = event->xbutton.x_root,
                                            .data.l[2] = event->xbutton.y_root,
                                    }
                            };

                            XSendEvent(
                                    display,
                                    XDefaultRootWindow(display),
                                    False,
                                    SubstructureRedirectMask | SubstructureNotifyMask,
                                    &message
                            );

                            continue;
                        }
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

static unsigned long windowHash(const void *const key) {
    return *(Window *) key;
}

static int windowCompare(const void *const a, const void *const b) {
    Window aWindow = *(Window *) a;
    Window bWindow = *(Window *) b;

    if (aWindow > bWindow) {
        return 1;
    }
    if (aWindow < bWindow) {
        return -1;
    }
    return 0;
}

void windowInit(JNIEnv *env) {
    jclass wrapper = (*env)->FindClass(env, "sun/awt/X11/XlibWrapper");

    JNINativeMethod nextEvent = {
            .name = "XNextEvent",
            .signature = "(JJ)V",
            .fnPtr = &delegatedXNextEvent,
    };

    if ((*env)->RegisterNatives(env, wrapper, &nextEvent, 1) != JNI_OK) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
        }
        abort();
    }

    windows = unordered_map_init(sizeof(Window), sizeof(WindowContext *), &windowHash, &windowCompare);
}

static void storeWindow(Display *display, Window window, WindowContext *hints) {
    if (!unordered_map_contains(windows, &window)) {
        unordered_map_put(windows, &window, &hints);
    } else {
        return;
    }

    Window root = 0;
    Window parent = 0;
    CLEANABLE(XFree)
    Window *children = NULL;
    uint32_t childrenLength = 0;

    XQueryTree(display, window, &root, &parent, &children, &childrenLength);
    for (uint32_t i = 0; i < childrenLength; i++) {
        storeWindow(display, children[i], hints);
    }
}

void windowSetWindowBorderless(void *handle) {
    CLEANABLE(XCloseDisplay)
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        abort();
    }

    LOCK(&windowsLock);

    Window window = (Window) handle;

    WindowContext *hints = malloc(sizeof(*hints));

    memset(hints, 0, sizeof(*hints));

    hints->root = window;

    storeWindow(display, window, hints);

    WindowContext *rootHints;
    if (!unordered_map_get(&rootHints, windows, &window) || rootHints != hints) {
        free(hints);
    }
}

void windowSetWindowFrameSize(void *handle, enum WindowFrame frame, int size) {
    LOCK(&windowsLock);

    Window window = (Window) handle;

    WindowContext *hints;
    if (unordered_map_get(&hints, windows, &window)) {
        hints->windowFrameSizes[frame] = size;
    }
}

void windowSetWindowControlPosition(
        void *handle,
        enum WindowControl control,
        int left,
        int top,
        int right,
        int bottom
) {
    LOCK(&windowsLock);

    Window window = (Window) handle;

    WindowContext *hints;
    if (unordered_map_get(&hints, windows, &window)) {
        hints->windowControlPositions[control].left = left;
        hints->windowControlPositions[control].top = top;
        hints->windowControlPositions[control].right = right;
        hints->windowControlPositions[control].bottom = bottom;
    }
}

void windowSetWindowMinimumSize(void *handle, int width, int height) {
    CLEANABLE(XCloseDisplay)
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        abort();
    }

    XSizeHints hints;
    memset(&hints, 0, sizeof(hints));

    if (XGetWMNormalHints(display, (Window) handle, &hints, NULL) == Success) {
        hints.min_width = width;
        hints.min_height = height;

        XSetWMNormalHints(display, (Window) handle, &hints);
    }
}
