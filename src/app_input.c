#include "bench_scope.h"
#include "component_registry.h"
#include "workbench_grid.h"
#include "app_input.h"
#include "app_editor.h"
#include "workbench_group_name.h"
#include "app_commands.h"
#include "app_editing.h"
#include "app_packing.h"
#include "workbench_pack.h"
#include "app_layout.h"
#include "app_palette.h"
#include "workbench_file.h"
#include "workbench_examples.h"
#include "spice_trace.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int hit_wire(const Workbench *bench, float x, float y, float tolerance, int rounded)
{
    for (int i = bench->wire_count - 1; i >= 0; --i)
    {
        const Wire *wire = &bench->wires[i];
        if (bench_group_wire_internal(bench, wire))
            continue;
        WirePoint start, end, projected;
        int slot;
        bench_terminal_position(bench, wire->a, &start.x, &start.y);
        bench_terminal_position(bench, wire->b, &end.x, &end.y);
        if (wire_pick_segment(wire, start, end, rounded ? 18.0f : 0, (WirePoint){x, y}, tolerance, &slot, &projected))
            return i;
    }
    return -1;
}

static void input_insert_knee(AppState *app, float x, float y)
{
    int index = app->selected_wire, slot;
    const Wire *wire = &app->bench.wires[index];
    WirePoint start, end, projected;
    bench_terminal_position(&app->bench, wire->a, &start.x, &start.y);
    bench_terminal_position(&app->bench, wire->b, &end.x, &end.y);
    if (!wire_pick_segment(wire, start, end, app->round_wires ? 18.0f : 0,
                           (WirePoint){x, y}, 7.0f / app->view.zoom, &slot, &projected))
        return;
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
        return;
    }
    if (!bench_insert_knee(&app->bench, index, slot, projected))
    {
        app->status = "Nao foi possivel inserir: limite atual de 16 joelhos por fio.";
        return;
    }
    bench_history_record(app->history, &before);
    app->show_handles = 1;
    app->status = "Joelho adicionado. Arraste para dobrar; duplo clique remove; Ctrl+Z desfaz.";
}

static int part_at(const Workbench *bench, float x, float y, float zoom)
{
    for (int g = WORKBENCH_MAX_COMPONENTS - 1; g >= 0; --g)
        if (bench->groups[g].active && fabsf(x - bench->groups[g].x) < bench_group_width(bench, g + 1) * .5f + 11 &&
            fabsf(y - bench->groups[g].y) < bench_group_height(bench, g + 1) * .5f)
            return bench_group_first(bench, g + 1);

    int pass, i;
    for (pass = 0; pass < 2; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            const Part *part = &bench->parts[i];
            if (part->group)
                continue;
            int board = part->type == PART_BREADBOARD;
            if (board != pass)
                continue;
            WirePoint local = bench_part_to_local(part, (WirePoint){x, y});
            float half_width = 44, half_height = 30;
            if (board)
            {
                half_width = 227;
                half_height = 112;
            }
            else if (component_has_flag(part->type, COMPONENT_QUAD_GATE))
            {
                half_width = 50;
                half_height = 20;
            }
            float minimum_hit = 6 / (zoom * bench_part_scale(part));
            half_width = fmaxf(half_width, minimum_hit);
            half_height = fmaxf(half_height, minimum_hit);
            if (fabsf(local.x) < half_width && fabsf(local.y) < half_height)
                return i;
        }
    return -1;
}

static void input_text(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    (void)window;

    if (app->editing == APP_EDIT_GROUP_NAME)
    {
        const char *input = event->text.text;
        size_t before = app->edit_select_all ? 0 : strlen(app->edit_text);
        if ((!bench_group_name_valid(input) && strspn(input, " ") != strlen(input)) ||
            before + strlen(input) >= sizeof(app->edit_text))
        {
            app->status = "Nome invalido ou longo demais; limite atual de 127 bytes UTF-8.";
            return;
        }
        memcpy(app->edit_text + before, input, strlen(input) + 1);
        app->edit_select_all = 0;
        return;
    }
    const char *p = event->text.text;
    size_t length = strlen(app->edit_text);
    while (*p && length + 1 < sizeof(app->edit_text))
    {
        if ((unsigned char)*p >= 32 && (unsigned char)*p < 127)
            app->edit_text[length++] = *p;
        ++p;
    }
    app->edit_text[length] = '\0';
}

