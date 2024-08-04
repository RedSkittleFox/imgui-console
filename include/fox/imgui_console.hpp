#ifndef FOX_IMGUI_CONSOLE_CONSOLE_H_
#define FOX_IMGUI_CONSOLE_CONSOLE_H_
#pragma once

#include "imgui.h"

#include <vector>
#include <string>
#include <array>

namespace fox::imgui
{
    struct config
    {
        const char* window_name = "Console Window";
    };

    struct state
    {
        std::array<char, 128> input_buffer;
        std::string buffer = R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec commodo nec sem id interdum. Pellentesque dictum neque ligula, vitae aliquet lacus consectetur at. Ut efficitur ornare lectus vel vehicula. Integer lorem turpis, convallis vel orci et, efficitur varius tortor. Cras accumsan mattis orci non convallis. Morbi massa ligula, efficitur sit amet mi id, iaculis congue erat. Aliquam nec urna ut libero sollicitudin aliquet et a libero. Fusce metus lectus, porta nec risus quis, condimentum vulputate ipsum. Sed hendrerit ex id ornare ultrices. Sed gravida finibus congue. Proin congue hendrerit ex pulvinar dignissim. Morbi nec mauris quis lacus mattis sollicitudin et quis leo. Curabitur nec tempor lacus.

Duis lobortis, nisl ut feugiat efficitur, odio arcu porttitor mauris, vitae posuere {{0;255;0}}dupadupadupa tellus. Duis congue eleifend quam, placerat sagittis lorem hendrerit in. Quisque mollis tellus ipsum, quis iaculis metus rutrum eu. Integer lobortis aliquam elit. Donec hendrerit sit amet sem pharetra pulvinar. Nam quis efficitur erat, eu pharetra turpis. Donec eleifend lectus vel lacus mattis, ut cursus mauris auctor. Quisque condimentum risus at sem dapibus, vel gravida ex sagittis. Nulla nec mauris nec ex condimentum accumsan. Integer euismod ultricies leo sed tincidunt. Curabitur malesuada, nisi id venenatis tristique, libero lorem porta magna, quis imperdiet leo tellus non sapien. Cras sodales feugiat turpis in ultricies.

Fusce ac consectetur ligula, vel tincidunt justo. Nulla auctor ultricies nulla, sed sagittis purus mollis a. Nullam nulla diam, efficitur id enim rhoncus, porta tempor diam. Vivamus dictum, metus vitae lobortis bibendum, eros lacus feugiat mi, ac ornare massa sem sit amet leo. Ut venenatis libero nulla. Integer suscipit eros vitae risus pulvinar, sit amet iaculis arcu feugiat. Vivamus in elementum ipsum. Aliquam at arcu et odio euismod sollicitudin a ac urna. Praesent non mollis neque. Suspendisse ac tortor tortor. Nulla facilisi. Vivamus eleifend blandit massa ac sodales.

Pellentesque efficitur fringilla purus vitae sodales. Proin volutpat odio nec massa {{255;255;0}}amogusamogusamogus, vel efficitur arcu venenatis. Maecenas ultricies mi at urna suscipit, id consectetur orci dictum. Vivamus a tellus rhoncus, volutpat purus sed, ultrices magna. Nam sed neque et nisl dictum convallis. Sed a felis enim. Ut mollis tempor lacus, vel bibendum arcu malesuada at. Sed sit amet molestie tortor. Proin eget metus faucibus, finibus mauris quis, imperdiet quam. Cras volutpat, lacus sit amet scelerisque interdum, ipsum quam elementum nibh, sed ultricies sem enim bibendum lectus. Nulla facilisis sit amet orci at ullamcorper. Mauris ut eleifend erat. Curabitur consequat leo eu enim laoreet, sit amet tincidunt purus ultricies. Praesent hendrerit a tellus id hendrerit.

In porta enim in ex pulvinar semper. Mauris efficitur vitae arcu et commodo. Nulla gravida tellus sit amet eros malesuada tincidunt. Donec non nibh nec ante pharetra elementum. Vivamus molestie mollis massa, condimentum tempor arcu lacinia quis. Nunc imperdiet sodales leo et rutrum. Nullam leo ligula, consequat ac elementum vel, blandit nec sapien. Duis odio ex, tincidunt eu suscipit ac, bibendum sed metus. Proin tincidunt vel sapien ac congue.)"
    	;

        struct segment
        {
            std::string_view string;
            ImU32 color;
        };

        std::vector<std::vector<segment>> segments;

        const char* selection_start = std::data(buffer);
        const char* selection_end = std::data(buffer) + 500;
    };

    void console_window(state& state, bool* open = nullptr, const config& cfg = config());
}

#endif