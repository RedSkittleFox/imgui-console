#include <fox/imgui_console.hpp>
#include <imgui.h>



#include <iostream>

namespace ImGuiSample
{
	void submit_log_callback(fox::imgui::console_window* window, std::string_view text)
	{
		window->push_back(static_cast<std::string>(text) + "\n");
	}

	void update_sample_window(bool* is_open)
	{
		(void)is_open;
		static fox::imgui::console_window wnd(submit_log_callback);

		wnd.draw(nullptr);
	}
}