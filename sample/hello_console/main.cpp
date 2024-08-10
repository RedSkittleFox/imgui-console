#include <fox/imgui_console.hpp>
#include <imgui.h>



#include <iostream>

namespace ImGuiSample
{
	void update_sample_window(bool* is_open)
	{
		(void)is_open;
		static fox::imgui::console_window wnd;

		wnd.draw(nullptr);
	}
}