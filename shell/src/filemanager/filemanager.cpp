#include "filemanager.h"
#include "../taskbar/taskbar.h"

#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <climits>

unsigned long FileManager::alloc_color(uint8_t r, uint8_t g, uint8_t b) {
    XColor c{};
    c.red=r*257; c.green=g*257; c.blue=b*257;
    c.flags=DoRed|DoGreen|DoBlue;
    XAllocColor(m_dpy, DefaultColormap(m_dpy, m_screen), &c);
    return c.pixel;
}

FileManager::FileManager(Display* dpy, int screen)
    : m_dpy(dpy), m_screen(screen), m_win(None)
{
    m_width  = 900;
    m_height = 600;

    const char* home = getenv("HOME");
    m_current_path = home ? home : "/";

    m_bookmarks = {
        {"Рабочий стол", std::string(home ? home : "/") + "/Desktop"},
        {"Документы",    std::string(home ? home : "/") + "/Documents"},
        {"Загрузки",     std::string(home ? home : "/") + "/Downloads"},
        {"Музыка",       std::string(home ? home : "/") + "/Music"},
        {"Изображения",  std::string(home ? home : "/") + "/Pictures"},
        {"Корень",       "/"},
        {"tmp",          "/tmp"},
    };
}

FileManager::~FileManager() {
    if (m_font) XFreeFont(m_dpy, m_font);
    if (m_buf)  XFreePixmap(m_dpy, m_buf);
    if (m_gc)   XFreeGC(m_dpy, m_gc);
    if (m_win)  XDestroyWindow(m_dpy, m_win);
}

void FileManager::create() {
    int sh = DisplayHeight(m_dpy, m_screen) - Taskbar::HEIGHT;

    XSetWindowAttributes attr{};
    attr.background_pixel = alloc_color(255,255,255);
    attr.event_mask = KeyPressMask | ButtonPressMask |
                      ExposureMask | Button4MotionMask;

    m_win = XCreateWindow(m_dpy, RootWindow(m_dpy, m_screen),
        100, 80, m_width, m_height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWEventMask, &attr);

    XSelectInput(m_dpy, m_win,
        KeyPressMask | ButtonPressMask | ButtonReleaseMask |
        ExposureMask | Button4MotionMask | Button5MotionMask |
        ButtonPressMask);

    XStoreName(m_dpy, m_win, "Файловый менеджер — MAT OS");

    m_buf  = XCreatePixmap(m_dpy, m_win, m_width, m_height,
                           DefaultDepth(m_dpy, m_screen));
    m_gc   = XCreateGC(m_dpy, m_buf, 0, nullptr);
    m_font = XLoadQueryFont(m_dpy, "fixed");

    XMapWindow(m_dpy, m_win);
    navigate(m_current_path);
}

// ─── Навигация ─────────────────────────────────────────────────────────────
void FileManager::navigate(const std::string& path) {
    // Проверим что путь существует
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return;

    if (m_history_pos < 0 ||
        m_history[m_history_pos] != path) {
        m_history.erase(m_history.begin() + m_history_pos + 1,
                        m_history.end());
        m_history.push_back(path);
        m_history_pos = m_history.size()-1;
    }
    m_current_path = path;
    m_selected = 0; m_scroll = 0;
    m_addr_buf = path;
    load_entries();
    draw();
}

void FileManager::navigate_back() {
    if (m_history_pos > 0)
        navigate(m_history[--m_history_pos]);
}

void FileManager::navigate_forward() {
    if (m_history_pos < (int)m_history.size()-1)
        navigate(m_history[++m_history_pos]);
}

void FileManager::navigate_up() {
    size_t p = m_current_path.rfind('/');
    if (p == std::string::npos || m_current_path == "/") return;
    std::string parent = (p == 0) ? "/" : m_current_path.substr(0, p);
    navigate(parent);
}

void FileManager::load_entries() {
    m_entries.clear();
    DIR* d = opendir(m_current_path.c_str());
    if (!d) return;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name == ".") continue;

        std::string full = m_current_path + "/" + name;
        struct stat st{};
        stat(full.c_str(), &st);

        FileEntry fe;
        fe.name   = name;
        fe.is_dir = S_ISDIR(st.st_mode);
        fe.size   = st.st_size;
        fe.mtime  = st.st_mtime;
        m_entries.push_back(fe);
    }
    closedir(d);

    std::sort(m_entries.begin(), m_entries.end(), [](auto& a, auto& b){
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });
}

