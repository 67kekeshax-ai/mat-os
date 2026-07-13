#include <cstdint>
using std::uint8_t;

#include "bsod.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

// ─── Цвета BSOD ──────────────────────────────────────────────────────────
static const uint8_t BG_R = 0x00, BG_G = 0x56, BG_B = 0xA0;
static const uint8_t FG_R = 0xFF, FG_G = 0xFF, FG_B = 0xFF;

// ─── Шрифт ───────────────────────────────────────────────────────────────
// Минимальный встроенный шрифт 8×8 только для цифр и латиницы (ASCII 32-95)
// Используем системный printf на tty как fallback; для FB рисуем без шрифта
// знаки через блоки (упрощённо).

struct FB {
    int fd = -1;
    uint8_t* mem = nullptr;
    int w = 0, h = 0, bpp = 0, stride = 0;

    bool open() {
        fd = ::open("/dev/fb0", O_RDWR);
        if (fd < 0) return false;
        fb_var_screeninfo v{};
        fb_fix_screeninfo f{};
        ioctl(fd, FBIOGET_VSCREENINFO, &v);
        ioctl(fd, FBIOGET_FSCREENINFO, &f);
        w = v.xres; h = v.yres;
        bpp = v.bits_per_pixel / 8;
        stride = f.line_length;
        mem = (uint8_t*)mmap(nullptr, stride*h,
                             PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        return mem != MAP_FAILED;
    }

    void pix(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x<0||y<0||x>=w||y>=h) return;
        uint8_t* p = mem + y*stride + x*bpp;
        if (bpp>=3) { p[0]=b; p[1]=g; p[2]=r; if(bpp==4) p[3]=0xFF; }
    }

    void rect(int x, int y, int rw, int rh,
              uint8_t r, uint8_t g, uint8_t b) {
        for (int j=y; j<y+rh; j++)
            for (int i=x; i<x+rw; i++)
                pix(i, j, r, g, b);
    }

    void circle(int cx, int cy, int radius,
                uint8_t r, uint8_t g, uint8_t b) {
        for (int dy=-radius; dy<=radius; dy++)
            for (int dx=-radius; dx<=radius; dx++)
                if (dx*dx+dy*dy <= radius*radius)
                    pix(cx+dx, cy+dy, r, g, b);
    }

    // Дуга (для улыбки/грустного рта)
    void arc(int cx, int cy, int radius, double a0, double a1,
             uint8_t r, uint8_t g, uint8_t b, int thick=3) {
        for (int t=0; t<360; t++) {
            double angle = t * 3.14159265 / 180.0;
            if (angle < a0 || angle > a1) continue;
            for (int ri=radius-thick; ri<=radius; ri++) {
                int x = (int)(cx + ri * cos(angle));
                int y = (int)(cy + ri * sin(angle));
                pix(x, y, r, g, b);
            }
        }
    }

    // Простой 5×7 блочный текст (только ASCII, масштаб scale)
    void text(int x, int y, const char* s,
              uint8_t r, uint8_t g, uint8_t b, int scale=2);
};

// 5×7 bitmap mini-font (ASCII 32-95, offset by 32)
static const uint8_t mini_font[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '\''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
};

void FB::text(int x, int y, const char* s,
              uint8_t r, uint8_t g, uint8_t b, int scale) {
    int cx = x;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c < 32 || c > 95) { cx += 6*scale; continue; }
        const uint8_t* glyph = mini_font[c-32];
        for (int col=0; col<5; col++) {
            uint8_t bits = glyph[col];
            for (int row=0; row<7; row++) {
                if (bits & (1<<row)) {
                    for (int sy=0; sy<scale; sy++)
                        for (int sx=0; sx<scale; sx++)
                            pix(cx+col*scale+sx, y+row*scale+sy, r, g, b);
                }
            }
        }
        cx += 6*scale;
    }
}

