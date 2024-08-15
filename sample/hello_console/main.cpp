#include <fox/imgui_console.hpp>
#include <imgui.h>



#include <iostream>

namespace ImGuiSample
{
	void submit_log_callback(fox::imgui::console_window* window, std::string_view text)
	{
		window->push_back(static_cast<std::string>(text) + "\n");
	}

	void prediction_callback(fox::imgui::console_window* window, std::string_view text, std::vector<std::string>& predictions)
	{
		const static std::vector<std::string> prediction_strings
		{
			"fox",
			"foxbox",
			"foxbox123",
			"box",
			"plum",
			"drum"
		};

		predictions.clear();

		auto last = text.find_last_of(" \n\t");
		std::string_view word = text;
		if(last != std::string_view::npos)
		{
			word = text.substr(last + 1);
		}

		if (auto start = word.find_first_not_of(" \n\t"); start != std::string_view::npos)
			word = word.substr(start);

		if (auto end = word.find_last_not_of(" \n\t"); end != std::string_view::npos)
			word = word.substr(0, end + 1);

		if (std::empty(word))
			return;

		predictions = prediction_strings;
		std::erase_if(predictions, [&](const std::string& str) -> bool { return !str.starts_with(word); });
	}

	void update_sample_window(bool* is_open)
	{
		(void)is_open;
		static fox::imgui::console_window wnd(&submit_log_callback, &prediction_callback);

		wnd.draw(nullptr);
	}
}