static void input_edit_key(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    if (app->editing == APP_EDIT_GROUP_NAME)
    {
        if (event->key.key == SDLK_ESCAPE)
        {
            app->editing = -1;
            app->name_target_group = -1;
            SDL_StopTextInput(window);
            app->status = "Renomeacao cancelada.";
        }
        else if (event->key.key == SDLK_A && (event->key.mod & SDL_KMOD_CTRL))
            app->edit_select_all = 1;
        else if (event->key.key == SDLK_BACKSPACE)
        {
            size_t length = strlen(app->edit_text);
            if (app->edit_select_all)
                length = 0;
            else if (length)
            {
                --length;
                while (length && ((unsigned char)app->edit_text[length] & 0xc0) == 0x80)
                    --length;
            }
            app->edit_text[length] = 0;
            app->edit_select_all = 0;
        }
        else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
        {
            if (app_editor_set_name(app, app->edit_text))
            {
                app->editing = -1;
                app->name_target_group = -1;
                SDL_StopTextInput(window);
            }
        }
        return;
    }
    if (event->key.key == SDLK_ESCAPE)
    {
        app->editing = -1;
        SDL_StopTextInput(window);
        app->status = "Edicao cancelada.";
    }
    else if (event->key.key == SDLK_BACKSPACE)
    {
        size_t length = strlen(app->edit_text);
        if (length)
            app->edit_text[length - 1] = '\0';
    }
    else if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
    {
        double value;
        Workbench before BENCH_AUTO_CLEAR = {0};
        if (!bench_snapshot_clone(&before, &app->bench))
        {
            app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
            return;
        }
        if (app->editing == APP_EDIT_SCALE)
        {
            if (app->selected >= 0 && bench_parse_value(app->edit_text, &value) &&
                bench_set_scale(&app->bench, app->selected, (float)(value / 100)))
            {
                if (before.revision != app->bench.revision)
                    bench_history_record(app->history, &before);
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = "Tamanho atualizado. Posicione os terminais nos furos desejados; Ctrl+Z desfaz.";
            }
            else
                app->status = "Use tamanho entre 1% e 10000%.";
        }
        else if (app->editing == APP_EDIT_GRID)
        {
            if (bench_parse_value(app->edit_text, &value) && value >= 1 && value <= 10000)
            {
                app->grid_spacing = value;
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = "Espacamento atualizado em unidades da montagem.";
            }
            else
                app->status = "Use espacamento de 1 a 10000 unidades.";
        }
        else if (app->editing >= WORKBENCH_MAX_COMPONENTS)
        {
            double step = app->transient_step, stop = app->transient_stop;
            int parsed = bench_parse_value(app->edit_text, &value);
            if (parsed && app->editing == WORKBENCH_MAX_COMPONENTS)
                step = value;
            else if (parsed)
                stop = value;
            if (parsed && step <= stop && stop / step <= 500.0 * (1.0 + 1e-12))
            {
                app->transient_step = step;
                app->transient_stop = stop;
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = "Parametro transitorio atualizado.";
            }
            else
                app->status = "Use tempos positivos: passo <= duracao e duracao/passo <= 500.";
        }
        else if (bench_parse_value(app->edit_text, &value) && bench_set_value(&app->bench, app->editing, value))
        {
            if (before.parts[app->editing].value != value)
                bench_history_record(app->history, &before);
            app->editing = -1;
            SDL_StopTextInput(window);
            app->status = "Valor alterado.";
        }
        else
            app->status = "Valor invalido. Use um numero positivo entre 1e-12 e 1e12.";
    }
}

