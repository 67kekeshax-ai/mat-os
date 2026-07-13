#include "window_manager.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cstdint>

// ─── Цвета (ARGB упакован как XColor через XAllocColor) ───────────────────
static unsigned long color(Display* dpy, int screen,
                           uint8_t r, uint8_t g, uint8_t b) {
    XColor c{};
    c.red   = r * 257; c.green = g * 257; c.blue  = b * 257;
    c.flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, DefaultColormap(dpy, screen), &c);
    return c.pixel;
}

WindowManager::WindowManager(Display* dpy)
    : m_dpy(dpy), m_screen(DefaultScreen(dpy))
{
    m_root = RootWindow(dpy, m_screen);

    m_wm_delete_window      = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    m_wm_protocols          = XInternAtom(dpy, "WM_PROTOCOLS", False);
    m_net_wm_name           = XInternAtom(dpy, "_NET_WM_NAME", False);
    m_net_wm_state          = XInternAtom(dpy, "_NET_WM_STATE", False);
    m_net_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    m_net_wm_state_above    = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);

    XSetWindowAttributes attr{};
    attr.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                      ButtonPressMask | KeyPressMask;
    XChangeWindowAttributes(dpy, m_root, CWEventMask, &attr);
    XSelectInput(dpy, m_root, attr.event_mask);
    XSync(dpy, False);
}

WindowManager::~WindowManager() {}

std::vector<ManagedWindow> WindowManager::get_tasks() const {
    std::vector<ManagedWindow> v;
    for (auto& [f, mw] : m_frames) v.push_back(mw);
    return v;
}

// ─── Создать рамку вокруг окна ─────────────────────────────────────────────
void WindowManager::frame_window(Window client) {
    if (m_clients.count(client)) return;

    XWindowAttributes wa{};
    XGetWindowAttributes(m_dpy, client, &wa);

    int fw = wa.width  + 2*BORDER;
    int fh = wa.height + TITLE_H + BORDER;
    int fx = wa.x;
    int fy = wa.y;
    if (fy < TITLE_H) fy = TITLE_H;

    XSetWindowAttributes fa{};
    fa.background_pixel = color(m_dpy, m_screen, 32,32,32);
    fa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                    ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask | ExposureMask;
    fa.override_redirect = False;

    Window frame = XCreateWindow(
        m_dpy, m_root,
        fx, fy, fw, fh, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWEventMask, &fa);

    XReparentWindow(m_dpy, client, frame, BORDER, TITLE_H);
    XSelectInput(m_dpy, client,
                 PropertyChangeMask | StructureNotifyMask);
    XAddToSaveSet(m_dpy, client);
    XMapWindow(m_dpy, frame);
    XMapWindow(m_dpy, client);

    ManagedWindow mw;
    mw.client  = client;
    mw.frame   = frame;
    mw.frame_x = fx; mw.frame_y = fy;
    mw.frame_w = fw; mw.frame_h = fh;
    mw.title   = get_window_title(client);

    m_frames[frame]  = mw;
    m_clients[client] = frame;

    draw_frame(mw);
}

void WindowManager::unframe_window(Window client) {
    auto it = m_clients.find(client);
    if (it == m_clients.end()) return;
    Window frame = it->second;

    XUnmapWindow(m_dpy, frame);
    XReparentWindow(m_dpy, client, m_root, 0, 0);
    XRemoveFromSaveSet(m_dpy, client);
    XDestroyWindow(m_dpy, frame);

    m_frames.erase(frame);
    m_clients.erase(it);
}

// ─── Нарисовать заголовок окна ─────────────────────────────────────────────
void WindowManager::draw_frame(const ManagedWindow& mw) {
    Display* dpy = m_dpy;
    Window frame = mw.frame;
    GC gc = XCreateGC(dpy, frame, 0, nullptr);

    // Фон заголовка
    XSetForeground(dpy, gc, color(dpy, m_screen, 0,120,215));
    XFillRectangle(dpy, frame, gc, 0, 0, mw.frame_w, TITLE_H);

    // Граница
    XSetForeground(dpy, gc, color(dpy, m_screen, 0,80,160));
    XDrawRectangle(dpy, frame, gc, 0, 0, mw.frame_w-1, mw.frame_h-1);

    // Название окна
    XSetForeground(dpy, gc, color(dpy, m_screen, 255,255,255));
    XFontStruct* fs = XLoadQueryFont(dpy, "fixed");
    if (fs) {
        XSetFont(dpy, gc, fs->fid);
        XDrawString(dpy, frame, gc, 8, 20,
                    mw.title.c_str(), mw.title.size());
        XFreeFont(dpy, fs);
    }

    // Кнопки (закрыть / развернуть / свернуть)
    int bx = mw.frame_w - BTN_SIZE - 4;
    // Закрыть (красная)
    XSetForeground(dpy, gc, color(dpy, m_screen, 232,17,35));
    XFillRectangle(dpy, frame, gc, bx, 4, BTN_SIZE, TITLE_H-8);
    XSetForeground(dpy, gc, color(dpy, m_screen, 255,255,255));
    XDrawString(dpy, frame, gc, bx+7, 20, "X", 1);

    // Развернуть (серая)
    bx -= BTN_SIZE + 2;
    XSetForeground(dpy, gc, color(dpy, m_screen, 80,80,80));
    XFillRectangle(dpy, frame, gc, bx, 4, BTN_SIZE, TITLE_H-8);
    XSetForeground(dpy, gc, color(dpy, m_screen, 255,255,255));
    XDrawString(dpy, frame, gc, bx+5, 20, "[]", 2);

    // Свернуть (серая)
    bx -= BTN_SIZE + 2;
    XSetForeground(dpy, gc, color(dpy, m_screen, 80,80,80));
    XFillRectangle(dpy, frame, gc, bx, 4, BTN_SIZE, TITLE_H-8);
    XSetForeground(dpy, gc, color(dpy, m_screen, 255,255,255));
    XDrawString(dpy, frame, gc, bx+7, 20, "_", 1);

    XFreeGC(dpy, gc);
}

