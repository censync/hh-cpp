#include "sheet.hpp"

#include <algorithm>
#include <cstdio>

namespace hh {
namespace lab {

page_style light_page() {
    return {{255, 255, 255, 255}, {0x33, 0x33, 0x33, 255}, 2};
}

page_style dark_page() {
    return {{0x12, 0x12, 0x12, 255}, {0xCC, 0xCC, 0xCC, 255}, 2};
}

namespace {

int title_height(const std::vector<std::string>& title, const page_style& style, int pad) {
    if (title.empty()) {
        return 0;
    }
    return static_cast<int>(title.size()) *
               (canvas::text_height(style.text_scale) + 4 * style.text_scale) +
           pad;
}

int title_width(const std::vector<std::string>& title, const page_style& style) {
    int w = 0;
    for (const auto& line : title) {
        w = std::max(w, canvas::text_width(line, style.text_scale));
    }
    return w;
}

void draw_title(canvas& c, const std::vector<std::string>& title, const page_style& style,
                int pad) {
    int y = pad;
    for (const auto& line : title) {
        c.text(pad, y, line, style.text_scale, style.ink);
        y += canvas::text_height(style.text_scale) + 4 * style.text_scale;
    }
}

}  // namespace

canvas grid_sheet(const std::vector<std::string>& title, const std::vector<tile>& tiles,
                  int columns, int cell_w, int cell_h, int pad, const page_style& style) {
    const int label_scale = 1;
    const int label_h = canvas::text_height(label_scale) + 4;
    int max_label = 0;
    for (const auto& t : tiles) {
        max_label = std::max(max_label, canvas::text_width(t.label, label_scale));
    }
    const int pitch_w = std::max(cell_w, max_label) + pad;
    const int pitch_h = cell_h + (max_label > 0 ? label_h : 0) + pad;
    const int rows = (static_cast<int>(tiles.size()) + columns - 1) / columns;
    const int top = pad + title_height(title, style, pad);
    const int width = std::max(pad + columns * pitch_w, 2 * pad + title_width(title, style));
    const int height = top + rows * pitch_h;
    canvas c(width, height, style.page);
    draw_title(c, title, style, pad);
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const int col = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        const int x = pad + col * pitch_w;
        const int y = top + row * pitch_h;
        c.draw(tiles[i].image, x, y);
        if (!tiles[i].label.empty()) {
            c.text(x, y + cell_h + 3, tiles[i].label, label_scale, style.ink);
        }
    }
    return c;
}

namespace {

std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out(1);
    for (char c : s) {
        if (c == '\n') {
            out.emplace_back();
        } else {
            out.back().push_back(c);
        }
    }
    return out;
}

}  // namespace

canvas row_sheet(const std::vector<std::string>& title, const std::vector<sheet_row>& rows,
                 int caption_w, int pad, const page_style& style) {
    const int label_scale = 1;
    const int label_h = canvas::text_height(label_scale) + 4;
    int width = 2 * pad + title_width(title, style);
    int height = pad + title_height(title, style, pad);
    for (const auto& r : rows) {
        int w = pad + caption_w;
        const int line_h = canvas::text_height(style.text_scale) + 4 * style.text_scale;
        int h = static_cast<int>(split_lines(r.caption).size()) * line_h;
        bool labels = false;
        for (const auto& t : r.tiles) {
            w += std::max(t.image.width(), canvas::text_width(t.label, label_scale)) + pad;
            h = std::max(h, t.image.height());
            labels = labels || !t.label.empty();
        }
        width = std::max(width, w);
        height += h + (labels ? label_h : 0) + pad;
    }
    canvas c(width, height, style.page);
    draw_title(c, title, style, pad);
    int y = pad + title_height(title, style, pad);
    for (const auto& r : rows) {
        const int line_h = canvas::text_height(style.text_scale) + 4 * style.text_scale;
        const auto caption_lines = split_lines(r.caption);
        for (std::size_t i = 0; i < caption_lines.size(); ++i) {
            c.text(pad, y + static_cast<int>(i) * line_h, caption_lines[i], style.text_scale,
                   style.ink);
        }
        int x = pad + caption_w;
        int h = static_cast<int>(caption_lines.size()) * line_h;
        bool labels = false;
        for (const auto& t : r.tiles) {
            c.draw(t.image, x, y);
            if (!t.label.empty()) {
                c.text(x, y + t.image.height() + 3, t.label, label_scale, style.ink);
                labels = true;
            }
            x += std::max(t.image.width(), canvas::text_width(t.label, label_scale)) + pad;
            h = std::max(h, t.image.height());
        }
        y += h + (labels ? label_h : 0) + pad;
    }
    return c;
}

bool save_sheet(const std::string& path, const canvas& c) {
    const bool ok = write_png(path, c, false);
    std::printf("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", path.c_str(), c.width(), c.height());
    return ok;
}

}  // namespace lab
}  // namespace hh
