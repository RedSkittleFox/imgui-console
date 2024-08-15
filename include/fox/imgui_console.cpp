#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_console.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <regex>

namespace fox::imgui
{
	const char* default_color_parser(const char* start, const char* end, ImU32& color)
	{
		if (start >= end)
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

	std::string console_window::default_prediction_insert_callback(console_window* console, std::string_view text, std::string_view selected_prediction)
	{
		auto last = text.find_last_of(" \n\t");
		std::string_view word = text;
		if (last != std::string_view::npos)
		{
			word = text.substr(last + 1);
		}

		if (auto start = word.find_first_not_of(" \n\t"); start != std::string_view::npos)
			word = word.substr(start);

		if (auto end = word.find_last_not_of(" \n\t"); end != std::string_view::npos)
			word = word.substr(0, end + 1);

		if (selected_prediction.starts_with(word))
		{
			return static_cast<std::string>(selected_prediction.substr(std::size(word))) + " ";
		}
		else
		{
			return static_cast<std::string>(selected_prediction) + " ";
		}
	}

	console_window::console_window(
		std::move_only_function<void(console_window*, std::string_view)>&& execute_callback, 
		std::move_only_function<void(console_window*, std::string_view, std::vector<std::string>&)>&& prediction_callback)
		: config_(
			{
				.execute_callback = std::move(execute_callback),
				.prediction_callback = std::move(prediction_callback)
			}
		)
	{
	}

	void console_window::draw(bool* open)
	{
		frame_state_.clear();

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

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		draw_text_region(footer_height_to_reserve);

		// Make sure this is within the style, we calculate the text height in there
		handle_selection();
		ImGui::PopStyleVar();

		ImGui::Separator();

		// Command-line
		const float execute_button_width = ImGui::CalcTextSize("Submit").x + ImGui::GetStyle().FramePadding.x * 2;
		const float execute_text_width = ImGui::GetContentRegionAvail().x - execute_button_width - ImGui::GetStyle().ItemSpacing.x;

		ImGui::SetNextItemWidth(execute_text_width);

		ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText("##execute_input", std::data(execute_input_buffer_), std::size(execute_input_buffer_), input_text_flags, 
			[](ImGuiInputTextCallbackData* data) -> int { return static_cast<console_window*>(data->UserData)->input_text_command(data); },
			static_cast<void*>(this)
			))
		{
			input_text_command_execute();
			frame_state_.execute_input_reclaim_focus = true;
		}

		// Auto-focus on window apparition
		ImGui::SetItemDefaultFocus();
		if (frame_state_.execute_input_reclaim_focus)
		{
			ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
			frame_state_.execute_input_reclaim_focus = false;
		}

		ImGui::SameLine();

		if(ImGui::Button("Submit##execute_button", ImVec2(execute_button_width, 0)))
		{
			input_text_command_execute();
			frame_state_.execute_input_reclaim_focus = true;
		}

		draw_suggestions();

		draw_search_popup();

		if(frame_state_.process_text == false) // Text probably changed and needs to be reprocessed
		{
			apply_search();
		}


		ImGui::End();

		(void)open;
	}

	void console_window::clear()
	{
		this->buffer_.clear();
		frame_state_.process_text = true;
	}

	void console_window::push_back(std::string_view sv)
	{
		buffer_.append(sv);
		frame_state_.process_text = true;
	}

	void console_window::rebuild_buffer()
	{
		frame_state_.process_text = true;
	}

	std::string& console_window::buffer() noexcept
	{
		return buffer_;
	}

	const std::string& console_window::buffer() const noexcept
	{
		return buffer_;
	}

	void console_window::input_text_command_execute()
	{
		if (static_cast<bool>(config_.execute_callback))
		{
			config_.execute_callback(this, std::data(execute_input_buffer_));
		}

		std::memset(std::data(execute_input_buffer_), '\0', std::size(execute_input_buffer_));
	}

	int console_window::input_text_search(ImGuiInputTextCallbackData* data)
	{
		if ((data->EventFlag & ImGuiInputTextFlags_CallbackEdit) == 0)
			return 0;

		this->frame_state_.search_apply = true;
		this->frame_state_.search_center_view = true;

		return 0;
	}