std::string WindowManager::get_window_title(Window w) {
    char* name = nullptr;
    if (XFetchName(m_dpy, w, &name) && name) {
        std::string s(name);
        XFree(name);
        return s;
    }
    return "Окно";
}

// ─── Event handlers ────────────────────────────────────────────────────────
void WindowManager::on_map_request(const XMapRequestEvent& e) {
    frame_window(e.window);
}

void WindowManager::on_unmap_notify(const XUnmapEvent& e) {
    if (m_clients.count(e.window))
        unframe_window(e.window);
}

void WindowManager::on_configure_request(const XConfigureRequestEvent& e) {
    XWindowChanges wc{};
    wc.x = e.x; wc.y = e.y;
    wc.width = e.width; wc.height = e.height;
    wc.border_width = e.border_width;
    wc.sibling = e.above; wc.stack_mode = e.detail;
    XConfigureWindow(m_dpy, e.window, e.value_mask, &wc);
}

void WindowManager::on_button_press(const XButtonEvent& e) {
    Window frame = e.window;
    if (!m_frames.count(frame)) return;
    auto& mw = m_frames[frame];

    int bx = mw.frame_w - BTN_SIZE - 4;
    // Кнопка закрыть
    if (e.y >= 4 && e.y <= TITLE_H-4 &&
        e.x >= bx && e.x < bx+BTN_SIZE) {
        close_window(frame); return;
    }
    // Развернуть
    bx -= BTN_SIZE + 2;
    if (e.y >= 4 && e.y <= TITLE_H-4 &&
        e.x >= bx && e.x < bx+BTN_SIZE) {
        maximize_window(frame); return;
    }
    // Свернуть
    bx -= BTN_SIZE + 2;
    if (e.y >= 4 && e.y <= TITLE_H-4 &&
        e.x >= bx && e.x < bx+BTN_SIZE) {
        minimize_window(frame); return;
    }

    // Drag по заголовку
    if (e.y < TITLE_H) {
        m_dragging    = true;
        m_drag_frame  = frame;
        m_drag_start_x = e.x_root;
        m_drag_start_y = e.y_root;
        m_drag_frame_x = mw.frame_x;
        m_drag_frame_y = mw.frame_y;
        XRaiseWindow(m_dpy, frame);
    }

    // Resize по нижнему краю
    if (e.y >= mw.frame_h - BORDER) {
        m_resizing      = true;
        m_resize_frame  = frame;
        m_resize_start_x = e.x_root;
        m_resize_start_y = e.y_root;
        m_resize_orig_w  = mw.frame_w;
        m_resize_orig_h  = mw.frame_h;
    }
}

void WindowManager::on_button_release(const XButtonEvent&) {
    m_dragging = false; m_resizing = false;
}

void WindowManager::on_motion_notify(const XMotionEvent& e) {
    if (m_dragging && m_frames.count(m_drag_frame)) {
        auto& mw = m_frames[m_drag_frame];
        int nx = m_drag_frame_x + (e.x_root - m_drag_start_x);
        int ny = m_drag_frame_y + (e.y_root - m_drag_start_y);
        ny = std::max(ny, 0);
        XMoveWindow(m_dpy, m_drag_frame, nx, ny);
        mw.frame_x = nx; mw.frame_y = ny;
    }
    if (m_resizing && m_frames.count(m_resize_frame)) {
        auto& mw = m_frames[m_resize_frame];
        int nw = m_resize_orig_w + (e.x_root - m_resize_start_x);
        int nh = m_resize_orig_h + (e.y_root - m_resize_start_y);
        nw = std::max(nw, 120); nh = std::max(nh, 80);
        XResizeWindow(m_dpy, m_resize_frame, nw, nh);
        XResizeWindow(m_dpy, mw.client,
                      nw - 2*BORDER, nh - TITLE_H - BORDER);
        mw.frame_w = nw; mw.frame_h = nh;
    }
}

