#include "app_commands.h"
#include "app_palette.h"
#include "workbench_examples.h"
#include "workbench_file.h"
#include <stdio.h>
#include <string.h>

static void command_example(AppState *app)
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
    return;
}

static void command_palette(AppState *app)
{
    app->palette_page = 1 - app->palette_page;
    app->placing = -1;
    app->status = app->palette_page ? "Biblioteca de portas logicas." : "Biblioteca de componentes.";
    return;
}

static void command_settings(AppState *app)
{
    app->settings = !app->settings;
    app->dragging = -1;
    app->drag_wire = -1;
    return;
}

static void command_home(AppState *app)
{
    app->view = (BenchView){1, 0, 0};
    app->status = "Vista restaurada.";
    return;
}

static void command_history(AppState *app, SDL_Keycode key)
{
    int changed =
        key == SDLK_Z ? bench_history_undo(app->history, &app->bench) : bench_history_redo(app->history, &app->bench);
    if (changed)
    {
        app->selected = -1;
        app->selected_wire = -1;
        app->dragging = -1;
        app->drag_wire = -1;
        app->pending.component = -1;
        app->route_count = 0;
        memset(app->leds, 0, sizeof(app->leds));
        app->status = key == SDLK_Z ? "Acao desfeita." : "Acao refeita.";
    }
    else
        app->status = "Nenhuma acao disponivel.";
    return;
}

static void command_cancel(AppState *app)
{
    app->pending.component = -1;
    app->route_count = 0;
    app->placing = -1;
    app->dragging = -1;
    app->drag_wire = -1;
    app->status = "Selecao cancelada.";
}

static void command_playback(AppState *app)
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
        app->status = app->sim.sim_running ? "Simulacao em execucao: o circuito e calculado automaticamente."
                                           : "Simulacao pausada. Espaco ou o botao de play continua.";
    else
        app->status = app->sim.sim_running ? "Simulacao executando." : "Simulacao pausada.";
}

static void command_recalculate(AppState *app)
{
    app->sim.auto_requested_valid = 0;
    app->sim.bench_changed_at = 0;
    app->sim.sim_running = 1;
    app->status = "Recalculando o circuito...";
}

static void command_rotate(AppState *app)
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

static void command_graph(AppState *app)
{
    app->show_trace = !app->show_trace;
    app->status = app->show_trace ? "Grafico visivel." : "Grafico oculto; pressione G para mostrar.";
}

static void command_trace_wire(AppState *app)
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

static int start_part_trace(AppState *app)
{
    if (app->show_current)
        return spice_job_start_transient_instrumented(
            &app->dc_job, &app->bench, app->ngspice, app->probe_current ? 2 : 1,
            (Pin){app->selected, app->probe_current ? 0 : app->probe_pin}, app->transient_step, app->transient_stop);
    if (app->probe_current)
        return spice_job_start_transient_current(&app->dc_job, &app->bench, app->ngspice, app->selected,
                                                 app->transient_step, app->transient_stop);
    return spice_job_start_transient(&app->dc_job, &app->bench, app->ngspice, (Pin){app->selected, app->probe_pin},
                                     app->transient_step, app->transient_stop);
}

static void command_trace_part(AppState *app)
{
    if (app->dc_job.thread)
        app->status = "Analise em andamento.";
    else if (!app->probe_current && app->probe_pin >= bench_pin_count(app->bench.parts[app->selected].type))
        app->status = "Terminal indisponivel para este componente.";
    else if (app->probe_current &&
             (is_gate(app->bench.parts[app->selected].type) || app->bench.parts[app->selected].type == PART_74HC00))
        app->status = "Corrente interna da porta nao disponivel.";
    else if (app->probe_current && app->bench.parts[app->selected].type == PART_SWITCH)
        app->status = "Corrente transitoria indisponivel para interruptor ideal.";
    else if (start_part_trace(app))
    {
        app->sim.trace_valid = 0;
        app->sim.dc_valid = 0;
        app->sim.dc_wire_valid = 0;
        app->status = "Calculando transitorio selecionado...";
    }
    else
        app->status = "Falha ao iniciar tarefa de analise.";
}

static void command_trace_step(AppState *app, SDL_Keycode key)
{
    if (key == SDLK_N && app->sim.trace_index + 1 < app->sim.trace.count)
        ++app->sim.trace_index;
    if (key == SDLK_B && app->sim.trace_index > 0)
        --app->sim.trace_index;
    const char *format = "Tensao: t=%.6g s; V=%.6g V (%d/%d).";
    if (app->sim.trace_current == 2)
        format = "Corrente fio A -> B: t=%.6g s; I=%.6g A (%d/%d).";
    else if (app->sim.trace_current)
        format = "Corrente pino 1 -> 2: t=%.6g s; I=%.6g A (%d/%d).";
    snprintf(app->sim.trace_status, sizeof(app->sim.trace_status), format,
             app->sim.trace.samples[app->sim.trace_index].time, app->sim.trace.samples[app->sim.trace_index].value,
             app->sim.trace_index + 1, app->sim.trace.count);
    app->status = app->sim.trace_status;
}

