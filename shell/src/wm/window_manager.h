#pragma once
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct ManagedWindow {
    Window client;
    Window frame;
    int    frame_x, frame_y, frame_w, frame_h;
    bool   maximized = false;
    int    restore_x, restore_y, restore_w, restore_h;
    std::string title;
};

class WindowManager {
public:
    explicit WindowManager(Display* dpy);
    ~WindowManager();

    void run();   // основной event loop

    // Список задач для taskbar
    std::vector<ManagedWindow> get_tasks() const;

private:
    Display* m_dpy;
    Window   m_root;
    int      m_screen;

    // Atoms
    Atom m_wm_delete_window;
    Atom m_wm_protocols;
    Atom m_net_wm_name;
    Atom m_net_wm_state;
    Atom m_net_wm_state_fullscreen;
    Atom m_net_wm_state_above;

    std::unordered_map<Window, ManagedWindow> m_frames; // frame → managed
    std::unordered_map<Window, Window>        m_clients; // client → frame

    // Drag state
    bool   m_dragging  = false;
    Window m_drag_frame = None;
    int    m_drag_start_x = 0, m_drag_start_y = 0;
    int    m_drag_frame_x = 0, m_drag_frame_y = 0;

    // Resize state
    bool   m_resizing    = false;
    Window m_resize_frame = None;
    int    m_resize_start_x = 0, m_resize_start_y = 0;
    int    m_resize_orig_w = 0, m_resize_orig_h = 0;

    // Helpers
    void frame_window(Window w);
    void unframe_window(Window w);
    void draw_frame(const ManagedWindow& mw);
    std::string get_window_title(Window w);

    void on_map_request(const XMapRequestEvent& e);
    void on_unmap_notify(const XUnmapEvent& e);
    void on_configure_request(const XConfigureRequestEvent& e);
    void on_button_press(const XButtonEvent& e);
    void on_button_release(const XButtonEvent& e);
    void on_motion_notify(const XMotionEvent& e);
    void on_key_press(const XKeyEvent& e);
    void on_expose(const XExposeEvent& e);
    void on_destroy_notify(const XDestroyWindowEvent& e);

    void close_window(Window frame);
    void maximize_window(Window frame);
    void minimize_window(Window frame);

    static const int TITLE_H  = 32;
    static const int BORDER    = 4;
    static const int BTN_SIZE  = 24;
    static const int TASKBAR_H = 48;
};