static void input_zoom(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    float x, y, direction = event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event->wheel.y : event->wheel.y;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        x = event->wheel.mouse_x * 1280.0f / w;
        y = event->wheel.mouse_y * 720.0f / h;
        if (x < 220 && y >= 140 && direction != 0)
        {
            int last = palette_count(app->palette_page) - 8;
            app->palette_offset += direction < 0 ? 1 : -1;
            if (app->palette_offset < 0)
                app->palette_offset = 0;
            if (app->palette_offset > last)
                app->palette_offset = last;
            app->status = "Biblioteca: use o scroll para ver mais componentes.";
            return;
        }
        if (x >= 220 && y >= 70 && y < 660 && direction != 0 && (SDL_GetModState() & SDL_KMOD_CTRL))
        {
            if (app->selected >= 0 && app->bench.parts[app->selected].group)
            {
                int group = app->bench.parts[app->selected].group;
                Workbench before BENCH_AUTO_CLEAR = {0};
                if (!bench_snapshot_clone(&before, &app->bench))
                {
                    app->status = "Memoria insuficiente para redimensionar o chip.";
                    return;
                }
                float width = bench_group_width(&app->bench, group);
                float height = bench_group_height(&app->bench, group);
                float factor = direction > 0 ? 1.1f : 1 / 1.1f;
                if (SDL_GetModState() & SDL_KMOD_SHIFT)
                    height = fminf(10000, fmaxf(44, height * factor));
                else
                    width = fminf(10000, fmaxf(64, width * factor));
                if (!bench_group_resize(&app->bench, group, width, height))
                    app->status = "Dimensoes invalidas para o chip.";
                else if (!bench_history_record(app->history, &before))
                {
                    bench_replace(&app->bench, &before);
                    app->status = "Historico indisponivel; dimensoes anteriores restauradas.";
                }
                else
                    app->status = "Chip: Ctrl+scroll ajusta largura; Ctrl+Shift+scroll ajusta altura; Ctrl+Z desfaz.";
                return;
            }
            if (app->selected < 0)
            {
                app->status = "Selecione uma peca para ajustar o tamanho com Ctrl+scroll.";
                return;
            }
            Workbench before BENCH_AUTO_CLEAR = {0};
            if (!bench_snapshot_clone(&before, &app->bench))
            {
                app->status = "Memoria insuficiente para ajustar o tamanho.";
                return;
            }
            float scale = bench_part_scale(&app->bench.parts[app->selected]);
            scale *= direction > 0 ? 1.1f : 1 / 1.1f;
            scale = fminf(PART_SCALE_MAX, fmaxf(PART_SCALE_MIN, scale));
            if (bench_set_scale(&app->bench, app->selected, scale) && app->bench.revision != before.revision)
            {
                if (bench_history_record(app->history, &before))
                    app->status = "Tamanho ajustado. Ctrl+scroll redimensiona; Ctrl+Z desfaz.";
                else
                {
                    bench_replace(&app->bench, &before);
                    app->status = "Nao foi possivel registrar o historico; tamanho anterior restaurado.";
                }
            }
            bench_snapshot_clear(&before);
            return;
        }
        if (x >= 220 && y >= 70 && y < 660 && direction != 0 && (SDL_GetModState() & SDL_KMOD_SHIFT) &&
            app->selected >= 0 && app->bench.parts[app->selected].type == PART_POTENTIOMETER)
        {
            Workbench before BENCH_AUTO_CLEAR = {0};
            if (!bench_snapshot_clone(&before, &app->bench))
            {
                app->status = "Memoria insuficiente para ajustar o potenciometro.";
                return;
            }
            double position = app->bench.parts[app->selected].control + (direction > 0 ? .05 : -.05);
            position = fmin(1, fmax(0, position));
            if (bench_set_control(&app->bench, app->selected, position) && app->bench.revision != before.revision)
                if (!bench_history_record(app->history, &before))
                {
                    bench_replace(&app->bench, &before);
                    app->status = "Historico indisponivel; cursor anterior restaurado.";
                    return;
                }
            app->status = "Cursor ajustado. Shift+scroll: posicao; E: resistencia total; Ctrl+Z: desfazer.";
            return;
        }
        if (x >= 220 && y >= 70 && y < 660 && direction != 0)
            bench_view_zoom_at(&app->view, direction > 0 ? 1.2f : 1.0f / 1.2f, x, y);
    }
}

static void input_rotate(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    /* Right click rotates the part under the cursor, the same as selecting it and pressing R. */
    int w, h, hit;
    float x, y;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    x = event->button.x * 1280.0f / w;
    y = event->button.y * 720.0f / h;
    if (x < 220 || y < 70 || y > 660)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    hit = part_at(&app->bench, x, y, app->view.zoom);
    if (hit >= 0 && app->bench.parts[hit].type != PART_BREADBOARD)
    {
        SDL_Event shortcut = {0};
        app->selected = hit;
        app->selected_wire = -1;
        shortcut.type = SDL_EVENT_KEY_DOWN;
        shortcut.key.key = SDLK_R;
        SDL_PushEvent(&shortcut);
    }
}

static void input_pan_start(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0 && event->button.x * 1280.0f / w >= 220 && event->button.y * 720.0f / h >= 70 &&
        event->button.y * 720.0f / h < 660)
        app->panning = 1;
}

static void input_pan(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        app->view.pan_x += event->motion.xrel * 1280.0f / w;
        app->view.pan_y += event->motion.yrel * 720.0f / h;
    }
}