void FileManager::open_selected() {
    if (m_selected < 0 || m_selected >= (int)m_entries.size()) return;
    auto& e = m_entries[m_selected];
    std::string full = m_current_path + "/" + e.name;
    if (e.is_dir) {
        navigate(full);
    } else {
        // xdg-open в фоне
        std::string cmd = "xdg-open " + full + " &";
        system(cmd.c_str());
    }
}

void FileManager::delete_selected() {
    if (m_selected < 0 || m_selected >= (int)m_entries.size()) return;
    auto& e = m_entries[m_selected];
    if (e.name == "..") return;
    std::string full = m_current_path + "/" + e.name;
    std::string cmd  = e.is_dir ? "rm -rf " + full : "rm -f " + full;
    system(cmd.c_str());
    load_entries();
    m_selected = std::max(0, m_selected-1);
    draw();
}

void FileManager::create_folder() {
    // Запрос через xterm
    system(("xterm -e bash -c 'read -p \"Имя папки: \" n; mkdir -p \"" +
            m_current_path + "/$n\"' &").c_str());
}

// ─── Отрисовка ─────────────────────────────────────────────────────────────
void FileManager::draw() {
    // Белый фон
    XSetForeground(m_dpy, m_gc, alloc_color(255,255,255));
    XFillRectangle(m_dpy, m_buf, m_gc, 0, 0, m_width, m_height);

    draw_toolbar();
    draw_sidebar();
    draw_filelist();
    draw_statusbar();

    XCopyArea(m_dpy, m_buf, m_win, m_gc,
              0, 0, m_width, m_height, 0, 0);
    XFlush(m_dpy);
}

void FileManager::draw_toolbar() {
    // Фон тулбара
    XSetForeground(m_dpy, m_gc, alloc_color(245,245,245));
    XFillRectangle(m_dpy, m_buf, m_gc, 0, 0, m_width, TOOLBAR_H);
    XSetForeground(m_dpy, m_gc, alloc_color(200,200,200));
    XDrawLine(m_dpy, m_buf, m_gc, 0, TOOLBAR_H-1, m_width, TOOLBAR_H-1);

    // Кнопки Back / Forward / Up
    struct Btn { const char* label; int x; };
    Btn btns[] = {{"<", 4}, {">", 30}, {"^", 56}};
    if (m_font) XSetFont(m_dpy, m_gc, m_font->fid);
    for (auto& b : btns) {
        XSetForeground(m_dpy, m_gc, alloc_color(220,220,220));
        XFillRectangle(m_dpy, m_buf, m_gc, b.x, 4, 24, TOOLBAR_H-8);
        XSetForeground(m_dpy, m_gc, alloc_color(0,0,0));
        if (m_font)
            XDrawString(m_dpy, m_buf, m_gc, b.x+7, TOOLBAR_H/2+5,
                        b.label, strlen(b.label));
    }

    // Адресная строка
    int ax = 90;
    bool focused = m_addr_focus;
    XSetForeground(m_dpy, m_gc, alloc_color(255,255,255));
    XFillRectangle(m_dpy, m_buf, m_gc, ax, 4, m_width-ax-8, TOOLBAR_H-8);
    XSetForeground(m_dpy, m_gc,
        focused ? alloc_color(0,120,215) : alloc_color(180,180,180));
    XDrawRectangle(m_dpy, m_buf, m_gc, ax, 4, m_width-ax-8, TOOLBAR_H-8);
    XSetForeground(m_dpy, m_gc, alloc_color(0,0,0));
    std::string addr = m_addr_focus ? (m_addr_buf + "_") : m_current_path;
    if (m_font)
        XDrawString(m_dpy, m_buf, m_gc, ax+6, TOOLBAR_H/2+5,
                    addr.c_str(), std::min((int)addr.size(), 80));
}