void WindowManager::on_key_press(const XKeyEvent& e) {
    KeySym ks = XLookupKeysym(const_cast<XKeyEvent*>(&e), 0);
    bool super = (e.state & Mod4Mask);

    if (!super) return;

    // Super+Q = закрыть активное окно
    if (ks == XK_q) {
        Window focus; int revert;
        XGetInputFocus(m_dpy, &focus, &revert);
        if (m_clients.count(focus))
            close_window(m_clients[focus]);
    }
    // Aero Snap: Super+Left / Super+Right
    int sw = DisplayWidth(m_dpy, m_screen);
    int sh = DisplayHeight(m_dpy, m_screen) - TASKBAR_H;

    Window focus; int revert;
    XGetInputFocus(m_dpy, &focus, &revert);
    if (!m_clients.count(focus)) return;
    Window frame = m_clients[focus];
    if (!m_frames.count(frame)) return;
    auto& mw = m_frames[frame];

    if (ks == XK_Left) {
        XMoveResizeWindow(m_dpy, frame, 0, 0, sw/2, sh);
        mw.frame_x=0; mw.frame_y=0; mw.frame_w=sw/2; mw.frame_h=sh;
    } else if (ks == XK_Right) {
        XMoveResizeWindow(m_dpy, frame, sw/2, 0, sw/2, sh);
        mw.frame_x=sw/2; mw.frame_y=0; mw.frame_w=sw/2; mw.frame_h=sh;
    } else if (ks == XK_Up) {
        maximize_window(frame);
    }
}

void WindowManager::on_expose(const XExposeEvent& e) {
    if (m_frames.count(e.window))
        draw_frame(m_frames[e.window]);
}

void WindowManager::on_destroy_notify(const XDestroyWindowEvent& e) {
    if (m_clients.count(e.window))
        unframe_window(e.window);
}

void WindowManager::close_window(Window frame) {
    if (!m_frames.count(frame)) return;
    Window client = m_frames[frame].client;

    // Сначала вежливо через WM_DELETE_WINDOW
    Atom* protos; int n;
    if (XGetWMProtocols(m_dpy, client, &protos, &n)) {
        for (int i=0; i<n; i++) {
            if (protos[i] == m_wm_delete_window) {
                XEvent ev{};
                ev.type = ClientMessage;
                ev.xclient.window = client;
                ev.xclient.message_type = m_wm_protocols;
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = m_wm_delete_window;
                ev.xclient.data.l[1] = CurrentTime;
                XSendEvent(m_dpy, client, False, NoEventMask, &ev);
                XFree(protos);
                return;
            }
        }
        XFree(protos);
    }
    XKillClient(m_dpy, client);
}

void WindowManager::maximize_window(Window frame) {
    if (!m_frames.count(frame)) return;
    auto& mw = m_frames[frame];
    int sw = DisplayWidth(m_dpy, m_screen);
    int sh = DisplayHeight(m_dpy, m_screen) - TASKBAR_H;

    if (!mw.maximized) {
        mw.restore_x = mw.frame_x; mw.restore_y = mw.frame_y;
        mw.restore_w = mw.frame_w; mw.restore_h = mw.frame_h;
        XMoveResizeWindow(m_dpy, frame, 0, 0, sw, sh);
        mw.frame_x=0; mw.frame_y=0; mw.frame_w=sw; mw.frame_h=sh;
        XResizeWindow(m_dpy, mw.client, sw-2*BORDER, sh-TITLE_H-BORDER);
        mw.maximized = true;
    } else {
        XMoveResizeWindow(m_dpy, frame,
            mw.restore_x, mw.restore_y, mw.restore_w, mw.restore_h);
        mw.frame_x=mw.restore_x; mw.frame_y=mw.restore_y;
        mw.frame_w=mw.restore_w; mw.frame_h=mw.restore_h;
        XResizeWindow(m_dpy, mw.client,
            mw.restore_w-2*BORDER, mw.restore_h-TITLE_H-BORDER);
        mw.maximized = false;
    }
    draw_frame(mw);
}

void WindowManager::minimize_window(Window frame) {
    if (!m_frames.count(frame)) return;
    XUnmapWindow(m_dpy, frame);
}

// ─── Main Event Loop ───────────────────────────────────────────────────────
void WindowManager::run() {
    XEvent ev;
    while (true) {
        XNextEvent(m_dpy, &ev);
        switch (ev.type) {
            case MapRequest:       on_map_request(ev.xmaprequest);         break;
            case UnmapNotify:      on_unmap_notify(ev.xunmap);             break;
            case ConfigureRequest: on_configure_request(ev.xconfigurerequest); break;
            case ButtonPress:      on_button_press(ev.xbutton);            break;
            case ButtonRelease:    on_button_release(ev.xbutton);          break;
            case MotionNotify:     on_motion_notify(ev.xmotion);           break;
            case KeyPress:         on_key_press(ev.xkey);                  break;
            case Expose:           on_expose(ev.xexpose);                  break;
            case DestroyNotify:    on_destroy_notify(ev.xdestroywindow);   break;
        }
    }
}
