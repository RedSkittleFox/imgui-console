#define IM_VEC2_CLASS_EXTRA
#include "imgui.h"
#include "imgui_internal.h"

#include <iostream>
#include <vector>
#include <string>
#include <array>

inline int InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end)
{
    int line_count = 0;
    const char* s = text_begin;
    while (char c = *s++) // We are only matching for \n so we can ignore UTF-8 decoding
        if (c == '\n')
            line_count++;
    s--;
    if (s[0] != '\n' && s[0] != '\r')
        line_count++;
    *out_text_end = s;
    return line_count;
}

inline ImVec2 InputTextCalcTextSizeW(ImGuiContext* ctx, const ImWchar* text_begin, const ImWchar* text_end, const ImWchar** remaining = nullptr, ImVec2* out_offset = nullptr, bool stop_on_new_line = false)
{
    ImGuiContext& g = *ctx;
    ImFont* font = g.Font;
    const float line_height = g.FontSize;
    const float scale = line_height / font->FontSize;

    ImVec2 text_size = ImVec2(0, 0);
    float line_width = 0.0f;

    const ImWchar* s = text_begin;
    while (s < text_end)
    {
        unsigned int c = (unsigned int)(*s++);
        if (c == '\n')
        {
            text_size.x = ImMax(text_size.x, line_width);
            text_size.y += line_height;
            line_width = 0.0f;
            if (stop_on_new_line)
                break;
            continue;
        }
        if (c == '\r')
            continue;

        const float char_width = font->GetCharAdvance((ImWchar)c) * scale;
        line_width += char_width;
    }

    if (text_size.x < line_width)
        text_size.x = line_width;

    if (out_offset)
        *out_offset = ImVec2(line_width, text_size.y + line_height);  // offset allow for the possibility of sitting after a trailing \n

    if (line_width > 0 || text_size.y == 0.0f)                        // whereas size.y will ignore the trailing \n
        text_size.y += line_height;

    if (remaining)
        *remaining = s;

    return text_size;
}