// ─── Основная функция BSOD ────────────────────────────────────────────────
static void draw_bsod(const char* error_code, const char* message) {
    // Переключиться на tty1
    system("chvt 1 2>/dev/null");
    sleep(1);

    FB fb;
    if (!fb.open()) {
        // Fallback: вывод в консоль
        fprintf(stderr, "\n\n*** КРИТИЧЕСКАЯ ОШИБКА MAT OS ***\n");
        fprintf(stderr, "Код: %s\n%s\n", error_code, message);
        return;
    }

    // Синий фон
    fb.rect(0, 0, fb.w, fb.h, BG_R, BG_G, BG_B);

    // Грустный смайлик :( в верхней трети
    int cx = fb.w/2, cy = fb.h/4;
    int face_r = std::min(fb.w, fb.h) / 8;

    // Лицо (контур)
    fb.circle(cx, cy, face_r, FG_R, FG_G, FG_B);
    fb.circle(cx, cy, face_r-3, BG_R, BG_G, BG_B);

    // Глаза
    int ey = cy - face_r/4;
    fb.circle(cx - face_r/3, ey, face_r/10, FG_R, FG_G, FG_B);
    fb.circle(cx + face_r/3, ey, face_r/10, FG_R, FG_G, FG_B);

    // Рот — дуга вниз (грусть)  π .. 2π  = нижняя дуга
    // рисуем как серию точек
    int mouth_cy = cy + face_r/4;
    int mouth_r  = face_r/3;
    for (int t = 200; t <= 340; t++) {
        double angle = t * 3.14159265 / 180.0;
        int mx = cx + (int)(mouth_r * cos(angle));
        int my = mouth_cy - (int)(mouth_r * sin(angle)); // инвертировано
        for (int k=-2; k<=2; k++) for (int j=-2; j<=2; j++)
            fb.pix(mx+k, my+j, FG_R, FG_G, FG_B);
    }

    // Заголовок
    int scale = std::max(1, fb.w / 640);
    int tw = 10 * 6 * scale * 2;
    fb.text(fb.w/2 - tw/2, fb.h/2,
            "MAT  OS", FG_R, FG_G, FG_B, scale*2);

    // Подзаголовок
    fb.text(fb.w/2 - 200*scale/2, fb.h/2 + 30*scale,
            "YOUR PC RAN INTO A PROBLEM", FG_R, FG_G, FG_B, scale);

    // Код ошибки
    char line[256];
    snprintf(line, sizeof(line), "ERROR: %s", error_code);
    fb.text(fb.w/2 - 150*scale/2, fb.h/2 + 60*scale,
            line, FG_R, FG_G, FG_B, scale);

    // Сообщение
    fb.text(60, fb.h*3/4, message, FG_R, FG_G, FG_B, scale);

    // Таймер перезагрузки
    char tbuf[64];
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    strftime(tbuf, sizeof(tbuf), "%d.%m.%Y %H:%M:%S", tm);
    fb.text(60, fb.h - 60, tbuf, FG_R, FG_G, FG_B, scale);
    fb.text(60, fb.h - 40, "Restarting in 10 seconds...", FG_R, FG_G, FG_B, scale);
}

// ─── Обработчик сигналов ──────────────────────────────────────────────────
static void signal_handler(int sig) {
    const char* code = "UNKNOWN_ERROR";
    const char* msg  = "An unexpected error has occurred.";
    switch (sig) {
        case SIGSEGV: code="SEGMENTATION_FAULT";        msg="Memory access violation."; break;
        case SIGABRT: code="ABNORMAL_TERMINATION";      msg="Process aborted.";         break;
        case SIGBUS:  code="BUS_ERROR";                 msg="Bus error (bad memory alignment)."; break;
        case SIGFPE:  code="FLOATING_POINT_EXCEPTION";  msg="Arithmetic error.";        break;
        case SIGILL:  code="ILLEGAL_INSTRUCTION";       msg="Illegal CPU instruction."; break;
    }
    draw_bsod(code, msg);
    sleep(10);
    system("reboot -f");
    _exit(1);
}

void bsod_install_handlers() {
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGBUS,  signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGILL,  signal_handler);
}

[[noreturn]] void bsod_trigger(const char* error_code, const char* message) {
    draw_bsod(error_code, message);
    sleep(10);
    system("reboot -f");
    _exit(1);
}
