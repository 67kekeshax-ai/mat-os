/*
 * MAT OS Installer — точка входа
 * Графический установщик на Linux framebuffer (/dev/fb0)
 * Без зависимости от X11.
 */
#include "gui/framebuffer.h"
#include "gui/widget.h"
#include "license/license.h"
#include "disk/disk.h"
#include "install/install.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

// ─── Цвета ────────────────────────────────────────────────────────────────
static const Color BG   = {3, 45, 96};     // тёмно-синий фон
static const Color FG   = {255,255,255};
static const Color BLUE = {0, 120, 215};
static const Color RED  = {196, 43, 28};
static const Color DARK = {2, 30, 64};

// ─── Состояния установщика ─────────────────────────────────────────────────
enum class State {
    WELCOME,
    LICENSE,
    DISK_SELECT,
    USER_SETUP,
    PROGRESS,
    DONE
};

static Framebuffer fb;
static InstallConfig install_cfg;

// ─── Вспомогательные функции ───────────────────────────────────────────────
static void draw_header(const std::string& title) {
    fb.fill_rect(0, 0, fb.width(), 64, DARK);
    fb.draw_text(20, 8, "MAT OS", FG, DARK, 2);
    fb.draw_text(20, 38, title, {180,200,220}, DARK, 1);
    fb.fill_rect(0, 64, fb.width(), 2, BLUE);
}

static void draw_footer(const std::string& hint) {
    int y = fb.height() - 36;
    fb.fill_rect(0, y, fb.width(), 36, DARK);
    fb.draw_text(20, y+10, hint, {180,180,180}, DARK, 1);
}

// Читает строку с клавиатуры (консоль в сыром режиме)
static std::string readline_raw(bool password = false) {
    std::string s;
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == '\n' || ch == '\r') break;
        if (ch == 127 || ch == '\b') { if (!s.empty()) s.pop_back(); }
        else if (ch >= 32) s += ch;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    return s;
}

// ─── Экраны ────────────────────────────────────────────────────────────────

static State screen_welcome() {
    fb.clear(BG);
    draw_header("Добро пожаловать в установщик MAT OS");

    int cx = fb.width()/2;
    fb.draw_text(cx - 200, 120, "MAT OS", FG, BG, 4);
    fb.draw_text(cx - 180, 190, "Операционная система нового поколения", {180,200,220}, BG, 1);

    fb.fill_rounded_rect(cx-150, 300, 300, 50, 6, BLUE);
    fb.draw_text(cx - fb.text_width("Начать установку")/2,
                 316, "Начать установку", FG, BLUE);

    fb.fill_rounded_rect(cx-150, 370, 300, 50, 6, {60,60,80});
    fb.draw_text(cx - fb.text_width("Выход")/2,
                 386, "Выход", FG, {60,60,80});

    draw_footer("Нажмите 1 для начала установки, Q для выхода");

    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == '1' || ch == '\n') return State::LICENSE;
        if (ch == 'q' || ch == 'Q') exit(0);
    }
    return State::LICENSE;
}

static State screen_license() {
    fb.clear(BG);
    draw_header("Активация лицензии");

    fb.draw_text(60, 100, "Введите лицензионный ключ MAT OS:", FG, BG, 1);
    fb.draw_text(60, 128, "PRO Edition:  67691200", {180,200,220}, BG, 1);
    fb.draw_text(60, 148, "HOME Edition: 12006769", {180,200,220}, BG, 1);

    fb.fill_rect(60, 200, 400, 40, {255,255,255});
    fb.draw_rect(60, 200, 400, 40, BLUE, 2);
    fb.draw_text(68, 212, "Ключ: ", {0,0,0}, {255,255,255});

    draw_footer("Введите ключ и нажмите Enter");

    while (true) {
        // Позиционируем курсор ввода
        printf("\033[12;8H");  // row 12, col 8 (approx)
        fflush(stdout);
        std::string key = readline_raw(false);

        auto res = check_license(key);
        if (res.valid) {
            install_cfg.edition = res.edition;
            fb.fill_rect(60, 260, 500, 30, BG);
            std::string msg = std::string("✓ ") + edition_name(res.edition) + " — ключ принят";
            fb.draw_text(60, 268, msg, {0,200,0}, BG);
            sleep(1);
            return State::DISK_SELECT;
        } else {
            fb.fill_rect(60, 260, 500, 30, BG);
            fb.draw_text(60, 268, "✗ Неверный ключ. Попробуйте ещё раз.", RED, BG);
        }
    }
}

