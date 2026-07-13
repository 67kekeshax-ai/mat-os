/*
 * matos-login — точка входа экрана входа MAT OS
 *
 * Запускается как shell пользователя matos_login через getty autologin.
 * 1. Показывает экран входа на framebuffer.
 * 2. После успешной аутентификации сбрасывает привилегии до реального пользователя.
 * 3. Запускает xinit → matos-shell.
 */
#include "login.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <fstream>
#include <string>

static std::string find_real_user() {
    // Первый нестандартный пользователь (uid >= 1000)
    setpwent();
    struct passwd* pw;
    while ((pw = getpwent()) != nullptr) {
        if (pw->pw_uid >= 1000 && pw->pw_uid < 65534)
            return pw->pw_name;
    }
    endpwent();
    return "user";
}

int main() {
    std::string username = find_real_user();

    // Экран входа (framebuffer)
    if (!run_login_screen(username)) {
        // Слишком много неудачных попыток
        fprintf(stderr, "Вход заблокирован.\n");
        sleep(5);
        system("reboot");
        return 1;
    }

    // Получаем данные пользователя
    struct passwd* pw = getpwnam(username.c_str());
    if (!pw) {
        fprintf(stderr, "Пользователь '%s' не найден.\n", username.c_str());
        return 1;
    }

    // Сбрасываем привилегии
    if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
        setgid(pw->pw_gid)  != 0 ||
        setuid(pw->pw_uid)  != 0) {
        fprintf(stderr, "Не удалось сбросить привилегии.\n");
        return 1;
    }

    // Устанавливаем переменные окружения
    setenv("HOME",    pw->pw_dir,   1);
    setenv("USER",    pw->pw_name,  1);
    setenv("LOGNAME", pw->pw_name,  1);
    setenv("SHELL",   pw->pw_shell, 1);
    setenv("PATH",    "/usr/local/bin:/usr/bin:/bin:/usr/games", 1);
    chdir(pw->pw_dir);

    // Запускаем X через xinit
    // xinit запустит ~/.xinitrc → matos-shell
    const char* xinit_args[] = { "xinit", "--", ":0", nullptr };
    execvp("xinit", const_cast<char**>(xinit_args));

    // Если xinit не найден — попробуем startx
    const char* startx_args[] = { "startx", nullptr };
    execvp("startx", const_cast<char**>(startx_args));

    fprintf(stderr, "Не удалось запустить X.\n");
    return 1;
}
