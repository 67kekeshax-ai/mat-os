#pragma once
#include <X11/Xlib.h>
#include <string>
#include <vector>

struct TaskEntry {
    Window window;
    Window frame;
    std::string title;
    bool active;
};

class Taskbar {
public:
    static const int HEIGHT = 48;

    Taskbar(Display* dpy, int screen);
    ~Taskbar();

    void create();
    void draw();
    void draw(const std::vector<TaskEntry>& tasks);

    // Обработчик кликов; возвращает true если клик был по панели
    bool handle_click(int x, int y, Window& activated_frame);

    Window window() const { return m_win; }

    // Открыть/закрыть меню Пуск
    void toggle_start_menu();
    bool start_menu_open() const { return m_start_open; }

private:
    Display* m_dpy;
    int      m_screen;
    Window   m_win;
    Pixmap   m_buf;         // back buffer
    GC       m_gc;
    int      m_width;

    bool     m_start_open = false;

    std::vector<TaskEntry> m_tasks;

    void draw_start_button();
    void draw_clock();
    void draw_show_desktop();
    void draw_tasks();

    unsigned long m_color_bg;
    unsigned long m_color_btn;
    unsigned long m_color_active;
    unsigned long m_color_text;
    unsigned long m_color_hover;

    unsigned long alloc_color(uint8_t r, uint8_t g, uint8_t b);
};
