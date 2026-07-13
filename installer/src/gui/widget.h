#pragma once
#include "framebuffer.h"
#include <string>
#include <vector>
#include <functional>

// ─── Кнопка ────────────────────────────────────────────────────────────────
struct Button {
    int x, y, w, h;
    std::string label;
    bool hovered = false;
    bool pressed = false;

    void draw(Framebuffer& fb) const;
    bool hit(int mx, int my) const;
};

// ─── Метка ─────────────────────────────────────────────────────────────────
struct Label {
    int x, y;
    std::string text;
    Color fg   = {255,255,255};
    Color bg   = {3, 45, 96};
    int   scale = 1;

    void draw(Framebuffer& fb) const;
};

// ─── Поле ввода ─────────────────────────────────────────────────────────────
struct TextInput {
    int x, y, w, h;
    std::string value;
    bool focused    = false;
    bool password   = false;   // показывает '●' вместо символов

    void draw(Framebuffer& fb) const;
    void handle_key(int key_code, char ch);
};

// ─── Прогресс-бар ──────────────────────────────────────────────────────────
struct ProgressBar {
    int x, y, w, h;
    float value = 0.0f;   // 0.0 – 1.0
    std::string label;

    void draw(Framebuffer& fb) const;
    void set(float v, Framebuffer& fb);
};

// ─── Группа радио-кнопок ───────────────────────────────────────────────────
struct RadioGroup {
    int x, y;
    int spacing = 36;
    std::vector<std::string> options;
    int selected = 0;

    void draw(Framebuffer& fb) const;
    bool handle_click(int mx, int my);
};
