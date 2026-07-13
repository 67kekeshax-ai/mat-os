#pragma once
#include <string>

// Экран входа на framebuffer.
// Показывает часы, аватар, поле пароля.
// Возвращает true если аутентификация прошла успешно.
bool run_login_screen(const std::string& username);
