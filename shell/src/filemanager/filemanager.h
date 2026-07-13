#pragma once
#include <X11/Xlib.h>
#include <string>
#include <vector>

struct FileEntry {
    std::string name;
    bool        is_dir;
    off_t       size;
    time_t      mtime;
};

class FileManager {
public:
    FileManager(Display* dpy, int screen);
    ~FileManager();

    void create();
    void run();           // собственный event loop (блокирующий)
    Window window() const { return m_win; }

private:
    Display* m_dpy;
    int      m_screen;
    Window   m_win;
    Pixmap   m_buf;
    GC       m_gc;
    XFontStruct* m_font = nullptr;

    int m_width, m_height;

    std::string m_current_path;
    std::vector<FileEntry> m_entries;
    std::vector<std::string> m_history;
    int m_history_pos = -1;

    int  m_selected  = 0;
    int  m_scroll    = 0;
    bool m_addr_focus = false;
    std::string m_addr_buf;

    static const int SIDEBAR_W = 160;
    static const int TOOLBAR_H = 36;
    static const int STATUS_H  = 24;
    static const int ROW_H     = 22;

    struct Bookmark { std::string label; std::string path; };
    std::vector<Bookmark> m_bookmarks;

    void navigate(const std::string& path);
    void navigate_back();
    void navigate_forward();
    void navigate_up();
    void open_selected();
    void delete_selected();
    void create_folder();

    void load_entries();
    void draw();
    void draw_toolbar();
    void draw_sidebar();
    void draw_filelist();
    void draw_statusbar();

    void on_key(const XKeyEvent& e);
    void on_button(const XButtonEvent& e);
    void on_scroll(int delta);

    std::string human_size(off_t b);
    std::string format_time(time_t t);

    unsigned long alloc_color(uint8_t r, uint8_t g, uint8_t b);
};