static void input_settings(AppState *app, SDL_Window *window, float x, float y)
{
    if (x >= 260 && x < 800)
    {
        if (y >= 410 && y < 450)
        {
            if (x >= 500 && x < 540 && app->gui_font_size > 12)
                --app->gui_font_size;
            else if (x >= 550 && x < 590 && app->gui_font_size < 20)
                ++app->gui_font_size;
            return;
        }
        if (y >= 340 && y < 385)
        {
            app->dark_theme = !app->dark_theme;
            return;
        }
        if (y >= 130 && y < 170)
            app->snap_grid = !app->snap_grid;
        else if (y >= 170 && y < 210 && SDL_StartTextInput(window))
        {
            app->editing = APP_EDIT_GRID;
            snprintf(app->edit_text, sizeof(app->edit_text), "%.9g", app->grid_spacing);
        }
        else if (y >= 210 && y < 250)
            app->snap_board = !app->snap_board;
        return;
    }
    if (x > 850 && x < 1240)
    {
        if (y >= 120 && y < 160)
            app->show_grid = !app->show_grid;
        else if (y >= 160 && y < 200)
            app->show_handles = !app->show_handles;
        else if (y >= 200 && y < 240)
            app->round_wires = !app->round_wires;
        else if (y >= 240 && y < 280)
        {
            app->show_current = !app->show_current;
            if (app->show_current && sim_state_has_result(&app->sim, SIM_RESULT_TRACE) &&
                !app->sim.trace.measures_wires)
                app->status = "Pressione T para medir correntes transitorias dos fios.";
            else if (app->show_current && !app->sim.dc_wire_valid && !sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
                app->status = "Pressione P ou T para medir correntes dos fios.";
        }
        else if (y >= 354 && y < 390)
            app->probe_current = !app->probe_current;
        else if (y >= 390 && y < 425)
            app->probe_pin = (app->probe_pin + 1) %
                             ((app->selected >= 0 && app->bench.parts[app->selected].type == PART_74HC00) ? 14 : 3);
        else if (y >= 430 && y < 465)
        {
            if (SDL_StartTextInput(window))
            {
                app->editing = WORKBENCH_MAX_COMPONENTS;
                snprintf(app->edit_text, sizeof(app->edit_text), "%.9g", app->transient_step);
            }
        }
        else if (y >= 470 && y < 505)
        {
            if (SDL_StartTextInput(window))
            {
                app->editing = WORKBENCH_MAX_COMPONENTS + 1;
                snprintf(app->edit_text, sizeof(app->edit_text), "%.9g", app->transient_stop);
            }
        }
        else if (y >= 320 && y < 352 && x >= 860 && x < 1160)
        {
            int chosen = (int)((x - 860) / 50);
            if (chosen >= 0 && chosen < WIRE_COLOR_COUNT)
            {
                app->wire_color = chosen;
                if (app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count &&
                    app->bench.wires[app->selected_wire].color != chosen)
                {
                    bench_history_record(app->history, &app->bench);
                    bench_set_wire_color(&app->bench, app->selected_wire, chosen);
                    app->status = "Cor do fio selecionado alterada.";
                }
                else
                    app->status = "Cor dos proximos fios selecionada.";
            }
        }
    }
}

static int input_projects(AppState *app, float x, float y)
{
    if (x >= 500 && x < 680 && y < 68)
    {
        app_editing_key(app, SDLK_F3, 0);
        return 1;
    }
    if (!app->projects_open)
        return 0;
    if (x >= 330 && x < 930 && y >= 200 && y < 470)
    {
        int column = (int)((x - 330) / 200), row = (int)((y - 200) / 90);
        app->save_slot = row * 3 + column;
    }
    if (y >= 500 && y < 545)
    {
        if (x >= 340 && x < 590)
            app_projects_save(app);
        else if (x >= 640 && x < 890)
            app_projects_load(app);
    }
    return 1;
}

static int input_click_controls(AppState *app, SDL_Window *window, float x, float y)
{
    if (app_editor_click(app, window, x, y))
        return 1;
    if (!app->settings && !app->projects_open && app->selected >= 0 &&
        !app->bench.parts[app->selected].group && x >= 850 && x < 1180 && y >= 154 && y < 182)
    {
        if (SDL_StartTextInput(window))
        {
            app->editing = APP_EDIT_SCALE;
            snprintf(app->edit_text, sizeof(app->edit_text), "%.6g", bench_part_scale(&app->bench.parts[app->selected]) * 100);
        }
        return 1;
    }
    if (input_projects(app, x, y))
        return 1;
    if (x >= 700 && x < 850 && y < 68)
    {
        SDL_Event shortcut = {0};
        shortcut.type = SDL_EVENT_KEY_DOWN;
        shortcut.key.key = SDLK_F2;
        SDL_PushEvent(&shortcut);
        return 1;
    }
    if (x >= 880 && x < 1030 && y < 68)
    {
        app->settings = !app->settings;
        app->dragging = -1;
        app->drag_wire = -1;
        return 1;
    }
    if (x >= PLAY_BUTTON_X && y < 68)
    {
        SDL_Event shortcut = {0};
        shortcut.type = SDL_EVENT_KEY_DOWN;
        shortcut.key.key = SDLK_SPACE;
        SDL_PushEvent(&shortcut);
        return 1;
    }
    if (!app->settings && app->selected >= 0 && app->bench.parts[app->selected].type != PART_BREADBOARD &&
        x >= ROTATE_BUTTON_X && x < ROTATE_BUTTON_X + 72 && y >= 86 && y < 112)
    {
        SDL_Event shortcut = {0};
        shortcut.type = SDL_EVENT_KEY_DOWN;
        shortcut.key.key = SDLK_R;
        SDL_PushEvent(&shortcut);
        return 1;
    }
    if (app->settings)
    {
        input_settings(app, window, x, y);
        return 1;
    }
    if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->show_trace && x >= 840 && x <= 1240 && y >= 384 &&
        y <= 628)
    {
        if (x >= 918 && x <= 1220 && y >= 431 && y <= 565)
        {
            app->sim.trace_index = spice_trace_nearest(&app->sim.trace, (x - 918) / 302.0);
            app->sim.sim_running = 0;
            const char *format = "Tensao: t=%.6g s; V=%.6g V (%d/%d).";
            if (app->sim.trace_current == 2)
                format = "Corrente fio A -> B: t=%.6g s; I=%.6g A (%d/%d).";
            else if (app->sim.trace_current)
                format = "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (%d/%d).";
            snprintf(app->sim.trace_status, sizeof(app->sim.trace_status), format,
                     app->sim.trace.samples[app->sim.trace_index].time,
                     app->sim.trace.samples[app->sim.trace_index].value, app->sim.trace_index + 1,
                     app->sim.trace.count);
            app->status = app->sim.trace_status;
        }
        return 1;
    }
    if (x < 220 && y >= 80 && y < 125)
    {
        app->palette_page = 2;
        if (x < 78)
            app->palette_page = 0;
        else if (x < 148)
            app->palette_page = 1;
        app->placing = -1;
        app->palette_offset = 0;
        app->packed_placing = 0;
        app_packing_refresh(app);
        return 1;
    }
    if (x < 220 && y >= 140 && y < 716)
    {
        int item = (int)((y - 140) / 72);
        if (item < (app->palette_page ? 8 : 8))
        {
            if (app->palette_page == 2)
            {
                app_packing_choose(app, item);
                return 1;
            }
            app->packed_placing = 0;
            app->placing = palette_part(app->palette_page, item + app->palette_offset);
            app->pending.component = -1;
            app->status = "Clique na bancada para posicionar.";
        }
        return 1;
    }
    return 0;
}

