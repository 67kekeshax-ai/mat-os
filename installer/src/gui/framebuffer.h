#pragma once
#include <cstdint>
#include <string>

struct Color {
    uint8_t r, g, b;
    static Color from_hex(uint32_t hex) {
        return { uint8_t((hex >> 16) & 0xFF),
                 uint8_t((hex >> 8)  & 0xFF),
                 uint8_t( hex        & 0xFF) };
    }
};

class Framebuffer {
public:
    explicit Framebuffer(const char* dev = "/dev/fb0");
    ~Framebuffer();

    bool open();
    void close();

    int width()  const { return m_width; }
    int height() const { return m_height; }

    void clear(Color c = {0,0,0});
    void set_pixel(int x, int y, Color c);

    void fill_rect(int x, int y, int w, int h, Color c);
    void draw_rect(int x, int y, int w, int h, Color c, int thickness = 1);
    void fill_rounded_rect(int x, int y, int w, int h, int r, Color c);
    void fill_circle(int x, int y, int radius, Color c);
    void draw_circle(int x, int y, int radius, Color c, int thickness = 1);

    // Линия
    void draw_line(int x0, int y0, int x1, int y1, Color c);

    // Текст (встроенный 8×16 шрифт)
    void draw_char(int x, int y, char ch, Color fg, Color bg, int scale = 1);
    void draw_text(int x, int y, const std::string& s,
                   Color fg, Color bg = {0,0,0}, int scale = 1);
    int  text_width(const std::string& s, int scale = 1) const;

    // Градиент (вертикальный)
    void fill_gradient_v(int x, int y, int w, int h,
                         Color top, Color bot);

private:
    const char* m_dev;
    int         m_fd   = -1;
    uint8_t*    m_fb   = nullptr;
    uint8_t*    m_back = nullptr;   // back buffer
    int         m_width = 0, m_height = 0;
    int         m_bpp   = 0, m_stride  = 0;

    void write_pixel_raw(uint8_t* buf, int x, int y, Color c);
    void flip();  // копирует back buffer в fb
};