static void command_logic_step(AppState *app)
{
    if (bench_boolean_supported(&app->bench))
    {
        bench_simulate(&app->bench, app->leds);
        app->status = "Um passo executado.";
    }
    else
        app->status = "Calculo eletrico ainda indisponivel.";
}

static void command_dc(AppState *app)
{
    if (app->dc_job.thread)
        app->status = "Analise em andamento.";
    else if ((app->show_current ? spice_job_start_wires : spice_job_start)(&app->dc_job, &app->bench, app->ngspice))
    {
        app->sim.trace_valid = 0;
        app->sim.dc_valid = 0;
        app->sim.dc_wire_valid = 0;
        app->status = "Analisando ponto DC...";
    }
    else
        app->status = "Falha ao iniciar tarefa de analise.";
}

static void command_edit_value(AppState *app, SDL_Window *window)
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

static void command_scale_value(AppState *app, SDL_Keycode key)
{
    double scale = key == SDLK_MINUS ? .5 : 2.0;
    Workbench before = app->bench;
    if (bench_set_value(&app->bench, app->selected, app->bench.parts[app->selected].value * scale))
    {
        bench_history_record(app->history, &before);
        app->status = "Valor alterado.";
    }
    else
        app->status = "Valor nao editavel ou fora do limite.";
}

static void command_save(AppState *app)
{
    WorkbenchSettings saved = {app->show_grid,     app->show_handles,   app->round_wires,
                               app->show_current,  app->wire_color,     app->probe_pin,
                               app->probe_current, app->transient_step, app->transient_stop};
    app->status =
        workbench_save_project(app->selftest ? "build/gui-settings-test.elec" : "bancada.elec", &app->bench, &saved)
            ? "Salvo em bancada.elec."
            : "Falha ao salvar.";
}

static void command_load(AppState *app)
{
    Workbench before = app->bench;
    WorkbenchSettings saved;
    if (workbench_load_project(app->selftest ? "build/gui-settings-test.elec" : "bancada.elec", &app->bench, &saved))
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

static void command_delete_part(AppState *app)
{
    bench_history_record(app->history, &app->bench);
    bench_remove(&app->bench, app->selected);
    app->selected = -1;
    app->selected_wire = -1;
    app->pending.component = -1;
    memset(app->leds, 0, sizeof(app->leds));
    app->status = "Componente e seus fios removidos.";
}

static void command_delete_wire(AppState *app)
{
    bench_history_record(app->history, &app->bench);
    memmove(&app->bench.wires[app->selected_wire], &app->bench.wires[app->selected_wire + 1],
            (size_t)(app->bench.wire_count - app->selected_wire - 1) * sizeof(Wire));
    --app->bench.wire_count;
    app->selected_wire = -1;
    app->status = "Fio removido.";
}

static void command_selection(AppState *app, SDL_Window *window, SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_R:
        command_rotate(app);
        break;
    case SDLK_T:
        if (app->bench.parts[app->selected].type != PART_BREADBOARD)
            command_trace_part(app);
        break;
    case SDLK_E:
        command_edit_value(app, window);
        break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_MINUS:
        command_scale_value(app, key);
        break;
    case SDLK_DELETE:
        command_delete_part(app);
        break;
    }
}

void app_commands_key(AppState *app, SDL_Window *window, SDL_Keycode key, SDL_Keymod mod)
{
    if ((mod & SDL_KMOD_CTRL) && (key == SDLK_Z || key == SDLK_Y))
    {
        command_history(app, key);
        return;
    }
    switch (key)
    {
    case SDLK_F2:
        command_example(app);
        return;
    case SDLK_TAB:
        command_palette(app);
        return;
    case SDLK_F1:
        command_settings(app);
        return;
    case SDLK_HOME:
        command_home(app);
        return;
    case SDLK_ESCAPE:
        command_cancel(app);
        return;
    case SDLK_SPACE:
        command_playback(app);
        return;
    case SDLK_P:
        if (app->sim.auto_enabled)
            command_recalculate(app);
        else
            command_dc(app);
        return;
    case SDLK_G:
        if (app->sim.trace_valid)
            command_graph(app);
        return;
    case SDLK_N:
    case SDLK_B:
        if (!app->sim.sim_running && app->sim.trace_valid)
            command_trace_step(app, key);
        else if (key == SDLK_N && !app->sim.sim_running)
            command_logic_step(app);
        return;
    case SDLK_S:
        command_save(app);
        return;
    case SDLK_L:
        command_load(app);
        return;
    }
    if (key == SDLK_T && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
        command_trace_wire(app);
    else if (app->selected >= 0)
        command_selection(app, window, key);
    else if (key == SDLK_DELETE && app->selected_wire >= 0 && app->selected_wire < app->bench.wire_count)
        command_delete_wire(app);
}
