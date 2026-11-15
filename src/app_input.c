#include "app_input.h"
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

/* The automatic transient records the selected terminal, or the first source terminal, for the optional graph. */
void app_input_poll(AppState *app, SDL_Window *window)
{
    SDL_Event e;
    int i;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
            app->running = 0;
        else if (app->editing >= 0 && e.type == SDL_EVENT_TEXT_INPUT)
        {
            const char *p = e.text.text;
            size_t length = strlen(app->edit_text);
            while (*p && length + 1 < sizeof(app->edit_text))
            {
                if ((unsigned char)*p >= 32 && (unsigned char)*p < 127)
                    app->edit_text[length++] = *p;
                ++p;
            }
            app->edit_text[length] = '\0';
        }
        else if (app->editing >= 0 && e.type == SDL_EVENT_KEY_DOWN)
        {
            if (e.key.key == SDLK_ESCAPE)
            {
                app->editing = -1;
                SDL_StopTextInput(window);
                app->status = "Edicao cancelada.";
            }
            else if (e.key.key == SDLK_BACKSPACE)
            {
                size_t length = strlen(app->edit_text);
                if (length)
                    app->edit_text[length - 1] = '\0';
            }
            else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER)
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
        else if (app->editing >= 0 && (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_MOTION))
        {
            continue;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
        {
            if (e.key.key == SDLK_F2)
            {
                Workbench example = {0};
                int choice = (app->example_index + 1) % WORKBENCH_EXAMPLE_COUNT, example_selected;
                if (workbench_example_create(choice, &example, &example_selected))
                {
                    bench_history_record(app->history, &app->bench);
                    app->bench = example;
                    app->example_index = choice;
                    app->selected = example_selected;
                    app->selected_wire = -1;
                    app->dragging = -1;
                    app->drag_wire = -1;
                    app->pending.component = -1;
                    app->route_count = 0;
                    app->placing = -1;
                    app->settings = 0;
                    app->view = (BenchView){1, 0, 0};
                    app->sim.trace_valid = 0;
                    app->sim.dc_valid = 0;
                    app->sim.dc_wire_valid = 0;
                    memset(app->leds, 0, sizeof(app->leds));
                    app->probe_pin = choice > 0 ? 2 : 0;
                    app->probe_current = 0;
                    app->transient_step = 1e-5;
                    app->transient_stop = .005;
                    app->status = workbench_example_name(choice);
                }
                else
                    app->status = "Falha ao preparar exemplo.";
                continue;
            }
            if (e.key.key == SDLK_TAB)
            {
                app->palette_page = 1 - app->palette_page;
                app->placing = -1;
                app->status = app->palette_page ? "Biblioteca de portas logicas." : "Biblioteca de componentes.";
                continue;
            }
            if (e.key.key == SDLK_F1)
            {
                app->settings = !app->settings;
                app->dragging = -1;
                app->drag_wire = -1;
                continue;
            }
            if (e.key.key == SDLK_HOME)
            {
                app->view = (BenchView){1, 0, 0};
                app->status = "Vista restaurada.";
                continue;
            }
            if ((e.key.mod & SDL_KMOD_CTRL) && (e.key.key == SDLK_Z || e.key.key == SDLK_Y))
            {
                int changed = e.key.key == SDLK_Z ? bench_history_undo(app->history, &app->bench)
                                                  : bench_history_redo(app->history, &app->bench);
                if (changed)
                {
                    app->selected = -1;
                    app->selected_wire = -1;
                    app->dragging = -1;
                    app->drag_wire = -1;
                    app->pending.component = -1;
                    app->route_count = 0;
                    memset(app->leds, 0, sizeof(app->leds));
                    app->status = e.key.key == SDLK_Z ? "Acao desfeita." : "Acao refeita.";
                }
                else
                    app->status = "Nenhuma acao disponivel.";
                continue;
            }
            if (e.key.key == SDLK_ESCAPE)
            {
                app->pending.component = -1;
                app->route_count = 0;
                app->placing = -1;
                app->dragging = -1;
                app->drag_wire = -1;
                app->status = "Selecao cancelada.";
            }
            else if (e.key.key == SDLK_SPACE)
            {
                app->sim.sim_running = !app->sim.sim_running;
                if (app->sim.trace_valid && app->sim.sim_running)
                {
                    if (app->sim.trace_index >= app->sim.trace.count - 1)
                        app->sim.trace_index = 0;
                    app->sim.trace_play_start_index = app->sim.trace_index;
                    app->sim.trace_play_started = SDL_GetTicks();
                    app->status = "Reproducao da curva: intervalo completo em 5 s reais.";
                    if (app->sim.trace_automatic)
                        app->status = "Simulacao em execucao: o circuito e calculado automaticamente.";
                }
                else if (app->sim.trace_valid)
                    app->status = "Reproducao pausada; B/N percorrem as amostras.";
                else if (app->sim.auto_enabled)
                    app->status = app->sim.sim_running
                                      ? "Simulacao em execucao: o circuito e calculado automaticamente."
                                      : "Simulacao pausada. Espaco ou o botao de play continua.";
                else
                    app->status = app->sim.sim_running ? "Simulacao executando." : "Simulacao pausada.";
            }
            else if (e.key.key == SDLK_P && app->sim.auto_enabled)
            {
                app->sim.auto_requested_valid = 0;
                app->sim.bench_changed_at = 0;
                app->sim.sim_running = 1;
                app->status = "Recalculando o circuito...";
            }
            else if (e.key.key == SDLK_R && app->selected >= 0)
            {
                Workbench before = app->bench;
                if (bench_rotate_part(&app->bench, app->selected))
                {
                    bench_snap_to_breadboard(&app->bench, app->selected);
                    bench_history_record(app->history, &before);
                    app->dragging = -1;
                    app->pending.component = -1;
                    app->route_count = 0;
                    app->status = "Componente girado 90 graus.";
                }
                else
                    app->status = "Esta placa ainda nao gira.";
            }
            else if (e.key.key == SDLK_G && app->sim.trace_valid)
            {
                app->show_trace = !app->show_trace;
                app->status = app->show_trace ? "Grafico visivel." : "Grafico oculto; pressione G para mostrar.";
            }
            else if (e.key.key == SDLK_T && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
            {
                if (app->dc_job.thread)
                    app->status = "Analise em andamento.";
                else if (spice_job_start_transient_wire(&app->dc_job, &app->bench, app->ngspice, app->selected_wire,
                                                        app->transient_step, app->transient_stop))
                {
                    app->sim.trace_valid = 0;
                    app->sim.dc_valid = 0;
                    app->sim.dc_wire_valid = 0;
                    app->status = "Calculando corrente transitoria do fio...";
                }
                else
                    app->status = "Falha ao iniciar tarefa de analise.";
            }
            else if (e.key.key == SDLK_T && app->selected >= 0 &&
                     app->bench.parts[app->selected].type != PART_BREADBOARD)
            {
                if (app->dc_job.thread)
                    app->status = "Analise em andamento.";
                else if (!app->probe_current && app->probe_pin >= bench_pin_count(app->bench.parts[app->selected].type))
                    app->status = "Terminal indisponivel para este componente.";
                else if (app->probe_current && (is_gate(app->bench.parts[app->selected].type) ||
                                                app->bench.parts[app->selected].type == PART_74HC00))
                    app->status = "Corrente interna da porta nao disponivel.";
                else if (app->probe_current && app->bench.parts[app->selected].type == PART_SWITCH)
                    app->status = "Corrente transitoria indisponivel para interruptor ideal.";
                else if (app->show_current ? spice_job_start_transient_instrumented(
                                                 &app->dc_job, &app->bench, app->ngspice, app->probe_current ? 2 : 1,
                                                 (Pin){app->selected, app->probe_current ? 0 : app->probe_pin},
                                                 app->transient_step, app->transient_stop)
                         : app->probe_current
                             ? spice_job_start_transient_current(&app->dc_job, &app->bench, app->ngspice, app->selected,
                                                                 app->transient_step, app->transient_stop)
                             : spice_job_start_transient(&app->dc_job, &app->bench, app->ngspice,
                                                         (Pin){app->selected, app->probe_pin}, app->transient_step,
                                                         app->transient_stop))
                {
                    app->sim.trace_valid = 0;
                    app->sim.dc_valid = 0;
                    app->sim.dc_wire_valid = 0;
                    app->status = "Calculando transitorio selecionado...";
                }
                else
                    app->status = "Falha ao iniciar tarefa de analise.";
            }
            else if ((e.key.key == SDLK_N || e.key.key == SDLK_B) && !app->sim.sim_running && app->sim.trace_valid)
            {
                if (e.key.key == SDLK_N && app->sim.trace_index + 1 < app->sim.trace.count)
                    ++app->sim.trace_index;
                if (e.key.key == SDLK_B && app->sim.trace_index > 0)
                    --app->sim.trace_index;
                snprintf(app->sim.trace_status, sizeof(app->sim.trace_status),
                         app->sim.trace_current == 2 ? "Corrente fio A -> B: t=%.6g s; I=%.6g A (%d/%d)."
                         : app->sim.trace_current    ? "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (%d/%d)."
                                                     : "Tensao: t=%.6g s; V=%.6g V (%d/%d).",
                         app->sim.trace.samples[app->sim.trace_index].time,
                         app->sim.trace.samples[app->sim.trace_index].value, app->sim.trace_index + 1,
                         app->sim.trace.count);
                app->status = app->sim.trace_status;
            }
            else if (e.key.key == SDLK_N && !app->sim.sim_running)
            {
                if (bench_boolean_supported(&app->bench))
                {
                    bench_simulate(&app->bench, app->leds);
                    app->status = "Um passo executado.";
                }
                else
                    app->status = "Calculo eletrico ainda indisponivel.";
            }
            else if (e.key.key == SDLK_P)
            {
                if (app->dc_job.thread)
                    app->status = "Analise em andamento.";
                else if ((app->show_current ? spice_job_start_wires : spice_job_start)(&app->dc_job, &app->bench,
                                                                                       app->ngspice))
                {
                    app->sim.trace_valid = 0;
                    app->sim.dc_valid = 0;
                    app->sim.dc_wire_valid = 0;
                    app->status = "Analisando ponto DC...";
                }
                else
                    app->status = "Falha ao iniciar tarefa de analise.";
            }
            else if (e.key.key == SDLK_E && app->selected >= 0)
            {
                PartType type = app->bench.parts[app->selected].type;
                if (type == PART_SOURCE || type == PART_RESISTOR || type == PART_CAPACITOR || type == PART_INDUCTOR ||
                    type == PART_CLOCK)
                {
                    if (SDL_StartTextInput(window))
                    {
                        app->editing = app->selected;
                        snprintf(app->edit_text, sizeof(app->edit_text), "%.9g", app->bench.parts[app->selected].value);
                        app->status = "Digite o valor em unidades SI; Enter confirma, Esc cancela.";
                    }
                    else
                        app->status = "Nao foi possivel iniciar edicao de texto.";
                }
                else
                    app->status = "Este componente nao possui valor editavel.";
            }
            else if (app->selected >= 0 &&
                     (e.key.key == SDLK_EQUALS || e.key.key == SDLK_PLUS || e.key.key == SDLK_MINUS))
            {
                double scale = e.key.key == SDLK_MINUS ? .5 : 2.0;
                Workbench before = app->bench;
                if (bench_set_value(&app->bench, app->selected, app->bench.parts[app->selected].value * scale))
                {
                    bench_history_record(app->history, &before);
                    app->status = "Valor alterado.";
                }
                else
                    app->status = "Valor nao editavel ou fora do limite.";
            }
            else if (e.key.key == SDLK_S)
            {
                WorkbenchSettings saved = {app->show_grid,     app->show_handles,   app->round_wires,
                                           app->show_current,  app->wire_color,     app->probe_pin,
                                           app->probe_current, app->transient_step, app->transient_stop};
                app->status = workbench_save_project(app->selftest ? "build/gui-settings-test.elec" : "bancada.elec",
                                                     &app->bench, &saved)
                                  ? "Salvo em bancada.elec."
                                  : "Falha ao salvar.";
            }
            else if (e.key.key == SDLK_L)
            {
                Workbench before = app->bench;
                WorkbenchSettings saved;
                if (workbench_load_project(app->selftest ? "build/gui-settings-test.elec" : "bancada.elec", &app->bench,
                                           &saved))
                {
                    bench_history_record(app->history, &before);
                    app->selected = -1;
                    app->selected_wire = -1;
                    app->dragging = -1;
                    app->drag_wire = -1;
                    app->pending.component = -1;
                    app->route_count = 0;
                    app->show_grid = saved.show_grid;
                    app->show_handles = saved.show_handles;
                    app->round_wires = saved.round_wires;
                    app->show_current = saved.show_current;
                    app->wire_color = saved.wire_color;
                    app->probe_pin = saved.probe_pin;
                    app->probe_current = saved.probe_current;
                    app->transient_step = saved.transient_step;
                    app->transient_stop = saved.transient_stop;
                    app->sim.dc_valid = 0;
                    app->sim.dc_wire_valid = 0;
                    app->sim.trace_valid = 0;
                    memset(app->leds, 0, sizeof(app->leds));
                    bench_simulate(&app->bench, app->leds);
                    app->status = "Bancada e configuracoes carregadas.";
                }
                else
                    app->status = "Falha ao carregar; bancada preservada.";
            }
            else if (e.key.key == SDLK_DELETE && app->selected >= 0)
            {
                bench_history_record(app->history, &app->bench);
                bench_remove(&app->bench, app->selected);
                app->selected = -1;
                app->selected_wire = -1;
                app->pending.component = -1;
                memset(app->leds, 0, sizeof(app->leds));
                app->status = "Componente e seus fios removidos.";
            }
            else if (e.key.key == SDLK_DELETE && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
            {
                bench_history_record(app->history, &app->bench);
                memmove(&app->bench.wires[app->selected_wire], &app->bench.wires[app->selected_wire + 1],
                        (size_t)(app->bench.wire_count - app->selected_wire - 1) * sizeof(Wire));
                --app->bench.wire_count;
                app->selected_wire = -1;
                app->status = "Fio removido.";
            }
        }
        else if (e.type == SDL_EVENT_MOUSE_WHEEL && !app->settings && app->editing < 0)
        {
            int w, h;
            float x, y, direction = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
            SDL_GetWindowSize(window, &w, &h);
            if (w > 0 && h > 0)
            {
                x = e.wheel.mouse_x * 1280.0f / w;
                y = e.wheel.mouse_y * 720.0f / h;
                if (x >= 220 && y >= 70 && y < 660 && direction != 0)
                    bench_view_zoom_at(&app->view, direction > 0 ? 1.2f : 1.0f / 1.2f, x, y);
            }
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT && !app->settings &&
                 app->editing < 0)
        {
            /* Right click rotates the part under the cursor, the same as selecting it and pressing R. */
            int w, h, hit;
            float x, y;
            SDL_GetWindowSize(window, &w, &h);
            if (w <= 0 || h <= 0)
                continue;
            x = e.button.x * 1280.0f / w;
            y = e.button.y * 720.0f / h;
            if (x < 220 || y < 70 || y > 660)
                continue;
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
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE && !app->settings &&
                 app->editing < 0)
        {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            if (w > 0 && h > 0 && e.button.x * 1280.0f / w >= 220 && e.button.y * 720.0f / h >= 70 &&
                e.button.y * 720.0f / h < 660)
                app->panning = 1;
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_MIDDLE)
            app->panning = 0;
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->panning && app->editing < 0)
        {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            if (w > 0 && h > 0)
            {
                app->view.pan_x += e.motion.xrel * 1280.0f / w;
                app->view.pan_y += e.motion.yrel * 720.0f / h;
            }
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            int w, h, hit = -1, hitpin = -1;
            float x, y;
            SDL_GetWindowSize(window, &w, &h);
            if (w <= 0 || h <= 0)
                continue;
            x = e.button.x * 1280.0f / w;
            y = e.button.y * 720.0f / h;
            if (x >= 700 && x < 850 && y < 68)
            {
                SDL_Event shortcut = {0};
                shortcut.type = SDL_EVENT_KEY_DOWN;
                shortcut.key.key = SDLK_F2;
                SDL_PushEvent(&shortcut);
                continue;
            }
            if (x >= 880 && x < 1030 && y < 68)
            {
                app->settings = !app->settings;
                app->dragging = -1;
                app->drag_wire = -1;
                continue;
            }
            if (x >= PLAY_BUTTON_X && y < 68)
            {
                SDL_Event shortcut = {0};
                shortcut.type = SDL_EVENT_KEY_DOWN;
                shortcut.key.key = SDLK_SPACE;
                SDL_PushEvent(&shortcut);
                continue;
            }
            if (!app->settings && app->selected >= 0 && app->bench.parts[app->selected].type != PART_BREADBOARD &&
                x >= ROTATE_BUTTON_X && x < ROTATE_BUTTON_X + 72 && y >= 86 && y < 112)
            {
                SDL_Event shortcut = {0};
                shortcut.type = SDL_EVENT_KEY_DOWN;
                shortcut.key.key = SDLK_R;
                SDL_PushEvent(&shortcut);
                continue;
            }
            if (app->settings)
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
                        app->probe_pin =
                            (app->probe_pin + 1) %
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
                                app->bench.wires[app->selected_wire].color = chosen;
                                app->status = "Cor do fio selecionado alterada.";
                            }
                            else
                                app->status = "Cor dos proximos fios selecionada.";
                        }
                    }
                }
                continue;
            }
            if (app->sim.trace_valid && app->show_trace && x >= 840 && x <= 1240 && y >= 384 && y <= 628)
            {
                if (x >= 918 && x <= 1220 && y >= 431 && y <= 565)
                {
                    app->sim.trace_index = spice_trace_nearest(&app->sim.trace, (x - 918) / 302.0);
                    app->sim.sim_running = 0;
                    snprintf(app->sim.trace_status, sizeof(app->sim.trace_status),
                             app->sim.trace_current == 2 ? "Corrente fio A -> B: t=%.6g s; I=%.6g A (%d/%d)."
                             : app->sim.trace_current    ? "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (%d/%d)."
                                                         : "Tensao: t=%.6g s; V=%.6g V (%d/%d).",
                             app->sim.trace.samples[app->sim.trace_index].time,
                             app->sim.trace.samples[app->sim.trace_index].value, app->sim.trace_index + 1,
                             app->sim.trace.count);
                    app->status = app->sim.trace_status;
                }
                continue;
            }
            if (x < 220 && y >= 80 && y < 125)
            {
                app->palette_page = 1 - app->palette_page;
                app->placing = -1;
                continue;
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
                continue;
            }
            if (x < 220 || y < 70 || y > 660)
                continue;
            bench_view_to_world(&app->view, x, y, &x, &y);
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
                    if (e.button.clicks >= 2)
                    {
                        Wire *wire = &app->bench.wires[app->drag_wire];
                        bench_history_record(app->history, &app->bench);
                        memmove(&wire->knees[app->drag_knee], &wire->knees[app->drag_knee + 1],
                                (size_t)(wire->knee_count - app->drag_knee - 1) * sizeof(WirePoint));
                        --wire->knee_count;
                        app->drag_wire = -1;
                        app->status = "Joelho removido.";
                    }
                    else
                    {
                        app->move_recorded = 0;
                        app->status = "Arraste o joelho; duplo clique remove.";
                    }
                    continue;
                }
            }
            if (app->placing >= 0)
            {
                Workbench before = app->bench;
                int added = bench_add(&app->bench, (PartType)app->placing, x, y);
                if (added >= 0)
                {
                    int snapped = bench_snap_to_breadboard(&app->bench, added);
                    bench_history_record(app->history, &before);
                    app->selected_wire = -1;
                    app->status = snapped ? "Componente encaixado na breadboard."
                                          : "Componente adicionado. Selecione terminais para ligar.";
                    app->placing = -1;
                }
                else
                    app->status = "Limite de componentes atingido.";
                continue;
            }
            {
                int pass;
                for (pass = 0; pass < 2 && hit < 0; ++pass)
                    for (i = app->bench.part_count - 1; i >= 0; --i)
                    {
                        int p, board = app->bench.parts[i].type == PART_BREADBOARD;
                        float radius = board ? 4.0f : app->bench.parts[i].type == PART_74HC00 ? 5.0f : 12.0f;
                        if (board != pass)
                            continue;
                        for (p = 0; p < bench_pin_count(app->bench.parts[i].type); ++p)
                        {
                            float px, py;
                            bench_pin_position(&app->bench.parts[i], p, &px, &py);
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
            }
            if (hitpin >= 0)
            {
                Pin clicked = {hit, hitpin};
                app->selected_wire = -1;
                if (app->pending.component < 0)
                {
                    app->pending = clicked;
                    app->route_count = 0;
                    app->status = "Clique para criar joelhos; termine em outro terminal.";
                }
                else if (app->pending.component == hit && app->pending.pin == hitpin)
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
                        Wire *wire = &app->bench.wires[app->bench.wire_count - 1];
                        wire->knee_count = app->route_count;
                        wire->color = app->wire_color;
                        memcpy(wire->knees, app->route, (size_t)app->route_count * sizeof(WirePoint));
                    }
                    if (result)
                        bench_history_record(app->history, &before);
                    app->status = result == 1   ? "Fio conectado."
                                  : result == 2 ? "Fio removido."
                                                : "Nao foi possivel conectar.";
                    app->pending.component = -1;
                    app->route_count = 0;
                }
                continue;
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
                continue;
            }
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
                    continue;
                }
            }
            if (hit >= 0)
            {
                if (e.button.clicks >= 2 &&
                    (app->bench.parts[hit].type == PART_SOURCE || app->bench.parts[hit].type == PART_SWITCH ||
                     app->bench.parts[hit].type == PART_CLOCK))
                {
                    bench_history_record(app->history, &app->bench);
                    app->bench.parts[hit].on = !app->bench.parts[hit].on;
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
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT)
        {
            if (app->dragging >= 0 && app->move_recorded && bench_snap_to_breadboard(&app->bench, app->dragging))
                app->status = "Componente encaixado na breadboard.";
            app->dragging = -1;
            app->drag_wire = -1;
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->drag_wire >= 0)
        {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            if (w > 0 && h > 0 && app->drag_wire < app->bench.wire_count)
            {
                float x = e.motion.x * 1280.0f / w, y = e.motion.y * 720.0f / h;
                WirePoint *point = &app->bench.wires[app->drag_wire].knees[app->drag_knee];
                bench_view_to_world(&app->view, x, y, &x, &y);
                if (point->x != x || point->y != y)
                {
                    if (!app->move_recorded)
                    {
                        bench_history_record(app->history, &app->bench);
                        app->move_recorded = 1;
                    }
                    *point = (WirePoint){x, y};
                }
            }
        }
        else if (e.type == SDL_EVENT_MOUSE_MOTION && app->dragging >= 0)
        {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            if (w > 0 && h > 0)
            {
                float x = e.motion.x * 1280.0f / w, y = e.motion.y * 720.0f / h;
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
                    app->bench.parts[app->dragging].x = x;
                    app->bench.parts[app->dragging].y = y;
                }
            }
        }
    }
}
