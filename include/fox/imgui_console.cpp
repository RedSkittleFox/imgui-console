#include "imgui_console.hpp"

#define IM_VEC2_CLASS_EXTRA
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

    void console_window(state& state, bool* open, const config& cfg)
    {
        constexpr std::string_view separateros{ "\n\t .,;?!" }; // TODO: Complete this list
        constexpr std::string_view spaces{ "\n\t " };

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGuiContext& g = *GImGui;

        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(cfg.window_name, open, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        ImVec2 vMin = ImGui::GetWindowContentRegionMin();
        ImVec2 vMax = ImGui::GetWindowContentRegionMax();
        auto max_width = vMax.x - vMin.x - 15; // TODO: Scroll width
        if(true) // Window width changed, preprocess word wrapping again
        {
            const char* beg = state.buffer.c_str();
            const char* end = state.buffer.c_str() + std::size(state.buffer);

            ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
            const char* current = beg;

            state.segments.clear();
            state.segments.emplace_back();

            bool next_inline = false;
            float offset_size = 0.0f;
	        while(current < end)
	        {
                float max_width_adjusted = std::max(max_width - (next_inline ? offset_size : 0), 5.f);

                // Find potential line
                const char* line_end1 = std::strstr(current, "{{");
                if(current == line_end1)
                {
                    current = default_color_parser(current, end, color);
                    next_inline = true;
                    continue;
                }

                const char* line_end2 = std::strchr(current, '\n');
                const char* line_end;

	        	if(line_end1 != nullptr && line_end2 != nullptr)
                {
					line_end = std::min(line_end1, line_end2);
                }
                else if(line_end1 != nullptr)
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

                if(size.x <= max_width_adjusted)
                {
                    push();

                    current = line_end + ( line_end == line_end2 );
                    continue;
                }

                // Otherwise split the line

                // Make an educated guess where to split
                std::size_t line_len = line_end - beg;
                line_len = static_cast<std::size_t>(static_cast<float>(line_len) * max_width_adjusted / size.x);

                // Try splitting by words
                while (line_len > 1)
                {
                	auto potential_line_len = std::string_view(current, line_len).find_last_of(separateros);
                    if (potential_line_len == std::string_view::npos)
                    {
                        line_len = 0;
                        break;
                    }

                    line_len = potential_line_len;
                    size = ImGui::CalcTextSize(current, current + line_len);

                    if (size.x <= max_width_adjusted)
                        break;

                    line_len -= 1;
                }

                if(line_len != 0)
                {
                    line_end = current + line_len;
                    push();
                    current = line_end;

                    // Trim white space in case of artificial new line
                    if(current < end && spaces.find(current[0]) != std::string_view::npos)
                    {
                        current += 1;
                    }

                    continue;
                }

                // Otherwise brute force shrink
                line_len = line_end - beg;
                size = og_size;
                line_len = static_cast<std::size_t>(static_cast<float>(line_len) * max_width_adjusted / size.x);
	        	while(line_len > 1)
                {
                    size = ImGui::CalcTextSize(current, current + line_len);

                    if (size.x <= max_width_adjusted)
                        break;

                    line_len -= 1;
                }

                line_end = current + std::max(line_len, static_cast<std::size_t>(1));
                push();
                current = line_end;
	        }
        }

        auto font_size = g.FontSize;

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
        {
            bool ScrollToBottom = false;
            bool AutoScroll = false;


            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

            ImGui::TextUnformatted("TestA\nTestB\nTestC");

            ImGuiListClipper clipper;
            clipper.Begin(std::ssize(state.segments));
            while (clipper.Step())
            {
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
				{
                    const auto& segment = state.segments[i];
                    if (std::empty(segment))
                    {
                        ImGui::TextUnformatted("");
                        continue;
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, segment[0].color);
                    ImGui::TextUnformatted(std::data(segment[0].string), std::data(segment[0].string) + std::size(segment[0].string));
                    ImGui::PopStyleColor();

                    for(std::size_t j = 1; j < std::size(segment); ++j)
                    {
                        ImGui::SameLine(0, 0.f);
                        ImGui::PushStyleColor(ImGuiCol_Text, segment[j].color);
						ImGui::TextUnformatted(std::data(segment[j].string), std::data(segment[j].string) + std::size(segment[j].string));
                        ImGui::PopStyleColor();
                    }
				}
            }

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
            // Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            ScrollToBottom = false;

            ImGui::PopStyleVar();
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