// Wrapper for stb_textedit.h to edit text (our wrapper is for: statically sized buffer, single-line, wchar characters. InputText converts between UTF-8 and wchar)
namespace ImStb
{
    static int     STB_TEXTEDIT_STRINGLEN(const ImGuiInputTextState* obj) { return obj->CurLenW; }
    static ImWchar STB_TEXTEDIT_GETCHAR(const ImGuiInputTextState* obj, int idx) { IM_ASSERT(idx <= obj->CurLenW); return obj->TextW[idx]; }
    static float   STB_TEXTEDIT_GETWIDTH(ImGuiInputTextState* obj, int line_start_idx, int char_idx) { ImWchar c = obj->TextW[line_start_idx + char_idx]; if (c == '\n') return IMSTB_TEXTEDIT_GETWIDTH_NEWLINE; ImGuiContext& g = *obj->Ctx; return g.Font->GetCharAdvance(c) * g.FontScale; }
    static int     STB_TEXTEDIT_KEYTOTEXT(int key) { return key >= 0x200000 ? 0 : key; }
    static ImWchar STB_TEXTEDIT_NEWLINE = '\n';
    static void    STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, ImGuiInputTextState* obj, int line_start_idx)
    {
        const ImWchar* text = obj->TextW.Data;
        const ImWchar* text_remaining = NULL;
        const ImVec2 size = InputTextCalcTextSizeW(obj->Ctx, text + line_start_idx, text + obj->CurLenW, &text_remaining, NULL, true);
        r->x0 = 0.0f;
        r->x1 = size.x;
        r->baseline_y_delta = size.y;
        r->ymin = 0.0f;
        r->ymax = size.y;
        r->num_chars = (int)(text_remaining - (text + line_start_idx));
    }

    static bool is_separator(unsigned int c)
    {
        return c == ',' || c == ';' || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' || c == '|' || c == '\n' || c == '\r' || c == '.' || c == '!' || c == '\\' || c == '/';
    }

    static int is_word_boundary_from_right(ImGuiInputTextState* obj, int idx)
    {
        // When ImGuiInputTextFlags_Password is set, we don't want actions such as CTRL+Arrow to leak the fact that underlying data are blanks or separators.
        if ((obj->Flags & ImGuiInputTextFlags_Password) || idx <= 0)
            return 0;

        bool prev_white = ImCharIsBlankW(obj->TextW[idx - 1]);
        bool prev_separ = is_separator(obj->TextW[idx - 1]);
        bool curr_white = ImCharIsBlankW(obj->TextW[idx]);
        bool curr_separ = is_separator(obj->TextW[idx]);
        return ((prev_white || prev_separ) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
    }
    static int is_word_boundary_from_left(ImGuiInputTextState* obj, int idx)
    {
        if ((obj->Flags & ImGuiInputTextFlags_Password) || idx <= 0)
            return 0;

        bool prev_white = ImCharIsBlankW(obj->TextW[idx]);
        bool prev_separ = is_separator(obj->TextW[idx]);
        bool curr_white = ImCharIsBlankW(obj->TextW[idx - 1]);
        bool curr_separ = is_separator(obj->TextW[idx - 1]);
        return ((prev_white) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
    }
    static int  STB_TEXTEDIT_MOVEWORDLEFT_IMPL(ImGuiInputTextState* obj, int idx) { idx--; while (idx >= 0 && !is_word_boundary_from_right(obj, idx)) idx--; return idx < 0 ? 0 : idx; }
    static int  STB_TEXTEDIT_MOVEWORDRIGHT_MAC(ImGuiInputTextState* obj, int idx) { idx++; int len = obj->CurLenW; while (idx < len && !is_word_boundary_from_left(obj, idx)) idx++; return idx > len ? len : idx; }
    static int  STB_TEXTEDIT_MOVEWORDRIGHT_WIN(ImGuiInputTextState* obj, int idx) { idx++; int len = obj->CurLenW; while (idx < len && !is_word_boundary_from_right(obj, idx)) idx++; return idx > len ? len : idx; }
    static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(ImGuiInputTextState* obj, int idx) { ImGuiContext& g = *obj->Ctx; if (g.IO.ConfigMacOSXBehaviors) return STB_TEXTEDIT_MOVEWORDRIGHT_MAC(obj, idx); else return STB_TEXTEDIT_MOVEWORDRIGHT_WIN(obj, idx); }
#define STB_TEXTEDIT_MOVEWORDLEFT   STB_TEXTEDIT_MOVEWORDLEFT_IMPL  // They need to be #define for stb_textedit.h
#define STB_TEXTEDIT_MOVEWORDRIGHT  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL

    static void STB_TEXTEDIT_DELETECHARS(ImGuiInputTextState* obj, int pos, int n)
    {
        ImWchar* dst = obj->TextW.Data + pos;

        // We maintain our buffer length in both UTF-8 and wchar formats
        obj->Edited = true;
        obj->CurLenA -= ImTextCountUtf8BytesFromStr(dst, dst + n);
        obj->CurLenW -= n;

        // Offset remaining text (FIXME-OPT: Use memmove)
        const ImWchar* src = obj->TextW.Data + pos + n;
        while (ImWchar c = *src++)
            *dst++ = c;
        *dst = '\0';
    }

    static bool STB_TEXTEDIT_INSERTCHARS(ImGuiInputTextState* obj, int pos, const ImWchar* new_text, int new_text_len)
    {
        const bool is_resizable = (obj->Flags & ImGuiInputTextFlags_CallbackResize) != 0;
        const int text_len = obj->CurLenW;
        IM_ASSERT(pos <= text_len);

        const int new_text_len_utf8 = ImTextCountUtf8BytesFromStr(new_text, new_text + new_text_len);
        if (!is_resizable && (new_text_len_utf8 + obj->CurLenA + 1 > obj->BufCapacityA))
            return false;

        // Grow internal buffer if needed
        if (new_text_len + text_len + 1 > obj->TextW.Size)
        {
            if (!is_resizable)
                return false;
            IM_ASSERT(text_len < obj->TextW.Size);
            obj->TextW.resize(text_len + ImClamp(new_text_len * 4, 32, ImMax(256, new_text_len)) + 1);
        }

        ImWchar* text = obj->TextW.Data;
        if (pos != text_len)
            memmove(text + pos + new_text_len, text + pos, (size_t)(text_len - pos) * sizeof(ImWchar));
        memcpy(text + pos, new_text, (size_t)new_text_len * sizeof(ImWchar));

        obj->Edited = true;
        obj->CurLenW += new_text_len;
        obj->CurLenA += new_text_len_utf8;
        obj->TextW[obj->CurLenW] = '\0';

        return true;
    }

    // We don't use an enum so we can build even with conflicting symbols (if another user of stb_textedit.h leak their STB_TEXTEDIT_K_* symbols)
#define STB_TEXTEDIT_K_LEFT         0x200000 // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT        0x200001 // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP           0x200002 // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN         0x200003 // keyboard input to move cursor down
#define STB_TEXTEDIT_K_LINESTART    0x200004 // keyboard input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND      0x200005 // keyboard input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART    0x200006 // keyboard input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND      0x200007 // keyboard input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE       0x200008 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE    0x200009 // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO         0x20000A // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO         0x20000B // keyboard input to perform redo
#define STB_TEXTEDIT_K_WORDLEFT     0x20000C // keyboard input to move cursor left one word
#define STB_TEXTEDIT_K_WORDRIGHT    0x20000D // keyboard input to move cursor right one word
#define STB_TEXTEDIT_K_PGUP         0x20000E // keyboard input to move cursor up a page
#define STB_TEXTEDIT_K_PGDOWN       0x20000F // keyboard input to move cursor down a page
#define STB_TEXTEDIT_K_SHIFT        0x400000

#define IMSTB_TEXTEDIT_IMPLEMENTATION
#define IMSTB_TEXTEDIT_memmove memmove
#include "imstb_textedit.h"

// stb_textedit internally allows for a single undo record to do addition and deletion, but somehow, calling
// the stb_textedit_paste() function creates two separate records, so we perform it manually. (FIXME: Report to nothings/stb?)
    static void stb_textedit_replace(ImGuiInputTextState* str, STB_TexteditState* state, const IMSTB_TEXTEDIT_CHARTYPE* text, int text_len)
    {
        stb_text_makeundo_replace(str, state, 0, str->CurLenW, text_len);
        ImStb::STB_TEXTEDIT_DELETECHARS(str, 0, str->CurLenW);
        state->cursor = state->select_start = state->select_end = 0;
        if (text_len <= 0)
            return;
        if (ImStb::STB_TEXTEDIT_INSERTCHARS(str, 0, text, text_len))
        {
            state->cursor = state->select_start = state->select_end = text_len;
            state->has_preferred_x = 0;
            return;
        }
        IM_ASSERT(0); // Failed to insert character, normally shouldn't happen because of how we currently use stb_textedit_replace()
    }

} // namespace ImStb

#define STB_TEXT_HAS_SELECTION(s)   ((s)->select_start != (s)->select_end)

namespace fox::imgui
{
    struct config
    {
        const char* window_name = "Console Window";
    };

    struct state
    {
        std::array<char, 128> input_buffer;
        std::vector<std::string> buffer{ "test", {"sussybaja====                  =========\n========                        ================================="} };
    };


    void render_text_wrapped_colorful(ImVec2 pos, const char* text, const char* text_end, float wrap_width)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;

        if (!text_end)
            text_end = text + strlen(text); // FIXME-OPT

        if (text != text_end)
        {
            window->DrawList->AddText(g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_end, wrap_width);
            if (g.LogEnabled)
                ImGui::LogRenderedText(&pos, text, text_end);
        }
    }

    void render_text_colorful(ImVec2 pos, const char* text, const char* text_end, bool hide_text_after_hash)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;

        // Hide anything after a '##' string
        const char* text_display_end;
        if (hide_text_after_hash)
        {
            text_display_end = ImGui::FindRenderedTextEnd(text, text_end);
        }
        else
        {
            if (!text_end)
                text_end = text + strlen(text); // FIXME-OPT
            text_display_end = text_end;
        }

        if (text != text_display_end)
        {
            window->DrawList->AddText(g.Font, g.FontSize, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_display_end);
            if (g.LogEnabled)
                ImGui::LogRenderedText(&pos, text, text_display_end);
        }
    }

    bool InputTextEx(const char* string)
    {
        auto ImVec2_add = [](const ImVec2& lhs, const ImVec2& rhs) -> ImVec2
            {
                return ImVec2{ lhs.x + rhs.x, lhs.y + rhs.y };
            };

        auto ImVec2_sub = [](const ImVec2& lhs, const ImVec2& rhs) -> ImVec2
            {
                return ImVec2{ lhs.x - rhs.x, lhs.y - rhs.y };
            };

        using namespace ImGui;

        ImVec2 size_arg = ImVec2(0, 0);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_Multiline | ImGuiInputTextFlags_ReadOnly;

        std::string buf_storage = string;
        char* buf = std::data(buf_storage);
        int buf_size = std::size(buf_storage) + 1;

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        ImGuiIO& io = g.IO;
        const ImGuiStyle& style = g.Style;

        const bool RENDER_SELECTION_WHEN_INACTIVE = false;
        const bool is_multiline = (flags & ImGuiInputTextFlags_Multiline) != 0;

        if (is_multiline) // Open group before calling GetID() because groups tracks id created within their scope (including the scrollbar)
            BeginGroup();

        const ImGuiID id = window->GetID(string);

        int num_lines = 1;
        {
            auto ptr = buf;

            while ((ptr = strchr(ptr, '\n')) != nullptr)
            {
                num_lines++;
                ptr++;
            }

            if (buf_size != 0 && buf[buf_size - 1] == '\n')
                num_lines -= 1;
        }

        const ImVec2 frame_size = CalcItemSize(size_arg, CalcItemWidth(), (is_multiline ? g.FontSize * static_cast<float>(num_lines) : 0)); // Arbitrary default of 8 lines high for multi-line
        const ImVec2 total_size = ImVec2(frame_size.x, frame_size.y);

        const ImRect frame_bb(window->DC.CursorPos, ImVec2_add(window->DC.CursorPos, frame_size));
        const ImRect total_bb(frame_bb.Min, ImVec2_add(frame_bb.Min, total_size));

        ImGuiWindow* draw_window = window;
        ImVec2 inner_size = frame_size;
        ImGuiLastItemData item_data_backup;
        if (is_multiline)
        {
            ImVec2 backup_pos = window->DC.CursorPos;
            ItemSize(total_bb);
            if (!ItemAdd(total_bb, id, &frame_bb))
            {
                EndGroup();
                return false;
            }
            item_data_backup = g.LastItemData;
            window->DC.CursorPos = backup_pos;

            if (g.ActiveId == id) // Prevent reactivation
                g.NavActivateId = 0;

            draw_window = g.CurrentWindow; // Child window
            draw_window->DC.NavLayersActiveMaskNext |= (1 << draw_window->DC.NavLayerCurrent); // This is to ensure that EndChild() will display a navigation highlight so we can "enter" into it.
            inner_size.x -= draw_window->ScrollbarSizes.x;
        }
        else
        {
            // Support for internal ImGuiInputTextFlags_MergedItem flag, which could be redesigned as an ItemFlags if needed (with test performed in ItemAdd)
            ItemSize(total_bb);
            if (!(flags & ImGuiInputTextFlags_MergedItem))
                if (!ItemAdd(total_bb, id, &frame_bb))
                    return false;
        }
        const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.InFlags);
        if (hovered)
            g.MouseCursor = ImGuiMouseCursor_TextInput;

        // We are only allowed to access the state if we are already the active widget.
        ImGuiInputTextState* state = GetInputTextState(id);

        if (g.LastItemData.InFlags & ImGuiItemFlags_ReadOnly)
            flags |= ImGuiInputTextFlags_ReadOnly;

        const bool is_readonly = true;
        const bool input_requested_by_nav = (g.ActiveId != id) && ((g.NavActivateId == id) && ((g.NavActivateFlags & ImGuiActivateFlags_PreferInput) || (g.NavInputSource == ImGuiInputSource_Keyboard)));

        const bool user_clicked = hovered && io.MouseClicked[0];
        const bool user_scroll_finish = is_multiline && state != NULL && g.ActiveId == 0 && g.ActiveIdPreviousFrame == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);
        const bool user_scroll_active = is_multiline && state != NULL && g.ActiveId == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);
        bool clear_active_id = false;
        bool select_all = false;

        const bool init_reload_from_user_buf = (state != NULL && state->ReloadUserBuf);
        const bool init_changed_specs = (state != NULL && state->Stb.single_line != !is_multiline); // state != NULL means its our state.
        const bool init_make_active = (user_clicked || user_scroll_finish || input_requested_by_nav);
        const bool init_state = (init_make_active || user_scroll_active);

        if ((init_state && g.ActiveId != id) || init_changed_specs || init_reload_from_user_buf)
        {
            // Access state even if we don't own it yet.
            state = &g.InputTextState;
            state->CursorAnimReset();
            state->ReloadUserBuf = false;

            // Backup state of deactivating item so they'll have a chance to do a write to output buffer on the same frame they report IsItemDeactivatedAfterEdit (#4714)
            InputTextDeactivateHook(state->ID);

            // From the moment we focused we are normally ignoring the content of 'buf' (unless we are in read-only mode)
            const int buf_len = (int)strlen(buf);
            if (!init_reload_from_user_buf)
            {
                // Take a copy of the initial buffer value.
                state->InitialTextA.resize(buf_len + 1);    // UTF-8. we use +1 to make sure that .Data is always pointing to at least an empty string.
                memcpy(state->InitialTextA.Data, buf, buf_len + 1);
            }

            // Preserve cursor position and undo/redo stack if we come back to same widget
            // FIXME: Since we reworked this on 2022/06, may want to differentiate recycle_cursor vs recycle_undostate?
            bool recycle_state = (state->ID == id && !init_changed_specs && !init_reload_from_user_buf);
            if (recycle_state && (state->CurLenA != buf_len || (state->TextAIsValid && strncmp(state->TextA.Data, buf, buf_len) != 0)))
                recycle_state = false;

            // Start edition
            const char* buf_end = NULL;
            state->ID = id;
            state->TextW.resize(buf_size + 1);          // wchar count <= UTF-8 count. we use +1 to make sure that .Data is always pointing to at least an empty string.
            state->TextA.resize(0);
            state->TextAIsValid = false;                // TextA is not valid yet (we will display buf until then)
            state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, buf_size, buf, NULL, &buf_end);
            state->CurLenA = (int)(buf_end - buf);      // We can't get the result from ImStrncpy() above because it is not UTF-8 aware. Here we'll cut off malformed UTF-8.

            if (recycle_state)
            {
                state->CursorClamp();
            }
            else
            {
                state->ScrollX = 0.0f;
                stb_textedit_initialize_state(&state->Stb, !is_multiline);
            }

            if (init_reload_from_user_buf)
            {
                state->Stb.select_start = state->ReloadSelectionStart;
                state->Stb.cursor = state->Stb.select_end = state->ReloadSelectionEnd;
                state->CursorClamp();
            }
            else if (!is_multiline)
            {
                if (flags & ImGuiInputTextFlags_AutoSelectAll)
                    select_all = true;
                if (input_requested_by_nav && (!recycle_state || !(g.NavActivateFlags & ImGuiActivateFlags_TryToPreserveState)))
                    select_all = true;
                if (user_clicked && io.KeyCtrl)
                    select_all = true;
            }

            if (flags & ImGuiInputTextFlags_AlwaysOverwrite)
                state->Stb.insert_mode = 1; // stb field name is indeed incorrect (see #2863)
        }

        const bool is_osx = io.ConfigMacOSXBehaviors;
        if (g.ActiveId != id && init_make_active)
        {
            IM_ASSERT(state && state->ID == id);
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
        }
        if (g.ActiveId == id)
        {
            // Declare some inputs, the other are registered and polled via Shortcut() routing system.
            if (user_clicked)
                SetKeyOwner(ImGuiKey_MouseLeft, id);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            if (is_multiline || (flags & ImGuiInputTextFlags_CallbackHistory))
                g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);

            // FIXME: May be a problem to always steal Alt on OSX, would ideally still allow an uninterrupted Alt down-up to toggle menu
            if (is_osx)
                SetKeyOwner(ImGuiMod_Alt, id);
        }

        // We have an edge case if ActiveId was set through another widget (e.g. widget being swapped), clear id immediately (don't wait until the end of the function)
        if (g.ActiveId == id && state == NULL)
            ClearActiveID();

        // Release focus when we click outside
        if (g.ActiveId == id && io.MouseClicked[0] && !init_state && !init_make_active) //-V560
            clear_active_id = true;

        // Lock the decision of whether we are going to take the path displaying the cursor or selection
        bool render_selection = state && (state->HasSelection() || select_all);

        // When read-only we always use the live data passed to the function
        // FIXME-OPT: Because our selection/cursor code currently needs the wide text we need to convert it when active, which is not ideal :(
        if (state != NULL && (render_selection))
        {
            const char* buf_end = NULL;
            state->TextW.resize(buf_size + 1);
            state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, buf, NULL, &buf_end);
            state->CurLenA = (int)(buf_end - buf);
            state->CursorClamp();
            render_selection &= state->HasSelection();
        }

        // Select the buffer to render.
        const bool buf_display_from_state = (render_selection || g.ActiveId == id) && !is_readonly;

        // Process mouse inputs and character inputs
        if (g.ActiveId == id)
        {
            IM_ASSERT(state != NULL);
            state->Edited = false;
            state->BufCapacityA = buf_size;
            state->Flags = flags;

            // Although we are active we don't prevent mouse from hovering other elements unless we are interacting right now with the widget.
            // Down the line we should have a cleaner library-wide concept of Selected vs Active.
            g.ActiveIdAllowOverlap = !io.MouseDown[0];

            // Edit in progress
            const float mouse_x = (io.MousePos.x - frame_bb.Min.x) + state->ScrollX;
            const float mouse_y = (is_multiline ? (io.MousePos.y - draw_window->DC.CursorPos.y) : (g.FontSize * 0.5f));

            if (select_all)
            {
                state->SelectAll();
                state->SelectedAllMouseLock = true;
            }
            else if (hovered && io.MouseClickedCount[0] >= 2 && !io.KeyShift)
            {
                stb_textedit_click(state, &state->Stb, mouse_x, mouse_y);
                const int multiclick_count = (io.MouseClickedCount[0] - 2);
                if ((multiclick_count % 2) == 0)
                {
                    // Double-click: Select word
                    // We always use the "Mac" word advance for double-click select vs CTRL+Right which use the platform dependent variant:
                    // FIXME: There are likely many ways to improve this behavior, but there's no "right" behavior (depends on use-case, software, OS)
                    const bool is_bol = (state->Stb.cursor == 0) || ImStb::STB_TEXTEDIT_GETCHAR(state, state->Stb.cursor - 1) == '\n';
                    if (STB_TEXT_HAS_SELECTION(&state->Stb) || !is_bol)
                        state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT);
                    //state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
                    if (!STB_TEXT_HAS_SELECTION(&state->Stb))
                        ImStb::stb_textedit_prep_selection_at_cursor(&state->Stb);
                    state->Stb.cursor = ImStb::STB_TEXTEDIT_MOVEWORDRIGHT_MAC(state, state->Stb.cursor);
                    state->Stb.select_end = state->Stb.cursor;
                    ImStb::stb_textedit_clamp(state, &state->Stb);
                }
                else
                {
                    // Triple-click: Select line
                    const bool is_eol = ImStb::STB_TEXTEDIT_GETCHAR(state, state->Stb.cursor) == '\n';
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART);
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINEEND | STB_TEXTEDIT_K_SHIFT);
                    state->OnKeyPressed(STB_TEXTEDIT_K_RIGHT | STB_TEXTEDIT_K_SHIFT);
                    if (!is_eol && is_multiline)
                    {
                        ImSwap(state->Stb.select_start, state->Stb.select_end);
                        state->Stb.cursor = state->Stb.select_end;
                    }
                    state->CursorFollow = false;
                }
                state->CursorAnimReset();
            }
            else if (io.MouseClicked[0] && !state->SelectedAllMouseLock)
            {
                if (hovered)
                {
                    if (io.KeyShift)
                        stb_textedit_drag(state, &state->Stb, mouse_x, mouse_y);
                    else
                        stb_textedit_click(state, &state->Stb, mouse_x, mouse_y);
                    state->CursorAnimReset();
                }
            }
            else if (io.MouseDown[0] && !state->SelectedAllMouseLock && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
            {
                stb_textedit_drag(state, &state->Stb, mouse_x, mouse_y);
                state->CursorAnimReset();
                state->CursorFollow = true;
            }

            if (state->SelectedAllMouseLock && !io.MouseDown[0])
                state->SelectedAllMouseLock = false;


            if (io.InputQueueCharacters.Size > 0)
            {
                // Consume characters
                io.InputQueueCharacters.resize(0);
            }
        }

        // Process other shortcuts/key-presses
        if (g.ActiveId == id && !g.ActiveIdIsJustActivated && !clear_active_id)
        {
            IM_ASSERT(state != NULL);

            const int row_count_per_page = ImMax((int)((inner_size.y) / g.FontSize), 1);
            state->Stb.row_count_per_page = row_count_per_page;

            const bool is_select_all = Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, 0, id);

            // FIXME: Should use more Shortcut() and reduce IsKeyPressed()+SetKeyOwner(), but requires modifiers combination to be taken account of.
            // FIXME-OSX: Missing support for Alt(option)+Right/Left = go to end of line, or next line if already in end of line.
            if (is_select_all)
            {
                state->SelectAll();
                state->CursorFollow = true;
            }

            // Update render selection flag after events have been handled, so selection highlight can be displayed during the same frame.
            render_selection |= state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE);
        }

        // Process callbacks and apply result back to user's buffer.
        const char* apply_new_text = NULL;
        int apply_new_text_length = 0;

        // Handle reapplying final data on deactivation (see InputTextDeactivateHook() for details)
        if (g.InputTextDeactivatedState.ID == id)
        {
            g.InputTextDeactivatedState.ID = 0;
        }

        // Copy result to user buffer. This can currently only happen when (g.ActiveId == id)
        if (apply_new_text != nullptr)
        {
            // We cannot test for 'backup_current_text_length != apply_new_text_length' here because we have no guarantee that the size
            // of our owned buffer matches the size of the string object held by the user, and by design we allow InputText() to be used
            // without any storage on user's side.
            IM_ASSERT(apply_new_text_length >= 0);

            // If the underlying buffer resize was denied or not carried to the next frame, apply_new_text_length+1 may be >= buf_size.
            ImStrncpy(buf, apply_new_text, ImMin(apply_new_text_length + 1, buf_size));
        }

        // Release active ID at the end of the function (so e.g. pressing Return still does a final application of the value)
        // Otherwise request text input ahead for next frame.
        if (g.ActiveId == id && clear_active_id)
            ClearActiveID();
        else if (g.ActiveId == id)
            g.WantTextInputNextFrame = 1;

        // Render frame
        if (!is_multiline)
        {
            RenderNavHighlight(frame_bb, id);
            RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
        }

        const ImVec4 clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + inner_size.x, frame_bb.Min.y + inner_size.y); // Not using frame_bb.Max because we have adjusted size
        ImVec2 draw_pos = is_multiline ? draw_window->DC.CursorPos : frame_bb.Min;
        ImVec2 text_size(0.0f, 0.0f);

        // Set upper limit of single-line InputTextEx() at 2 million characters strings. The current pathological worst case is a long line
        // without any carriage return, which would makes ImFont::RenderText() reserve too many vertices and probably crash. Avoid it altogether.
        // Note that we only use this limit on single-line InputText(), so a pathologically large line on a InputTextMultiline() would still crash.
        const int buf_display_max_length = 2 * 1024 * 1024;
        const char* buf_display = buf_display_from_state ? state->TextA.Data : buf; //-V595
        const char* buf_display_end = NULL; // We have specialized paths below for setting the length

        // Render text. We currently only render selection when the widget is active or while scrolling.
        // FIXME: We could remove the '&& render_cursor' to keep rendering selection when inactive.
        if (render_selection)
        {
            IM_ASSERT(state != NULL);
            buf_display_end = buf_display + state->CurLenA;

            // Render text (with cursor and selection)
            // This is going to be messy. We need to:
            // - Display the text (this alone can be more easily clipped)
            // - Handle scrolling, highlight selection, display cursor (those all requires some form of 1d->2d cursor position calculation)
            // - Measure text height (for scrollbar)
            // We are attempting to do most of that in **one main pass** to minimize the computation cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we could merge them by an extra refactoring effort)
            // FIXME: This should occur on buf_display but we'd need to maintain cursor/select_start/select_end for UTF-8.
            const ImWchar* text_begin = state->TextW.Data;
            ImVec2 cursor_offset, select_start_offset;

            {
                // Find lines numbers straddling 'cursor' (slot 0) and 'select_start' (slot 1) positions.
                const ImWchar* searches_input_ptr[2] = { NULL, NULL };
                int searches_result_line_no[2] = { -1000, -1000 };
                int searches_remaining = 0;

                searches_input_ptr[1] = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
                searches_result_line_no[1] = -1;
                searches_remaining++;

                // Iterate all lines to find our line numbers
                // In multi-line mode, we never exit the loop until all lines are counted, so add one extra to the searches_remaining counter.
                searches_remaining += is_multiline ? 1 : 0;
                int line_count = 0;
                //for (const ImWchar* s = text_begin; (s = (const ImWchar*)wcschr((const wchar_t*)s, (wchar_t)'\n')) != NULL; s++)  // FIXME-OPT: Could use this when wchar_t are 16-bit
                for (const ImWchar* s = text_begin; *s != 0; s++)
                    if (*s == '\n')
                    {
                        line_count++;
                        if (searches_result_line_no[0] == -1 && s >= searches_input_ptr[0]) { searches_result_line_no[0] = line_count; if (--searches_remaining <= 0) break; }
                        if (searches_result_line_no[1] == -1 && s >= searches_input_ptr[1]) { searches_result_line_no[1] = line_count; if (--searches_remaining <= 0) break; }
                    }
                line_count++;
                if (searches_result_line_no[0] == -1)
                    searches_result_line_no[0] = line_count;
                if (searches_result_line_no[1] == -1)
                    searches_result_line_no[1] = line_count;

                // Calculate 2d position by finding the beginning of the line and measuring distance
                cursor_offset.x = InputTextCalcTextSizeW(&g, ImStrbolW(searches_input_ptr[0], text_begin), searches_input_ptr[0]).x;
                cursor_offset.y = searches_result_line_no[0] * g.FontSize;
                if (searches_result_line_no[1] >= 0)
                {
                    select_start_offset.x = InputTextCalcTextSizeW(&g, ImStrbolW(searches_input_ptr[1], text_begin), searches_input_ptr[1]).x;
                    select_start_offset.y = searches_result_line_no[1] * g.FontSize;
                }

                // Store text height (note that we haven't calculated text width at all, see GitHub issues #383, #1224)
                if (is_multiline)
                    text_size = ImVec2(inner_size.x, line_count * g.FontSize);
            }

            // Draw selection
            const ImVec2 draw_scroll = ImVec2(state->ScrollX, 0.0f);
            {
                const ImWchar* text_selected_begin = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
                const ImWchar* text_selected_end = text_begin + ImMax(state->Stb.select_start, state->Stb.select_end);

                ImU32 bg_color = GetColorU32(ImGuiCol_TextSelectedBg, 0.6f); // FIXME: current code flow mandate that render_cursor is always true here, we are leaving the transparent one for tests.
                float bg_offy_up = is_multiline ? 0.0f : -1.0f;    // FIXME: those offsets should be part of the style? they don't play so well with multi-line selection.
                float bg_offy_dn = is_multiline ? 0.0f : 2.0f;
                ImVec2 rect_pos = ImVec2_sub(ImVec2_add(draw_pos, select_start_offset), draw_scroll);
                for (const ImWchar* p = text_selected_begin; p < text_selected_end; )
                {
                    if (rect_pos.y > clip_rect.w + g.FontSize)
                        break;
                    if (rect_pos.y < clip_rect.y)
                    {
                        //p = (const ImWchar*)wmemchr((const wchar_t*)p, '\n', text_selected_end - p);  // FIXME-OPT: Could use this when wchar_t are 16-bit
                        //p = p ? p + 1 : text_selected_end;
                        while (p < text_selected_end)
                            if (*p++ == '\n')
                                break;
                    }
                    else
                    {
                        ImVec2 rect_size = InputTextCalcTextSizeW(&g, p, text_selected_end, &p, NULL, true);
                        if (rect_size.x <= 0.0f) rect_size.x = IM_TRUNC(g.Font->GetCharAdvance((ImWchar)' ') * 0.50f); // So we can see selected empty lines
                        ImRect rect(ImVec2_add(rect_pos, ImVec2(0.0f, bg_offy_up - g.FontSize)), ImVec2_add(rect_pos, ImVec2(rect_size.x, bg_offy_dn)));
                        rect.ClipWith(clip_rect);
                        if (rect.Overlaps(clip_rect))
                            draw_window->DrawList->AddRectFilled(rect.Min, rect.Max, bg_color);
                    }
                    rect_pos.x = draw_pos.x - draw_scroll.x;
                    rect_pos.y += g.FontSize;
                }
            }

            // We test for 'buf_display_max_length' as a way to avoid some pathological cases (e.g. single-line 1 MB string) which would make ImDrawList crash.
            if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
            {
                ImU32 col = GetColorU32(ImGuiCol_Text);
                draw_window->DrawList->AddText(g.Font, g.FontSize, ImVec2_sub(draw_pos, draw_scroll), col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
            }
        }
        else
        {
            // Render text only (no selection, no cursor)
            if (is_multiline)
                text_size = ImVec2(inner_size.x, InputTextCalcTextLenAndLineCount(buf_display, &buf_display_end) * g.FontSize); // We don't need width
            else if (g.ActiveId == id)
                buf_display_end = buf_display + state->CurLenA;
            else
                buf_display_end = buf_display + strlen(buf_display);

            if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
            {
                ImU32 col = GetColorU32(ImGuiCol_Text);
                draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
            }
        }

        if (is_multiline)
        {
            // For focus requests to work on our multiline we need to ensure our child ItemAdd() call specifies the ImGuiItemFlags_Inputable (ref issue #4761)...
            Dummy(ImVec2(text_size.x, text_size.y));
            g.NextItemData.ItemFlags |= ImGuiItemFlags_Inputable | ImGuiItemFlags_NoTabStop;

            item_data_backup.StatusFlags |= (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredWindow);

            // ...and then we need to undo the group overriding last item data, which gets a bit messy as EndGroup() tries to forward scrollbar being active...
            // FIXME: This quite messy/tricky, should attempt to get rid of the child window.
            EndGroup();
            if (g.LastItemData.ID == 0)
            {
                g.LastItemData.ID = id;
                g.LastItemData.InFlags = item_data_backup.InFlags;
                g.LastItemData.StatusFlags = item_data_backup.StatusFlags;
            }
        }

        // Log as text
        if (g.LogEnabled)
        {
            LogSetNextTextDecoration("{", "}");
            LogRenderedText(&draw_pos, buf_display, buf_display_end);
        }
    }

    inline void console_window(state& state, bool* open = nullptr, const config& cfg = config())
    {
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(cfg.window_name, open, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
        {
            // if (ImGui::BeginPopupContextWindow())
            // {
            //     if (ImGui::Selectable("Clear")) ClearLog();
            //     ImGui::EndPopup();
            // }

            bool ScrollToBottom = false;
            bool AutoScroll = false;

            // Display every line as a separate entry so we can change their color or add custom widgets.
            // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
            // NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
            // to only process visible items. The clipper will automatically measure the height of your first item and then
            // "seek" to display only items in the visible area.
            // To use the clipper we can replace your standard loop:
            //      for (int i = 0; i < Items.Size; i++)
            //   With:
            //      ImGuiListClipper clipper;
            //      clipper.Begin(Items.Size);
            //      while (clipper.Step())
            //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            // - That your items are evenly spaced (same height)
            // - That you have cheap random access to your elements (you can access them given their index,
            //   without processing all the ones before)
            // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
            // We would need random-access on the post-filtered list.
            // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
            // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
            // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
            // to improve this example code!
            // If your items are of variable height:
            // - Split them into same height items would be simpler and facilitate random-seeking into your list.
            // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
            // if (copy_to_clipboard)
                // ImGui::LogToClipboard();

            ImGui::TextUnformatted("TestA\nTestB\nTestC");

            for (auto&& item : state.buffer)
            {
                // Normally you would store more information in your item than just a string.
                // (e.g. make Items[] an array of structure, store color/type etc.)
                ImVec4 color;
                bool has_color = false;
                if (strstr(item.c_str(), "[error]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
                else if (strncmp(item.c_str(), "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
                if (has_color)
                    ImGui::PushStyleColor(ImGuiCol_Text, color);

                InputTextEx(item.c_str());

                if (has_color)
                    ImGui::PopStyleColor();
            }
            // if (copy_to_clipboard)
               //  ImGui::LogFinish();

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
