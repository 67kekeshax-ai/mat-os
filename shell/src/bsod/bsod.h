#pragma once

// Устанавливает обработчики сигналов (SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL).
// При срабатывании рисует синий экран смерти на /dev/fb0 и перезагружает систему.
void bsod_install_handlers();

// Явный вызов BSOD с пользовательским сообщением.
[[noreturn]] void bsod_trigger(const char* error_code,
                               const char* message);