	int console_window::input_text_command(ImGuiInputTextCallbackData* data)
	{
		std::string_view buf(data->Buf, data->BufTextLen);

		if(data->EventFlag & ImGuiInputTextFlags_CallbackEdit)
		{
			selected_prediction_ = -1;
			config_.prediction_callback(this, buf, predictions_);
		}
		else if(data->EventFlag & ImGuiInputTextFlags_CallbackCompletion)
		{
			std::ptrdiff_t prediction_index = -1;
			if(selected_prediction_ >= 0 && selected_prediction_ < std::ssize(predictions_))
			{
				prediction_index = selected_prediction_;
			}
			else if(!std::empty(predictions_))
			{
				prediction_index = 0;
			}

			if(prediction_index != -1)
			{
				data->InsertChars(data->BufTextLen, config_.prediction_insert_callback(this, buf, predictions_[prediction_index]).c_str());
				selected_prediction_ = -1;
				config_.prediction_callback(this, std::string_view(data->Buf, data->BufTextLen), predictions_);
			}
		}
		else if((data->EventFlag & ImGuiInputTextFlags_CallbackHistory) && !std::empty(buf) && !std::empty(predictions_))
		{
			if(data->EventKey == ImGuiKey_UpArrow)
			{
				selected_prediction_ = std::clamp(selected_prediction_ - 1, 0, static_cast<int>(std::ssize(predictions_) - 1));
			}
			else if (data->EventKey == ImGuiKey_DownArrow)
			{
				selected_prediction_ = std::clamp(selected_prediction_ + 1, 0, static_cast<int>(std::ssize(predictions_) - 1));
			}
		}
		else if((data->EventFlag & ImGuiInputTextFlags_CallbackHistory) && std::empty(buf))
		{
			// TODO: History
		}

		return 0;
	}

