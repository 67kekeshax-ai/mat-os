#include "startmenu.h"
#include "../taskbar/taskbar.h"

#include <cstdint>
#include <unistd.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>

unsigned long StartMenu::alloc_color(uint8_t r, uint8_t g, uint8_t b) {
    XColor c{};
    c.red=r*257; c.green=g*257; c.blue=b*257;
    c.flags=DoRed|DoGreen|DoBlue;
    XAllocColor(m_dpy, DefaultColormap(m_dpy, m_screen), &c);
    return c.pixel;
}

StartMenu::StartMenu(Display* dpy, int screen)
    : m_dpy(dpy), m_screen(screen), m_win(None)
{
    m_width  = 340;
    m_height = 540;

    m_apps = {
        {"Файловый менеджер", "matos-filemanager",  "F"},
        {"Терминал",          "xterm",              "T"},
        {"Браузер",           "x-www-browser",      "B"},
        {"Настройки",         "/opt/matos-shell/matos-settings.sh", "S"},
        {"Менеджер задач",    "xterm -e top",       "Z"},
        {"Текстовый редактор","xterm -e nano",      "N"},
    };
    m_filtered = m_apps;
}

StartMenu::~StartMenu() {
    if (m_font) XFreeFont(m_dpy, m_font);
    if (m_gc)   XFreeGC(m_dpy, m_gc);
    if (m_win)  XDestroyWindow(m_dpy, m_win);
}

void StartMenu::create() {
    int sh = DisplayHeight(m_dpy, m_screen);

    XSetWindowAttributes attr{};
    attr.background_pixel  = alloc_color(20, 20, 20);
    attr.override_redirect = True;
    attr.event_mask = ButtonPressMask | KeyPressMask |
                      ExposureMask | PointerMotionMask;

    m_win = XCreateWindow(m_dpy, RootWindow(m_dpy, m_screen),
        0, sh - Taskbar::HEIGHT - m_height,
        m_width, m_height, 1,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWOverrideRedirect | CWEventMask, &attr);

    m_gc   = XCreateGC(m_dpy, m_win, 0, nullptr);
    m_font = XLoadQueryFont(m_dpy, "fixed");
}

void StartMenu::show() {
    XMapRaised(m_dpy, m_win);
    XSetInputFocus(m_dpy, m_win, RevertToPointerRoot, CurrentTime);
    m_visible = true;
    draw();
}

void StartMenu::hide() {
    XUnmapWindow(m_dpy, m_win);
    m_visible = false;
    m_search.clear();
    m_filtered = m_apps;
}

bool StartMenu::handle_event(const XEvent& ev) {
    if (!m_visible) return false;
    if (ev.xany.window != m_win) return false;

    switch (ev.type) {
        case Expose:       on_expose();              return true;
        case KeyPress:     on_key(ev.xkey);          return true;
        case ButtonPress:  on_button(ev.xbutton);    return true;
    }
    return false;
}

void StartMenu::draw() {
    // Фон
    XSetForeground(m_dpy, m_gc, alloc_color(20,20,20));
    XFillRectangle(m_dpy, m_win, m_gc, 0, 0, m_width, m_height);
    // Граница
    XSetForeground(m_dpy, m_gc, alloc_color(60,60,60));
    XDrawRectangle(m_dpy, m_win, m_gc, 0, 0, m_width-1, m_height-1);

    draw_search_bar();
    draw_app_list();
    draw_power_bar();
    XFlush(m_dpy);
}

void StartMenu::draw_search_bar() {
    // Поле поиска
    XSetForeground(m_dpy, m_gc, alloc_color(40,40,40));
    XFillRectangle(m_dpy, m_win, m_gc, 12, 12, m_width-24, 36);
    XSetForeground(m_dpy, m_gc, alloc_color(0,120,215));
    XDrawRectangle(m_dpy, m_win, m_gc, 12, 12, m_width-24, 36);

    std::string disp = m_search.empty() ? "Поиск..." : m_search + "_";
    XSetForeground(m_dpy, m_gc, alloc_color(
        m_search.empty() ? 120 : 255, 120, 120));
    if (m_font) {
        XSetFont(m_dpy, m_gc, m_font->fid);
        XDrawString(m_dpy, m_win, m_gc, 20, 36,
                    disp.c_str(), disp.size());
    }
}

