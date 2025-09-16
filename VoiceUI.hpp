#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include "DrawLines.hpp"

// Lightweight UI primitives for NDC overlay (DrawLines space).
// NDC here means x in [-aspect, +aspect], y in [-1, +1].

namespace UI
{

    struct Rect
    {
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool contains(glm::vec2 p) const
        {
            return (p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1);
        }
    };

    struct Button
    {
        float H = 0.08f;        // text height
        Rect rect;              // NDC rect of the button hit area
        std::string label = ""; // text content

        // draw a simple labeled button with shadow + border:
        void draw(DrawLines &lines, glm::uvec2 drawable_size) const
        {
            glm::vec3 X(H, 0.0f, 0.0f), Y(0.0f, H, 0.0f);
            float ofs = 2.0f / drawable_size.y;
            float pad = 0.15f * H;
            glm::vec3 label_pos(rect.x0 + pad, 0.5f * (rect.y0 + rect.y1), 0.0f);

            // shadow
            lines.draw_text(label, label_pos, X, Y, glm::u8vec4(0x00, 0x00, 0x00, 0xff));
            // main
            lines.draw_text(label, label_pos + glm::vec3(ofs, ofs, 0.0f), X, Y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));

            glm::u8vec4 border(0xff, 0xff, 0xff, 0x66);
            lines.draw(glm::vec3(rect.x0, rect.y0, 0.0f), glm::vec3(rect.x1, rect.y0, 0.0f), border);
            lines.draw(glm::vec3(rect.x1, rect.y0, 0.0f), glm::vec3(rect.x1, rect.y1, 0.0f), border);
            lines.draw(glm::vec3(rect.x1, rect.y1, 0.0f), glm::vec3(rect.x0, rect.y1, 0.0f), border);
            lines.draw(glm::vec3(rect.x0, rect.y1, 0.0f), glm::vec3(rect.x0, rect.y0, 0.0f), border);
        }

        bool hit(glm::vec2 ndc) const { return rect.contains(ndc); }
    };

    struct RadioButton
    {
        float H = 0.07f;       // text height (circle size scales with this)
        glm::vec2 center = {}; // NDC center for the circle
        char value = '?';      // e.g., 'F','M','N' or 'L','M','H'
        bool selected = false;

        Rect hit_rect() const // sus//??
        {
            float w = 3.0f * H; // comfy hit box around text-drawn "( )"
            float h = 1.5f * H;
            // printf("value: %c, hit_rect: center=(%f,%f) w=%f h=%f\n", value, center.x, center.y, w, h);
            return Rect{center.x - 0.5f * w, center.y - 0.5f * h, center.x + 0.5f * w, center.y + 0.5f * h};
        }

        void draw(DrawLines &lines, glm::uvec2 drawable_size) const
        {
            glm::vec3 X(H, 0.0f, 0.0f), Y(0.0f, H, 0.0f);
            float ofs = 2.0f / drawable_size.y;

            // Render as text "( )" or "(X)" centered at 'center'
            // (DrawLines draws from baseline-left; nudge left by ~0.6*H to center visually)
            glm::vec3 pos(center.x - 0.6f * H, center.y, 0.0f);
            const char *g = selected ? "(X)" : "( )";

            lines.draw_text(g, pos, X, Y, glm::u8vec4(0x00, 0x00, 0x00, 0xff)); // shadow
            lines.draw_text(g, pos + glm::vec3(ofs, ofs, 0.0f), X, Y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
        }

        bool hit(glm::vec2 ndc) const { return hit_rect().contains(ndc); }
    };

    struct RadioButtons
    {
        std::vector<RadioButton> items;
        bool allow_deselect = false; // true for gender; false for pitch/speed

        // Returns currently-selected index or -1 if none:
        int selected_index() const
        {
            for (int i = 0; i < (int)items.size(); ++i)
                if (items[i].selected)
                    return i;
            return -1;
        }

        char selected_value() const
        {
            int si = selected_index();
            return (si >= 0 ? items[si].value : '\0');
        }

        // Set selected by value; returns true if changed:
        bool set_selected(char v)
        {
            bool changed = false;
            for (auto &rb : items)
            {
                bool sel = (rb.value == v);
                changed = changed || (rb.selected != sel);
                rb.selected = sel;
            }
            return changed;
        }

        // Click handler: returns true if handled; writes new_value if selection changed
        bool click(glm::vec2 ndc, char *out_new_value = nullptr)
        {
            for (int i = 0; i < (int)items.size(); ++i)
            {
                auto &rb = items[i];
                if (rb.hit(ndc))
                {
                    printf("rb::click: hit item %d (value '%c')\n", i, rb.value);
                    if (rb.selected)
                    {
                        if (allow_deselect)
                        {
                            rb.selected = false; // toggle off
                            if (out_new_value)
                                *out_new_value = '\0';
                            // ensure no others are selected
                            for (int j = 0; j < (int)items.size(); ++j)
                                if (j != i)
                                    items[j].selected = false;
                            return true;
                        }
                        else
                        {
                            // already selected in strict radio; no change
                            return true;
                        }
                    }
                    else
                    {
                        // exclusive select
                        for (auto &o : items)
                            o.selected = false;
                        rb.selected = true;
                        if (out_new_value)
                            *out_new_value = rb.value;
                        return true;
                    }
                }
            }
            return false;
        }

        void draw(DrawLines &lines, glm::uvec2 drawable_size) const
        {
            for (auto const &rb : items)
                rb.draw(lines, drawable_size);
        }
    };

} // namespace UI

