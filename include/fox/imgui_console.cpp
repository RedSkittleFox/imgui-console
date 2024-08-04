#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_console.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <algorithm>


namespace fox::imgui
{
    char* find_previous_word_end(const char* min, const char* current)
    {
        

        return nullptr;
    }

    const char* default_color_parser(const char* start, const char* end, ImU32& color)
    {
        if (start >= end) [[unlikely]]
            return end;

        int r = 0, g = 0, b = 0;
        auto result = std::sscanf(start, "{{%i;%i;%i}}", &r, &g, &b);

        const auto end_m = std::string_view(start, end - start).find("}}");

        if(result == EOF || start + result > end)
        {
            if (end_m == std::string_view::npos)
                return end;

            return start + end_m + 2;
        }

        color = ImGui::ColorConvertFloat4ToU32(ImColor(r, g, b));

    	return start + end_m + 2;
    }

    void process_text(state& state, const ImRect& text_clip)
    {
        auto max_width = text_clip.GetWidth();

        constexpr std::string_view separateros{ "\n\t .,;?!" }; // TODO: Complete this list
        constexpr std::string_view spaces{ "\n\t " };

        const char* beg = state.buffer.c_str();
        const char* end = state.buffer.c_str() + std::size(state.buffer);

        ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
        const char* current = beg;

        state.segments.clear();

        bool next_inline = false;
        float offset_size = 0.0f;
        while (current < end)
        {
            float max_width_adjusted = std::max(max_width - (next_inline ? offset_size : 0), 5.f);

            // Find potential line
            const char* line_end1 = std::strstr(current, "{{");
            if (current == line_end1)
            {
                current = default_color_parser(current, end, color);
                next_inline = true;
                continue;
            }

            const char* line_end2 = std::strchr(current, '\n');
            const char* line_end;

            if (line_end1 != nullptr && line_end2 != nullptr)
            {
                line_end = std::min(line_end1, line_end2);
            }
            else if (line_end1 != nullptr)
            {
                line_end = line_end1;
            }
            else
            {
                line_end = line_end2;
            }

            if (line_end == nullptr) [[unlikely]]
                line_end = end;


            // Check if it fits
            auto size = ImGui::CalcTextSize(current, line_end);
            auto og_size = size;

            auto push = [&]()
                {
                    if (next_inline) [[unlikely]]
                        {
                            state.segments.back().emplace_back(state::segment{ std::string_view(current, line_end - current), color });
                            next_inline = false;
                            offset_size = 0.0f;
                        }
                    else
                    {
                        state.segments.emplace_back(std::vector{ state::segment{ std::string_view(current, line_end - current), color } });
                    }
                    offset_size = size.x;
                };

            if (size.x < max_width_adjusted)
            {
                push();

                current = line_end + (line_end == line_end2);
                continue;
            }

            // Otherwise split the line

            // Make an educated guess where to split
            std::size_t line_len = line_end - beg;
            line_len = static_cast<std::size_t>(static_cast<float>(line_len) * max_width_adjusted / size.x);

            // Try splitting by words
            while (line_len > 0)
            {
                auto potential_line_len = std::string_view(current, line_len).find_last_of(separateros);
                if (potential_line_len == std::string_view::npos)
                {
                    line_len = 0;
                    break;
                }

                line_len = potential_line_len;
                size = ImGui::CalcTextSize(current, current + line_len);

                if (size.x < max_width_adjusted)
                    break;

                // line_len -= 1;
            }

            if (line_len != 0)
            {
                line_end = current + line_len;
                push();
                current = line_end;

                // Trim white space in case of artificial new line
                if (current < end && spaces.find(current[0]) != std::string_view::npos)
                {
                    current += 1;
                }

                continue;
            }

            // Otherwise brute force shrink
            line_len = line_end - beg;
            size = og_size;
            line_len = static_cast<std::size_t>(static_cast<float>(line_len) * max_width_adjusted / size.x);
            while (line_len > 1)
            {
                size = ImGui::CalcTextSize(current, current + line_len);

                if (size.x < max_width_adjusted)
                    break;

                line_len -= 1;
            }

            // If we did a brute force shrink but we are forced to use inline try another chance on the new line...
            if(line_len == 1 && next_inline)
            {
                next_inline = false;
                continue;
            }

            line_end = current + std::max(line_len, static_cast<std::size_t>(1));
            push();
            current = line_end;
        }
    }

