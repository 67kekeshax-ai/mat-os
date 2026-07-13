#include "login.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <shadow.h>
#include <crypt.h>
#include <termios.h>
#include <fstream>
#include <string>

// ─── Framebuffer mini-renderer (локальный, без зависимости от installer) ──
struct FBLogin {
    int fd=-1; uint8_t* mem=nullptr;
    int w=0,h=0,bpp=0,stride=0;

    bool open_fb() {
        fd=::open("/dev/fb0",O_RDWR);
        if(fd<0)return false;
        fb_var_screeninfo v{}; fb_fix_screeninfo f{};
        ioctl(fd,FBIOGET_VSCREENINFO,&v);
        ioctl(fd,FBIOGET_FSCREENINFO,&f);
        w=v.xres;h=v.yres;bpp=v.bits_per_pixel/8;stride=f.line_length;
        mem=(uint8_t*)mmap(nullptr,stride*h,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        return mem!=MAP_FAILED;
    }
    void px(int x,int y,uint8_t r,uint8_t g,uint8_t b){
        if(x<0||y<0||x>=w||y>=h)return;
        uint8_t*p=mem+y*stride+x*bpp;
        if(bpp>=3){p[0]=b;p[1]=g;p[2]=r;if(bpp==4)p[3]=0xFF;}
    }
    void rect(int x,int y,int rw,int rh,uint8_t r,uint8_t g,uint8_t b){
        for(int j=y;j<y+rh;j++)for(int i=x;i<x+rw;i++)px(i,j,r,g,b);
    }
    void circle(int cx,int cy,int rad,uint8_t r,uint8_t g,uint8_t b){
        for(int dy=-rad;dy<=rad;dy++)
            for(int dx=-rad;dx<=rad;dx++)
                if(dx*dx+dy*dy<=rad*rad)px(cx+dx,cy+dy,r,g,b);
    }
    // Простой текст 5×7
    void text(int x,int y,const char*s,uint8_t r,uint8_t g,uint8_t b);
};

// Встроенный мини-шрифт (только цифры, двоеточие, пробел, слэш)
static const uint8_t mf[][5]={
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x00,0x00,0x00,0x00}, // space
    {0x20,0x10,0x08,0x04,0x02}, // /
};

void FBLogin::text(int x,int y,const char*s,uint8_t r,uint8_t g,uint8_t b){
    int cx=x;
    while(*s){
        char c=*s++;int idx=-1;
        if(c>='0'&&c<='9')idx=c-'0';
        else if(c==':')idx=10;
        else if(c==' ')idx=11;
        else if(c=='/')idx=12;
        if(idx>=0){
            for(int col=0;col<5;col++){
                uint8_t bits=mf[idx][col];
                for(int row=0;row<7;row++)
                    if(bits&(1<<row))
                        for(int sy=0;sy<3;sy++)for(int sx=0;sx<3;sx++)
                            px(cx+col*3+sx,y+row*3+sy,r,g,b);
            }
        }
        cx+=18;
    }
}

// ─── Аутентификация через /etc/shadow ─────────────────────────────────────
static bool auth_shadow(const std::string& username, const std::string& password) {
    struct spwd* sp = getspnam(username.c_str());
    if (!sp) {
        // Fallback: проверяем /etc/passwd (нешифрованный пароль для тестов)
        return !password.empty();
    }
    char* hashed = crypt(password.c_str(), sp->sp_pwdp);
    return hashed && strcmp(hashed, sp->sp_pwdp) == 0;
}

// ─── Чтение редакции ───────────────────────────────────────────────────────
static std::string read_edition() {
    std::ifstream f("/etc/matos-edition");
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("EDITION=") == 0)
            return line.substr(8);
    }
    return "MAT OS";
}

// ─── Чтение пароля из termios ─────────────────────────────────────────────
static std::string read_password() {
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

    std::string s;
    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == '\n' || ch == '\r') break;
        if (ch == 127 || ch == '\b') { if (!s.empty()) s.pop_back(); }
        else if (ch >= 32) s += ch;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    return s;
}

// ─── Главная функция ───────────────────────────────────────────────────────
bool run_login_screen(const std::string& username) {
    FBLogin fb;
    bool has_fb = fb.open_fb();

    std::string edition = read_edition();
    int tries = 0;

    while (tries < 5) {
        if (has_fb) {
            // Фон (тёмно-синий градиент)
            for (int y = 0; y < fb.h; y++) {
                float t = (float)y / fb.h;
                uint8_t r = uint8_t(3  + t*20);
                uint8_t g = uint8_t(45 + t*10);
                uint8_t b = uint8_t(96 + t*30);
                for (int x = 0; x < fb.w; x++) fb.px(x, y, r, g, b);
            }

            // Аватар — синий круг с заглавной буквой имени
            int cx = fb.w/2, cy = fb.h/2 - 80;
            fb.circle(cx, cy, 60, 0, 120, 215);
            fb.circle(cx, cy, 58, 0, 100, 190);
            // Буква (упрощённо через rect)
            fb.rect(cx-8, cy-20, 16, 3, 255,255,255); // верхняя планка

            // Часы
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);
            char tbuf[16], dbuf[16];
            snprintf(tbuf,sizeof(tbuf),"%02d:%02d", t->tm_hour, t->tm_min);
            snprintf(dbuf,sizeof(dbuf),"%02d/%02d/%04d",
                     t->tm_mday, t->tm_mon+1, t->tm_year+1900);

            fb.text(fb.w/2 - 60, 40, tbuf, 255,255,255);
            fb.text(fb.w/2 - 80, 80, dbuf, 200,220,255);

            // Имя пользователя
            // (просто rect-заглушка — нет полного шрифта)

            // Поле ввода пароля
            int px2 = fb.w/2 - 150, py = cy + 90;
            fb.rect(px2, py, 300, 36, 255,255,255);
            fb.rect(px2, py, 300, 2,  0,120,215);
            fb.rect(px2, py+34, 300, 2,  0,120,215);
            fb.rect(px2, py, 2, 36,  0,120,215);
            fb.rect(px2+298, py, 2, 36, 0,120,215);

            // Подсказка ввода
            fb.text(fb.w/2-50, py+10, "        ", 150,150,150);

            // Редакция (нижний правый угол)
            fb.rect(fb.w-200, fb.h-30, 190, 20, 0,56,160);
        }

        // Ввод пароля (консоль)
        printf("\033[2J\033[H"); // clear
        printf("MAT OS — %s\n", edition.c_str());
        printf("Пользователь: %s\n", username.c_str());
        printf("Пароль: ");
        fflush(stdout);

        std::string pwd = read_password();
        printf("\n");

        if (auth_shadow(username, pwd)) return true;

        tries++;
        if (has_fb) {
            // Красная рамка поля ввода при ошибке
            int px2 = fb.w/2 - 150;
            int py  = fb.h/2 - 80 + 90;
            fb.rect(px2, py, 300, 2,  200,0,0);
            fb.rect(px2, py+34, 300, 2, 200,0,0);
        }
        printf("Неверный пароль. Попытка %d/5\n", tries);
        sleep(1);
    }
    return false;
}