static State screen_disk_select() {
    fb.clear(BG);
    draw_header("Выбор диска для установки");

    auto disks = list_disks();
    if (disks.empty()) {
        fb.draw_text(60, 120, "Диски не найдены! Проверьте подключение.", RED, BG, 1);
        draw_footer("Нажмите Enter...");
        char ch; read(STDIN_FILENO, &ch, 1);
        return State::DISK_SELECT;
    }

    fb.draw_text(60, 100, "ВНИМАНИЕ: все данные на выбранном диске будут уничтожены!", RED, BG, 1);
    fb.draw_text(60, 124, "Доступные диски:", FG, BG, 1);

    for (int i = 0; i < (int)disks.size(); i++) {
        int y = 155 + i*44;
        Color rowbg = {20,60,110};
        fb.fill_rounded_rect(60, y, 600, 36, 4, rowbg);
        std::string info = std::to_string(i+1) + ". " +
                           disks[i].path + "  " +
                           disks[i].model + "  " +
                           disks[i].size_human;
        fb.draw_text(76, y+10, info, FG, rowbg, 1);
    }

    draw_footer("Введите номер диска и нажмите Enter");

    while (true) {
        std::string s = readline_raw();
        if (s.empty()) continue;
        int n = atoi(s.c_str());
        if (n >= 1 && n <= (int)disks.size()) {
            install_cfg.disk = disks[n-1].path;
            return State::USER_SETUP;
        }
        fb.draw_text(60, 155 + (int)disks.size()*44 + 10,
                     "Неверный выбор.", RED, BG);
    }
}

static State screen_user_setup() {
    fb.clear(BG);
    draw_header("Настройка пользователя");

    fb.draw_text(60, 110, "Имя пользователя:", FG, BG);
    fb.fill_rect(60, 135, 400, 36, {255,255,255});
    fb.draw_rect(60, 135, 400, 36, BLUE, 2);

    fb.draw_text(60, 190, "Имя компьютера:", FG, BG);
    fb.fill_rect(60, 215, 400, 36, {255,255,255});
    fb.draw_rect(60, 215, 400, 36, BLUE, 2);

    fb.draw_text(60, 270, "Пароль:", FG, BG);
    fb.fill_rect(60, 295, 400, 36, {255,255,255});
    fb.draw_rect(60, 295, 400, 36, BLUE, 2);

    draw_footer("Заполните поля, нажимая Enter после каждого");

    fb.draw_text(68, 147, "Введите имя: ", {0,0,0}, {255,255,255});
    install_cfg.username = readline_raw();
    if (install_cfg.username.empty()) install_cfg.username = "user";

    fb.draw_text(68, 227, "Введите hostname: ", {0,0,0}, {255,255,255});
    install_cfg.hostname = readline_raw();
    if (install_cfg.hostname.empty()) install_cfg.hostname = "matos-pc";

    fb.draw_text(68, 307, "Пароль: ", {0,0,0}, {255,255,255});
    install_cfg.password = readline_raw(true);
    if (install_cfg.password.empty()) install_cfg.password = "matos";

    return State::PROGRESS;
}

static State screen_progress() {
    fb.clear(BG);
    draw_header("Установка MAT OS");

    ProgressBar pbar;
    pbar.x = 60; pbar.y = fb.height()/2;
    pbar.w = fb.width()-120; pbar.h = 36;
    pbar.draw(fb);

    Label status_lbl;
    status_lbl.x = 60; status_lbl.y = fb.height()/2 + 50;
    status_lbl.bg = BG;

    bool ok = run_install(install_cfg,
        [&](int pct, const std::string& msg) {
            pbar.set(pct/100.0f, fb);
            fb.fill_rect(60, status_lbl.y, fb.width()-120, 20, BG);
            fb.draw_text(60, status_lbl.y, msg, FG, BG);
        });

    return ok ? State::DONE : State::DONE;
}

static State screen_done() {
    fb.clear(BG);
    draw_header("Установка завершена");

    int cx = fb.width()/2;
    fb.fill_circle(cx, 200, 60, {0,180,0});
    fb.draw_text(cx-4, 192, "OK", FG, {0,180,0}, 2);

    fb.draw_text(cx - 200, 290, "MAT OS успешно установлена!", FG, BG, 2);
    fb.draw_text(cx - 220, 330, "Извлеките установочный носитель и нажмите Enter", {180,200,220}, BG);

    draw_footer("Нажмите Enter для перезагрузки");
    char ch; read(STDIN_FILENO, &ch, 1);
    system("reboot");
    return State::DONE;
}

// ─── main ──────────────────────────────────────────────────────────────────
int main() {
    if (!fb.open()) {
        fprintf(stderr, "Не удалось открыть framebuffer /dev/fb0\n");
        return 1;
    }

    State state = State::WELCOME;
    while (true) {
        switch (state) {
            case State::WELCOME:     state = screen_welcome();     break;
            case State::LICENSE:     state = screen_license();     break;
            case State::DISK_SELECT: state = screen_disk_select(); break;
            case State::USER_SETUP:  state = screen_user_setup();  break;
            case State::PROGRESS:    state = screen_progress();    break;
            case State::DONE:        state = screen_done();        break;
        }
    }
}
