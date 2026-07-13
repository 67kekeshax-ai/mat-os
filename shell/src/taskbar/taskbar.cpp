#include "taskbar.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <ctime>
#include <cstring>
#include <cstdio>

unsigned long Taskbar::alloc_color(uint8_t r, uint8_t g, uint8_t b) {
    XColor c{};
    c.red=r*257; c.green=g*257; c.blue=b*257;
    c.flags=DoRed|DoGreen|DoBlue;
    XAllocColor(m_dpy, DefaultColormap(m_dpy, m_screen), &c);
    return c.pixel;
}

Taskbar::Taskbar(Display* dpy, int screen)
    : m_dpy(dpy), m_screen(screen), m_win(None), m_buf(None)
{
    m_width = DisplayWidth(dpy, screen);
    m_color_bg     = alloc_color(30, 30, 30);
    m_color_btn    = alloc_color(50, 50, 50);
    m_color_active = alloc_color(0, 120, 215);
    m_color_text   = alloc_color(255,255,255);
    m_color_hover  = alloc_color(70, 70, 70);
}

Taskbar::~Taskbar() {
    if (m_buf) XFreePixmap(m_dpy, m_buf);
    if (m_gc)  XFreeGC(m_dpy, m_gc);
    if (m_win) XDestroyWindow(m_dpy, m_win);
}

void Taskbar::create() {
    int sh = DisplayHeight(m_dpy, m_screen);

    XSetWindowAttributes attr{};
    attr.background_pixel  = m_color_bg;
    attr.override_redirect = True;
    attr.event_mask = ButtonPressMask | ExposureMask;

    m_win = XCreateWindow(m_dpy, RootWindow(m_dpy, m_screen),
        0, sh - HEIGHT, m_width, HEIGHT, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWOverrideRedirect | CWEventMask, &attr);

    // EWMH: поверх всех окон
    Atom state  = XInternAtom(m_dpy, "_NET_WM_STATE", False);
    Atom above  = XInternAtom(m_dpy, "_NET_WM_STATE_ABOVE", False);
    Atom strut  = XInternAtom(m_dpy, "_NET_WM_STRUT", False);
    Atom type   = XInternAtom(m_dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dock   = XInternAtom(m_dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);

    XChangeProperty(m_dpy, m_win, state, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&above, 1);
    XChangeProperty(m_dpy, m_win, type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&dock, 1);

    long strut_vals[4] = {0, 0, 0, HEIGHT};
    XChangeProperty(m_dpy, m_win, strut, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)strut_vals, 4);

    m_buf = XCreatePixmap(m_dpy, m_win, m_width, HEIGHT,
                          DefaultDepth(m_dpy, m_screen));
    m_gc  = XCreateGC(m_dpy, m_buf, 0, nullptr);

    XMapRaised(m_dpy, m_win);
}

void Taskbar::draw() {
    draw(m_tasks);
}

void Taskbar::draw(const std::vector<TaskEntry>& tasks) {
    m_tasks = tasks;

    // Фон
    XSetForeground(m_dpy, m_gc, m_color_bg);
    XFillRectangle(m_dpy, m_buf, m_gc, 0, 0, m_width, HEIGHT);

    // Разделитель сверху
    XSetForeground(m_dpy, m_gc, alloc_color(60,60,60));
    XDrawLine(m_dpy, m_buf, m_gc, 0, 0, m_width, 0);

    draw_start_button();
    draw_tasks();
    draw_clock();
    draw_show_desktop();

    // Копировать буфер на экран
    XCopyArea(m_dpy, m_buf, m_win, m_gc, 0, 0, m_width, HEIGHT, 0, 0);
    XFlush(m_dpy);
}