static int input_click_knee(AppState *app, float x, float y, int clicks)
{
    int i;
    if (app->pending.component < 0 && app->placing < 0 && app->show_handles)
    {
        int k;
        for (i = 0; i < app->bench.wire_count && app->drag_wire < 0; ++i)
        {
            if (bench_group_wire_internal(&app->bench, &app->bench.wires[i]))
                continue;
            for (k = 0; k < app->bench.wires[i].knee_count; ++k)
            {
                WirePoint point = app->bench.wires[i].knees[k];
                if (fabsf(x - point.x) < 8 / app->view.zoom && fabsf(y - point.y) < 8 / app->view.zoom)
                {
                    app->drag_wire = i;
                    app->drag_knee = k;
                    break;
                }
            }
        }
        if (app->drag_wire >= 0)
        {
            if (clicks >= 2)
            {
                bench_history_record(app->history, &app->bench);
                bench_remove_knee(&app->bench, app->drag_wire, app->drag_knee);
                app->drag_wire = -1;
                app->status = "Joelho removido.";
            }
            else
            {
                app->move_recorded = 0;
                app->status = "Arraste o joelho; duplo clique remove.";
            }
            return 1;
        }
    }
    return 0;
}

static int snap_suspended(void)
{
    return (SDL_GetModState() & SDL_KMOD_ALT) != 0;
}

static void input_snap_grid(const AppState *app, float *x, float *y)
{
    if (!app->snap_grid || snap_suspended())
        return;
    BenchGrid grid = bench_grid_default();
    grid.spacing = app->grid_spacing;
    double snapped_x, snapped_y;
    if (bench_grid_snap(&grid, *x, *y, &snapped_x, &snapped_y))
    {
        *x = (float)snapped_x;
        *y = (float)snapped_y;
    }
}