namespace VoiceUI
{

    // the logical state used by PlayMode:
    struct State
    {
        Fan::Gender gender = Fan::Gender::F;
        Fan::Pitch pitch = Fan::Pitch::M;
        Fan::Speed speed = Fan::Speed::M;
    };

    // layout of controls used by PlayMode:
    struct Layout
    {
        UI::Button listen;
        UI::Button speak;
        UI::RadioButtons gender;
        UI::RadioButtons pitch;
        UI::RadioButtons speed;
    };

    // geometry helpers for right panel:
    inline constexpr float UI_H() { return 0.07f; }
    inline float get_x0(float aspect) { return +aspect - 15.0f * UI_H(); }
    inline float row_y(int row) { return +1.0f - 1.7f * UI_H() - row * 1.7f * UI_H(); }

    inline Layout make_layout(float aspect)
    {
        Layout L{};

        L.listen.H = 0.09f;
        L.listen.label = "[ Listen ]";
        {
            float H = L.listen.H;
            L.listen.rect.x0 = -aspect + 0.8f * H;
            L.listen.rect.x1 = -aspect + 5.0f * H;
            L.listen.rect.y0 = -1.0f + 0.6f * H;
            L.listen.rect.y1 = -1.0f + 1.5f * H;
        }

        // Right panel rows:
        float H = UI_H();
        float x = get_x0(aspect);
        float y_gender = row_y(0), y_pitch = row_y(1), y_speed = row_y(2);

        float col_left = x + 6.0f * H;
        float col_mid = x + 10.0f * H;
        float col_right = x + 14.0f * H;

        auto create_radio_button = [&](float cx, float cy, char v)
        { UI::RadioButton r; r.H=H; r.center={cx,cy}; r.value=v; return r; };

        L.gender.allow_deselect = true;
        L.gender.items = {create_radio_button(col_left, y_gender, 'F'), create_radio_button(col_mid, y_gender, 'M')};

        L.pitch.allow_deselect = false;
        L.pitch.items = {create_radio_button(col_left, y_pitch, 'L'), create_radio_button(col_mid, y_pitch, 'M'), create_radio_button(col_right, y_pitch, 'H')};

        L.speed.allow_deselect = false;
        L.speed.items = {create_radio_button(col_left, y_speed, 'L'), create_radio_button(col_mid, y_speed, 'M'), create_radio_button(col_right, y_speed, 'H')};

        L.speak.H = H;
        L.speak.label = "[ Speak ]";
        float y_speak = row_y(3) - 0.2f * H;
        L.speak.rect = UI::Rect{x, y_speak - 0.6f * H, x + 7.0f * H, y_speak + 0.6f * H};

        return L;
    }

    // seed visual selection from state:
    inline void sync_from_state(State const &s, Layout &L)
    {
        L.gender.set_selected((s.gender == Fan::Gender::F) ? 'F' : (s.gender == Fan::Gender::M) ? 'M'
                                                                                                : '\0');
        L.pitch.set_selected((s.pitch == Fan::Pitch ::L) ? 'L' : (s.pitch == Fan::Pitch ::M) ? 'M'
                                                                                             : 'H');
        L.speed.set_selected((s.speed == Fan::Speed ::L) ? 'L' : (s.speed == Fan::Speed ::M) ? 'M'
                                                                                             : 'H');
    }

} // namespace VoiceUI