void Taskbar::draw_start_button() {
    // 4-цветный логотип (только изображение, без текста)
    int px = 8, py = 8, sz = HEIGHT - 16;
    int h = sz/2, w = sz/2;

    // Красный (левый верхний)
    XSetForeground(m_dpy, m_gc, alloc_color(246,83,20));
    XFillRectangle(m_dpy, m_buf, m_gc, px, py, w-1, h-1);
    // Зелёный (правый верхний)
    XSetForeground(m_dpy, m_gc, alloc_color(124,187,0));
    XFillRectangle(m_dpy, m_buf, m_gc, px+w+1, py, w-1, h-1);
    // Синий (левый нижний)
    XSetForeground(m_dpy, m_gc, alloc_color(0,161,241));
    XFillRectangle(m_dpy, m_buf, m_gc, px, py+h+1, w-1, h-1);
    // Жёлтый (правый нижний)
    XSetForeground(m_dpy, m_gc, alloc_color(255,187,0));
    XFillRectangle(m_dpy, m_buf, m_gc, px+w+1, py+h+1, w-1, h-1);
}

void Taskbar::draw_tasks() {
    int x = HEIGHT + 8;
    XFontStruct* fs = XLoadQueryFont(m_dpy, "fixed");

    for (auto& t : m_tasks) {
        int tw = 160;
        // Фон кнопки
        XSetForeground(m_dpy, m_gc, t.active ? m_color_active : m_color_btn);
        XFillRectangle(m_dpy, m_buf, m_gc, x, 4, tw, HEIGHT-8);

        // Текст (обрезаем до ~18 символов)
        std::string title = t.title;
        if (title.size() > 18) title = title.substr(0,17) + "…";
        if (fs) {
            XSetForeground(m_dpy, m_gc, m_color_text);
            XSetFont(m_dpy, m_gc, fs->fid);
            XDrawString(m_dpy, m_buf, m_gc, x+8, HEIGHT/2+6,
                        title.c_str(), title.size());
        }

        // Синяя линия снизу для активного окна
        if (t.active) {
            XSetForeground(m_dpy, m_gc, alloc_color(0,120,215));
            XFillRectangle(m_dpy, m_buf, m_gc, x, HEIGHT-4, tw, 4);
        }

        x += tw + 2;
    }
    if (fs) XFreeFont(m_dpy, fs);
}

void Taskbar::draw_clock() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char time_str[16], date_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", t);
    strftime(date_str, sizeof(date_str), "%d.%m.%Y", t);

    XFontStruct* fs = XLoadQueryFont(m_dpy, "fixed");
    if (!fs) return;

    int tw = XTextWidth(fs, time_str, strlen(time_str));
    int dw = XTextWidth(fs, date_str, strlen(date_str));
    int cx = m_width - std::max(tw,dw) - 20;

    XSetForeground(m_dpy, m_gc, m_color_text);
    XSetFont(m_dpy, m_gc, fs->fid);
    XDrawString(m_dpy, m_buf, m_gc, cx + (std::max(tw,dw)-tw)/2,
                HEIGHT/2 - 2, time_str, strlen(time_str));
    XDrawString(m_dpy, m_buf, m_gc, cx + (std::max(tw,dw)-dw)/2,
                HEIGHT/2 + 12, date_str, strlen(date_str));
    XFreeFont(m_dpy, fs);
}

void Taskbar::draw_show_desktop() {
    // Узкая кнопка в правом краю
    XSetForeground(m_dpy, m_gc, alloc_color(60,60,60));
    XFillRectangle(m_dpy, m_buf, m_gc, m_width-16, 1, 15, HEIGHT-2);
}

bool Taskbar::handle_click(int x, int y, Window& activated) {
    (void)y;
    // Кнопка Пуск
    if (x >= 0 && x < HEIGHT) { toggle_start_menu(); return true; }

    // Кнопки задач
    int tx = HEIGHT + 8;
    for (auto& t : m_tasks) {
        if (x >= tx && x < tx+160) {
            activated = t.frame;
            return true;
        }
        tx += 162;
    }

    // Show Desktop
    if (x >= m_width-16) return true;
    return false;
}

void Taskbar::toggle_start_menu() {
    m_start_open = !m_start_open;
}