static void input_place_part(AppState *app, float x, float y)
{
    Workbench before BENCH_AUTO_CLEAR = {0};
    if (!bench_snapshot_clone(&before, &app->bench))
    {
        app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
        return;
    }
    int added = bench_add(&app->bench, (PartType)app->placing, x, y);
    if (added >= 0)
    {
        int snapped = app->snap_board && !snap_suspended() && bench_snap_to_breadboard(&app->bench, added);
        if (!snapped)
        {
            input_snap_grid(app, &x, &y);
            bench_move_part(&app->bench, added, x, y);
        }
        bench_history_record(app->history, &before);
        app->selected_wire = -1;
        app->status =
            snapped ? "Componente encaixado na breadboard." : "Componente adicionado. Selecione terminais para ligar.";
        app->placing = -1;
    }
    else
        app->status = "Limite de componentes atingido.";
}

static Pin input_pin_at(const Workbench *bench, float x, float y)
{
    for (int i = bench->part_count - 1; i >= 0; --i)
        if (bench->parts[i].group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (bench_pin_exposed(&bench->parts[i], p))
                {
                    float px, py;
                    bench_terminal_position(bench, (Pin){i, p}, &px, &py);
                    if ((x - px) * (x - px) + (y - py) * (y - py) <= 64)
                        return (Pin){i, p};
                }

    int i, hit = -1, hitpin = -1;
    int pass;
    for (pass = 0; pass < 2 && hit < 0; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            if (bench->parts[i].group)
                continue;
            int p, board = bench->parts[i].type == PART_BREADBOARD;
            float radius = 12.0f;
            if (board)
                radius = 4.0f;
            else if (component_has_flag(bench->parts[i].type, COMPONENT_QUAD_GATE))
                radius = 5.0f;
            radius *= fminf(1, bench_part_scale(&bench->parts[i]));
            if (board != pass)
                continue;
            for (p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
            {
                float px, py;
                bench_pin_position(&bench->parts[i], p, &px, &py);
                if ((x - px) * (x - px) + (y - py) * (y - py) <= radius * radius)
                {
                    hit = i;
                    hitpin = p;
                    break;
                }
            }
            if (hit >= 0)
                break;
        }
    return (Pin){hit, hitpin};
}

static void input_connect_pin(AppState *app, Pin clicked)
{
    app->selected_wire = -1;
    if (app->pending.component < 0)
    {
        app->pending = clicked;
        app->route_count = 0;
        app->status = "Clique para criar joelhos; termine em outro terminal.";
    }
    else if (app->pending.component == clicked.component && app->pending.pin == clicked.pin)
    {
        app->pending.component = -1;
        app->route_count = 0;
        app->status = "Conexao cancelada.";
    }
    else
    {
        Workbench before BENCH_AUTO_CLEAR = {0};
        if (!bench_snapshot_clone(&before, &app->bench))
        {
            app->status = "Memoria insuficiente para copiar a montagem; operacao cancelada.";
            return;
        }
        int result = bench_toggle_wire(&app->bench, app->pending, clicked);
        if (result == 1)
        {
            int wire = app->bench.wire_count - 1;
            bench_set_wire_route(&app->bench, wire, app->route, app->route_count);
            bench_set_wire_color(&app->bench, wire, app->wire_color);
        }
        if (result)
            bench_history_record(app->history, &before);
        if (result == 1)
            app->status = "Fio conectado.";
        else if (result == 2)
            app->status = "Fio removido.";
        else
            app->status = "Nao foi possivel conectar.";
        app->pending.component = -1;
        app->route_count = 0;
    }
}

static void input_select(AppState *app, float x, float y, int clicks)
{
    int hit;
    hit = part_at(&app->bench, x, y, app->view.zoom);
    if (hit < 0 || !app->selection.parts[hit])
        bench_selection_clear(&app->selection);
    if (hit >= 0)
        app->selection.parts[hit] = 1;
    bench_selection_expand_groups(&app->selection, &app->bench);
    app->selected = hit;
    app->selected_wire = -1;
    if (hit < 0 || app->bench.parts[hit].type == PART_BREADBOARD)
    {
        app->selected_wire = hit_wire(&app->bench, x, y, 7.0f / app->view.zoom, app->round_wires);
        if (app->selected_wire >= 0)
        {
            app->selected = -1;
            app->wire_color = app->bench.wires[app->selected_wire].color;
            app->status = "Fio selecionado; duplo clique no trecho adiciona joelho; F1 abre as cores.";
            if (clicks >= 2)
                input_insert_knee(app, x, y);
            return;
        }
    }
    if (hit >= 0)
    {
        if (app->bench.parts[hit].type == PART_BUTTON)
        {
            bench_set_on(&app->bench, hit, 1);
            app->held_button = hit;
            app->status = "Botao pressionado; solte para abrir o contato. Contato ideal, sem bounce.";
            return;
        }
        if (!app->bench.parts[hit].group && clicks >= 2 &&
            (app->bench.parts[hit].type == PART_SOURCE || app->bench.parts[hit].type == PART_SWITCH ||
             app->bench.parts[hit].type == PART_CLOCK))
        {
            bench_history_record(app->history, &app->bench);
            bench_set_on(&app->bench, hit, !app->bench.parts[hit].on);
            app->status = "Estado alterado.";
        }
        else
        {
            app->dragging = hit;
            app->move_recorded = 0;
            app->offset_x = x - app->bench.parts[hit].x;
            app->offset_y = y - app->bench.parts[hit].y;
            app->status = "Arraste para mover; dois cliques alternam fonte ou chave.";
        }
    }
    else
    {
        app->pending.component = -1;
        app->selecting_area = 1;
        app->area_start = (WirePoint){x, y};
        app->area_end = app->area_start;
        app->status = "Arraste para selecionar uma area. Ctrl+C/X/V; Delete remove.";
    }
}

static void input_click(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    int w, h, hitpin = -1;
    float x, y;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0)
        return;
    x = event->button.x * 1280.0f / w;
    y = event->button.y * 720.0f / h;
    if (input_click_controls(app, window, x, y))
        return;
    if (x < 220 || y < 70 || y > 660)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    if (SDL_GetModState() & SDL_KMOD_SHIFT)
    {
        app->selecting_area = 1;
        app->area_start = (WirePoint){x, y};
        app->area_end = app->area_start;
        app->dragging = -1;
        app->drag_wire = -1;
        app->pending.component = -1;
        app->placing = -1;
        return;
    }
    if (SDL_GetModState() & SDL_KMOD_ALT)
    {
        input_select(app, x, y, 1);
        return;
    }
    if (input_click_knee(app, x, y, event->button.clicks))
        return;
    if (app->packed_placing)
    {
        input_snap_grid(app, &x, &y);
        app_packing_place(app, x, y);
        return;
    }
    if (app->placing >= 0)
    {
        input_place_part(app, x, y);
        return;
    }
    Pin clicked = input_pin_at(&app->bench, x, y);
    hitpin = clicked.pin;
    if (hitpin >= 0)
    {
        input_connect_pin(app, clicked);
        return;
    }
    if (app->pending.component >= 0)
    {
        if (app->route_count < WIRE_MAX_KNEES)
        {
            app->route[app->route_count++] = (WirePoint){x, y};
            app->status = "Joelho adicionado; termine em um terminal.";
        }
        else
            app->status = "Limite de 16 joelhos por fio.";
        return;
    }
    input_select(app, x, y, event->button.clicks);
}

static void input_release(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    (void)window;
    (void)event;

    if (app->held_button >= 0 && app->held_button < app->bench.part_count &&
        app->bench.parts[app->held_button].type == PART_BUTTON)
        bench_set_on(&app->bench, app->held_button, 0);
    app->held_button = -1;
    app->selecting_area = 0;
    if (bench_selection_count(&app->selection, &app->bench) <= 1 && app->dragging >= 0 && app->move_recorded &&
        !app->bench.parts[app->dragging].group && app->snap_board && !snap_suspended() && bench_snap_to_breadboard(&app->bench, app->dragging))
        app->status = "Componente encaixado na breadboard.";
    app->dragging = -1;
    app->drag_wire = -1;
}

static void input_drag_wire(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0 && app->drag_wire < app->bench.wire_count)
    {
        float x = event->motion.x * 1280.0f / w, y = event->motion.y * 720.0f / h;
        WirePoint *point = &app->bench.wires[app->drag_wire].knees[app->drag_knee];
        bench_view_to_world(&app->view, x, y, &x, &y);
        input_snap_grid(app, &x, &y);
        if (point->x != x || point->y != y)
        {
            if (!app->move_recorded)
            {
                bench_history_record(app->history, &app->bench);
                app->move_recorded = 1;
            }
            bench_move_knee(&app->bench, app->drag_wire, app->drag_knee, (WirePoint){x, y});
        }
    }
}

