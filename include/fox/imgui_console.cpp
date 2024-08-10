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
	const char* default_color_parser(const char* start, const char* end, ImU32& color)
	{
		if (start >= end) [[unlikely]]
			return end;

			int r = 0, g = 0, b = 0;
			auto result = std::sscanf(start, "{{%i;%i;%i}}", &r, &g, &b);

			const auto end_m = std::string_view(start, end - start).find("}}");

			if (result == EOF || start + result > end)
			{
				if (end_m == std::string_view::npos)
					return end;

				return start + end_m + 2;
			}

			color = ImGui::ColorConvertFloat4ToU32(ImColor(r, g, b));

			return start + end_m + 2;
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

	void console_window::draw(bool* open)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImGuiContext* g = ImGui::GetCurrentContext();

		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(config_.window_name, open, ImGuiWindowFlags_MenuBar))
		{
			ImGui::End();
			return;
		}

		frame_state_.mouse_pos = ImGui::GetMousePos();

		draw_menus();

		// Reserve enough left-over height for 1 separator + 1 input text
		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		draw_text_region(footer_height_to_reserve);

		ImGui::Separator();

		auto callback = [](ImGuiInputTextCallbackData*) -> int
			{
				return 0;
			};

		// Command-line
		bool reclaim_focus = false;
		ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText("Input", std::data(input_buffer_), std::size(input_buffer_), input_text_flags, static_cast<ImGuiInputTextCallback>(callback), nullptr))
		{
			// Execute command
			[[maybe_unused]] char* s = std::data(input_buffer_);
			reclaim_focus = true;
		}

		// Auto-focus on window apparition
		ImGui::SetItemDefaultFocus();
		if (reclaim_focus)
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

		ImGui::End();

		(void)open;
	}

	void console_window::draw_menus()
	{
		bool open_search_popup = false;
		if (ImGui::BeginMenuBar())
		{
			disable_selection_ = false;

			if (ImGui::BeginMenu("Edit"))
			{
				disable_selection_ = true;

				if (ImGui::MenuItem("Find", "CTRL+F"))
				{
					open_search_popup = true;
				}

				if (ImGui::MenuItem("Select All", "CTRL+A"))
				{

				}

				if (ImGui::MenuItem("Copy", "CTRL+C"))
				{

				}

				if (ImGui::MenuItem("Copy with formatting"))
				{

				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				disable_selection_ = true;

				if (ImGui::Checkbox("Wrap", std::addressof(word_wrapping_)))
				{

				}

				if (ImGui::MenuItem("Clear"))
				{

				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		if (open_search_popup)
			ImGui::OpenPopup("search_popup", ImGuiPopupFlags_None);

		// Search Popup
		const auto search_popup_size = ImVec2(300.f, ImGui::GetTextLineHeightWithSpacing() * 2);
		const auto search_popup_pos = ImGui::GetCursorScreenPos() + ImVec2(ImGui::GetContentRegionAvail().x - search_popup_size.x, 0);
		if (ImGui::IsPopupOpen("search_popup"))
		{
			ImGui::SetNextWindowPos(search_popup_pos);
			ImGui::SetNextWindowSize(search_popup_size, ImGuiCond_Always);
		}

		if (ImGui::BeginPopup("search_popup", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration))
		{
			const auto button_size = ImGui::GetFrameHeight();
			const auto x_spacing = ImGui::GetStyle().ItemSpacing.x;

			auto text_size = search_popup_size.x - button_size * 3.33 - 6 * x_spacing;

			bool open_search_properties_popup = false;
			if (ImGui::Button(" ##options_button", ImVec2(button_size / 3, button_size)))
			{
				open_search_properties_popup = true;
			}

			if (open_search_properties_popup)
				ImGui::OpenPopup("search_properties_popup", ImGuiPopupFlags_None);

			// Search Popup
			if (ImGui::IsPopupOpen("search_properties_popup"))
			{
				ImGui::SetNextWindowPos(search_popup_pos + ImVec2(0, search_popup_size.y));
				ImGui::SetNextWindowSize(search_popup_size, ImGuiCond_Always);
			}

			if (ImGui::BeginPopup("search_properties_popup", ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration))
			{
				ImGui::RadioButton("Match Word", false);
				ImGui::SameLine();
				ImGui::RadioButton("Match Casing", false);
				ImGui::SameLine();
				ImGui::RadioButton("Regex", false);

				ImGui::EndPopup();
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(text_size);
			ImGui::InputText("##Search Box", std::data(search_input_), std::size(search_input_));
			ImGui::SameLine();
			if (ImGui::ArrowButton("FindPrev", ImGuiDir_Up))
			{

			}
			ImGui::SameLine();
			if (ImGui::ArrowButton("FindNExt", ImGuiDir_Down))
			{

			}
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(button_size, button_size)))
			{

			}
			ImGui::EndPopup();
		}
	}

	void console_window::draw_text_region(float footer_height_to_reserve)
	{
		if (!ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::EndChild();
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

		ImGuiID id = ImGui::GetCurrentWindow()->GetID("ScrollingRegion");
		auto scroll = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

		frame_state_.text_region_clip = ImRect(ImGui::GetCursorScreenPos() + scroll, ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail() + scroll);

		ImGui::GetWindowDrawList()->AddRect(frame_state_.text_region_clip.Min, frame_state_.text_region_clip.Max, ImGui::ColorConvertFloat4ToU32(ImVec4{ 1.f, 1.f, 0.f, 1.f }));

		if (frame_state_.text_region_clip.Contains(ImGui::GetMousePos()) && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetCurrentContext()->ActiveId == id)
		{
			ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
		}

		// These two require frame_state_.text_region_clip to be processed
		process_text();
		text_box_autoscroll();

		bool ScrollToBottom = false;
		bool AutoScroll = false;

		frame_state_.clicked_char = nullptr;
		frame_state_.clicked_segment = -1;
		frame_state_.clicked_subsegment = -1;

		// Drawing
		frame_state_.visible_row_min = std::numeric_limits<int>::max();
		frame_state_.visible_row_max = std::numeric_limits<int>::min();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(std::ssize(segments_)), ImGui::GetTextLineHeight());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			{
				frame_state_.visible_row_min = std::min(frame_state_.visible_row_min, i);
				frame_state_.visible_row_max = std::max(frame_state_.visible_row_max, i);

				const auto& segment = segments_[i];
				assert(!std::empty(segment));

				for (std::ptrdiff_t j = 0; j < std::ssize(segment); ++j)
				{
					draw_text_subsegment(i, j);
				}
			}
		}


		// Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
		// Using a scrollbar or mouse-wheel will take away from the bottom edge.
		if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;

		// Make sure this is within the style, we calculate the text height in there
		handle_selection();

		ImGui::PopStyleVar();
		ImGui::EndChild();
	}

	void console_window::draw_text_subsegment(std::ptrdiff_t segment, std::ptrdiff_t subsegment)
	{
		auto string = segments_[segment][subsegment].string;
		auto color = segments_[segment][subsegment].color;

		if (std::empty(string))
			return;

		auto g = ImGui::GetCurrentContext();

		auto beg = std::data(string);
		auto end = std::data(string) + std::size(string);

		auto id = ImGui::GetCurrentWindow()->GetID(beg, end);

		auto pos = ImGui::GetCursorScreenPos();
		auto size = ImGui::CalcTextSize(beg, end);

		ImRect bb(ImVec2(frame_state_.text_region_clip.Min.x, pos.y), ImVec2(frame_state_.text_region_clip.Max.x, size.y + pos.y + g->Style.ItemInnerSpacing.y));

		// Check if mouse is within rectangle
		auto mouse_pos = ImGui::GetMousePos();
		if (disable_selection_ == false && ImGui::IsMouseDown(ImGuiMouseButton_Left) && frame_state_.text_region_clip.Contains(mouse_pos) && bb.Contains(mouse_pos))
		{
			ImRect text_bb(pos, pos + size + ImVec2(0, g->Style.ItemSpacing.y));

			const char* c = nullptr;
			if (text_bb.Contains(mouse_pos))
			{
				// ImGui::GetWindowDrawList()->AddRect(text_bb.Min, text_bb.Max, ImGui::ColorConvertFloat4ToU32(ImVec4{ 1.f, 1.f, 0.f, 1.f }));
				c = locate_clicked_character(string, mouse_pos.x - pos.x);
			}

			if (c == nullptr && mouse_pos.x >= text_bb.Max.x)
			{
				c = end;
			}

			if (c != nullptr)
			{
				frame_state_.clicked_char = c;
				frame_state_.clicked_segment = segment;
				frame_state_.clicked_subsegment = subsegment;
			}
		}

		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(beg, end);
		ImGui::PopStyleColor();

		ImGui::ItemAdd(bb, id);
		// ImGui::DebugDrawItemRect();

		if (ImGui::ItemHoverable(bb, id, g->LastItemData.InFlags))
			g->MouseCursor = ImGuiMouseCursor_TextInput;

		// Check if selection should be rendered - check if we are within range
		if (auto [selection_start, selection_end] = std::minmax(selection_start_, selection_end_);
			selection_start != nullptr && selection_end != nullptr && (beg < selection_end || selection_start <= end - 1))
		{
			ImU32 bg_color = ImGui::GetColorU32(ImGuiCol_TextSelectedBg, 0.6f);

			// If only a subregion should be rendered

			float segment_begin_selection_offset = 0.f;

			bool start_within = beg < selection_start;
			bool ends_within = selection_end < end;

			if (start_within && ends_within)
			{
				size = ImGui::CalcTextSize(selection_start, selection_end);
				segment_begin_selection_offset = ImGui::CalcTextSize(beg, selection_start).x;
			}
			else if (start_within)
			{
				size = ImGui::CalcTextSize(selection_start, end);
				segment_begin_selection_offset = ImGui::CalcTextSize(beg, selection_start).x;
			}
			else if (ends_within)
			{
				size = ImGui::CalcTextSize(beg, selection_end);
			}

			ImRect rect(pos + ImVec2(segment_begin_selection_offset, 0.f), pos + size + ImVec2(segment_begin_selection_offset, 0.f));

			rect.ClipWith(frame_state_.text_region_clip);
			if (rect.Overlaps(frame_state_.text_region_clip))
				ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, bg_color);
		}
	}

	void console_window::text_box_autoscroll()
	{
		// Handle mouse scrolling
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && valid_dragging_)
		{
			assert(mouse_scroll_speed_ > 0.0f && "Scroll speed must be greater than zero.");
			assert(mouse_scroll_boundary_ > 0.0f && "Scroll boundary must be greater than zero.");

			const auto mouse_pos = frame_state_.mouse_pos;

			// Check mouse distance from borders
			auto dist_left = mouse_pos.x - mouse_scroll_boundary_ - frame_state_.text_region_clip.Min.x;
			auto dist_right = frame_state_.text_region_clip.Max.x - mouse_scroll_boundary_ - mouse_pos.x;
			auto dist_top = mouse_pos.y - mouse_scroll_boundary_ - frame_state_.text_region_clip.Min.y;
			auto dist_bottom = frame_state_.text_region_clip.Max.y - mouse_pos.y - mouse_scroll_boundary_;

			if (dist_left <= 0.f)
			{
				auto scroll = ImGui::GetScrollX();
				float f = std::clamp(std::abs(dist_left) / mouse_scroll_boundary_, 0.f, 1.f);
				scroll -= f * f * mouse_scroll_speed_;
				ImGui::SetScrollX(std::clamp(scroll, 0.f, ImGui::GetScrollMaxX()));
			}
			else if (dist_right <= 0.f)
			{
				auto scroll = ImGui::GetScrollX();
				float f = std::clamp(std::abs(dist_right) / mouse_scroll_boundary_, 0.f, 1.f);
				scroll += f * f * mouse_scroll_speed_;
				ImGui::SetScrollX(std::clamp(scroll, 0.f, ImGui::GetScrollMaxX()));
			}

			if (dist_top <= 0.f)
			{
				auto scroll = ImGui::GetScrollY();
				float f = std::clamp(std::abs(dist_top) / mouse_scroll_boundary_, 0.f, 1.f);
				scroll -= f * f * mouse_scroll_speed_;
				ImGui::SetScrollY(std::clamp(scroll, 0.f, ImGui::GetScrollMaxY()));
			}
			else if (dist_bottom <= 0.f)
			{
				auto scroll = ImGui::GetScrollY();
				float f = std::clamp(std::abs(dist_bottom) / mouse_scroll_boundary_, 0.f, 1.f);
				scroll += f * mouse_scroll_speed_;
				ImGui::SetScrollY(std::clamp(scroll, 0.f, ImGui::GetScrollMaxY()));
			}
		}
	}

	void console_window::handle_selection()
	{
		// Reset dragging state
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left) && valid_dragging_)
		{
			valid_dragging_ = false;
		}

		if (frame_state_.clicked_char == nullptr)
		{
			// Check if we are dragging out of bounds
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && valid_dragging_)
			{
				const auto mouse_pos = ImGui::GetMousePos();

				// Narrow text clipping
				ImGui::GetWindowDrawList()->AddRect(frame_state_.text_region_clip.Min, frame_state_.text_region_clip.Max, ImGui::ColorConvertFloat4ToU32(ImVec4{ 1.f, 0.f, 0.f, 1.f }));

				if (frame_state_.visible_row_min == 0 && frame_state_.text_region_clip.Min.y >= mouse_pos.y)
				{
					selection_end_ = segments_[frame_state_.visible_row_min].front().string.data();
				}
				else if (frame_state_.visible_row_max + 1 == std::ssize(segments_) && frame_state_.text_region_clip.Min.y < mouse_pos.y)
				{
					const auto& sv = segments_[frame_state_.visible_row_max].back().string;
					selection_end_ = std::data(sv) + std::size(sv);
				}
			}

			return;
		}

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetCurrentContext()->IO.MouseClickedCount[ImGuiMouseButton_Left] == 3)
		{
			selection_start_ = std::addressof(segments_[frame_state_.clicked_segment].front().string.front());
			selection_end_ = std::addressof(segments_[frame_state_.clicked_segment].back().string.back()) + 1;
		}
		else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			selection_start_ = selection_end_ = nullptr;

			bool(*select_char)(char c);

			if (*frame_state_.clicked_char == ' ' || *frame_state_.clicked_char == '\t') // Select spaces
			{
				select_char = [](char c) { return c == ' ' || c == '\t'; };
			}
			else
			{
				select_char = [](char c) { return std::isalnum(c) || c == '_' || c == '-'; };
			}

			// Get word start

			if (std::isalnum(*frame_state_.clicked_char) || *frame_state_.clicked_char == ' ' || *frame_state_.clicked_char == '\t' || *frame_state_.clicked_char == '_' || *frame_state_.clicked_char == '-')
			{
				// Get the offset of clicked character within subsegment
				std::ptrdiff_t i_ch = (frame_state_.clicked_char - std::data(segments_[frame_state_.clicked_segment][frame_state_.clicked_subsegment].string));

				for (std::ptrdiff_t j = frame_state_.clicked_subsegment; j >= 0; --j)
				{
					for (std::ptrdiff_t i = i_ch; i >= 0; --i)
					{
						// Current character is invalid
						if (select_char(segments_[frame_state_.clicked_segment][j].string[i]) == false)
						{
							selection_start_ = std::addressof(segments_[frame_state_.clicked_segment][j].string[i + 1]);
							goto exit_outer_1;
						}
					}
				}

				selection_start_ = std::addressof(segments_[frame_state_.clicked_segment].front().string.front());

			exit_outer_1:

				for (std::ptrdiff_t j = frame_state_.clicked_subsegment; j < std::size(segments_[frame_state_.clicked_segment]); ++j)
				{
					for (std::ptrdiff_t i = i_ch; i < std::size(segments_[frame_state_.clicked_segment][j].string); ++i)
					{
						// Current character is invalid
						if (select_char(segments_[frame_state_.clicked_segment][j].string[i]) == false)
						{
							selection_end_ = std::addressof(segments_[frame_state_.clicked_segment][j].string[i]);
							goto exit_outer_2;
						}
					}
				}

				selection_end_ = std::addressof(segments_[frame_state_.clicked_segment].back().string.back());

			exit_outer_2:

				if (selection_start_ >= selection_end_)
				{
					selection_start_ = selection_end_ = nullptr;
				}
			}
			else
			{
				selection_start_ = frame_state_.clicked_char;
				selection_end_ = frame_state_.clicked_char + 1;
			}

		}
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			selection_start_ = frame_state_.clicked_char;
			selection_end_ = frame_state_.clicked_char + 1;
			valid_dragging_ = true;
		}
		else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && valid_dragging_)
		{
			selection_end_ = frame_state_.clicked_char;
		}
	}

	void console_window::process_text()
	{
		auto max_width = frame_state_.text_region_clip.GetWidth();

		constexpr std::string_view separateros{ "\n\t .,;?!" }; // TODO: Complete this list
		constexpr std::string_view spaces{ "\n\t " };

		const char* beg = buffer_.c_str();
		const char* end = buffer_.c_str() + std::size(buffer_);

		ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
		const char* current = beg;

		segments_.clear();

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

			if (line_end == nullptr)
			{
				line_end = end;
			}

			// Check if it fits
			auto size = ImGui::CalcTextSize(current, line_end);
			auto og_size = size;

			auto push = [&]()
				{
					if (next_inline) [[unlikely]]
						{
							segments_.back().emplace_back(segment{ std::string_view(current, line_end - current), color });
							next_inline = false;
							offset_size = 0.0f;
						}
					else
					{
						segments_.emplace_back(std::vector{ segment{ std::string_view(current, line_end - current), color } });
					}
					offset_size = size.x;
				};

			if (size.x < max_width_adjusted || word_wrapping_ == false)
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
			if (line_len == 1 && next_inline)
			{
				next_inline = false;
				continue;
			}

			line_end = current + std::max(line_len, static_cast<std::size_t>(1));
			push();
			current = line_end;
		}
	}
}