	void console_window::draw_menus()
	{
		if (ImGui::BeginMenuBar())
		{
			frame_state_.disable_selection = frame_state_.disable_selection_next_frame;
			frame_state_.disable_selection_next_frame = false;

			if (ImGui::BeginMenu("Edit"))
			{
				frame_state_.disable_selection = true;

				if (ImGui::MenuItem("Find", "CTRL+F"))
				{
					search_draw_popup_ = true;
					search_selected_ = {}; // Remove selected
				}

				if (ImGui::MenuItem("Select All", "CTRL+A"))
				{
					selection_start_ = std::data(buffer_);
					selection_end_ = std::data(buffer_) + std::size(buffer_);
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
				frame_state_.disable_selection = true;

				if (ImGui::Checkbox("Wrap", std::addressof(word_wrapping_)))
				{
					frame_state_.process_text = true;
				}

				if (ImGui::MenuItem("Clear"))
				{
					this->clear();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
	}

	void console_window::draw_search_popup()
	{
		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);

		if (search_draw_popup_ == false)
			return;

		// Search Popup
		const auto search_popup_size = ImVec2(300.f,
			ImGui::GetStyle().WindowPadding.y * 2 +
			ImGui::GetTextLineHeight() * (1 + search_draw_popup_properties_) +
			(ImGui::GetStyle().ItemSpacing.y) * (2 + search_draw_popup_properties_)
		);

		const auto search_popup_pos = ImVec2(text_region_clip.Max.x - search_popup_size.x, text_region_clip.Min.y);
		ImRect popup_rect(search_popup_pos, search_popup_pos + search_popup_size);
		ImGui::SetNextWindowPos(search_popup_pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(search_popup_size, ImGuiCond_Always);

		if (!ImGui::BeginChild("search_popup", search_popup_size, true,
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration)
			)
		{
			ImGui::EndChild();
			return;
		}

		// If mouse within window disable selection
		if (popup_rect.Contains(frame_state_.mouse_pos))
		{
			frame_state_.disable_selection_next_frame = true;
		}
		
		// Draw black background (default is transparent)
		{
			auto bg_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
			ImGui::GetWindowDrawList()->AddRectFilled(popup_rect.Min, popup_rect.Max, ImGui::ColorConvertFloat4ToU32(bg_color));
		}

		const auto button_size = ImGui::GetFrameHeight();
		const auto x_spacing = ImGui::GetStyle().ItemSpacing.x;

		auto text_size = search_popup_size.x - button_size * 4 - 6 * x_spacing;

		if (ImGui::ArrowButton("Advanced Search", search_draw_popup_properties_ ? ImGuiDir_Up : ImGuiDir_Down))
		{
			search_draw_popup_properties_ = !search_draw_popup_properties_;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(static_cast<float>(text_size));
		ImGui::InputText("##Search Box", std::data(search_input_buffer_), std::size(search_input_buffer_),
			ImGuiInputTextFlags_None | ImGuiInputTextFlags_CallbackEdit,
			[](ImGuiInputTextCallbackData* data) -> int { return static_cast<console_window*>(data->UserData)->input_text_search(data); },
			static_cast<void*>(this)
			);

		ImGui::SameLine();
		if (ImGui::ArrowButton("FindPrev", ImGuiDir_Up))
		{
			frame_state_.search_previous = true;
			frame_state_.search_apply = true;
		}
		ImGui::SameLine();
		if (ImGui::ArrowButton("FindNext", ImGuiDir_Down))
		{
			frame_state_.search_next = true;
			frame_state_.search_apply = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("X", ImVec2(button_size, button_size)))
		{
			search_draw_popup_ = false;
		}

		if (search_draw_popup_properties_)
		{
			if (ImGui::Checkbox("Match Word", &search_whole_word_)) frame_state_.search_apply = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("Match Casing", &search_match_casing_)) frame_state_.search_apply = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("Regex", &search_regex_)) frame_state_.search_apply = true;
		}

		ImGui::EndChild();
	}

	void console_window::draw_suggestions()
	{
		if (!ImGui::IsWindowFocused())
			return;

		if (std::empty(predictions_))
			return;

		auto suggestion_location = ImGui::GetWindowPos() + ImVec2(ImGui::GetStyle().ItemSpacing.x, ImGui::GetWindowHeight());
		std::string_view sv_input(std::data(execute_input_buffer_));

		auto r = std::find_if(std::rbegin(sv_input), std::rend(sv_input), [](char c) -> bool {return std::isspace(c); });
		if(r != std::rend(sv_input))
		{
			auto distance = std::distance(std::begin(sv_input), r.base());

			float offset = 0.f;
			if(distance != 0)
			{
				offset = ImGui::CalcTextSize(std::data(sv_input), std::data(sv_input) + distance).x;
			}

			suggestion_location.x += offset;
		}


		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.f, 1.f));
		// Calculate the size

		const auto count = std::min(std::ssize(predictions_), config_.predictions_count);

		auto test = ImGui::GetStyle().WindowPadding.y * 2;
		auto t2 = ImGui::GetTextLineHeight();
		float height = ImGui::GetStyle().WindowPadding.y * 2 + ImGui::GetStyle().ItemSpacing.y * (count - 1) + ImGui::GetTextLineHeight() * count;
		float width = 0.f;
		for(std::ptrdiff_t i = {}; i < count; ++i)
		{
			width = std::max(width, ImGui::CalcTextSize(predictions_[i].c_str()).x);
		}
		width += ImGui::GetStyle().WindowPadding.x * 2;

		const auto suggestion_size = ImVec2(width, height);
		const auto suggestion_rect = ImRect(suggestion_location, suggestion_location + suggestion_size);

		ImGui::SetNextWindowPos(suggestion_location, ImGuiCond_Always);
		ImGui::SetNextWindowSize(suggestion_size, ImGuiCond_Always);

		constexpr auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing;
		if (ImGui::Begin("search_popup", nullptr, flags))
		{
			// If mouse within window disable selection
			if (suggestion_rect.Contains(frame_state_.mouse_pos))
			{
				frame_state_.disable_selection_next_frame = true;
			}

			// Draw black background (default is transparent)
			{
				auto bg_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
				ImGui::GetWindowDrawList()->AddRectFilled(suggestion_rect.Min, suggestion_rect.Max, ImGui::ColorConvertFloat4ToU32(bg_color));
			}

			for (int i = 0; i < std::ssize(predictions_); ++i)
			{
				if (i == selected_prediction_)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
					ImGui::TextUnformatted(predictions_[i].c_str());
					ImGui::PopStyleColor();
					continue;
				}

				ImGui::TextUnformatted(predictions_[i].c_str());
			}
		}
		ImGui::End();

		ImGui::PopStyleVar(2);

	}

