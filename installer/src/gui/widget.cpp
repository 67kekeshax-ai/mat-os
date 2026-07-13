#include "widget.h"
#include <algorithm>

// ─────────────────────────────────────────────────────── Button ────────────
void Button::draw(Framebuffer& fb) const {
    Color bg  = pressed  ? Color{0,86,179}  :
                hovered  ? Color{0,120,215} : Color{0,99,195};
    Color brd = Color{0,70,160};
    fb.fill_rounded_rect(x, y, w, h, 4, bg);
    fb.draw_rect(x, y, w, h, brd, 1);
    int tx = x + (w - fb.text_width(label)) / 2;
    int ty = y + (h - 16) / 2;
    fb.draw_text(tx, ty, label, {255,255,255}, bg);
}

bool Button::hit(int mx, int my) const {
    return mx >= x && mx < x+w && my >= y && my < y+h;
}

// ─────────────────────────────────────────────────────── Label ─────────────
void Label::draw(Framebuffer& fb) const {
    fb.draw_text(x, y, text, fg, bg, scale);
}

// ─────────────────────────────────────────────────────── TextInput ─────────
void TextInput::draw(Framebuffer& fb) const {
    Color border = focused ? Color{0,120,215} : Color{170,170,170};
    fb.fill_rect(x, y, w, h, {255,255,255});
    fb.draw_rect(x, y, w, h, border, focused ? 2 : 1);

    std::string disp = password ? std::string(value.size(), '*') : value;
    if (focused) disp += "_";
    int ty = y + (h - 16) / 2;
    fb.draw_text(x + 8, ty, disp.substr(0, (w-16)/8), {0,0,0}, {255,255,255});
}

void TextInput::handle_key(int /*key_code*/, char ch) {
    if (ch == '\b') {
        if (!value.empty()) value.pop_back();
    } else if (ch >= 32 && ch < 127) {
        value += ch;
    }
}

// ─────────────────────────────────────────────────────── ProgressBar ───────
void ProgressBar::draw(Framebuffer& fb) const {
    fb.fill_rect(x, y, w, h, {220,220,220});
    fb.draw_rect(x, y, w, h, {180,180,180}, 1);
    int fill = int(value * (w - 2));
    if (fill > 0)
        fb.fill_rect(x+1, y+1, fill, h-2, {0,120,215});
    if (!label.empty()) {
        int tx = x + (w - fb.text_width(label)) / 2;
        fb.draw_text(tx, y + (h-16)/2, label, {255,255,255}, {0,120,215});
    }
}

void ProgressBar::set(float v, Framebuffer& fb) {
    value = std::clamp(v, 0.0f, 1.0f);
    draw(fb);
}

// ─────────────────────────────────────────────────────── RadioGroup ────────
void RadioGroup::draw(Framebuffer& fb) const {
    for (int i = 0; i < (int)options.size(); i++) {
        int cy = y + i * spacing + 10;
        Color c = (i == selected) ? Color{0,120,215} : Color{200,200,200};
        fb.draw_circle(x+10, cy, 10, {100,100,100}, 1);
        if (i == selected) fb.fill_circle(x+10, cy, 6, c);
        fb.draw_text(x + 28, cy - 8, options[i], {255,255,255}, {3,45,96});
    }
}

bool RadioGroup::handle_click(int mx, int my) {
    for (int i = 0; i < (int)options.size(); i++) {
        int cy = y + i * spacing + 10;
        int dx = mx - (x+10), dy = my - cy;
        if (dx*dx + dy*dy <= 100) { selected = i; return true; }
        // клик по тексту
        if (mx > x+22 && mx < x+22+(int)options[i].size()*8 &&
            std::abs(my - cy) < 10) { selected = i; return true; }
    }
    return false;
}
