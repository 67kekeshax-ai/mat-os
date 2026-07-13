#pragma once
#include <cstdint>
using std::uint8_t;
#include <X11/Xlib.h>
#include <string>
#include <vector>
#include <functional>

struct AppEntry {
    std::string name;
    std::string command;
    std::string icon_label; // однобуквенный значок
};

class StartMenu {
public:
    StartMenu(Display* dpy, int screen);
    ~StartMenu();

    void create();
    void show();
    void hide();
    bool visible() const { return m_visible; }

    // Обрабатывает XEvent. Возвращает true если событие потреблено.
    bool handle_event(const XEvent& ev);

    Window window() const { return m_win; }

private:
    Display* m_dpy;
    int      m_screen;
    Window   m_win;
    GC       m_gc;
    bool     m_visible = false;
    int      m_width, m_height;

    std::string m_search;
    int         m_hover_idx = -1;

    std::vector<AppEntry> m_apps;
    std::vector<AppEntry> m_filtered;

    void draw();
    void draw_search_bar();
    void draw_app_list();
    void draw_power_bar();

    void filter_apps();
    void launch(const AppEntry& app);
    void on_key(const XKeyEvent& e);
    void on_button(const XButtonEvent& e);
    void on_expose();

    unsigned long alloc_color(std::uint8_t r, std::uint8_t g, std::uint8_t b);

    XFontStruct* m_font = nullptr;
};