	void console_window::draw_text_region(float footer_height_to_reserve)
	{
		if (!ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::EndChild();
			return;
		}

		ImGuiID id = ImGui::GetCurrentWindow()->GetID("ScrollingRegion");
		auto scroll = ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

		frame_state_.text_region_clip_min = ImGui::GetCursorScreenPos() + scroll;
		frame_state_.text_region_clip_max = ImGui::GetCursorScreenPos() + ImGui::GetContentRegionAvail() + scroll;
		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);
		// ImGui::GetWindowDrawList()->AddRect(text_region_clip.Min, text_region_clip.Max, ImGui::ColorConvertFloat4ToU32(ImVec4{ 1.f, 1.f, 0.f, 1.f }));

		if (text_region_clip.Contains(ImGui::GetMousePos()) && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetCurrentContext()->ActiveId == id)
		{
			ImGui::SetKeyOwner(ImGuiKey_MouseLeft, id);
		}

		// Scroll in the window
		if(frame_state_.scroll_this_frame >= 0.0f)
		{
			ImGui::SetScrollY(ImGui::GetScrollMaxY() * frame_state_.scroll_this_frame);
		}

		if (auto [x, y] = ImGui::GetWindowSize(); frame_state_.last_size_x != x || frame_state_.last_size_y != y)
		{
			frame_state_.last_size_x = x;
			frame_state_.last_size_y = y;
			frame_state_.process_text = true;
		}

		// These two require frame_state_.text_region_clip to be processed
		if(frame_state_.process_text)
		{
			draw_text_process_text();
			frame_state_.process_text = false;
		}
		draw_text_autoscroll();

		if (auto this_scroll = ImGui::GetScrollX(); frame_state_.last_scroll_x != this_scroll)
		{
			frame_state_.last_scroll_x = this_scroll;
		}
		
