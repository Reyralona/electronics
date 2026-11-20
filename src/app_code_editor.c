#include "app_code_editor.h"
#include "app_text_utf8.h"
#include "code_text.h"
#include "mcu_board.h"
#include "workbench_history.h"
#include "bench_scope.h"
#include "ui_text.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CODE_TEXT_PADDING 6.0f
#define CODE_WHEEL_LINES 3
#define CODE_PAGE_LINES 16
#define CODE_POPUP_WIDTH 300.0f
#define CODE_POPUP_ROW 22.0f
#define BYTES_PER_KILOBYTE 1024

int app_code_visible_lines(void)
{
    return (int)((app_layout_code_text().h - CODE_TEXT_PADDING) / APP_CODE_LINE_HEIGHT);
}

float app_code_advance(void)
{
    const char *sample = "0123456789";
    app_text_utf8_set_monospace(1);
    int width = app_text_utf8_width(sample, 4096);
    app_text_utf8_set_monospace(0);
    return width > 0 ? width / 10.0f : 8.0f;
}

/* Tabs become indent spaces so columns and the fixed-pitch drawing agree. */
static int load_draft(AppState *app, const char *source)
{
    size_t used = 0;
    for (const char *c = source; *c; ++c)
    {
        if (*c == '\t')
            used += CODE_INDENT;
        else if (*c != '\r')
            ++used;
        if (used >= sizeof(app->code_text))
            return 0;
    }
    used = 0;
    for (const char *c = source; *c; ++c)
    {
        if (*c == '\t')
        {
            memset(app->code_text + used, ' ', CODE_INDENT);
            used += CODE_INDENT;
        }
        else if (*c != '\r')
            app->code_text[used++] = *c;
    }
    app->code_text[used] = 0;
    return 1;
}

void app_code_open(AppState *app, SDL_Window *window, int part)
{
    if (!app_code_editable(app, part))
        return;
    if (app->code_part >= 0 && app->code_part != part && !app_code_close(app, window))
        return;
    if (app->code_part != part)
    {
        if (!load_draft(app, mcu_program_text(&app->bench.parts[part])))
        {
            app->status = tr(TXT_MCU_CODE_TOO_LONG);
            return;
        }
        app->code_cursor = 0;
        app->code_scroll = 0;
        app->code_dirty = 0;
    }
    app->code_part = part;
    app->completion_selected = 0;
    app->completion_hidden = 1;
    app->settings = app->projects_open = app->examples_open = 0;
    if (window)
        SDL_StartTextInput(window);
    app->status = tr(TXT_MCU_EDITOR_KEYS);
}

void app_code_validate(AppState *app, SDL_Window *window)
{
    if (app->code_part < 0)
        return;
    if (app_code_editable(app, app->code_part))
    {
        if (app->editing < 0 && !SDL_TextInputActive(window))
            SDL_StartTextInput(window); /* opened from a double click, which has no window */
        return;
    }
    app->code_part = -1;
    app->code_dirty = 0;
    SDL_StopTextInput(window);
    app->status = tr(TXT_MCU_BOARD_GONE);
}

int app_code_completions(const AppState *app, int *matches, int capacity)
{
    if (app->code_part < 0 || app->completion_hidden)
        return 0;
    return code_completions(app->code_text, app->code_cursor, matches, capacity);
}

AppRect app_code_completion_item(const AppState *app, int index)
{
    AppRect area = app_layout_code_text();
    int line = code_line_index(app->code_text, app->code_cursor) - app->code_scroll;
    float x = area.x + APP_CODE_GUTTER + CODE_TEXT_PADDING +
              code_column(app->code_text, code_word_start(app->code_text, app->code_cursor)) * app_code_advance();
    x = fminf(x, area.x + area.w - CODE_POPUP_WIDTH);
    float y = area.y + CODE_TEXT_PADDING + (line + 1) * APP_CODE_LINE_HEIGHT + 2;
    if (y + CODE_COMPLETION_MAX * CODE_POPUP_ROW > area.y + area.h + 90)
        y -= (CODE_COMPLETION_MAX + 1) * CODE_POPUP_ROW;
    return (AppRect){x, y + index * CODE_POPUP_ROW, CODE_POPUP_WIDTH, CODE_POPUP_ROW};
}

