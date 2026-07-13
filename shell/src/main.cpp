/*
 * MAT OS Shell — точка входа оболочки рабочего стола
 *
 * Запускается через xinit (из .xinitrc после matos-login).
 * Инициализирует: BSOD обработчики, WM, Taskbar, StartMenu.
 * Основной event loop обрабатывает события всех компонентов.
 */
#include "wm/window_manager.h"
#include "taskbar/taskbar.h"
#include "startmenu/startmenu.h"
#include "bsod/bsod.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>

// ─── Установить обои (градиент через Xlib) ─────────────────────────────────
static void set_wallpaper(Display* dpy, Window root, int screen) {
    int sw = DisplayWidth(dpy, screen);
    int sh = DisplayHeight(dpy, screen);

    GC gc = XCreateGC(dpy, root, 0, nullptr);

    // Вертикальный градиент: тёмно-синий → синий (как Win10)
    for (int y = 0; y < sh; y++) {
        float t = (float)y / sh;
        uint8_t r = uint8_t(3   + t * 10);
        uint8_t g = uint8_t(57  + t * 40);
        uint8_t b = uint8_t(128 + t * 60);

        XColor c{};
        c.red   = r * 257;
        c.green = g * 257;
        c.blue  = b * 257;
        c.flags = DoRed | DoGreen | DoBlue;
        XAllocColor(dpy, DefaultColormap(dpy, screen), &c);
        XSetForeground(dpy, gc, c.pixel);
        XDrawLine(dpy, root, gc, 0, y, sw, y);
    }

    XFreeGC(dpy, gc);
    XFlush(dpy);
}

// ─── main ──────────────────────────────────────────────────────────────────
int main() {
    // Устанавливаем BSOD обработчики первыми
    bsod_install_handlers();

    // Открываем дисплей
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        fprintf(stderr, "MAT OS Shell: не удалось открыть X дисплей.\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    // Обои
    set_wallpaper(dpy, root, screen);

    // ─── Taskbar ───────────────────────────────────────────────────────────
    Taskbar taskbar(dpy, screen);
    taskbar.create();
    taskbar.draw();

    // ─── Start Menu ────────────────────────────────────────────────────────
    StartMenu start_menu(dpy, screen);
    start_menu.create();

    // ─── Window Manager ────────────────────────────────────────────────────
    // WM запускаем в отдельном потоке, т.к. у него собственный event loop
    WindowManager wm(dpy);

    // Обновление часов на taskbar каждую секунду
    std::atomic<bool> running(true);
    std::thread clock_thread([&](){
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
std::vector<TaskEntry> task_list_init;
for (const auto& mw : wm.get_tasks()) {
    task_list_init.push_back({mw.client, mw.frame, mw.title, false});
}
taskbar.draw(task_list_init);
        }
    });

    // ─── Главный event loop ────────────────────────────────────────────────
    // Обрабатываем события taskbar и start menu;
    // WM обрабатывает свои события через XSubstructureRedirect на root.
    XEvent ev;
    while (true) {
        XNextEvent(dpy, &ev);

        // Меню Пуск
        if (start_menu.handle_event(ev)) {
            std::vector<TaskEntry> task_list;
for (const auto& mw : wm.get_tasks()) {
    task_list.push_back({mw.client, mw.frame, mw.title, false});
}
taskbar.draw(task_list);
            continue;
        }

        // Клик по taskbar
        if (ev.type == ButtonPress &&
            ev.xbutton.window == taskbar.window()) {
            Window activated = None;
            if (taskbar.handle_click(ev.xbutton.x, ev.xbutton.y, activated)) {
                if (taskbar.start_menu_open()) start_menu.show();
                else start_menu.hide();

                if (activated != None) {
                    XMapWindow(dpy, activated);
                    XRaiseWindow(dpy, activated);
                    XSetInputFocus(dpy, activated,
                                   RevertToPointerRoot, CurrentTime);
                }
            }
            std::vector<TaskEntry> task_list_update;
for (const auto& mw : wm.get_tasks()) {
    task_list_update.push_back({mw.client, mw.frame, mw.title, false});
}
taskbar.draw(task_list_update);
            continue;
        }

        // WM события (MapRequest, ConfigureRequest и т.д.)
        switch (ev.type) {
            case MapRequest:
            case ConfigureRequest:
            case UnmapNotify:
            case DestroyNotify:
            case ButtonPress:
            case ButtonRelease:
            case MotionNotify:
            case KeyPress:
            case Expose:
                // Перенаправляем в WM через его метод
                // (WM зарегистрирован через SubstructureRedirect)
                break;
        }
    }

    running = false;
    if (clock_thread.joinable()) clock_thread.join();
    XCloseDisplay(dpy);
    return 0;
}