		if(auto this_scroll = ImGui::GetScrollY(); frame_state_.last_scroll_y != this_scroll)
		{
			frame_state_.search_apply = true;
			frame_state_.last_scroll_y = this_scroll;
		}

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
			frame_state_.visible_row_min = std::min(frame_state_.visible_row_min, clipper.DisplayStart);
			frame_state_.visible_row_max = std::max(frame_state_.visible_row_max, clipper.DisplayEnd);

			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			{
				const auto& segment = segments_[i];
				assert(!std::empty(segment));

				draw_text_subsegment(i, 0);

				for (std::ptrdiff_t j = 1; j < std::ssize(segment); ++j)
				{
					ImGui::SameLine(0, 0);
					draw_text_subsegment(i, j);
				}
			}
		}

		// Edge case if there are no rendered rows
		if(frame_state_.visible_row_min == std::numeric_limits<int>::max())
		{
			frame_state_.visible_row_min = 0;
			frame_state_.visible_row_max = 0;
		}

		// Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
		// Using a scrollbar or mouse-wheel will take away from the bottom edge.
		if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		ScrollToBottom = false;

		ImGui::EndChild();
	}

	void console_window::draw_text_subsegment(std::ptrdiff_t segment, std::ptrdiff_t subsegment)
	{
		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);

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

		ImRect bb(ImVec2(text_region_clip.Min.x, pos.y), ImVec2(text_region_clip.Max.x, size.y + pos.y + g->Style.ItemInnerSpacing.y));

		// Check if mouse is within rectangle
		auto mouse_pos = ImGui::GetMousePos();
		if (frame_state_.disable_selection == false && ImGui::IsMouseDown(ImGuiMouseButton_Left) && text_region_clip.Contains(mouse_pos) && bb.Contains(mouse_pos))
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

		draw_selection(ImGui::GetColorU32(ImGuiCol_TextSelectedBg, 0.6f), beg, end, selection_start_, selection_end_, pos, size);

		if(search_draw_popup_ == true)
		{
			auto found_color = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, 0.6f));
			for (const auto& found : search_found_)
			{
				if (std::data(found) == std::data(search_selected_)) // compare locations
					continue;

				draw_selection(found_color, beg, end, std::data(found), std::data(found) + std::size(found), pos, size);
			}

			draw_selection(ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 0.f, 0.6f)), beg, end, std::data(search_selected_), std::data(search_selected_) + std::size(search_selected_), pos, size);
		}
	}

	void console_window::draw_selection(ImU32 bg_color, const char* beg, const char* end, const char* sel_beg, const char* sel_end, ImVec2 pos, ImVec2 size)
	{
		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);

		// Check if selection should be rendered - check if we are within range
		auto [selection_start, selection_end] = std::minmax(sel_beg, sel_end);
		if (selection_start == nullptr || selection_end == nullptr || (beg >= selection_end && selection_start > end - 1))
		{
			return;
		}

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

		rect.ClipWith(text_region_clip);
		if (rect.Overlaps(text_region_clip))
			ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, bg_color);
	}

	void console_window::draw_text_autoscroll()
	{
		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);

		// Handle mouse scrolling
		if (!(ImGui::IsMouseDragging(ImGuiMouseButton_Left) && valid_dragging_))
		{
			return;
		}

		assert(mouse_scroll_speed_ > 0.0f && "Scroll speed must be greater than zero.");
		assert(mouse_scroll_boundary_ > 0.0f && "Scroll boundary must be greater than zero.");

		const auto mouse_pos = frame_state_.mouse_pos;

		// Check mouse distance from borders
		auto dist_left = mouse_pos.x - mouse_scroll_boundary_ - text_region_clip.Min.x;
		auto dist_right = text_region_clip.Max.x - mouse_scroll_boundary_ - mouse_pos.x;
		auto dist_top = mouse_pos.y - mouse_scroll_boundary_ - text_region_clip.Min.y;
		auto dist_bottom = text_region_clip.Max.y - mouse_pos.y - mouse_scroll_boundary_;

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

	void console_window::handle_selection()
	{
		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);

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
				ImGui::GetWindowDrawList()->AddRect(text_region_clip.Min, text_region_clip.Max, ImGui::ColorConvertFloat4ToU32(ImVec4{ 1.f, 0.f, 0.f, 1.f }));

				if (frame_state_.visible_row_min == 0 && text_region_clip.Min.y >= mouse_pos.y)
				{
					selection_end_ = segments_[frame_state_.visible_row_min].front().string.data();
				}
				else if (frame_state_.visible_row_max + 1 == std::ssize(segments_) && text_region_clip.Min.y < mouse_pos.y)
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

				for (std::ptrdiff_t j = frame_state_.clicked_subsegment; j < std::ssize(segments_[frame_state_.clicked_segment]); ++j)
				{
					for (std::ptrdiff_t i = i_ch; i < std::ssize(segments_[frame_state_.clicked_segment][j].string); ++i)
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

	void console_window::apply_search()
	{
		if (search_draw_popup_ == false || frame_state_.search_apply == false)
			return;

		search_found_.clear();

		// Don't search if search input is empty
		if (std::strlen(std::data(search_input_buffer_)) == 0)
			return;

		// Visible segments
		apply_search_segment_range(frame_state_.visible_row_min, frame_state_.visible_row_max);

		// Search inside visible range first
		if (frame_state_.search_center_view && std::empty(search_found_)) // Nothing found and we need to recenter the view
		{
			frame_state_.search_next = true;
		}

		std::ptrdiff_t search_segment_start = frame_state_.visible_row_min;
		if(!std::empty(search_selected_))
		{
			search_segment_start = find_segment_from_pointer(std::data(search_selected_));
		}

		for (int r = 0; r < 2; ++r)
		{
			bool towards_top = true;
			bool circle = false;
			if (frame_state_.search_next)
			{
				towards_top = true;
				circle = false;
				search_found_.clear();
				auto i = (r == 0 ? search_segment_start : search_segment_start + 1); 
				while (i + 1 < std::ssize(segments_) && std::empty(search_found_))
				{
					apply_search_segment_range(i, i + 2);
					i = i + 1;
				}

				if (i < std::ssize(segments_) && std::empty(search_found_))
				{
					apply_search_segment_range(i, i + 1);
					i = i + 1;
				}

				if (i >= std::ssize(segments_) && std::empty(search_found_))
				{
					circle = true;

					i = 0;
					while (i + 1 <= search_segment_start && std::empty(search_found_))
					{
						apply_search_segment_range(i, i + 2);
						i = i + 1;
					}
				}
			}
			else if (frame_state_.search_previous)
			{
				towards_top = false;
				circle = false;
				search_found_.clear();
				auto i = (r == 0 ? search_segment_start : search_segment_start - 1);
				while (i - 1 >= 0 && std::empty(search_found_))
				{
					apply_search_segment_range(i - 1, i + 1);
					i = i - 1;
				}

				if (i >= 0 && std::empty(search_found_))
				{
					apply_search_segment_range(i, i + 1);
					i = i - 1;
				}

				if (i < 0 && std::empty(search_found_))
				{
					circle = true;

					i = std::ssize(segments_) - 1;
					while (i - 1 >= search_segment_start && std::empty(search_found_))
					{
						apply_search_segment_range(i - 1, i + 1);
						i = i - 1;
					}
				}
			}

			if (!std::empty(search_found_) && (frame_state_.search_previous || frame_state_.search_next))
			{
				const auto r = std::find_if(std::begin(search_found_), std::end(search_found_), [&](std::string_view sv) -> bool { return std::data(sv) == std::data(search_selected_); });
				if(r == std::end(search_found_) && towards_top)
				{
					search_selected_ = search_found_.front();
				}
				else if (r == std::end(search_found_) && towards_top == false)
				{
					search_selected_ = search_found_.back();
				}
				else if(r + 1 != std::end(search_found_) && towards_top && circle == false)
				{
					search_selected_ = *(r + 1);
				}
				else if(r != std::end(search_found_) && towards_top && circle == true)
				{
					search_selected_ = search_found_.front();
				}
				else if(r != std::begin(search_found_) && towards_top == false && circle == false)
				{
					search_selected_ = *(r - 1);
				}
				else if (r != std::end(search_found_) && towards_top == false && circle == true)
				{
					search_selected_ = search_found_.back();
				}
				else
				{
					continue;
				}

				const auto search_segment = find_segment_from_pointer(std::data(search_selected_));

				if (search_segment < std::ssize(segments_) && 
					(search_segment < frame_state_.visible_row_min || search_segment > frame_state_.visible_row_max) // scroll only if outside visible range
					)
				{
					frame_state_.scroll_next_frame = static_cast<float>(search_segment) / static_cast<float>(std::ssize(segments_));
				}


				search_found_.clear();

				if(frame_state_.scroll_next_frame < 0.f)
				{
					apply_search_segment_range(frame_state_.visible_row_min, frame_state_.visible_row_max);
				}
				else
				{
					const auto num_visible = frame_state_.visible_row_max - frame_state_.visible_row_min;
					const auto new_start = static_cast<std::ptrdiff_t>(frame_state_.scroll_next_frame * static_cast<float>(std::ssize(segments_) - num_visible));
					auto new_end = std::min(std::ssize(segments_), new_start + num_visible + 1);
					apply_search_segment_range(new_start, new_end);
				}
			}
			break;
		}
	}

	void console_window::apply_search_segment_range(std::ptrdiff_t segment_start, std::ptrdiff_t segment_end)
	{
		segment_start = std::clamp(segment_start, static_cast<std::ptrdiff_t>(0), std::ssize(segments_));
		segment_end = std::clamp(segment_end, static_cast<std::ptrdiff_t>(0), std::ssize(segments_));

		// Join strings not separated by new line / color
		std::ptrdiff_t i = segment_start;
		std::ptrdiff_t j = 0;
		std::regex regex;

		// build regex
		if (search_regex_) // TODO: Move this to where the search string is set
		{
			std::string regex_string = std::string("(") + std::data(search_input_buffer_) + ")";
			regex = std::regex(regex_string, std::regex::flag_type::basic | std::regex::flag_type::optimize | (search_match_casing_ ? std::regex::flag_type{} : std::regex::flag_type::icase));
		}

		while (i < segment_end)
		{
			if (j >= std::ssize(segments_[i]) || std::empty(segments_[i][j].string))
			{
				i = i + 1;
				j = 0;
				continue;
			}

			const char* start = std::addressof(segments_[i][j].string.front());
			const char* end = std::addressof(segments_[i][j].string.back());

			j = j + 1;

			while (i < segment_end)
			{
				if (j >= std::ssize(segments_[i]) || std::empty(segments_[i][j].string))
				{
					i = i + 1;
					j = 0;
					continue;
				}

				if (std::addressof(segments_[i][j].string.front()) != end)
				{
					break;
				}

				end = std::addressof(segments_[i][j].string.back());
			}

			// Perform search inside our string
			const auto og_start = start;
			while (start < end)
			{
				if (search_regex_)
				{
					using namespace std::regex_constants;

					// We only start searching if it starts with whitespace
					match_flag_type match_flag{};
					if (search_whole_word_)
					{
						match_flag = format_first_only;

						// Find space
						if (og_start != start)
						{
							while (start < end && !std::isspace(*(start - 1)))
								start++;

							if (start == end)
								break;
						}
					}

					std::cmatch result;
					if (std::regex_search(start, end, result, regex,
						match_not_bol | match_not_eol | match_not_bow | match_not_eow | match_not_null | match_flag))
					{
						const char* found_start = std::begin(result)->first;
						const char* found_end = std::begin(result)->second;

						if (search_whole_word_)
						{
							if (
								(start != found_start && !std::isspace(*(found_start - 1))) ||
								(end != found_end) && !std::isspace(*found_end))
							{
								start = start + 1; // Search next
								break;
							}
						}

						search_found_.emplace_back(start, end);
						start = end; // Search next
					}
					else
					{
						break; // Nothing to be found
					}
				}
				else if (search_whole_word_)
				{
					// Find space
					if (og_start != start)
					{
						while (start < end && !std::isspace(*(start - 1)))
							start++;

						if (start == end)
							break;
					}

					std::string_view sv(start, end);

					if (!sv.starts_with(std::data(search_input_buffer_)))
					{
						start = start + 1;
						continue;
					}
					else
					{
						auto ee = start + std::strlen(std::data(search_input_buffer_));
						search_found_.emplace_back(start, ee);
						start = ee;
					}
				}
				else
				{
					std::string_view sv(start, end);
					const auto p = sv.find(std::data(search_input_buffer_));

					if (p == std::string_view::npos)
						break;

					start = start + p;
					auto ee = start + std::strlen(std::data(search_input_buffer_));
					search_found_.emplace_back(start, ee);
					start = ee;
				}
			}
		}
	}

	struct segment_pointer_comp
	{
		[[nodiscard]] bool operator()(const std::vector<console_window::segment>& lhs, const char* rhs) const
		{
			assert(!std::empty(lhs));
			assert(rhs);

			return std::data(lhs.front().string) < rhs;
		}

		[[nodiscard]] bool operator()(const char* lhs, const std::vector<console_window::segment>& rhs) const
		{
			assert(!std::empty(rhs));
			assert(lhs);

			return lhs < std::data(rhs.front().string);
		}
	};

	std::ptrdiff_t console_window::find_segment_from_pointer(const char* str)
	{
		// Binary search the segments pointer wise, they are always in order
		const auto r = std::upper_bound(
			std::begin(segments_),
			std::end(segments_),
			str,
			segment_pointer_comp{}
		);

		return std::distance(std::begin(segments_), r - 1);
	}

	void console_window::draw_text_process_text()
	{
		frame_state_.search_apply = true;

		ImRect text_region_clip(frame_state_.text_region_clip_min, frame_state_.text_region_clip_max);

		auto max_width = text_region_clip.GetWidth();

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
