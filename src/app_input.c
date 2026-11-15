#include "app_input.h"
#include "app_commands.h"
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
    int i;
    for (i = bench->wire_count - 1; i >= 0; --i)
    {
        const Wire *w = &bench->wires[i];
        WirePoint a, b, path[WIRE_PATH_CAPACITY];
        int k, n;
        bench_pin_position(&bench->parts[w->a.component], w->a.pin, &a.x, &a.y);
        bench_pin_position(&bench->parts[w->b.component], w->b.pin, &b.x, &b.y);
        n = wire_path(w, a, b, rounded ? 18.0f : 0.0f, path);
        for (k = 1; k < n; ++k)
        {
            float dx = path[k].x - path[k - 1].x, dy = path[k].y - path[k - 1].y, den = dx * dx + dy * dy, t = 0, px,
                  py;
            if (den > 0)
                t = fminf(1, fmaxf(0, ((x - path[k - 1].x) * dx + (y - path[k - 1].y) * dy) / den));
            px = path[k - 1].x + t * dx;
            py = path[k - 1].y + t * dy;
            if ((x - px) * (x - px) + (y - py) * (y - py) <= tolerance * tolerance)
                return i;
        }
    }
    return -1;
}
static int part_at(const Workbench *bench, float x, float y)
{
    int pass, i;
    for (pass = 0; pass < 2; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            const Part *part = &bench->parts[i];
            int board = part->type == PART_BREADBOARD, vertical = part->rotation % 2, chip = part->type == PART_74HC00;
            float hw, hh;
            if (board != pass)
                continue;
            if (board)
            {
                hw = 227.0f;
                hh = 112.0f;
            }
            else if (chip)
            {
                hw = vertical ? 20.0f : 50.0f;
                hh = vertical ? 50.0f : 20.0f;
            }
            else
            {
                hw = vertical ? 30.0f : 44.0f;
                hh = vertical ? 44.0f : 30.0f;
            }
            if (fabsf(x - part->x) < hw && fabsf(y - part->y) < hh)
                return i;
        }
    return -1;
}

static void input_text(AppState *app, SDL_Window *window, const SDL_Event *event)
{
    (void)window;

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
        Workbench before = app->bench;
        if (app->editing >= WORKBENCH_MAX_COMPONENTS)
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
    hit = part_at(&app->bench, x, y);
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
            if (app->show_current && app->sim.trace_valid && !app->sim.trace.measures_wires)
                app->status = "Pressione T para medir correntes transitorias dos fios.";
            else if (app->show_current && !app->sim.dc_wire_valid && !app->sim.trace_valid)
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

static int input_click_controls(AppState *app, SDL_Window *window, float x, float y)
{
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
    if (app->sim.trace_valid && app->show_trace && x >= 840 && x <= 1240 && y >= 384 && y <= 628)
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
        app->palette_page = 1 - app->palette_page;
        app->placing = -1;
        return 1;
    }
    if (x < 220 && y >= 140 && y < 716)
    {
        int item = (int)((y - 140) / 72);
        if (item < (app->palette_page ? 8 : 8))
        {
            app->placing = palette_part(app->palette_page, item);
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
            for (k = 0; k < app->bench.wires[i].knee_count; ++k)
            {
                WirePoint point = app->bench.wires[i].knees[k];
                if (fabsf(x - point.x) < 8 && fabsf(y - point.y) < 8)
                {
                    app->drag_wire = i;
                    app->drag_knee = k;
                    break;
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

static void input_place_part(AppState *app, float x, float y)
{
    Workbench before = app->bench;
    int added = bench_add(&app->bench, (PartType)app->placing, x, y);
    if (added >= 0)
    {
        int snapped = bench_snap_to_breadboard(&app->bench, added);
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
    int i, hit = -1, hitpin = -1;
    int pass;
    for (pass = 0; pass < 2 && hit < 0; ++pass)
        for (i = bench->part_count - 1; i >= 0; --i)
        {
            int p, board = bench->parts[i].type == PART_BREADBOARD;
            float radius = 12.0f;
            if (board)
                radius = 4.0f;
            else if (bench->parts[i].type == PART_74HC00)
                radius = 5.0f;
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
        Workbench before = app->bench;
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
    hit = part_at(&app->bench, x, y);
    app->selected = hit;
    app->selected_wire = -1;
    if (hit < 0 || app->bench.parts[hit].type == PART_BREADBOARD)
    {
        app->selected_wire = hit_wire(&app->bench, x, y, 7.0f / app->view.zoom, app->round_wires);
        if (app->selected_wire >= 0)
        {
            app->selected = -1;
            app->wire_color = app->bench.wires[app->selected_wire].color;
            app->status = "Fio selecionado; F1 abre as cores.";
            return;
        }
    }
    if (hit >= 0)
    {
        if (clicks >= 2 && (app->bench.parts[hit].type == PART_SOURCE || app->bench.parts[hit].type == PART_SWITCH ||
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
        app->status = "Bancada livre.";
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
    if (input_click_knee(app, x, y, event->button.clicks))
        return;
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

    if (app->dragging >= 0 && app->move_recorded && bench_snap_to_breadboard(&app->bench, app->dragging))
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
        if (app->bench.parts[app->dragging].x != x || app->bench.parts[app->dragging].y != y)
        {
            if (!app->move_recorded)
            {
                bench_history_record(app->history, &app->bench);
                app->move_recorded = 1;
            }
            bench_move_part(&app->bench, app->dragging, x, y);
        }
    }
}

/* Dispatch in priority order: text editing consumes input before workspace interaction. */
void app_input_poll(AppState *app, SDL_Window *window)
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
            app->running = 0;
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
        else if (e.type == SDL_EVENT_MOUSE_WHEEL && !app->settings && app->editing < 0)
        {
            input_zoom(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT && !app->settings &&
                 app->editing < 0)
        {
            input_rotate(app, window, &e);
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE && !app->settings &&
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
}
