#ifndef FOX_IMGUI_CONSOLE_CONSOLE_H_
#define FOX_IMGUI_CONSOLE_CONSOLE_H_
#pragma once

#include "imgui.h"

#include <memory>
#include <vector>
#include <string>
#include <array>
#include <string_view>
#include <functional>

namespace fox::imgui
{
    class console_window
    {
        static std::string default_prediction_insert_callback(console_window* console, std::string_view text, std::string_view selected_prediction);
        static void default_prediction_callback(console_window*, std::string_view, std::vector<std::string>&);
    public:
        friend struct segment_pointer_comp;

#ifdef __cpp_lib_move_only_function
        template<class T>
        using function_t = std::move_only_function<T>;
#else
        template<class T>
        using function_t = std::function<T>;
#endif

        struct config
        {
            std::string window_name = "Console Window";
            function_t<void(console_window*, std::string_view)> execute_callback;
            function_t<void(console_window*, std::string_view, std::vector<std::string>&)> prediction_callback = &console_window::default_prediction_callback;
            function_t<std::string(console_window*, std::string_view, std::string_view)> prediction_insert_callback = &console_window::default_prediction_insert_callback;
            std::ptrdiff_t predictions_count = 5;
        };

    private:

    	config config_;

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

            bool search_apply_next_frame = false;
            bool search_apply;
            bool search_next;
            bool search_previous;
            bool search_center_view;

            float scroll_next_frame = -1.f;
            float scroll_this_frame;

            // Don't clear
            float last_scroll_x = 0.f;
            float last_scroll_y = 0.f;
            float last_size_x = 0.f;
            float last_size_y = 0.f;

            // Reset manually
            bool process_text = false;
            bool execute_input_reclaim_focus = false;

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

        std::vector<char> execute_input_buffer_ = std::vector<char>(128, '\0');;
        std::string buffer_;

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

        bool search_draw_popup_ = false;
        bool search_draw_popup_properties_ = false;
        bool search_match_casing_ = false;
        bool search_whole_word_ = false;
        bool search_regex_ = false;

        std::vector<std::string_view> search_found_;
        std::string_view search_selected_;
        std::vector<char> search_input_buffer_ = std::vector<char>(64, '\0');

        std::vector<std::string> predictions_;
        int selected_prediction_ = -1;

    public:
        console_window(config cfg);

        void draw(bool* open);
        void clear();
        void push_back(std::string_view sv);

        void rebuild_buffer();
        [[nodiscard]] std::string& buffer() noexcept;
        [[nodiscard]] const std::string& buffer() const noexcept;
    private:
        void input_text_command_execute();
        int input_text_search(ImGuiInputTextCallbackData* data);
        int input_text_command(ImGuiInputTextCallbackData* data);

    private:
        void draw_menus();
        void draw_search_popup();
        void draw_suggestions();

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