#include "framebuffer.h"
#include "font8x16.inc"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <algorithm>

Framebuffer::Framebuffer(const char* dev) : m_dev(dev) {}

Framebuffer::~Framebuffer() { close(); }

bool Framebuffer::open() {
    m_fd = ::open(m_dev, O_RDWR);
    if (m_fd < 0) return false;

    fb_var_screeninfo vinfo{};
    fb_fix_screeninfo finfo{};
    if (ioctl(m_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) return false;
    if (ioctl(m_fd, FBIOGET_FSCREENINFO, &finfo) < 0) return false;

    m_width  = vinfo.xres;
    m_height = vinfo.yres;
    m_bpp    = vinfo.bits_per_pixel / 8;
    m_stride = finfo.line_length;

    size_t size = m_stride * m_height;
    m_fb   = (uint8_t*)mmap(nullptr, size, PROT_READ|PROT_WRITE,
                             MAP_SHARED, m_fd, 0);
    m_back = new uint8_t[size];
    if (m_fb == MAP_FAILED) { m_fb = nullptr; return false; }
    return true;
}

void Framebuffer::close() {
    if (m_fb) {
        munmap(m_fb, m_stride * m_height);
        m_fb = nullptr;
    }
    delete[] m_back; m_back = nullptr;
    if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
}

void Framebuffer::write_pixel_raw(uint8_t* buf, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) return;
    uint8_t* p = buf + y * m_stride + x * m_bpp;
    if (m_bpp == 4 || m_bpp == 3) {
        p[0] = c.b; p[1] = c.g; p[2] = c.r;
        if (m_bpp == 4) p[3] = 0xFF;
    } else if (m_bpp == 2) {
        uint16_t px = ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
        p[0] = px & 0xFF; p[1] = px >> 8;
    }
}

void Framebuffer::flip() {
    memcpy(m_fb, m_back, m_stride * m_height);
}

void Framebuffer::set_pixel(int x, int y, Color c) {
    write_pixel_raw(m_back, x, y, c);
}

void Framebuffer::clear(Color c) {
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
            write_pixel_raw(m_back, x, y, c);
    flip();
}

void Framebuffer::fill_rect(int x, int y, int w, int h, Color c) {
    for (int j = y; j < y+h; j++)
        for (int i = x; i < x+w; i++)
            write_pixel_raw(m_back, i, j, c);
    flip();
}

void Framebuffer::draw_rect(int x, int y, int w, int h, Color c, int t) {
    fill_rect(x,     y,     w, t,  c);
    fill_rect(x,     y+h-t, w, t,  c);
    fill_rect(x,     y,     t, h,  c);
    fill_rect(x+w-t, y,     t, h,  c);
}

void Framebuffer::fill_rounded_rect(int x, int y, int w, int h, int r, Color c) {
    fill_rect(x+r, y,   w-2*r, h, c);
    fill_rect(x,   y+r, r,     h-2*r, c);
    fill_rect(x+w-r, y+r, r,   h-2*r, c);
    for (int dy = 0; dy < r; dy++)
        for (int dx = 0; dx < r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                write_pixel_raw(m_back, x+r-1-dx,   y+r-1-dy,   c);
                write_pixel_raw(m_back, x+w-r+dx,   y+r-1-dy,   c);
                write_pixel_raw(m_back, x+r-1-dx,   y+h-r+dy,   c);
                write_pixel_raw(m_back, x+w-r+dx,   y+h-r+dy,   c);
            }
        }
    flip();
}

void Framebuffer::fill_circle(int cx, int cy, int radius, Color c) {
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++)
            if (dx*dx + dy*dy <= radius*radius)
                write_pixel_raw(m_back, cx+dx, cy+dy, c);
    flip();
}

void Framebuffer::draw_circle(int cx, int cy, int radius, Color c, int t) {
    for (int r2 = radius-t; r2 <= radius; r2++)
        for (int dy = -r2; dy <= r2; dy++)
            for (int dx = -r2; dx <= r2; dx++)
                if (dx*dx + dy*dy >= (r2-1)*(r2-1) &&
                    dx*dx + dy*dy <= r2*r2)
                    write_pixel_raw(m_back, cx+dx, cy+dy, c);
    flip();
}

void Framebuffer::draw_line(int x0, int y0, int x1, int y1, Color c) {
    int dx = std::abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx+dy;
    while (true) {
        write_pixel_raw(m_back, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    flip();
}

void Framebuffer::draw_char(int x, int y, char ch, Color fg, Color bg, int scale) {
    const uint8_t* glyph = font8x16[(uint8_t)ch < 128 ? (uint8_t)ch : 0];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            Color c = (bits & (0x80 >> col)) ? fg : bg;
            for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    write_pixel_raw(m_back,
                        x + col*scale + sx,
                        y + row*scale + sy, c);
        }
    }
    flip();
}

void Framebuffer::draw_text(int x, int y, const std::string& s,
                             Color fg, Color bg, int scale) {
    int cx = x;
    for (char ch : s) {
        draw_char(cx, y, ch, fg, bg, scale);
        cx += 8 * scale;
    }
}

int Framebuffer::text_width(const std::string& s, int scale) const {
    return (int)s.size() * 8 * scale;
}

void Framebuffer::fill_gradient_v(int x, int y, int w, int h,
                                   Color top, Color bot) {
    for (int j = 0; j < h; j++) {
        float t = (float)j / h;
        Color c { uint8_t(top.r + t*(bot.r-top.r)),
                  uint8_t(top.g + t*(bot.g-top.g)),
                  uint8_t(top.b + t*(bot.b-top.b)) };
        for (int i = x; i < x+w; i++)
            write_pixel_raw(m_back, i, y+j, c);
    }
    flip();
}