    static float char_width(char c)
    {
        ImGuiContext& g = *GImGui;
        // auto g = ImGui::GetCurrentContext();

        if (c == '\n' || !std::isprint(c))
            return 0.0;

        return ImGui::GetFont()->GetCharAdvance(c) * ImGui::GetCurrentWindow()->FontWindowScale;
    }

    const char* locate_clicked_character(std::string_view sv, float x)
    {
        float sum = 0.0f;
        std::size_t i = 0;
        for (; i < std::size(sv); ++i)
        {
            sum += char_width(sv[i]);

            if (x <= sum)
                return std::data(sv) + i + (i + 1 != std::size(sv));
        }

        return nullptr;
    }

    void draw_text(state& state, ImGuiWindow* window, const state::segment& s, const ImRect& text_clip, const char*& out_clicked_char)
    {
        if (std::empty(s.string))
            return;

        auto g = ImGui::GetCurrentContext();

        auto str = std::data(s.string);
        auto end = std::data(s.string) + std::size(s.string);

        auto id = window->GetID(str, end);
        auto pos = ImGui::GetCursorScreenPos();
        auto size = ImGui::CalcTextSize(str, end);

        ImRect bb(pos, pos + ImVec2(text_clip.GetWidth(), ImGui::GetTextLineHeight() + 1));
        ImRect text_bb(pos, pos + size);
        // Check if mouse is within rectangle
        auto mouse_pos = ImGui::GetMousePos();
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left) && text_clip.Contains(mouse_pos) && bb.Contains(mouse_pos))
        {
            const char* c = nullptr;
            if(text_bb.Contains(mouse_pos))
            {
				c = locate_clicked_character(s.string, mouse_pos.x - pos.x);
            }
            else if(mouse_pos.x >= text_bb.Max.x)
            {
                c = std::addressof(s.string.back());
            }

            if(c != nullptr)
            {
                out_clicked_char = c;
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Text, s.color);
        ImGui::TextUnformatted(str, end);
        ImGui::PopStyleColor();

        ImGui::ItemAdd(bb, id);
        // ImGui::DebugDrawItemRect();

        const bool hovered = ImGui::ItemHoverable(bb, id, g->LastItemData.InFlags);
        if (hovered)
            g->MouseCursor = ImGuiMouseCursor_TextInput;

        // Check if selection should be rendered - check if we are within range
        if (auto [selection_start, selection_end] = std::minmax(state.selection_start, state.selection_end);
            selection_start != nullptr && selection_end != nullptr && ( str < selection_end || selection_start <= end - 1))
        {
            ImU32 bg_color = ImGui::GetColorU32(ImGuiCol_TextSelectedBg, 0.6f);

            // If only a subregion should be rendered

            float segment_begin_selection_offset = 0.f;

            bool start_within = str < selection_start;
            bool ends_within = selection_end < end;

            if(start_within && ends_within)
            {
                size = ImGui::CalcTextSize(selection_start, selection_end);
                segment_begin_selection_offset = ImGui::CalcTextSize(str, selection_start).x;
            }
            else if(start_within)
            {
                size = ImGui::CalcTextSize(selection_start, end);
                segment_begin_selection_offset = ImGui::CalcTextSize(str, selection_start).x;
            }
            else if(ends_within)
            {
                size = ImGui::CalcTextSize(str, selection_end);
            }

            ImRect rect(pos + ImVec2(segment_begin_selection_offset, 0.f), pos + size + ImVec2(segment_begin_selection_offset, 0.f));

            rect.ClipWith(text_clip);
            if (rect.Overlaps(text_clip))
                ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, bg_color);
        }
    };

    void handle_selection(state& state, const char* clicked_char, std::ptrdiff_t clicked_segment, std::ptrdiff_t clicked_subsegment)
    {
        if (clicked_char == nullptr)
            return;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetCurrentContext()->IO.MouseClickedCount[ImGuiMouseButton_Left] == 3)
        {
            state.selection_start = std::addressof(state.segments[clicked_segment].front().string.front());
            state.selection_end = std::addressof(state.segments[clicked_segment].back().string.back());
        }
        else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            state.selection_start = state.selection_end = nullptr;

            bool(*select_char)(char c);

            if (*clicked_char == ' ' || *clicked_char == '\t') // Select spaces
            {
                select_char = [](char c) { return c == ' ' || c == '\t'; };
            }
            else
            {
                select_char = [](char c) { return std::isalnum(c) || c == '_' || c == '-'; };
            }

            // Get word start

            if (std::isalnum(*clicked_char) || *clicked_char == ' ' || *clicked_char == '\t' || *clicked_char == '_' || *clicked_char == '-')
            {
                // Get the offset of clicked character within subsegment
                std::ptrdiff_t i_ch = (clicked_char - std::data(state.segments[clicked_segment][clicked_subsegment].string));

                for (std::ptrdiff_t j = clicked_subsegment; j >= 0; --j)
                {
                    for (std::ptrdiff_t i = i_ch; i >= 0; --i)
                    {
                        // Current character is invalid
                        if (select_char(state.segments[clicked_segment][j].string[i]) == false)
                        {
                            state.selection_start = std::addressof(state.segments[clicked_segment][j].string[i + 1]);
                            goto exit_outer_1;;
                        }
                    }
                }

                state.selection_start = std::addressof(state.segments[clicked_segment].front().string.front());

            exit_outer_1:

                for (std::ptrdiff_t j = clicked_subsegment; j < std::size(state.segments[clicked_segment]); ++j)
                {
                    for (std::ptrdiff_t i = i_ch; i < std::size(state.segments[clicked_segment][j].string); ++i)
                    {
                        // Current character is invalid
                        if (select_char(state.segments[clicked_segment][j].string[i]) == false)
                        {
                            state.selection_end = std::addressof(state.segments[clicked_segment][j].string[i]);
                            goto exit_outer_2;
                        }
                    }
                }

                state.selection_end = std::addressof(state.segments[clicked_segment].back().string.back());

            exit_outer_2:

                if (state.selection_start >= state.selection_end)
                {
                    state.selection_start = state.selection_end = nullptr;
                }
            }
            else
            {
                state.selection_start = clicked_char;
                state.selection_end = clicked_char + 1;
            }

        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            state.selection_start = clicked_char;
            state.selection_end = clicked_char + 1;
        }
        else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            state.selection_end = clicked_char;
        }
    }

    void console_window(state& state, bool* open, const config& cfg)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiContext* g = ImGui::GetCurrentContext();

        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(cfg.window_name, open, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
        {
            auto scrolly = ImGui::GetScrollY();
            ImRect text_clip(ImGui::GetCursorScreenPos() + ImVec2(0, scrolly), ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail() + ImVec2(0, scrolly));
            ImGui::GetWindowDrawList()->AddRect(text_clip.Min, text_clip.Max, ImGui::ColorConvertFloat4ToU32(ImVec4{1.f, 1.f, 0.f, 1.f}));
            // text_clip.Max.y -= footer_height_to_reserve;

            if (true) // Window width changed, preprocess word wrapping again
            {
                process_text(state, text_clip);
            }

            bool ScrollToBottom = false;
            bool AutoScroll = false;

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

            const char* clicked_char = nullptr;
            std::ptrdiff_t clicked_segment = -1;
            std::ptrdiff_t clicked_subsegment = -1;

        	{
                ImGuiListClipper clipper;
                clipper.Begin(std::ssize(state.segments), ImGui::GetTextLineHeight());
                while (clipper.Step())
                {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                    {
                        const auto& segment = state.segments[i];
                        assert(!std::empty(segment));

                        const char* tmp_clicked_char = nullptr;
                        draw_text(state, window, segment[0], text_clip, tmp_clicked_char);

                        if(tmp_clicked_char)
                        {
                            clicked_char = tmp_clicked_char;
                            clicked_segment = i;
                            clicked_subsegment = 0;
                        }

                        for (std::size_t j = 1; j < std::size(segment); ++j)
                        {
                            tmp_clicked_char = nullptr;
                            ImGui::SameLine(0, 0.f);
                            draw_text(state, window, segment[j], text_clip, tmp_clicked_char);

                            if (clicked_segment == -1 && clicked_char)
                            {
                                clicked_char = tmp_clicked_char;
                                clicked_segment = i;
                                clicked_subsegment = j;
                            }
                        }
                    }
                }
            }
            

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            ScrollToBottom = false;

            ImGui::PopStyleVar();

            handle_selection(state, clicked_char, clicked_segment, clicked_subsegment);
        }


    	ImGui::EndChild();
        ImGui::Separator();

        auto callback = [](ImGuiInputTextCallbackData*) -> int
            {
                return 0;
            };

        // Command-line
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("Input", std::data(state.input_buffer), std::size(state.input_buffer), input_text_flags, static_cast<ImGuiInputTextCallback>(callback), nullptr))
        {
            // Execute command
            [[maybe_unused]] char* s = std::data(state.input_buffer);
            reclaim_focus = true;
        }

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

        ImGui::End();

        (void)state;
        (void)open;
        (void)cfg;
    }
}