void FileManager::draw_sidebar() {
    int y0 = TOOLBAR_H;
    // Фон
    XSetForeground(m_dpy, m_gc, alloc_color(240,240,240));
    XFillRectangle(m_dpy, m_buf, m_gc, 0, y0,
                   SIDEBAR_W, m_height-y0-STATUS_H);
    XSetForeground(m_dpy, m_gc, alloc_color(200,200,200));
    XDrawLine(m_dpy, m_buf, m_gc,
              SIDEBAR_W, y0, SIDEBAR_W, m_height-STATUS_H);

    int y = y0+10;
    for (auto& bm : m_bookmarks) {
        bool active = (bm.path == m_current_path);
        if (active) {
            XSetForeground(m_dpy, m_gc, alloc_color(0,120,215));
            XFillRectangle(m_dpy, m_buf, m_gc, 2, y, SIDEBAR_W-4, 22);
        }
        XSetForeground(m_dpy, m_gc,
            active ? alloc_color(255,255,255) : alloc_color(0,0,0));
        if (m_font)
            XDrawString(m_dpy, m_buf, m_gc, 10, y+15,
                        bm.label.c_str(), bm.label.size());
        y += 26;
    }
}

void FileManager::draw_filelist() {
    int x0 = SIDEBAR_W + 1, y0 = TOOLBAR_H;
    int listw = m_width - x0;
    int listh = m_height - y0 - STATUS_H;

    // Заголовок колонок
    XSetForeground(m_dpy, m_gc, alloc_color(240,240,240));
    XFillRectangle(m_dpy, m_buf, m_gc, x0, y0, listw, ROW_H);
    XSetForeground(m_dpy, m_gc, alloc_color(0,0,0));
    if (m_font) {
        XSetFont(m_dpy, m_gc, m_font->fid);
        XDrawString(m_dpy, m_buf, m_gc, x0+4,  y0+15, "Имя",         3);
        XDrawString(m_dpy, m_buf, m_gc, x0+400, y0+15, "Размер",      6);
        XDrawString(m_dpy, m_buf, m_gc, x0+520, y0+15, "Изменён",     7);
    }
    XSetForeground(m_dpy, m_gc, alloc_color(200,200,200));
    XDrawLine(m_dpy, m_buf, m_gc, x0, y0+ROW_H, m_width, y0+ROW_H);

    // Строки файлов
    int max_rows = (listh - ROW_H) / ROW_H;
    int start = m_scroll;
    for (int i=start; i < (int)m_entries.size() && i < start+max_rows; i++) {
        int ry = y0 + ROW_H + (i-start)*ROW_H;
        auto& e = m_entries[i];
        bool sel = (i == m_selected);

        if (sel) {
            XSetForeground(m_dpy, m_gc, alloc_color(0,120,215));
            XFillRectangle(m_dpy, m_buf, m_gc, x0, ry, listw, ROW_H);
        }

        XSetForeground(m_dpy, m_gc,
            sel ? alloc_color(255,255,255) : alloc_color(0,0,0));

        if (m_font) {
            // Иконка
            const char* icon = e.is_dir ? "[D]" : "[F]";
            XDrawString(m_dpy, m_buf, m_gc, x0+4, ry+15,
                        icon, 3);
            // Имя
            std::string name = e.name;
            if (name.size()>40) name = name.substr(0,38)+"..";
            XDrawString(m_dpy, m_buf, m_gc, x0+30, ry+15,
                        name.c_str(), name.size());
            // Размер
            if (!e.is_dir) {
                std::string sz = human_size(e.size);
                XDrawString(m_dpy, m_buf, m_gc, x0+400, ry+15,
                            sz.c_str(), sz.size());
            }
            // Дата
            std::string dt = format_time(e.mtime);
            XDrawString(m_dpy, m_buf, m_gc, x0+520, ry+15,
                        dt.c_str(), dt.size());
        }
    }
}

void FileManager::draw_statusbar() {
    int y = m_height - STATUS_H;
    XSetForeground(m_dpy, m_gc, alloc_color(240,240,240));
    XFillRectangle(m_dpy, m_buf, m_gc, 0, y, m_width, STATUS_H);
    XSetForeground(m_dpy, m_gc, alloc_color(200,200,200));
    XDrawLine(m_dpy, m_buf, m_gc, 0, y, m_width, y);

    char info[256];
    snprintf(info, sizeof(info), "  Объектов: %d   Путь: %s",
             (int)m_entries.size(), m_current_path.c_str());
    XSetForeground(m_dpy, m_gc, alloc_color(80,80,80));
    if (m_font)
        XDrawString(m_dpy, m_buf, m_gc, 4, y+16,
                    info, strlen(info));
}