void StartMenu::draw_app_list() {
    int y = 60;
    for (int i=0; i<(int)m_filtered.size(); i++) {
        bool hovered = (i == m_hover_idx);
        // Строка
        XSetForeground(m_dpy, m_gc,
            hovered ? alloc_color(0,90,180) : alloc_color(30,30,30));
        XFillRectangle(m_dpy, m_win, m_gc, 8, y, m_width-16, 44);

        // Иконка-кружок с буквой
        XSetForeground(m_dpy, m_gc, alloc_color(0,120,215));
        XFillArc(m_dpy, m_win, m_gc, 14, y+8, 28, 28, 0, 360*64);
        XSetForeground(m_dpy, m_gc, alloc_color(255,255,255));
        if (m_font)
            XDrawString(m_dpy, m_win, m_gc, 22, y+27,
                        m_filtered[i].icon_label.c_str(), 1);

        // Название
        XSetForeground(m_dpy, m_gc, alloc_color(255,255,255));
        if (m_font)
            XDrawString(m_dpy, m_win, m_gc, 52, y+26,
                        m_filtered[i].name.c_str(),
                        m_filtered[i].name.size());

        y += 46;
    }
}

void StartMenu::draw_power_bar() {
    int y = m_height - 50;
    XSetForeground(m_dpy, m_gc, alloc_color(10,10,10));
    XFillRectangle(m_dpy, m_win, m_gc, 0, y, m_width, 50);
    XSetForeground(m_dpy, m_gc, alloc_color(60,60,60));
    XDrawLine(m_dpy, m_win, m_gc, 0, y, m_width, y);

    struct Btn { const char* label; int x; };
    Btn btns[] = {{"Сеанс", 10}, {"Перезагрузка", 80}, {"Выключение", 200}};
    for (auto& b : btns) {
        XSetForeground(m_dpy, m_gc, alloc_color(255,255,255));
        if (m_font)
            XDrawString(m_dpy, m_win, m_gc, b.x, y+30,
                        b.label, strlen(b.label));
    }
}

void StartMenu::filter_apps() {
    m_filtered.clear();
    for (auto& a : m_apps) {
        std::string name = a.name;
        // регистронезависимый поиск
        auto li = m_search, ln = name;
        std::transform(li.begin(),li.end(),li.begin(),::tolower);
        std::transform(ln.begin(),ln.end(),ln.begin(),::tolower);
        if (ln.find(li) != std::string::npos)
            m_filtered.push_back(a);
    }
}

void StartMenu::launch(const AppEntry& app) {
    hide();
    if (fork() == 0) {
        setsid();
        execlp("sh","sh","-c", app.command.c_str(), nullptr);
        _exit(1);
    }
}

void StartMenu::on_key(const XKeyEvent& e) {
    KeySym ks = XLookupKeysym(const_cast<XKeyEvent*>(&e), 0);
    if (ks == XK_Escape) { hide(); return; }
    if (ks == XK_BackSpace) {
        if (!m_search.empty()) m_search.pop_back();
    } else {
        char buf[8]{};
        XLookupString(const_cast<XKeyEvent*>(&e), buf, sizeof(buf)-1, nullptr, nullptr);
        if (buf[0] >= 32 && buf[0] < 127) m_search += buf[0];
    }
    filter_apps();
    m_hover_idx = -1;
    draw();
}

void StartMenu::on_button(const XButtonEvent& e) {
    // Клик по приложению
    int y = 60;
    for (int i=0; i<(int)m_filtered.size(); i++) {
        if (e.y >= y && e.y < y+44) { launch(m_filtered[i]); return; }
        y += 46;
    }
    // Power bar
    int py = m_height - 50;
    if (e.y >= py) {
        if      (e.x < 80)  { hide(); /* завершить сеанс */ }
        else if (e.x < 200) { system("reboot"); }
        else                 { system("shutdown -h now"); }
    }
}

void StartMenu::on_expose() { draw(); }
