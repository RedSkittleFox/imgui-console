[![Build and Deploy ImGui Sample](https://github.com/RedSkittleFox/imgui-console/actions/workflows/deploy.yml/badge.svg)](https://github.com/RedSkittleFox/imgui-console/actions/workflows/deploy.yml)

# imgui-console
A fully fledged imgui console widget supporting:

- text selection
- text coloring
- wrapping
- searching
- command predictions

https://github.com/user-attachments/assets/e5c0bfd2-0b2a-45ff-8122-2c5292eb1db7

# Sample Usage

Please visit [samples](/sample) folder for more usage examples.

```cpp
#include <fox/imgui_console.hpp>
#include <imgui.h>

#include <iostream>

void submit_log_callback(fox::imgui::console_window* window, std::string_view text)
{
	window->push_back(static_cast<std::string>(text) + "\n");
}

int main()
{
	fox::imgui::console_window wnd(fox::imgui::console_window::config{ 
		.execute_callback = &submit_log_callback
	});

	while(true)
	{
		.. imgui begin frame..

		wnd.draw(nullptr);

		.. end frame..
	}
}

```

# Integration

All that is needed to use this library are `imgui_console.hpp` and `imgui_console.cpp` files. You can copy them directly into your project (imgui style) or use CMake to add it to your project.

## Cmake
```cmake
include(FetchContent)

FetchContent_Declare(imgui_console GIT_REPOSITORY https://github.com/RedSkittleFox/imgui-console.git)
FetchContent_MakeAvailable(imgui_console)
target_link_libraries(foo PRIVATE fox::imgui_console)
```

# License
This library is licensed under the [MIT License](LICENSE).