// ─── Вспомогательные ──────────────────────────────────────────────────────
std::string FileManager::human_size(off_t b) {
    char buf[32];
    if      (b >= 1073741824LL) snprintf(buf,sizeof(buf),"%.1f ГБ", b/1073741824.0);
    else if (b >= 1048576LL)    snprintf(buf,sizeof(buf),"%.1f МБ", b/1048576.0);
    else if (b >= 1024LL)       snprintf(buf,sizeof(buf),"%.0f КБ", b/1024.0);
    else                        snprintf(buf,sizeof(buf),"%lld Б",  (long long)b);
    return buf;
}

std::string FileManager::format_time(time_t t) {
    char buf[32];
    struct tm* tm = localtime(&t);
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", tm);
    return buf;
}

// ─── Обработчики событий ──────────────────────────────────────────────────
void FileManager::on_key(const XKeyEvent& e) {
    if (m_addr_focus) {
        KeySym ks = XLookupKeysym(const_cast<XKeyEvent*>(&e), 0);
        if (ks == XK_Return) {
            m_addr_focus = false;
            navigate(m_addr_buf);
            return;
        }
        if (ks == XK_Escape) { m_addr_focus = false; draw(); return; }
        if (ks == XK_BackSpace) {
            if (!m_addr_buf.empty()) m_addr_buf.pop_back();
            draw(); return;
        }
        char buf[8]{};
        XLookupString(const_cast<XKeyEvent*>(&e), buf, 7, nullptr, nullptr);
        if (buf[0] >= 32) { m_addr_buf += buf[0]; draw(); }
        return;
    }

    KeySym ks = XLookupKeysym(const_cast<XKeyEvent*>(&e), 0);
    if (ks == XK_Up) {
        m_selected = std::max(0, m_selected-1);
        if (m_selected < m_scroll) m_scroll--;
    } else if (ks == XK_Down) {
        m_selected = std::min((int)m_entries.size()-1, m_selected+1);
        int max_rows = (m_height - TOOLBAR_H - STATUS_H - ROW_H) / ROW_H;
        if (m_selected >= m_scroll + max_rows) m_scroll++;
    } else if (ks == XK_Return) {
        open_selected();
        return;
    } else if (ks == XK_BackSpace) {
        navigate_up(); return;
    } else if (ks == XK_Delete) {
        delete_selected(); return;
    } else if (ks == XK_F5) {
        load_entries(); 
    } else if (ks == XK_F7) {
        create_folder(); return;
    }
    draw();
}

void FileManager::on_button(const XButtonEvent& e) {
    // Колесо мыши
    if (e.button == 4) { m_scroll = std::max(0, m_scroll-1); draw(); return; }
    if (e.button == 5) { m_scroll = std::min((int)m_entries.size()-1, m_scroll+1); draw(); return; }

    // Тулбар
    if (e.y < TOOLBAR_H) {
        if (e.x < 28)  { navigate_back();    return; }
        if (e.x < 54)  { navigate_forward();  return; }
        if (e.x < 80)  { navigate_up();       return; }
        // Клик по адресной строке
        if (e.x >= 90) {
            m_addr_focus = true;
            m_addr_buf   = m_current_path;
            draw(); return;
        }
    }

    // Сайдбар
    if (e.x < SIDEBAR_W) {
        int idx = (e.y - TOOLBAR_H - 10) / 26;
        if (idx >= 0 && idx < (int)m_bookmarks.size())
            navigate(m_bookmarks[idx].path);
        return;
    }

    // Список файлов
    int rel_y = e.y - TOOLBAR_H - ROW_H;
    if (rel_y < 0) return;
    int clicked = m_scroll + rel_y / ROW_H;
    if (clicked >= 0 && clicked < (int)m_entries.size()) {
        if (clicked == m_selected && e.type == ButtonPress) {
            open_selected();
        } else {
            m_selected = clicked;
            draw();
        }
    }
}

void FileManager::run() {
    XEvent ev;
    while (true) {
        XNextEvent(m_dpy, &ev);
        if (ev.xany.window != m_win) continue;
        switch (ev.type) {
            case Expose:      draw();               break;
            case KeyPress:    on_key(ev.xkey);      break;
            case ButtonPress: on_button(ev.xbutton); break;
        }
    }
}