static void keep_cursor_visible(AppState *app)
{
    int line = code_line_index(app->code_text, app->code_cursor), visible = app_code_visible_lines();
    if (line < app->code_scroll)
        app->code_scroll = line;
    if (line >= app->code_scroll + visible)
        app->code_scroll = line - visible + 1;
}

static void scroll_by(AppState *app, int lines)
{
    int last = code_line_count(app->code_text) - app_code_visible_lines();
    app->code_scroll += lines;
    if (app->code_scroll > last)
        app->code_scroll = last;
    if (app->code_scroll < 0)
        app->code_scroll = 0;
}

static void edited(AppState *app, int changed)
{
    if (!changed)
    {
        int kilobytes = (int)(sizeof(app->code_text) / BYTES_PER_KILOBYTE);
        snprintf(app->selection_status, sizeof(app->selection_status), tr(TXT_MCU_CODE_FULL), kilobytes);
        app->status = app->selection_status;
        return;
    }
    app->code_dirty = 1;
    app->completion_selected = 0;
    keep_cursor_visible(app);
}

static void move_vertical(AppState *app, int lines)
{
    int line = code_line_index(app->code_text, app->code_cursor) + lines;
    int column = code_column(app->code_text, app->code_cursor);
    int last = code_line_count(app->code_text) - 1;
    line = line < 0 ? 0 : line > last ? last : line;
    app->code_cursor = code_position(app->code_text, line, column);
}

static int accept_completion(AppState *app)
{
    int matches[CODE_COMPLETION_MAX];
    int count = app_code_completions(app, matches, CODE_COMPLETION_MAX);
    if (!count)
        return 0;
    int chosen = app->completion_selected < count ? app->completion_selected : 0;
    edited(app, code_accept(app->code_text, sizeof(app->code_text), &app->code_cursor, matches[chosen]));
    app->completion_hidden = 1;
    return 1;
}

static int completion_navigation(AppState *app, SDL_Keycode key)
{
    int matches[CODE_COMPLETION_MAX];
    int count = app_code_completions(app, matches, CODE_COMPLETION_MAX);
    if (!count)
        return 0;
    if (key == SDLK_UP || key == SDLK_DOWN)
    {
        app->completion_selected = (app->completion_selected + (key == SDLK_DOWN ? 1 : count - 1)) % count;
        return 1;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_TAB)
        return accept_completion(app);
    if (key == SDLK_ESCAPE)
    {
        app->completion_hidden = 1;
        return 1;
    }
    return 0;
}

static void cursor_key(AppState *app, SDL_Keycode key, SDL_Keymod mod)
{
    char *text = app->code_text;
    if (key == SDLK_LEFT)
        app->code_cursor = code_previous(text, app->code_cursor);
    else if (key == SDLK_RIGHT)
        app->code_cursor = code_next(text, app->code_cursor);
    else if (key == SDLK_UP || key == SDLK_DOWN)
        move_vertical(app, key == SDLK_UP ? -1 : 1);
    else if (key == SDLK_PAGEUP || key == SDLK_PAGEDOWN)
        move_vertical(app, key == SDLK_PAGEUP ? -CODE_PAGE_LINES : CODE_PAGE_LINES);
    else if (key == SDLK_HOME)
        app->code_cursor = (mod & SDL_KMOD_CTRL) ? 0 : code_line_start(text, app->code_cursor);
    else if (key == SDLK_END)
        app->code_cursor = (mod & SDL_KMOD_CTRL) ? (int)strlen(text) : code_line_end(text, app->code_cursor);
    else
        return;
    app->completion_hidden = 1;
    keep_cursor_visible(app);
}

static void editing_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod)
{
    char *text = app->code_text;
    size_t capacity = sizeof(app->code_text);
    if ((mod & SDL_KMOD_CTRL) && (key == SDLK_RETURN || key == SDLK_KP_ENTER))
    {
        if (!app_code_apply(app))
            app->status = tr(TXT_MCU_NO_CHANGES);
        return;
    }
    if ((mod & SDL_KMOD_CTRL) && key == SDLK_SPACE)
    {
        app->completion_hidden = 0;
        app->completion_selected = 0;
        return;
    }
    if (completion_navigation(app, key))
        return;
    if (key == SDLK_ESCAPE)
        app_code_close(app, window);
    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
        edited(app, code_newline(text, capacity, &app->code_cursor));
    else if (key == SDLK_TAB)
        edited(app, code_insert(text, capacity, &app->code_cursor, "    "));
    else if (key == SDLK_BACKSPACE)
    {
        code_backspace(text, &app->code_cursor);
        edited(app, 1);
    }
    else if (key == SDLK_DELETE)
    {
        code_delete(text, app->code_cursor);
        edited(app, 1);
    }
    else
        cursor_key(app, key, mod);
}

