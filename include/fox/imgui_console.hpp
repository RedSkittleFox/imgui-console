#ifndef FOX_IMGUI_CONSOLE_CONSOLE_H_
#define FOX_IMGUI_CONSOLE_CONSOLE_H_
#pragma once

#include "imgui.h"

#include <vector>
#include <string>
#include <array>
#include <string_view>

namespace fox::imgui
{
    class console_window
    {
        friend struct segment_pointer_comp;

        struct config
        {
            const char* window_name = "Console Window";
        } config_;

        struct frame_state
        {
            ImVec2 mouse_pos;
            ImVec2 text_region_clip_min;
            ImVec2 text_region_clip_max;

            // Rows visible during this frame
        	int visible_row_min;
            int visible_row_max;

            // Last clicked char
            const char* clicked_char;
            std::ptrdiff_t clicked_segment;
            std::ptrdiff_t clicked_subsegment;

            bool disable_selection_next_frame = false;;
            bool disable_selection;

            bool search_apply;
            bool search_next;
            bool search_previous;
            bool search_center_view;

            float scroll_next_frame = -1.f;
            float scroll_this_frame;

            void clear()
            {
                mouse_pos = ImVec2(0, 0);
                text_region_clip_min = ImVec2(0, 0);
                text_region_clip_max = ImVec2(0, 0);

                clicked_char = nullptr;
                clicked_segment = std::numeric_limits<std::ptrdiff_t>::max();
                clicked_subsegment = std::numeric_limits<std::ptrdiff_t>::max();

                disable_selection = disable_selection_next_frame;
                disable_selection_next_frame = false;

                search_apply = false;
                search_next = false;
                search_previous = false;
                search_center_view = false;

                scroll_this_frame = scroll_next_frame;
                scroll_next_frame = -1.f;
            }

        } frame_state_;

        std::array<char, 128> input_buffer_;
        std::string buffer_ = R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec commodo nec sem id interdum. Pellentesque dictum neque ligula, vitae aliquet lacus consectetur at. Ut efficitur ornare lectus vel vehicula. Integer lorem turpis, convallis vel orci et, efficitur varius tortor. Cras accumsan mattis orci non convallis. Morbi massa ligula, efficitur sit amet mi id, iaculis congue erat. Aliquam nec urna ut libero sollicitudin aliquet et a libero. Fusce metus lectus, porta nec risus quis, condimentum vulputate ipsum. Sed hendrerit ex id ornare ultrices. Sed gravida finibus congue. Proin congue hendrerit ex pulvinar dignissim. Morbi nec mauris quis lacus mattis sollicitudin et quis leo. Curabitur nec tempor lacus.

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

        bool word_wrapping_ = true;

        bool valid_dragging_ = false;
        std::vector<std::vector<segment>> segments_;

        const char* selection_start_ = nullptr;
        const char* selection_end_ = nullptr;

        float mouse_scroll_boundary_ = 50.f;
        float mouse_scroll_speed_ = 20.f;

        bool search_draw_popup_ = true;
        bool search_draw_popup_properties_ = false;
        bool search_match_casing_ = false;
        bool search_whole_word_ = false;
        bool search_regex_ = false;

        std::vector<std::string_view> search_found_;
        std::string_view search_selected_;
        std::array<char, 32> search_input_ {};

    public:
        void draw(bool* open);

    private:
        void draw_menus();
        void draw_search_popup();
        void draw_search_popup_properties();

        void draw_text_region(float footer_height_to_reserve);
        void draw_text_subsegment(std::ptrdiff_t segment, std::ptrdiff_t subsegment);
        void draw_text_process_text();
        void draw_selection(ImU32 bg_color, const char* beg, const char* end, const char* sel_beg, const char* sel_end, ImVec2 pos, ImVec2 size);
        void draw_text_autoscroll();

        void handle_selection();
        void apply_search();
        void apply_search_segment_range(std::ptrdiff_t segment_start, std::ptrdiff_t segment_end);

        std::ptrdiff_t find_segment_from_pointer(const char* str);
    };
}

#endif