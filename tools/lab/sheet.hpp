#pragma once

// Layout helpers for the lab sheets: a titled page with a grid of labelled tiles.

#include <string>
#include <vector>

#include "canvas.hpp"

namespace hh {
namespace lab {

struct page_style {
    rgba8 page;
    rgba8 ink;
    int text_scale;
};

page_style light_page();
page_style dark_page();

struct tile {
    canvas image;
    std::string label;
};

// Lays out `tiles` in rows of `columns`, each in a cell of `cell_w` x `cell_h`
// pixels plus one label line, under the title lines.
canvas grid_sheet(const std::vector<std::string>& title, const std::vector<tile>& tiles,
                  int columns, int cell_w, int cell_h, int pad, const page_style& style);

// A row-oriented sheet: each row is a caption followed by images side by side.
struct sheet_row {
    std::string caption;
    std::vector<tile> tiles;
};

canvas row_sheet(const std::vector<std::string>& title, const std::vector<sheet_row>& rows,
                 int caption_w, int pad, const page_style& style);

// Writes the sheet and prints its path; returns false on failure.
bool save_sheet(const std::string& path, const canvas& c);

}  // namespace lab
}  // namespace hh
