// about where a Text element's words are, for picking and the 2D view's handles. the words themselves are drawn by
// ui/TextNodes.cpp with the real font

#include <algorithm>

#include "render/Scene.h"

void TextLines(const std::string& text, int& lines, int& longest)
{
    int column = 0;
    lines = 1;
    longest = 0;

    for (char c : text)
    {
        if (c == '\n')
        {
            lines++;
            column = 0;
            continue;
        }

        longest = std::max(longest, ++column);
    }
}

void TextArea(const Element& text, Vec3& lo, Vec3& hi)
{
    int lines, longest;
    TextLines(text.text, lines, longest);

    // a character is about half as wide as the text is tall
    float w = std::max(longest, 1) * text.textSize * 0.5f, h = lines * text.textSize;
    float left = text.align == 0 ? 0.0f : text.align == 2 ? -w : -w / 2;

    // on the screen the node is its top, in the level its middle, like the game draws them
    float top = text.screen ? 0.0f : -h / 2;
    lo = { left, top, -h / 4 };
    hi = { left + w, top + h, h / 4 };
}