static void input_drag_part(AppState *app, SDL_Window *window, const SDL_Event *event)
{

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w > 0 && h > 0)
    {
        float x = event->motion.x * 1280.0f / w, y = event->motion.y * 720.0f / h;
        bench_view_to_world(&app->view, x, y, &x, &y);
        x -= app->offset_x;
        y -= app->offset_y;
        const Part *dragged = &app->bench.parts[app->dragging];
        if (dragged->group)
        {
            const BenchGroup *group = &app->bench.groups[dragged->group - 1];
            float body_x = group->x + x - dragged->x;
            float body_y = group->y + y - dragged->y;
            input_snap_grid(app, &body_x, &body_y);
            x = dragged->x + body_x - group->x;
            y = dragged->y + body_y - group->y;
        }
        else
        {
            Part candidate = *dragged;
            candidate.x = x;
            candidate.y = y;
            int board_snap = app->snap_board && !snap_suspended() &&
                             bench_selection_count(&app->selection, &app->bench) <= 1 &&
                             bench_breadboard_snap_position(&app->bench, &candidate, &x, &y);
            if (!board_snap)
                input_snap_grid(app, &x, &y);
        }
        if (app->bench.parts[app->dragging].x != x || app->bench.parts[app->dragging].y != y)
        {
            if (!app->move_recorded)
            {
                bench_history_record(app->history, &app->bench);
                app->move_recorded = 1;
            }
            if (bench_selection_count(&app->selection, &app->bench) > 1 || app->bench.parts[app->dragging].group)
                bench_selection_move(&app->bench, &app->selection, x - app->bench.parts[app->dragging].x,
                                     y - app->bench.parts[app->dragging].y);
            else
                bench_move_part(&app->bench, app->dragging, x, y);
        }
    }
}