static void text_input(AppState *app, const char *input)
{
    char filtered[64];
    size_t used = 0;
    for (const char *c = input; *c && used + 1 < sizeof(filtered); ++c)
        if ((unsigned char)*c >= 32) /* printable ASCII and UTF-8 bytes */
            filtered[used++] = *c;
    filtered[used] = 0;
    if (!used)
        return;
    int changed = !strcmp(filtered, "}") ? code_close_brace(app->code_text, sizeof(app->code_text), &app->code_cursor)
                                         : code_insert(app->code_text, sizeof(app->code_text), &app->code_cursor, filtered);
    edited(app, changed);
    app->completion_hidden = !(isalnum((unsigned char)filtered[used - 1]) || filtered[used - 1] == '_' ||
                               filtered[used - 1] == '.');
}

static void place_cursor(AppState *app, float x, float y)
{
    AppRect area = app_layout_code_text();
    int line = app->code_scroll + (int)floorf((y - area.y - CODE_TEXT_PADDING) / APP_CODE_LINE_HEIGHT);
    int last = code_line_count(app->code_text) - 1;
    float column = (x - area.x - APP_CODE_GUTTER - CODE_TEXT_PADDING) / app_code_advance();
    line = line < 0 ? 0 : line > last ? last : line;
    app->code_cursor = code_position(app->code_text, line, column < 0 ? 0 : (int)lroundf(column));
    app->completion_hidden = 1;
}

static int panel_click(AppState *app, SDL_Window *window, float x, float y)
{
    int matches[CODE_COMPLETION_MAX];
    int count = app_code_completions(app, matches, CODE_COMPLETION_MAX);
    for (int i = 0; i < count; ++i)
        if (app_rect_contains(app_code_completion_item(app, i), x, y))
        {
            app->completion_selected = i;
            accept_completion(app);
            return 1;
        }
    if (!app_rect_contains(app_layout_code_panel(), x, y))
        return 0;
    if (app_rect_contains(app_layout_code_close(), x, y))
        app_code_close(app, window);
    else if (app_rect_contains(app_layout_code_run(), x, y))
    {
        if (!app_code_apply(app))
            app->status = tr(TXT_MCU_NO_CHANGES);
    }
    else if (app_rect_contains(app_layout_code_text(), x, y))
        place_cursor(app, x, y);
    return 1;
}

static void interface_point(SDL_Window *window, float window_x, float window_y, float *x, float *y)
{
    int width = 1, height = 1;
    SDL_GetWindowSize(window, &width, &height);
    WirePoint point = app_layout_to_ui(width, height, window_x, window_y);
    *x = point.x;
    *y = point.y;
}

int app_code_event(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    if (app->code_part < 0)
        return 0;
    float x, y;
    switch (event->type)
    {
    case SDL_EVENT_TEXT_INPUT:
        text_input(app, event->text.text);
        return 1;
    case SDL_EVENT_KEY_DOWN:
        editing_key(app, window, event->key.key, event->key.mod);
        return 1;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        interface_point(window, event->button.x, event->button.y, &x, &y);
        return event->button.button == SDL_BUTTON_LEFT ? panel_click(app, window, x, y)
                                                       : app_rect_contains(app_layout_code_panel(), x, y);
    case SDL_EVENT_MOUSE_WHEEL:
        interface_point(window, event->wheel.mouse_x, event->wheel.mouse_y, &x, &y);
        if (!app_rect_contains(app_layout_code_panel(), x, y))
            return 0;
        if (event->wheel.y != 0)
            scroll_by(app, event->wheel.y > 0 ? -CODE_WHEEL_LINES : CODE_WHEEL_LINES);
        return 1;
    default:
        return 0;
    }
}