static void input_mouse_position(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0)
        return;
    float x = event->motion.x * 1280.0f / width, y = event->motion.y * 720.0f / height;
    bench_view_to_world(&app->view, x, y, &app->mouse_world.x, &app->mouse_world.y);
    app->mouse_screen = (WirePoint){x, y};
    app->mouse_position_valid = 1;
    if (!app->selecting_area)
        return;
    app->area_end = app->mouse_world;
    bench_selection_rect(&app->selection, &app->bench, app->area_start, app->area_end);
    app->selected = -1;
    for (int i = 0; i < app->bench.part_count; ++i)
        if (app->selection.parts[i])
            app->selected = i;
    snprintf(app->selection_status, sizeof(app->selection_status),
             "%d componentes selecionados. Ctrl+C/X/V; Delete remove.",
             bench_selection_count(&app->selection, &app->bench));
    app->status = app->selection_status;
}

/* Coalesce hover hit testing to one pass per frame; drag/model events still all execute. */
static void input_refresh_hover(AppState *app)
{
    app->hovered_pin = (Pin){-1, -1};
    app->hover_revision = app->bench.revision;
    float x = app->mouse_screen.x, y = app->mouse_screen.y;
    if (x < 220 || y < 70 || y >= 660 || app->settings || app->projects_open || app->editing >= 0 ||
        app->selecting_area || app->editor_confirm_close || app->panning || app->dragging >= 0 || app->drag_wire >= 0)
        return;
    bench_view_to_world(&app->view, x, y, &x, &y);
    app->hovered_pin = input_pin_at(&app->bench, x, y);
}

/* Dispatch in priority order: text editing consumes input before workspace interaction. */
void app_input_poll(AppState *app, SDL_Window *window)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        /* Finish draining this context before routing to another editor. */
        if (app->editor_open_requested || app->editor_close_requested)
            continue;
        if (e.type == SDL_EVENT_MOUSE_MOTION)
            input_mouse_position(app, window, &e);
        if (e.type == SDL_EVENT_QUIT)
            app->running = 0;
        else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            input_release(app, window, &e);
        else if (app->editing >= 0 && e.type == SDL_EVENT_TEXT_INPUT)
        {
            input_text(app, window, &e);
        }
        else if (app->editing >= 0 && e.type == SDL_EVENT_KEY_DOWN)
        {
            input_edit_key(app, window, &e);
        }
        else if (app->editing >= 0 && (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_MOTION))
        {
            continue;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
        {
            app_commands_key(app, window, e.key.key, e.key.mod);
        }
        else if (app->projects_open && e.type != SDL_EVENT_MOUSE_BUTTON_DOWN)
            continue;
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->selecting_area)
            continue;
        else if (e.type == SDL_EVENT_MOUSE_WHEEL && !app->settings && !app->editor_confirm_close && app->editing < 0)
        {
            input_zoom(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT && !app->settings && !app->editor_confirm_close &&
                 app->editing < 0)
        {
            input_rotate(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE && !app->settings && !app->editor_confirm_close &&
                 app->editing < 0)
        {
            input_pan_start(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_MIDDLE)
            app->panning = 0;
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->panning && app->editing < 0)
        {
            input_pan(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            input_click(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
        {
            input_release(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->drag_wire >= 0)
        {
            input_drag_wire(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->dragging >= 0)
        {
            input_drag_part(app, window, &e);
        }
    }
    if (app->mouse_position_valid)
        input_refresh_hover(app);
}
