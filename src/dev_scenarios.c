#include "dev_scenarios.h"
#include "component_registry.h"
#include "dev_capture.h"
#include "app_editor.h"
#include "app_packing.h"
#include "workbench_group_name.h"
#include "workbench_examples.h"
#include "workbench_pack.h"
#include "workbench_file.h"
#include "spice_trace.h"
#include "app_layout.h"
#include "app_palette.h"
#include "ui_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int component_closeup(AppState *app, const char *argument)
{
    const char *id = NULL;
    int dark = 0;
    if (!strncmp(argument, "--capture-component=", 20))
        id = argument + 20;
    else if (!strncmp(argument, "--capture-component-dark=", 25))
    {
        id = argument + 25;
        dark = 1;
    }
    if (!id)
        return 0;
    const ComponentInfo *info = component_by_id(id);
    if (!info)
    {
        fprintf(stderr, "Componente desconhecido para close-up: %s\n", id);
        return -1;
    }
    bench_snapshot_clear(&app->bench);
    if (bench_add(&app->bench, info->type, 750, 365) < 0)
        return -1;
    float zoom = info->type == PART_BREADBOARD ? 1.8f : 7;
    Part *part = &app->bench.parts[0];
    float min_y = part->y - 48, max_y = part->y + 45;
    for (int pin = 0; pin < info->pin_count; ++pin)
    {
        float px, py;
        bench_pin_position(part, pin, &px, &py);
        min_y = fminf(min_y, py - 5);
        max_y = fmaxf(max_y, py + 5);
    }
    if (info->type != PART_BREADBOARD)
        zoom = fminf(zoom, 520 / (max_y - min_y));
    float center_y = (min_y + max_y) * .5f;
    app->view = (BenchView){zoom, 530 * (1 - zoom), 295 - (center_y - 70) * zoom};
    app->selected = -1;
    app->dark_theme = dark;
    app->show_grid = 0;
    app->show_handles = 0;
    app->sim.sim_running = 0;
    snprintf(app->selection_status, sizeof(app->selection_status), "Revisao visual: %s | zoom %.1fx | modelo nominal.",
             info->display_name, zoom);
    app->status = app->selection_status;
    return 1;
}

int dev_scenario_prepare(AppState *app, int argc, char **argv)
{
    if (argc > 1)
    {
        int closeup = component_closeup(app, argv[1]);
        if (closeup)
            return closeup < 0;
    }
    /* Capture each catalog page without changing the saved workspace. */
    if (argc > 1 && !strncmp(argv[1], "--capture-palette=", 18))
    {
        char *end;
        long page = strtol(argv[1] + 18, &end, 10);
        if (end == argv[1] + 18 || *end || page < 0 || page >= PALETTE_PAGES)
            return 1;
        app->palette_page = (int)page;
        app->palette_offset = 0;
        app->sim.sim_running = 0;
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--capture-environment"))
    {
        app->selected = bench_add(&app->bench, PART_LDR, 600, 400);
        app->sim.sim_running = 0;
        return 0;
    }
    /* Review a saved project as the user sees it: whole circuit fitted at the chosen zoom limits. */
    if (argc > 1 && !strncmp(argv[1], "--capture-file=", 15))
    {
        /* Optional "@top,bottom" frames only the parts in that height band (close-up review). */
        char path[512];
        float band_top = -1e9f, band_bottom = 1e9f;
        snprintf(path, sizeof(path), "%s", argv[1] + 15);
        char *band = strchr(path, '@');
        if (band)
        {
            *band = 0;
            if (sscanf(band + 1, "%f,%f", &band_top, &band_bottom) != 2)
                return 1;
        }
        WorkbenchSettings settings;
        if (!workbench_load_project(path, &app->bench, &settings))
            return 1;
        float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
        for (int i = 0; i < app->bench.part_count; ++i)
        {
            /* Chip members are hidden: frame the package instead of their inner positions. */
            const Part *part = &app->bench.parts[i];
            float x = part->group ? app->bench.groups[part->group - 1].x : part->x;
            float y = part->group ? app->bench.groups[part->group - 1].y : part->y;
            if (y < band_top || y > band_bottom)
                continue;
            min_x = fminf(min_x, x - 60);
            min_y = fminf(min_y, y - 60);
            max_x = fmaxf(max_x, x + 60);
            max_y = fmaxf(max_y, y + 60);
        }
        bench_view_fit(&app->view, min_x, min_y, max_x, max_y);
        app->sim.sim_running = 0;
        return 0;
    }
    if (argc > 1 && (!strcmp(argv[1], "--capture-inspector") || !strcmp(argv[1], "--capture-inspector-minimized")))
    {
        app->selected = bench_add(&app->bench, PART_LED, 600, 400);
        app->inspector_minimized = !strcmp(argv[1], "--capture-inspector-minimized");
        app->sim.sim_running = 0;
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--capture-dark"))
    {
        app->dark_theme = 1;
        app->settings = 1;
    }
    if (argc > 1 && strcmp(argv[1], "--capture-projects") == 0)
        app->projects_open = 1;
    if (argc > 1 && strcmp(argv[1], "--capture-examples") == 0)
        app->examples_open = 1;
    if (argc > 1 && strcmp(argv[1], "--capture-settings") == 0)
        app->settings = 1;
    if (app->capture)
    {
        bench_add(&app->bench, PART_SOURCE, 360, 220);
        bench_add(&app->bench, PART_LED, 720, 220);
        bench_add(&app->bench, PART_BREADBOARD, 550, 430);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        app->bench.wires[0].knee_count = 2;
        app->bench.wires[0].knees[0] = (WirePoint){308, 130};
        app->bench.wires[0].knees[1] = (WirePoint){668, 130};
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){0, 1});
        app->bench.wires[1].knee_count = 2;
        app->bench.wires[1].knees[0] = (WirePoint){772, 300};
        app->bench.wires[1].knees[1] = (WirePoint){412, 300};
        if (strcmp(argv[1], "--capture-colors") == 0)
        {
            app->bench.wires[0].color = 1;
            app->bench.wires[1].color = 3;
            app->wire_color = 1;
            app->selected_wire = 0;
            app->settings = 1;
        }
        if (strcmp(argv[1], "--capture-analog") == 0)
        {
            bench_add(&app->bench, PART_RESISTOR, 380, 520);
            bench_add(&app->bench, PART_CAPACITOR, 720, 520);
            bench_add(&app->bench, PART_INDUCTOR, 970, 520);
        }
        if (strcmp(argv[1], "--capture-snap") == 0)
        {
            int part = bench_add(&app->bench, PART_RESISTOR, 538, 377);
            bench_snap_to_breadboard(&app->bench, part);
            bench_view_zoom_at(&app->view, 1.2f, 550, 430);
        }
        if (strcmp(argv[1], "--capture-rotation") == 0)
        {
            int part = bench_add(&app->bench, PART_RESISTOR, 538, 393);
            bench_rotate_part(&app->bench, part);
            bench_snap_to_breadboard(&app->bench, part);
            bench_view_zoom_at(&app->view, 1.2f, 550, 430);
        }
        if (strcmp(argv[1], "--capture-board") == 0)
        {
            bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){2, 100});
            app->bench.wires[2].knee_count = 1;
            app->bench.wires[2].knees[0] = (WirePoint){431, 300};
            bench_toggle_wire(&app->bench, (Pin){0, 1}, (Pin){2, 140});
            app->bench.wires[3].knee_count = 1;
            app->bench.wires[3].knees[0] = (WirePoint){380, 441};
            bench_view_zoom_at(&app->view, 1.4f, 550, 430);
        }
        if (strcmp(argv[1], "--capture-dc") == 0 || strcmp(argv[1], "--capture-current") == 0 ||
            strcmp(argv[1], "--capture-edit") == 0)
        {
            bench_add(&app->bench, PART_RESISTOR, 530, 220);
            app->selected = 3;
            bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
            bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){3, 0});
            app->bench.wires[1].knee_count = 2;
            app->bench.wires[1].knees[0] = (WirePoint){308, 150};
            app->bench.wires[1].knees[1] = (WirePoint){478, 150};
            bench_toggle_wire(&app->bench, (Pin){3, 1}, (Pin){1, 0});
            for (int wire = 0; wire < app->bench.wire_count; ++wire)
                bench_set_wire_color(&app->bench, wire, wire == 0 ? 0 : wire == 1 ? 1 : 3);
        }
    }
    if (app->capture && (!strcmp(argv[1], "--capture-live-rc") || !strcmp(argv[1], "--capture-live-off") ||
                         !strcmp(argv[1], "--capture-wire-panel")))
    {
        bench_snapshot_clear(&app->bench);
        bench_add(&app->bench, PART_SOURCE, 350, 300);
        bench_add(&app->bench, PART_RESISTOR, 550, 300);
        bench_add(&app->bench, PART_CAPACITOR, 750, 300);
        bench_add(&app->bench, PART_RESISTOR, 1000, 300);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){2, 0});
        bench_toggle_wire(&app->bench, (Pin){2, 0}, (Pin){3, 0});
        bench_toggle_wire(&app->bench, (Pin){2, 1}, (Pin){0, 1});
        bench_toggle_wire(&app->bench, (Pin){3, 1}, (Pin){0, 1});
        bench_set_wire_route(&app->bench, 0, (WirePoint[]){{301, 180}, {501, 180}}, 2);
        bench_set_wire_route(&app->bench, 2, (WirePoint[]){{701, 200}, {951, 200}}, 2);
        bench_set_wire_route(&app->bench, 3, (WirePoint[]){{799, 450}, {399, 450}}, 2);
        bench_set_wire_route(&app->bench, 4, (WirePoint[]){{1049, 520}, {399, 520}}, 2);
        app->selected = 3;
        app->dark_theme = 1;
        if (!strcmp(argv[1], "--capture-wire-panel"))
        {
            app->selected = -1;
            app->selected_wire = 2;
            app->show_current = 1;
            app->dark_theme = 0;
            bench_set_wire_color(&app->bench, 2, BENCH_COLOR_ORANGE);
        }
    }
    if (app->capture && !strcmp(argv[1], "--capture-mirror"))
    {
        /* Top row as placed, bottom row mirrored: terminals swap sides, displays stay readable. */
        static const PartType types[] = {PART_SOURCE, PART_SEVEN_SEGMENT, PART_MULTIMETER, PART_OSCILLOSCOPE,
                                         PART_BATTERY_9V, PART_DIODE, PART_LED, PART_AND};
        const int count = (int)(sizeof(types) / sizeof(types[0]));
        bench_snapshot_clear(&app->bench);
        for (int row = 0; row < 2; ++row)
            for (int i = 0; i < count; ++i)
            {
                int part = bench_add(&app->bench, types[i], 300 + i * 120.0f, 200 + row * 220.0f);
                if (part >= 0 && row)
                    bench_mirror_part(&app->bench, part);
            }
        app->selected = -1;
    }
    if (app->capture && !strcmp(argv[1], "--capture-live-graph"))
    {
        bench_snapshot_clear(&app->bench);
        bench_add(&app->bench, PART_CLOCK, 400, 300);
        bench_add(&app->bench, PART_RESISTOR, 650, 300);
        bench_set_value(&app->bench, 0, 100);
        bench_set_on(&app->bench, 0, 1);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){0, 1});
        app->transient_stop = .05;
        app->selected = 1;
        sim_live_graph_open(&app->live_graph, (SimLiveProbe){SIM_LIVE_PROBE_VOLTAGE, {1, 0}, 0}, &app->bench);
        app->show_trace = 1;
    }
    if (app->capture && !strcmp(argv[1], "--capture-leads"))
    {
        bench_snapshot_clear(&app->bench);
        bench_add(&app->bench, PART_SOURCE, 350, 300);
        bench_add(&app->bench, PART_RESISTOR, 550, 300);
        bench_add(&app->bench, PART_LED, 750, 300);
        bench_lead_move(&app->bench, 1, 0, 0, (WirePoint){301, 300});
        bench_lead_insert(&app->bench, 1, 0, 0, (WirePoint){450, 240});
        bench_lead_move(&app->bench, 2, 0, 0, (WirePoint){599, 300});
        bench_lead_insert(&app->bench, 2, 0, 0, (WirePoint){650, 360});
        bench_lead_move(&app->bench, 2, 1, 0, (WirePoint){399, 300});
        bench_lead_insert(&app->bench, 2, 1, 0, (WirePoint){820, 450});
        bench_lead_insert(&app->bench, 2, 1, 1, (WirePoint){400, 450});
        app->selected = 2;
        app->dark_theme = 1;
        app->status = "Sem fios: terminais em contato fecham o circuito. Alt+arraste / Alt+duplo clique.";
    }
    if (app->capture && !strcmp(argv[1], "--capture-catalog"))
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_POTENTIOMETER, 500, 260);
        bench_add(&app->bench, PART_BUTTON, 750, 260);
        bench_add(&app->bench, PART_NOR, 500, 460);
        bench_add(&app->bench, PART_XNOR, 750, 460);
        bench_add(&app->bench, PART_74HC08, 400, 590);
        bench_add(&app->bench, PART_74HC32, 650, 590);
        bench_add(&app->bench, PART_74HC86, 900, 590);
        bench_add(&app->bench, PART_LED, 1000, 350);
        bench_add(&app->bench, PART_LED, 1100, 460);
        bench_rotate_part(&app->bench, 8);
        bench_toggle_wire(&app->bench, (Pin){2, 2}, (Pin){3, 0});
        bench_set_wire_color(&app->bench, 0, 3);
        app->selected = 0;
        app->dark_theme = 1;
        app->palette_offset = 2;
        app->status = "Biblioteca: scroll | Potenciometro: Shift+scroll | Botao: segure e solte.";
    }
    if (app->capture && !strcmp(argv[1], "--capture-scale"))
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_BREADBOARD, 550, 430);
        bench_add(&app->bench, PART_RESISTOR, 550, 395);
        bench_set_scale(&app->bench, 1, 1.0f / 7);
        app->selected = 1;
        app->status = "Resistor reduzido entre dois furos vizinhos. Selecione e clique em Tamanho para ajustar.";
    }
    if (app->capture && !strcmp(argv[1], "--capture-knee-edit"))
    {
        bench_insert_knee(&app->bench, 0, 1, (WirePoint){500, 130});
        bench_move_knee(&app->bench, 0, 1, (WirePoint){500, 240});
        app->selected_wire = 0;
        app->status = "Duplo clique no trecho adiciona joelho; no joelho remove. Arraste para dobrar; Ctrl+Z desfaz.";
    }
    if (app->capture && strcmp(argv[1], "--capture-and") == 0)
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_SOURCE, 340, 270);
        bench_add(&app->bench, PART_AND, 570, 270);
        bench_add(&app->bench, PART_RESISTOR, 760, 270);
        bench_add(&app->bench, PART_LED, 960, 270);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        app->bench.wires[0].knee_count = 2;
        app->bench.wires[0].knees[0] = (WirePoint){291, 175};
        app->bench.wires[0].knees[1] = (WirePoint){521, 175};
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 1});
        app->bench.wires[1].knee_count = 2;
        app->bench.wires[1].knees[0] = (WirePoint){291, 320};
        app->bench.wires[1].knees[1] = (WirePoint){521, 320};
        bench_toggle_wire(&app->bench, (Pin){1, 2}, (Pin){2, 0});
        bench_toggle_wire(&app->bench, (Pin){2, 1}, (Pin){3, 0});
        bench_toggle_wire(&app->bench, (Pin){3, 1}, (Pin){0, 1});
        app->bench.wires[4].knee_count = 2;
        app->bench.wires[4].knees[0] = (WirePoint){1009, 400};
        app->bench.wires[4].knees[1] = (WirePoint){389, 400};
        app->selected = 1;
    }
    if (app->capture && strcmp(argv[1], "--capture-logic") == 0)
    {
        int kind;
        app->bench = (Workbench){0};
        app->palette_page = 1;
        for (kind = PART_AND; kind <= PART_NOT; ++kind)
            bench_add(&app->bench, (PartType)kind, 360 + (kind - PART_AND) * 170, 300);
        app->selected = 4;
    }
    if (app->capture && strcmp(argv[1], "--capture-clock") == 0)
    {
        app->bench = (Workbench){0};
        app->palette_page = 1;
        bench_add(&app->bench, PART_CLOCK, 400, 250);
        bench_add(&app->bench, PART_NOT, 650, 250);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        app->selected = 0;
    }
    if (app->capture && strcmp(argv[1], "--capture-ci") == 0)
    {
        app->bench = (Workbench){0};
        app->palette_page = 1;
        bench_add(&app->bench, PART_BREADBOARD, 650, 380);
        bench_add(&app->bench, PART_74HC00, 650, 380);
        bench_snap_to_breadboard(&app->bench, 1);
        app->selected = 1;
        bench_view_zoom_at(&app->view, 1.3f, 650, 380);
    }
    int numbered_example = app->capture && !strncmp(argv[1], "--capture-example=", 18);
    if (numbered_example || (app->capture && (strcmp(argv[1], "--capture-example0") == 0 ||
                                               strcmp(argv[1], "--capture-example1") == 0 ||
                                               strcmp(argv[1], "--capture-example2") == 0)))
    {
        int which = numbered_example ? atoi(argv[1] + 18)
                    : strcmp(argv[1], "--capture-example2") == 0 ? 2
                                                                  : strcmp(argv[1], "--capture-example1") == 0;
        if (!workbench_example_create(which, &app->bench, &app->selected))
        {
            fprintf(stderr, "Falha no exemplo de captura.\n");
            return 1;
        }
        app->probe_pin = which == 1 || which == 2 ? 2 : 0;
        app->status = workbench_example_name(which);
    }
    if (app->capture && (strcmp(argv[1], "--capture-auto") == 0 || strcmp(argv[1], "--capture-auto-clock") == 0))
    {
        int which = strcmp(argv[1], "--capture-auto-clock") == 0;
        if (!workbench_example_create(which, &app->bench, &app->selected))
        {
            fprintf(stderr, "Falha no exemplo de captura.\n");
            return 1;
        }
        app->probe_pin = which == 1 || which == 2 ? 2 : 0;
    }
    if (app->capture && strcmp(argv[1], "--capture-burn") == 0)
    {
        /* LED straight on the source burns; the LED with a resistor in parallel keeps working. */
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_SOURCE, 360, 260);
        bench_add(&app->bench, PART_LED, 700, 180);
        bench_add(&app->bench, PART_RESISTOR, 600, 380);
        bench_add(&app->bench, PART_LED, 820, 380);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        app->bench.wires[0].knee_count = 1;
        app->bench.wires[0].knees[0] = (WirePoint){311, 180};
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){0, 1});
        app->bench.wires[1].knee_count = 2;
        app->bench.wires[1].knees[0] = (WirePoint){780, 300};
        app->bench.wires[1].knees[1] = (WirePoint){409, 300};
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){2, 0});
        app->bench.wires[2].knee_count = 1;
        app->bench.wires[2].knees[0] = (WirePoint){311, 380};
        bench_toggle_wire(&app->bench, (Pin){2, 1}, (Pin){3, 0});
        bench_toggle_wire(&app->bench, (Pin){3, 1}, (Pin){0, 1});
        app->bench.wires[4].knee_count = 2;
        app->bench.wires[4].knees[0] = (WirePoint){900, 470};
        app->bench.wires[4].knees[1] = (WirePoint){409, 470};
        app->selected = 1;
    }
    if (app->capture && strcmp(argv[1], "--capture-zoomout") == 0)
        bench_view_zoom_at(&app->view, .3f, 750, 365);
    if (argc > 1 && strcmp(argv[1], "--capture-selection") == 0)
    {
        app->selecting_area = 1;
        app->area_start = (WirePoint){280, 150};
        app->area_end = (WirePoint){800, 490};
        bench_selection_rect(&app->selection, &app->bench, app->area_start, app->area_end);
        app->status = "Selecao por area: Ctrl+C/X/V, Delete e arraste em grupo.";
    }
    if (argc > 1 && (strcmp(argv[1], "--capture-packed") == 0 ||
                     strcmp(argv[1], "--capture-packed-rotated") == 0 || strncmp(argv[1], "--capture-editor", 16) == 0))
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_SOURCE, 360, 250);
        bench_add(&app->bench, PART_RESISTOR, 620, 250);
        bench_add(&app->bench, PART_LED, 820, 250);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){2, 0});
        bench_toggle_wire(&app->bench, (Pin){2, 1}, (Pin){0, 1});
        bench_set_wire_route(&app->bench, 0, (WirePoint[]){{311, 170}, {660, 170}}, 2);
        bench_set_wire_route(&app->bench, 2, (WirePoint[]){{780, 380}, {409, 380}}, 2);
        app->selection.parts[1] = app->selection.parts[2] = 1;
        if (!bench_pack_selection(&app->bench, &app->selection, app->selection_status, sizeof(app->selection_status)))
            return 1;
        app->selected = 1;
        bench_group_set_name(&app->bench, 1, "Indicador \xc3\xa1rvore");
        if (!strcmp(argv[1], "--capture-editor-nested") || !strcmp(argv[1], "--capture-editor-deep"))
        {
            int outer = bench_pack_selection(&app->bench, &app->selection, app->selection_status, sizeof(app->selection_status));
            if (!outer || !bench_group_set_name(&app->bench, outer, "Indicador composto"))
                return 1;
        }
        if (!strcmp(argv[1], "--capture-packed-rotated"))
        {
            bench_group_resize(&app->bench, 1, 72, 180);
            bench_rotate_part(&app->bench, app->selected);
        }
        app->editor_open_requested = strncmp(argv[1], "--capture-editor", 16) == 0;
        app->palette_page = 2;
        app->library_available[0] = 1;
        app->status = "Fonte externa alimenta resistor e LED encapsulados; terminais preservados.";
    }
    if (argc > 1 && strcmp(argv[1], "--capture-pins") == 0)
    {
        if (!workbench_example_create(2, &app->bench, &app->selected))
            return 1;
        app->hovered_pin = (Pin){2, 13};
        app->hover_revision = app->bench.revision;
        app->status = "Pinagem do 74HC00: I entrada, O saida, G terra, + alimentacao. Aponte para um terminal.";
    }
    if (argc > 1 && strcmp(argv[1], "--capture-pin-led") == 0)
    {
        app->selected = 1;
        app->hovered_pin = (Pin){1, 1};
        app->hover_revision = app->bench.revision;
        app->status = "LED: A anodo, K catodo. A funcao construtiva e separada da tensao medida.";
    }
    return 0;
}

/* Live graph capture: a few synchronous live batches feed the graph before the first frame. */
static int capture_live_graph(AppState *app)
{
    const int batches = 5;
    const double batch_seconds = .01;
    SimAutoOptions options = {0, {1, 0}, sim_auto_live_max_step(&app->bench), app->transient_stop,
                              burn_limits_default(), app->live_graph.probe};
    options.live_probe.interval = app->transient_stop / 400;
    for (int batch = 0; batch < batches; ++batch)
    {
        if (!spice_job_start_live(&app->dc_job, &app->bench, app->ngspice, &options, batch_seconds))
            return 1;
        while (!SDL_GetAtomicInt(&app->dc_job.done))
            SDL_Delay(1);
        spice_job_poll(&app->dc_job);
        if (!app->dc_job.success)
        {
            fprintf(stderr, "%s\n", app->dc_job.message);
            return 1;
        }
        sim_live_graph_append(&app->live_graph, &app->dc_job.auto_result.live_samples);
    }
    app->status = tr(TXT_APP_LIVE_GRAPH_OPENED);
    return 0;
}

int dev_scenario_analyze(AppState *app, char **argv)
{
    if (app->capture && !strcmp(argv[1], "--capture-live-graph") && capture_live_graph(app))
        return 1;
    if (app->capture && (!strcmp(argv[1], "--capture-live-rc") || !strcmp(argv[1], "--capture-live-off") ||
                         !strcmp(argv[1], "--capture-wire-panel")))
    {
        SimAutoOptions options = {1, {2, 0}, 1e-5, .005, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
        if (!spice_job_start_live(&app->dc_job, &app->bench, app->ngspice, &options, .005))
            return 1;
        while (!SDL_GetAtomicInt(&app->dc_job.done))
            SDL_Delay(1);
        if (!app->dc_job.success)
        {
            fprintf(stderr, "%s\n", app->dc_job.message);
            return 1;
        }
        if (!strcmp(argv[1], "--capture-live-off"))
        {
            spice_job_poll(&app->dc_job);
            bench_set_on(&app->bench, 0, 0);
            if (!spice_job_start_live(&app->dc_job, &app->bench, app->ngspice, &options, .001))
                return 1;
            while (!SDL_GetAtomicInt(&app->dc_job.done))
                SDL_Delay(1);
        }
    }

    if (app->auto_capture)
    {
        /* Captures need the result in the first frame: start the analysis now and wait for it. */
        SimAutoOptions options = {app->show_current, sim_state_probe(&app->bench, app->selected, app->probe_pin),
                                  app->transient_step, app->transient_stop, burn_limits_default(), {SIM_LIVE_PROBE_NONE, {0, 0}, 0}};
        if (spice_job_start_auto(&app->dc_job, &app->bench, app->ngspice, &options))
        {
            while (!SDL_GetAtomicInt(&app->dc_job.done))
                SDL_Delay(5);
            app->sim.requested_revision = app->bench.revision;
            app->sim.auto_requested_valid = 1;
            app->sim.auto_show_current = app->show_current;
            app->sim.auto_step = app->transient_step;
            app->sim.auto_stop = app->transient_stop;
        }
    }
    if (app->capture && strcmp(argv[1], "--capture-zoom") == 0)
        bench_view_zoom_at(&app->view, 1.8f, 550, 430);
    if (app->capture && strcmp(argv[1], "--capture-edit") == 0)
    {
        app->editing = 3;
        strcpy(app->edit_text, "2200");
    }
    if (app->capture && (strcmp(argv[1], "--capture-dc") == 0 || strcmp(argv[1], "--capture-current") == 0))
    {
        app->show_current = strcmp(argv[1], "--capture-current") == 0;
        if (app->show_current)
        {
            sim_state_set_result(&app->sim, SIM_RESULT_DC,
                                 spice_dc_analyze_wires(&app->bench, app->ngspice, &app->sim.dc_current,
                                                        app->sim.dc_led_currents, app->sim.dc_wire_currents,
                                                        app->sim.dc_status, sizeof(app->sim.dc_status)),
                                 app->bench.revision);
            app->sim.dc_wire_valid = sim_state_has_result(&app->sim, SIM_RESULT_DC);
        }
        else
            sim_state_set_result(&app->sim, SIM_RESULT_DC,
                                 spice_dc_analyze(&app->bench, app->ngspice, &app->sim.dc_current,
                                                  app->sim.dc_led_currents, app->sim.dc_status,
                                                  sizeof(app->sim.dc_status)),
                                 app->bench.revision);
        app->status = app->sim.dc_status;
    }
    if (app->capture && (strcmp(argv[1], "--capture-trace") == 0 || strcmp(argv[1], "--capture-trace-current") == 0 ||
                         strcmp(argv[1], "--capture-trace-wire") == 0 || strcmp(argv[1], "--capture-play") == 0))
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_SOURCE, 360, 220);
        bench_add(&app->bench, PART_RESISTOR, 550, 220);
        bench_add(&app->bench, PART_CAPACITOR, 720, 220);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){2, 0});
        bench_toggle_wire(&app->bench, (Pin){2, 1}, (Pin){0, 1});
        app->sim.trace_current =
            strcmp(argv[1], "--capture-trace-wire") == 0 ? 2 : strcmp(argv[1], "--capture-trace-current") == 0;
        if (app->sim.trace_current == 2)
        {
            app->sim.trace_wire = 0;
            app->show_current = 1;
            app->bench.wires[0].knee_count = 2;
            app->bench.wires[0].knees[0] = (WirePoint){311, 150};
            app->bench.wires[0].knees[1] = (WirePoint){501, 150};
        }
        sim_state_set_result(
            &app->sim, SIM_RESULT_TRACE,
            app->sim.trace_current == 2
                ? spice_transient_wire(&app->bench, app->ngspice, 0, app->transient_step, app->transient_stop,
                                       &app->sim.trace, app->sim.dc_status, sizeof(app->sim.dc_status))
            : app->sim.trace_current
                ? spice_transient_current(&app->bench, app->ngspice, 1, app->transient_step, app->transient_stop,
                                          &app->sim.trace, app->sim.dc_status, sizeof(app->sim.dc_status))
                : spice_transient_analyze(&app->bench, app->ngspice, (Pin){2, 0}, app->transient_step,
                                          app->transient_stop, &app->sim.trace, app->sim.dc_status,
                                          sizeof(app->sim.dc_status)),
            app->bench.revision);
        if (sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
        {

            app->sim.trace_index = app->sim.trace.count / 3;
            app->sim.sim_running = 0;
            app->status = "Sondagem RC calculada.";
            if (strcmp(argv[1], "--capture-play") == 0)
            {
                app->sim.trace_play_start_index = 0;
                app->sim.trace_play_started = SDL_GetTicks() - 2000;
                app->sim.sim_running = 1;
                app->status = "Reproducao da curva: intervalo completo em 5 s reais.";
            }
        }
        else
        {
            fprintf(stderr, "Captura transitoria: %s\n", app->sim.dc_status);
            return 1;
        }
    }
    if (app->capture && (strcmp(argv[1], "--capture-led-on") == 0 || strcmp(argv[1], "--capture-led-off") == 0))
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_CLOCK, 400, 250);
        bench_add(&app->bench, PART_RESISTOR, 600, 250);
        bench_add(&app->bench, PART_LED, 800, 250);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        app->bench.wires[0].knee_count = 2;
        app->bench.wires[0].knees[0] = (WirePoint){351, 150};
        app->bench.wires[0].knees[1] = (WirePoint){551, 150};
        bench_toggle_wire(&app->bench, (Pin){1, 1}, (Pin){2, 0});
        bench_toggle_wire(&app->bench, (Pin){2, 1}, (Pin){0, 1});
        app->bench.wires[2].knee_count = 2;
        app->bench.wires[2].knees[0] = (WirePoint){849, 370};
        app->bench.wires[2].knees[1] = (WirePoint){449, 370};
        app->show_current = 1;
        sim_state_set_result(&app->sim, SIM_RESULT_TRACE,
                             spice_transient_instrumented(&app->bench, app->ngspice, 1, (Pin){0, 0}, 1e-5, .002,
                                                          &app->sim.trace, app->sim.dc_status,
                                                          sizeof(app->sim.dc_status)),
                             app->bench.revision);
        if (!sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
        {
            fprintf(stderr, "Captura LED: %s\n", app->sim.dc_status);
            return 1;
        }

        app->sim.trace_index =
            spice_trace_nearest(&app->sim.trace, strcmp(argv[1], "--capture-led-on") == 0 ? .125 : .375);
        app->sim.sim_running = 0;
        app->status = "LED acompanha corrente transitoria calculada.";
    }
    if (app->capture && strcmp(argv[1], "--capture-dff") == 0)
    {
        app->bench = (Workbench){0};
        app->palette_page = 1;
        bench_add(&app->bench, PART_CLOCK, 350, 200);
        bench_add(&app->bench, PART_CLOCK, 350, 350);
        bench_set_value(&app->bench, 1, 400);
        bench_add(&app->bench, PART_DFF, 620, 260);
        bench_add(&app->bench, PART_RESISTOR, 780, 260);
        bench_add(&app->bench, PART_LED, 1010, 260);
        bench_toggle_wire(&app->bench, (Pin){0, 1}, (Pin){1, 1});
        bench_toggle_wire(&app->bench, (Pin){1, 0}, (Pin){2, 0});
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){2, 1});
        bench_toggle_wire(&app->bench, (Pin){2, 2}, (Pin){3, 0});
        bench_toggle_wire(&app->bench, (Pin){3, 1}, (Pin){4, 0});
        bench_toggle_wire(&app->bench, (Pin){4, 1}, (Pin){0, 1});
        sim_state_set_result(&app->sim, SIM_RESULT_TRACE,
                             spice_transient_analyze(&app->bench, app->ngspice, (Pin){2, 2}, 1e-5, .0035,
                                                     &app->sim.trace, app->sim.dc_status, sizeof(app->sim.dc_status)),
                             app->bench.revision);
        if (!sim_state_has_result(&app->sim, SIM_RESULT_TRACE))
        {
            fprintf(stderr, "Captura DFF: %s\n", app->sim.dc_status);
            return 1;
        }

        app->sim.trace_index = spice_trace_nearest(&app->sim.trace, .43);
        app->sim.trace_pin = 2;
        app->sim.sim_running = 0;
        app->selected = 2;
        app->status = "DFF: saida retida entre bordas do clock.";
    }
    return 0;
}

static int move_pointer(SDL_Window *window, float x, float y)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.x = x;
    event.motion.y = y;
    return SDL_PushEvent(&event);
}

static int click_rect(SDL_Window *window, AppRect rect)
{
    return dev_test_click(window, app_rect_center_x(rect), app_rect_center_y(rect));
}

static int click_palette(SDL_Window *window, int slot)
{
    return click_rect(window, app_layout_palette_item(slot));
}

static int click_setting(SDL_Window *window, AppSetting setting)
{
    return click_rect(window, app_layout_setting(setting));
}

static float slider_x(double position)
{
    AppRect track = app_layout_environment_track();
    return track.x + (float)(track.w * position);
}

static int first_saved_block(const AppState *app)
{
    for (int slot = 0; slot < BLOCK_LIBRARY_SLOTS; ++slot)
        if (app->library_available[slot])
            return slot;
    return -1;
}

static int reports_project_saved(const AppState *app)
{
    char expected[sizeof(app->selection_status)];
    snprintf(expected, sizeof(expected), tr(TXT_APP_PROJECT_SAVED), app->save_slot + 1);
    return !strcmp(app->status, expected);
}

static int deleted_block = -1;

static WirePoint junction_test_middle(const AppState *app)
{
    float ax, ay, bx, by;
    bench_pin_position(&app->bench.parts[0], 0, &ax, &ay);
    bench_pin_position(&app->bench.parts[1], 0, &bx, &by);
    return (WirePoint){(ax + bx) / 2, (ay + by) / 2};
}

static int queue_junction_phase(AppState *app, SDL_Window *window)
{
    float x, y;
    WirePoint middle;
    if (app->selftest_phase == 229)
    {
        app->bench = (Workbench){0};
        app->view = (BenchView){1, 0, 0};
        app->round_wires = 0;
        bench_selection_clear(&app->selection);
        app->pending.component = -1;
        bench_add(&app->bench, PART_SOURCE, 400, 250);
        bench_add(&app->bench, PART_RESISTOR, 800, 250);
        bench_add(&app->bench, PART_RESISTOR, 600, 480);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        middle = junction_test_middle(app);
        bench_pin_position(&app->bench.parts[2], 0, &x, &y);
        return dev_test_click(window, x, y) && dev_test_drag_finish(window, x, y) &&
               dev_test_click(window, middle.x, middle.y) && dev_test_drag_finish(window, middle.x, middle.y);
    }
    if (app->selftest_phase == 230)
        return dev_test_shortcut(window, SDLK_Z, SDL_KMOD_CTRL);
    if (app->selftest_phase == 231)
    {
        middle = junction_test_middle(app);
        SDL_SetModState(SDL_KMOD_ALT);
        return dev_test_click(window, middle.x, middle.y) && dev_test_drag_finish(window, middle.x, middle.y);
    }
    bench_pin_position(&app->bench.parts[2], 1, &x, &y);
    return dev_test_click(window, x, y) && dev_test_drag_finish(window, x, y);
}

static int junction_phase_valid(const AppState *app)
{
    const Workbench *b = &app->bench;
    int junction = b->part_count == 4 && b->parts[3].type == PART_JUNCTION;
    if (app->selftest_phase == 229)
        return junction && b->wire_count == 3 && app->pending.component < 0;
    if (app->selftest_phase == 230)
        return b->part_count == 3 && b->wire_count == 1;
    if (app->selftest_phase == 231)
        return junction && b->wire_count == 2 && app->pending.component == 3;
    return junction && b->wire_count == 3 && app->pending.component < 0;
}

void dev_scenario_queue(AppState *app, SDL_Window *window)
{

    SDL_SetModState(SDL_KMOD_NONE);
    int queued = 1;
    if (app->selftest_phase == 0)
        queued = click_palette(window, 0) && dev_test_click(window, 400, 250);
    else if (app->selftest_phase == 1)
        queued = click_palette(window, 4) && dev_test_click(window, 600, 250);
    else if (app->selftest_phase == 2)
        queued =
            dev_test_click(window, 351, 250) && dev_test_click(window, 450, 180) && dev_test_click(window, 551, 250);
    else if (app->selftest_phase == 3)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 4)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 5)
        queued = dev_test_click(window, 450, 180) && dev_test_drag_finish(window, 460, 190);
    else if (app->selftest_phase == 6)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 7)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 8)
        queued = dev_test_click_count(window, 460, 190, 2);
    else if (app->selftest_phase == 9)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 10)
        queued = dev_test_wheel(window, 400, 250, 1);
    else if (app->selftest_phase == 11)
        queued = dev_test_click(window, 341.2f, 250);
    else if (app->selftest_phase == 12)
        queued = dev_test_click(window, 581.2f, 250);
    else if (app->selftest_phase == 13)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 14)
        queued = dev_test_key(window, SDLK_HOME);
    else if (app->selftest_phase == 15)
        queued = dev_test_pan(window);
    else if (app->selftest_phase == 16)
        queued = dev_test_click(window, 451, 290);
    else if (app->selftest_phase == 17)
        queued = dev_test_key(window, SDLK_ESCAPE);
    else if (app->selftest_phase == 18)
        queued = dev_test_key(window, SDLK_HOME);
    else if (app->selftest_phase == 19)
        queued = click_palette(window, 3) && dev_test_click(window, 850, 430);
    else if (app->selftest_phase == 20)
        queued = click_palette(window, 4) && dev_test_click(window, 838, 377);
    else if (app->selftest_phase == 21)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 22)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 23)
        queued = dev_test_click(window, 836, 375);
    else if (app->selftest_phase == 24)
        queued = dev_test_key(window, SDLK_R);
    else if (app->selftest_phase == 25)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 26)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 27)
        queued = dev_test_click(window, 505, 220);
    else if (app->selftest_phase == 28)
        queued = click_rect(window, app_layout_color_swatch(BENCH_COLOR_RED));
    else if (app->selftest_phase == 29)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 30)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 31)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 32)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 33)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 34)
        queued = click_setting(window, APP_SETTING_PROBE_PIN);
    else if (app->selftest_phase == 35)
        queued = click_setting(window, APP_SETTING_STEP);
    else if (app->selftest_phase == 36)
        queued = dev_test_key(window, SDLK_RETURN);
    else if (app->selftest_phase == 37)
        queued = click_setting(window, APP_SETTING_STOP);
    else if (app->selftest_phase == 38)
        queued = dev_test_key(window, SDLK_ESCAPE);
    else if (app->selftest_phase == 39)
        queued = click_setting(window, APP_SETTING_STEP);
    else if (app->selftest_phase == 40)
        queued = dev_test_replace_text(window, "2e-5");
    else if (app->selftest_phase == 41)
        queued = click_setting(window, APP_SETTING_STEP);
    else if (app->selftest_phase == 42)
        queued = dev_test_replace_text(window, "1e-9");
    else if (app->selftest_phase == 43)
        queued = dev_test_key(window, SDLK_ESCAPE);
    else if (app->selftest_phase == 44)
    {
        app->sim.trace.count = 3;
        app->sim.trace.samples[0] = (SpiceSample){0, 0};
        app->sim.trace.samples[1] = (SpiceSample){1, 2};
        app->sim.trace.samples[2] = (SpiceSample){2, 3};
        app->sim.trace_index = 0;
        sim_state_set_result(&app->sim, SIM_RESULT_TRACE, 1, app->bench.revision);

        app->sim.sim_running = 0;
        queued = dev_test_key(window, SDLK_F1);
    }
    else if (app->selftest_phase == 45)
        queued = dev_test_key(window, SDLK_N);
    else if (app->selftest_phase == 46)
        queued = dev_test_key(window, SDLK_B);
    else if (app->selftest_phase == 47)
        queued = dev_test_click(window, 1219, 500);
    else if (app->selftest_phase == 48)
        queued = dev_test_key(window, SDLK_N);
    else if (app->selftest_phase == 49)
        queued = dev_test_key(window, SDLK_B);
    else if (app->selftest_phase == 50)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 51)
        queued = click_setting(window, APP_SETTING_PROBE_MEASURE);
    else if (app->selftest_phase == 52)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 53)
        queued = dev_test_click(window, 505, 220);
    else if (app->selftest_phase == 54)
        queued = dev_test_key(window, SDLK_T);
    else if (app->selftest_phase == 55)
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 56)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 57)
        queued = click_setting(window, APP_SETTING_PROBE_MEASURE);
    else if (app->selftest_phase == 58)
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 59)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 60)
        queued = click_palette(window, 7) && dev_test_click(window, 1000, 300);
    else if (app->selftest_phase == 61)
        queued = dev_test_key(window, SDLK_TAB);
    else if (app->selftest_phase == 62)
        queued = click_palette(window, 3) && dev_test_click(window, 1100, 400);
    else if (app->selftest_phase == 63)
        queued = click_palette(window, 5) && dev_test_click(window, 1000, 500);
    else if (app->selftest_phase == 64)
        queued = dev_test_click_count(window, 1000, 500, 2);
    else if (app->selftest_phase == 65)
    {
        app->sim.trace.count = 3;
        app->sim.trace.samples[0] = (SpiceSample){0, 0};
        app->sim.trace.samples[1] = (SpiceSample){1, 2};
        app->sim.trace.samples[2] = (SpiceSample){2, 3};
        app->sim.trace_index = 0;
        sim_state_set_result(&app->sim, SIM_RESULT_TRACE, 1, app->bench.revision);

        app->sim.sim_running = 0;
        queued = dev_test_key(window, SDLK_SPACE);
    }
    else if (app->selftest_phase == 66)
        queued = dev_test_key(window, SDLK_SPACE);
    else if (app->selftest_phase == 67)
        queued = dev_test_key(window, SDLK_SPACE);
    else if (app->selftest_phase == 68)
    {
        app->sim.trace_play_started = SDL_GetTicks() - 6000;
        queued = dev_test_key(window, SDLK_UNKNOWN);
    }
    else if (app->selftest_phase == 69)
        queued = dev_test_key(window, SDLK_UNKNOWN);
    else if (app->selftest_phase == 70)
    {
        bench_add(&app->bench, PART_LED, 1200, 500);

        sim_state_set_result(&app->sim, SIM_RESULT_TRACE, 1, app->bench.revision);
        app->sim.trace_index = 0;
        app->sim.trace.leds_on[0] = UINT64_C(1) << 7;
        app->sim.trace.leds_on[1] = 0;
        app->sim.sim_running = 0;
        queued = dev_test_key(window, SDLK_UNKNOWN);
    }
    else if (app->selftest_phase == 71)
    {
        app->sim.trace_index = 1;
        queued = dev_test_key(window, SDLK_UNKNOWN);
    }
    else if (app->selftest_phase == 72)
        queued = dev_test_key(window, SDLK_UNKNOWN);
    else if (app->selftest_phase == 73)
        queued = dev_test_key(window, SDLK_ESCAPE) && click_palette(window, 6) && dev_test_click(window, 750, 550);
    else if (app->selftest_phase == 74)
        queued = dev_test_key(window, SDLK_ESCAPE) && click_palette(window, 7) && dev_test_click(window, 750, 450);
    else if (app->selftest_phase == 75)
        queued = dev_test_key(window, SDLK_F2);
    else if (app->selftest_phase == 76)
        queued = dev_test_key(window, SDLK_F2);
    else if (app->selftest_phase == 77)
        queued = dev_test_key(window, SDLK_F2);
    else if (app->selftest_phase == 78)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 79)
        queued = dev_test_click(window, 250, 150);
    else if (app->selftest_phase == 80)
        queued = dev_test_drag_finish(window, 1040, 600);
    else if (app->selftest_phase == 81)
        queued = dev_test_shortcut(window, SDLK_C, SDL_KMOD_CTRL) && dev_test_shortcut(window, SDLK_V, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 82)
        queued = dev_test_key(window, SDLK_DELETE);
    else if (app->selftest_phase == 83 || app->selftest_phase == 84)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 85)
        queued = dev_test_key(window, SDLK_F3);
    else if (app->selftest_phase == 86)
        queued = click_rect(window, app_layout_project_slot(8));
    else if (app->selftest_phase == 87)
        queued = dev_test_key(window, SDLK_ESCAPE);
    else if (app->selftest_phase == 88)
        queued = dev_test_shortcut(window, SDLK_A, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 89)
        queued = dev_test_shortcut(window, SDLK_G, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 90)
        queued = click_palette(window, 0);
    else if (app->selftest_phase == 91)
        queued = dev_test_click(window, 1200, 500);
    else if (app->selftest_phase == 92)
        queued = dev_test_shortcut(window, SDLK_G, SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
    else if (app->selftest_phase == 93 || app->selftest_phase == 98)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 94)
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 95)
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 96)
        queued = dev_test_click(window, 620, 280);
    else if (app->selftest_phase == 97)
        queued = dev_test_drag_finish(window, 660, 320);
    else if (app->selftest_phase == 99)
        queued = dev_test_key(window, SDLK_F3);
    else if (app->selftest_phase == 100)
        queued = dev_test_key(window, SDLK_ESCAPE);
    else if (app->selftest_phase == 101)
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_LED, 300, 200);
        app->selected = 0;
        app->show_trace = 0;
        bench_selection_clear(&app->selection);
        queued = dev_test_drag_finish(window, 251, 200);
    }
    else if (app->selftest_phase == 102)
        queued = dev_test_drag_finish(window, 349, 200);
    else if (app->selftest_phase == 103)
        queued = dev_test_drag_finish(window, 700, 400);
    else if (app->selftest_phase == 104)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 105)
        queued = dev_test_drag_finish(window, 251, 200);
    else if (app->selftest_phase == 106)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 107)
    {
        app->bench = (Workbench){0};
        bench_selection_clear(&app->selection);
        bench_add(&app->bench, PART_RESISTOR, 500, 250);
        bench_add(&app->bench, PART_LED, 700, 250);
        bench_toggle_wire(&app->bench, (Pin){0, 1}, (Pin){1, 0});
        app->selection.parts[0] = app->selection.parts[1] = 1;
        bench_pack_selection(&app->bench, &app->selection, app->selection_status, sizeof(app->selection_status));
        app->selected = 0;
        queued = dev_test_key(window, SDLK_F4); /* No rename entry point on the outer bench. */
    }
    else if (app->selftest_phase == 108 || app->selftest_phase == 116)
        queued = click_rect(window, app_layout_group_button(0));
    else if (app->selftest_phase == 109 || app->selftest_phase == 117)
        queued = click_rect(window, app_layout_editor_header(APP_EDITOR_RENAME));
    else if (app->selftest_phase == 110)
        queued = dev_test_replace_text(window, "Indicador \xc3\xa1rvore");
    else if (app->selftest_phase == 111)
    {
        bench_set_value(&app->editor_child->bench, 0, 2200);
        queued = dev_test_key(window, SDLK_HOME);
    }
    else if (app->selftest_phase == 118)
    {
        bench_set_value(&app->editor_child->bench, 0, 3300);
        queued = dev_test_replace_text(window, "Driver \xc3\xa1rvore");
    }
    else if (app->selftest_phase == 112 || app->selftest_phase == 114)
        queued = click_rect(window, app_layout_editor_header(APP_EDITOR_BACK));
    else if (app->selftest_phase == 113)
        queued = click_rect(window, app_layout_confirm_button(2));
    else if (app->selftest_phase == 115)
        queued = click_rect(window, app_layout_confirm_button(1));
    else if (app->selftest_phase == 119)
        queued = click_rect(window, app_layout_editor_header(APP_EDITOR_APPLY));
    else if (app->selftest_phase == 120)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 121)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 122)
        queued = click_setting(window, APP_SETTING_SNAP_GRID);
    else if (app->selftest_phase == 123)
        queued = click_setting(window, APP_SETTING_GRID_SPACING);
    else if (app->selftest_phase == 124)
        queued = dev_test_replace_text(window, "16");
    else if (app->selftest_phase == 125)
        queued = click_setting(window, APP_SETTING_SNAP_BOARD);
    else if (app->selftest_phase == 126)
    {
        app->palette_page = 0;
        queued = dev_test_key(window, SDLK_F1);
    }
    else if (app->selftest_phase == 127)
        queued = click_palette(window, 4) && dev_test_click(window, 530, 257);
    else if (app->selftest_phase == 128)
        queued = dev_test_click(window, 528, 260) && dev_test_drag_finish(window, 559, 283);
    else if (app->selftest_phase == 129)
    {
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click(window, 560, 276) && dev_test_drag_finish(window, 569, 281);
    }
    else if (app->selftest_phase == 130)
    {
        SDL_SetModState(SDL_KMOD_NONE);
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    }
    else if (app->selftest_phase == 131)
    {
        app->grid_spacing = 24;
        app->snap_grid = 0;
        app->snap_board = 1;
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    }
    else if (app->selftest_phase == 132)
    {
        app->bench = (Workbench){0};
        app->view = (BenchView){1, 0, 0};
        app->settings = 0;
        app->selected = -1;
        app->show_handles = 1;
        bench_selection_clear(&app->selection);
        bench_add(&app->bench, PART_SOURCE, 400, 200);
        bench_add(&app->bench, PART_LED, 700, 200);
        bench_toggle_wire(&app->bench, (Pin){0, 0}, (Pin){1, 0});
        queued = dev_test_click_count(window, 500, 200, 2);
    }
    else if (app->selftest_phase == 133)
        queued = dev_test_click_count(window, 500, 200, 2);
    else if (app->selftest_phase == 134)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 135)
        queued = dev_test_click_count(window, 620, 200, 2);
    else if (app->selftest_phase == 136)
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 137)
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 138)
    {
        app->bench = (Workbench){0};
        app->view = (BenchView){1, 0, 0};
        bench_add(&app->bench, PART_BREADBOARD, 550, 430);
        bench_add(&app->bench, PART_RESISTOR, 650, 200);
        app->selected = 1;
        app->snap_board = 0;
        app->snap_grid = 0;
        queued = click_rect(window, app_layout_inspector_scale());
    }
    else if (app->selftest_phase == 139)
        queued = dev_test_replace_text(window, "25");
    else if (app->selftest_phase == 140)
        queued = dev_test_click(window, 650, 200) && dev_test_drag_finish(window, 700, 250);
    else if (app->selftest_phase == 141 || app->selftest_phase == 142)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 143)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 144)
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 145)
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 146)
    {
        app->selected = 1;
        SDL_SetModState(SDL_KMOD_CTRL);
        queued = dev_test_wheel(window, 650, 200, 1);
    }
    else if (app->selftest_phase == 147)
    {
        SDL_SetModState(SDL_KMOD_NONE);
        queued = dev_test_key(window, SDLK_Z);
    }
    else if (app->selftest_phase == 148)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 149)
        queued = click_setting(window, APP_SETTING_DARK_THEME);
    else if (app->selftest_phase == 150)
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 151)
    {
        app->dark_theme = 0;
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    }
    else if (app->selftest_phase == 152)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 153)
    {
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click(window, 650, 200);
    }
    else if (app->selftest_phase == 154)
    {
        SDL_SetModState(SDL_KMOD_NONE);
        queued = dev_test_key(window, SDLK_UNKNOWN);
    }
    else if (app->selftest_phase == 155)
    {
        app->bench = (Workbench){0};
        bench_add(&app->bench, PART_POTENTIOMETER, 500, 300);
        bench_add(&app->bench, PART_BUTTON, 800, 300);
        app->selected = 0;
        app->palette_page = 0;
        app->palette_offset = 0;
        queued = dev_test_wheel(window, 50, 400, -1);
    }
    else if (app->selftest_phase == 156)
        queued = dev_test_wheel(window, 50, 400, -1);
    else if (app->selftest_phase == 157)
        queued = click_palette(window, 7);
    else if (app->selftest_phase == 158)
    {
        app->placing = -1;
        app->selected = 0;
        SDL_SetModState(SDL_KMOD_SHIFT);
        queued = dev_test_wheel(window, 500, 300, 1);
    }
    else if (app->selftest_phase == 159)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 160)
        queued = dev_test_click(window, 800, 300);
    else if (app->selftest_phase == 161)
        queued = dev_test_drag_finish(window, 800, 300);
    else if (app->selftest_phase == 162)
        queued = dev_test_key(window, SDLK_TAB);
    else if (app->selftest_phase == 163)
        queued = dev_test_wheel(window, 50, 400, -2);
    else if (app->selftest_phase == 164)
        queued = dev_test_key(window, SDLK_ESCAPE) && dev_test_wheel(window, 50, 400, -1) && click_palette(window, 6);
    else if (app->selftest_phase == 165)
        queued = dev_test_click(window, 700, 500);
    else if (app->selftest_phase == 166)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 167)
        queued = click_setting(window, APP_SETTING_FONT_SMALLER);
    else if (app->selftest_phase == 168)
    {
        app->settings = 0;
        app->placing = -1;
        bench_selection_clear(&app->selection);
        app->selection.parts[0] = 1;
        app->selection.parts[1] = 1;
        char message[256];
        if (!bench_pack_selection(&app->bench, &app->selection, message, sizeof(message)))
        {
            app->exit_code = 1;
            app->running = 0;
            return;
        }
        app->selected = 0;
        SDL_SetModState(SDL_KMOD_CTRL);
        queued = dev_test_wheel(window, 500, 300, 1);
    }
    else if (app->selftest_phase == 169)
    {
        SDL_SetModState(SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
        queued = dev_test_wheel(window, 500, 300, 1);
    }
    else if (app->selftest_phase == 170)
    {
        SDL_SetModState(SDL_KMOD_CTRL);
        queued = dev_test_key(window, SDLK_Z);
    }
    else if (app->selftest_phase == 171)
    {
        bench_snapshot_clear(&app->bench);
        bench_add(&app->bench, PART_LED, 500, 300);
        bench_add(&app->bench, PART_RESISTOR, 800, 300);
        app->selected = 0;
        app->pending.component = -1;
        app->placing = -1;
        app->settings = 0;
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click(window, 451, 300) && dev_test_drag_finish(window, 600, 350);
    }
    else if (app->selftest_phase == 172)
    {
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click_count(window, 545, 334, 2);
    }
    else if (app->selftest_phase == 173)
    {
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click(window, 545, 334) && dev_test_drag_finish(window, 560, 400);
    }
    else if (app->selftest_phase == 174)
    {
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click_count(window, 560, 400, 2);
    }
    else if (app->selftest_phase == 175)
    {
        SDL_SetModState(SDL_KMOD_ALT);
        queued = dev_test_click(window, 600, 350) && dev_test_drag_finish(window, 753, 301);
    }
    else if (app->selftest_phase == 176 || app->selftest_phase == 177)
    {
        SDL_SetModState(SDL_KMOD_CTRL);
        queued = dev_test_key(window, app->selftest_phase == 176 ? SDLK_Z : SDLK_Y);
    }
    else if (app->selftest_phase == 178)
        queued = dev_test_shortcut(window, SDLK_S, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 179)
        queued = dev_test_shortcut(window, SDLK_L, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 180)
    {
        SDL_SetModState(SDL_KMOD_CTRL);
        queued = dev_test_key(window, SDLK_SPACE);
    }
    else if (app->selftest_phase == 181)
    {
        int chip = bench_add(&app->bench, PART_74HC08, 800, 500);
        app->selected = chip;
        app->settings = 1;
        app->probe_pin = 12;
        queued = click_setting(window, APP_SETTING_PROBE_PIN);
    }
    else if (app->selftest_phase == 182)
        queued = click_setting(window, APP_SETTING_PROBE_PIN);
    else if (app->selftest_phase == 183)
    {
        app->settings = 0;
        app->selected = bench_add(&app->bench, PART_LDR, 600, 400);
        queued = dev_test_click(window, slider_x(0), app_rect_center_y(app_layout_environment_track())) && dev_test_drag_finish(window, slider_x(.75), app_rect_center_y(app_layout_environment_track()));
    }
    else if (app->selftest_phase == 184 || app->selftest_phase == 185)
    {
        SDL_SetModState(SDL_KMOD_CTRL);
        queued = dev_test_key(window, app->selftest_phase == 184 ? SDLK_Z : SDLK_Y);
    }
    else if (app->selftest_phase == 186)
    {
        app->selected = app->bench.part_count - 1;
        SDL_SetModState(SDL_KMOD_SHIFT);
        queued = dev_test_wheel(window, 700, 400, 1);
    }
    else if (app->selftest_phase == 187)
    {
        SDL_SetModState(SDL_KMOD_NONE);
        queued = click_rect(window, app_layout_header(APP_HEADER_EXAMPLES));
    }
    else if (app->selftest_phase == 188)
        queued = click_rect(window, app_layout_example_card(13));
    else if (app->selftest_phase == 189)
    {
        bench_snapshot_clear(&app->bench);
        bench_selection_clear(&app->selection);
        app->view = (BenchView){1, 0, 0};
        app->snap_grid = 0;
        app->selected = -1;
        bench_add(&app->bench, PART_RESISTOR, 500, 300);
        bench_add(&app->bench, PART_LED, 760, 300);
        float anode_x, anode_y;
        bench_pin_position(&app->bench.parts[1], 0, &anode_x, &anode_y);
        queued = dev_test_click(window, 500, 300) && dev_test_drag_finish(window, anode_x - 49 + 5, anode_y + 3);
    }
    else if (app->selftest_phase == 190 || app->selftest_phase == 191)
    {
        app->palette_page = 2;
        app_packing_refresh(app);
        if (app->selftest_phase == 190)
            deleted_block = first_saved_block(app);
        queued = deleted_block >= 0 && click_rect(window, app_layout_palette_delete(deleted_block));
    }
    /* Human flow: select and rotate a chip, dock a switch, separate, reconnect, undo/redo. */
    else if (app->selftest_phase == 192)
    {
        bench_snapshot_clear(&app->bench);
        bench_selection_clear(&app->selection);
        bench_history_clear(app->history);
        app->view = (BenchView){1, 0, 0};
        app->snap_grid = 0;
        app->selected = -1;
        bench_add(&app->bench, PART_SWITCH, 500, 300);
        app->selection.parts[0] = 1;
        queued = bench_pack_selection(&app->bench, &app->selection, app->selection_status,
                                      sizeof(app->selection_status));
        bench_group_resize(&app->bench, 1, 72, 180);
        bench_add(&app->bench, PART_SWITCH, 750, 450);
        queued = queued && dev_test_click(window, 500, 300) && dev_test_key(window, SDLK_R);
    }
    else if (app->selftest_phase == 193)
    {
        float x, y, sx, sy;
        bench_terminal_position(&app->bench, (Pin){0, 0}, &x, &y);
        bench_pin_position(&app->bench.parts[1], 0, &sx, &sy);
        queued = dev_test_click(window, 750, 450) &&
                 dev_test_drag_finish(window, 750 + x - sx + 5, 450 + y - sy + 3);
    }
    else if (app->selftest_phase == 194)
        queued = dev_test_click(window, 500, 300) && dev_test_drag_finish(window, 700, 500);
    else if (app->selftest_phase == 195)
        queued = dev_test_click(window, 700, 500) && dev_test_drag_finish(window, 505, 303);
    else if (app->selftest_phase == 196)
        queued = dev_test_shortcut(window, SDLK_Z, SDL_KMOD_CTRL);
    else if (app->selftest_phase == 197)
        queued = dev_test_shortcut(window, SDLK_Y, SDL_KMOD_CTRL);
    /* Inspector: minimize, try a hidden control, expand from the tab, then use a control. */
    else if (app->selftest_phase == 198)
    {
        app->bench = (Workbench){0};
        app->view = (BenchView){1, 0, 0};
        bench_add(&app->bench, PART_LED, 400, 300);
        app->selected = 0;
        app->inspector_minimized = 0;
        queued = click_rect(window, app_layout_inspector_minimize());
    }
    else if (app->selftest_phase == 199)
        queued = click_rect(window, app_layout_color_swatch(2));
    else if (app->selftest_phase == 200)
    {
        app->selected = 0;
        queued = click_rect(window, app_layout_inspector_tab());
    }
    else if (app->selftest_phase == 201)
        queued = click_rect(window, app_layout_color_swatch(2));
    /* Header panels: opening one closes the other; clicking the open one closes it. */
    else if (app->selftest_phase == 202 || app->selftest_phase == 204)
        queued = click_rect(window, app_layout_header(APP_HEADER_EXAMPLES));
    else if (app->selftest_phase == 203 || app->selftest_phase == 207)
        queued = click_rect(window, app_layout_header(APP_HEADER_PROJECTS));
    else if (app->selftest_phase == 205)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 206)
        queued = dev_test_key(window, SDLK_F3);
    /* Area selection: hold the drag at the right edge; the bench scrolls until the far part is inside. */
    else if (app->selftest_phase == 208 || app->selftest_phase == 220)
    {
        app->bench = (Workbench){0};
        app->view = (BenchView){1, 0, 0};
        bench_add(&app->bench, PART_RESISTOR, 500, 400);
        bench_add(&app->bench, PART_RESISTOR, 1400, 400);
        app->selected = -1;
        float edge_x = app->selftest_phase == 208 ? 1278 : 700;
        queued = dev_test_click(window, 300, 300) && move_pointer(window, edge_x, 420);
    }
    else if ((app->selftest_phase > 208 && app->selftest_phase < 219) || app->selftest_phase == 221)
        SDL_Delay(20);
    else if (app->selftest_phase == 219 || app->selftest_phase == 222)
        queued = dev_test_drag_finish(window, app->selftest_phase == 219 ? 1278 : 700, 420);
    /* 3AND: Alt+drag the third input from the right side to the bottom of the left side; Ctrl+Z undoes. */
    else if (app->selftest_phase == 223)
    {
        app->bench = (Workbench){0};
        app->view = (BenchView){1, 0, 0};
        bench_selection_clear(&app->selection);
        int first = bench_add(&app->bench, PART_AND, 500, 300), last = bench_add(&app->bench, PART_AND, 660, 340);
        bench_toggle_wire(&app->bench, (Pin){first, 2}, (Pin){last, 0});
        app->selection.parts[first] = app->selection.parts[last] = 1;
        queued = bench_pack_selection(&app->bench, &app->selection, app->selection_status,
                                      sizeof(app->selection_status));
        bench_selection_clear(&app->selection);
        float x, y;
        bench_terminal_position(&app->bench, (Pin){last, 1}, &x, &y);
        const BenchGroup *chip = &app->bench.groups[0];
        SDL_SetModState(SDL_KMOD_ALT);
        queued = queued && dev_test_click(window, x, y) && dev_test_drag_finish(window, chip->x - 60, chip->y + 40);
    }
    else if (app->selftest_phase == 224)
        queued = dev_test_shortcut(window, SDLK_Z, SDL_KMOD_CTRL);
    /* Name the 3AND output: double-click the terminal, type Y, Enter; Ctrl+Z; a single click still wires. */
    else if (app->selftest_phase == 225 || app->selftest_phase == 228)
    {
        float x, y;
        bench_terminal_position(&app->bench, (Pin){1, 2}, &x, &y);
        queued = dev_test_click(window, x, y) && dev_test_drag_finish(window, x, y);
        if (app->selftest_phase == 225)
            queued = queued && dev_test_click_count(window, x, y, 2) && dev_test_drag_finish(window, x, y);
    }
    else if (app->selftest_phase == 226)
        queued = dev_test_replace_text(window, "Y");
    else if (app->selftest_phase == 227)
        queued = dev_test_shortcut(window, SDLK_Z, SDL_KMOD_CTRL);
    /* Wire on wire: start at a resistor, click the middle of another wire; Ctrl+Z; Alt+click branches. */
    else if (app->selftest_phase >= 229 && app->selftest_phase <= 232)
        queued = queue_junction_phase(app, window);
    if (!queued)
    {
        fprintf(stderr, "Nao foi possivel inserir eventos de teste.\n");
        app->exit_code = 1;
        app->running = 0;
    }
}

void dev_scenario_validate(AppState *app)
{

    int valid = 1;
    if (app->selftest_phase == 0)
        valid = app->bench.part_count == 1 && app->bench.parts[0].type == PART_SOURCE;
    else if (app->selftest_phase == 1)
        valid = app->bench.part_count == 2 && app->bench.parts[1].type == PART_RESISTOR;
    else if (app->selftest_phase == 2)
        valid =
            app->bench.wire_count == 1 && app->bench.wires[0].knee_count == 1 && app->bench.wires[0].knees[0].x == 450;
    else if (app->selftest_phase == 3)
        valid = app->bench.wire_count == 0;
    else if (app->selftest_phase == 4)
        valid =
            app->bench.wire_count == 1 && app->bench.wires[0].knee_count == 1 && app->bench.wires[0].knees[0].y == 180;
    else if (app->selftest_phase == 5)
        valid = app->bench.wires[0].knees[0].x == 460 && app->bench.wires[0].knees[0].y == 190;
    else if (app->selftest_phase == 6)
        valid = app->bench.wires[0].knees[0].x == 450 && app->bench.wires[0].knees[0].y == 180;
    else if (app->selftest_phase == 7)
        valid = app->bench.wires[0].knees[0].x == 460 && app->bench.wires[0].knees[0].y == 190;
    else if (app->selftest_phase == 8)
        valid = app->bench.wires[0].knee_count == 0;
    else if (app->selftest_phase == 9)
        valid = app->bench.wires[0].knee_count == 1 && app->bench.wires[0].knees[0].x == 460;
    else if (app->selftest_phase == 10)
        valid = fabsf(app->view.zoom - 1.2f) < .001f;
    else if (app->selftest_phase == 11)
        valid = app->pending.component == 0 && app->pending.pin == 0;
    else if (app->selftest_phase == 12)
        valid = app->bench.wire_count == 0;
    else if (app->selftest_phase == 13)
        valid = app->bench.wire_count == 1;
    else if (app->selftest_phase == 14)
        valid = app->view.zoom == 1 && app->view.pan_x == 0 && app->view.pan_y == 0;
    else if (app->selftest_phase == 15)
        valid = app->view.pan_x == 100 && app->view.pan_y == 40;
    else if (app->selftest_phase == 16)
        valid = app->pending.component == 0 && app->pending.pin == 0;
    else if (app->selftest_phase == 17)
        valid = app->pending.component < 0;
    else if (app->selftest_phase == 18)
        valid = app->view.zoom == 1 && app->view.pan_x == 0 && app->view.pan_y == 0;
    else if (app->selftest_phase == 19)
        valid = app->bench.part_count == 3 && app->bench.parts[2].type == PART_BREADBOARD;
    else if (app->selftest_phase == 20 || app->selftest_phase == 22)
    {
        Pin a, b;
        valid = app->bench.part_count == 4 && app->bench.parts[3].type == PART_RESISTOR &&
                bench_find_contact(&app->bench, 3, 0, &a) && bench_find_contact(&app->bench, 3, 1, &b) &&
                a.component == 2 && b.component == 2;
    }
    else if (app->selftest_phase == 21)
        valid = app->bench.part_count == 3;
    else if (app->selftest_phase == 23)
        valid = app->selected == 3;
    else if (app->selftest_phase == 24)
        valid = app->bench.parts[3].rotation == 1;
    else if (app->selftest_phase == 25)
        valid = app->bench.parts[3].rotation == 0;
    else if (app->selftest_phase == 26)
        valid = app->bench.parts[3].rotation == 1;
    else if (app->selftest_phase == 27)
        valid = app->selected_wire == 0;
    else if (app->selftest_phase == 28)
        valid = app->bench.wires[0].color == BENCH_COLOR_RED;
    else if (app->selftest_phase == 29)
        valid = app->settings == 1;
    else if (app->selftest_phase == 30)
        valid = app->settings == 0;
    else if (app->selftest_phase == 31)
        valid = app->bench.wires[0].color == BENCH_DEFAULT_WIRE_COLOR;
    else if (app->selftest_phase == 32)
        valid = app->bench.wires[0].color == BENCH_COLOR_RED;
    else if (app->selftest_phase == 33)
        valid = app->settings == 1;
    else if (app->selftest_phase == 34)
        valid = app->probe_pin == 1;
    else if (app->selftest_phase == 35)
        valid = app->editing == WORKBENCH_MAX_COMPONENTS;
    else if (app->selftest_phase == 36)
        valid = app->editing < 0 && app->transient_step == 1e-5;
    else if (app->selftest_phase == 37)
        valid = app->editing == WORKBENCH_MAX_COMPONENTS + 1;
    else if (app->selftest_phase == 38)
        valid = app->editing < 0 && app->transient_stop == .005;
    else if (app->selftest_phase == 39)
        valid = app->editing == WORKBENCH_MAX_COMPONENTS;
    else if (app->selftest_phase == 40)
        valid = app->editing < 0 && app->transient_step == 2e-5;
    else if (app->selftest_phase == 41)
        valid = app->editing == WORKBENCH_MAX_COMPONENTS;
    else if (app->selftest_phase == 42)
        valid = app->editing == WORKBENCH_MAX_COMPONENTS && app->transient_step == 2e-5;
    else if (app->selftest_phase == 43)
        valid = app->editing < 0 && app->transient_step == 2e-5;
    else if (app->selftest_phase == 44)
        valid = sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->settings && app->sim.trace_index == 0;
    else if (app->selftest_phase == 45)
        valid = app->sim.trace_index == 1;
    else if (app->selftest_phase == 46)
        valid = app->sim.trace_index == 0;
    else if (app->selftest_phase == 47)
        valid = app->sim.trace_index == 2;
    else if (app->selftest_phase == 48)
        valid = app->sim.trace_index == 2;
    else if (app->selftest_phase == 49)
        valid = app->sim.trace_index == 1;
    else if (app->selftest_phase == 50)
        valid = app->settings == 1;
    else if (app->selftest_phase == 51)
        valid = app->probe_current == 1;
    else if (app->selftest_phase == 52)
        valid = app->settings == 0 && app->probe_current == 1;
    else if (app->selftest_phase == 53)
        valid = app->selected_wire == 0;
    else if (app->selftest_phase == 54)
        valid = app->dc_job.transient == 3;
    else if (app->selftest_phase == 55)
        valid = reports_project_saved(app);
    else if (app->selftest_phase == 56)
        valid = app->settings == 1;
    else if (app->selftest_phase == 57)
        valid = app->probe_current == 0;
    else if (app->selftest_phase == 58)
        valid = app->probe_current == 1 && app->probe_pin == 1 && app->transient_step == 2e-5 && app->wire_color == BENCH_COLOR_RED;
    else if (app->selftest_phase == 59)
        valid = app->settings == 0;
    else if (app->selftest_phase == 60)
        valid = app->bench.part_count == 5 && app->bench.parts[4].type == PART_AND && bench_pin_count(PART_AND) == 3;
    else if (app->selftest_phase == 61)
        valid = app->palette_page == 1;
    else if (app->selftest_phase == 62)
        valid = app->bench.part_count == 6 && app->bench.parts[5].type == PART_XOR;
    else if (app->selftest_phase == 63)
        valid = app->bench.part_count == 7 && app->bench.parts[6].type == PART_CLOCK &&
                app->bench.parts[6].value == 1000 && app->bench.parts[6].on;
    else if (app->selftest_phase == 64)
        valid = app->bench.parts[6].on == 0;
    else if (app->selftest_phase == 65)
        valid = sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->sim.sim_running && app->sim.trace_index == 0;
    else if (app->selftest_phase == 66)
        valid = sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.sim_running;
    else if (app->selftest_phase == 67 || app->selftest_phase == 68)
        valid = sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && app->sim.sim_running;
    else if (app->selftest_phase == 69)
        valid = sim_state_has_result(&app->sim, SIM_RESULT_TRACE) && !app->sim.sim_running &&
                app->sim.trace_index == app->sim.trace.count - 1;
    else if (app->selftest_phase == 70)
        valid = app->bench.part_count == 8 && app->bench.parts[7].type == PART_LED;
    else if (app->selftest_phase == 71)
        valid = app->leds[7] == 1;
    else if (app->selftest_phase == 72)
        valid = app->leds[7] == 0;
    else if (app->selftest_phase == 73)
        valid = app->bench.part_count == 9 && app->bench.parts[8].type == PART_DFF;
    else if (app->selftest_phase == 74)
        valid = app->bench.part_count == 10 && app->bench.parts[9].type == PART_74HC00;
    else if (app->selftest_phase == 75)
        valid = app->bench.part_count == 4 && app->bench.parts[1].type == PART_BREADBOARD;
    else if (app->selftest_phase == 76)
        valid = app->bench.part_count == 5 && app->bench.parts[2].type == PART_DFF && app->probe_pin == 2;
    else if (app->selftest_phase == 77)
        valid = app->bench.part_count == 6 && app->bench.parts[2].type == PART_74HC00 && app->probe_pin == 2;
    else if (app->selftest_phase == 78)
        valid = app->bench.part_count == 5 && app->bench.parts[2].type == PART_DFF;
    else if (app->selftest_phase == 79)
        valid = app->selecting_area;
    else if (app->selftest_phase == 80)
        valid = !app->selecting_area && bench_selection_count(&app->selection, &app->bench) == 5;
    else if (app->selftest_phase == 81 || app->selftest_phase == 83)
        valid = app->bench.part_count == 10 && app->bench.wire_count == 12;
    else if (app->selftest_phase == 82 || app->selftest_phase == 84)
        valid = app->bench.part_count == 5 && app->bench.wire_count == 6;
    else if (app->selftest_phase == 85)
        valid = app->projects_open;
    else if (app->selftest_phase == 86)
        valid = app->save_slot == 8;
    else if (app->selftest_phase == 87)
        valid = !app->projects_open;
    else if (app->selftest_phase == 88)
        valid = bench_selection_count(&app->selection, &app->bench) == 5;
    else if (app->selftest_phase == 89)
        valid = app->bench.groups[0].active && app->library_available[0] && app->palette_page == 2;
    else if (app->selftest_phase == 90)
        valid = app->packed_placing;
    else if (app->selftest_phase == 91 || app->selftest_phase == 93 || app->selftest_phase == 95)
        valid = app->bench.part_count == 10 && app->bench.groups[0].active && app->bench.groups[1].active;
    else if (app->selftest_phase == 92)
        valid = app->bench.groups[0].active && !app->bench.groups[1].active;
    else if (app->selftest_phase == 94)
        valid = reports_project_saved(app);
    else if (app->selftest_phase == 96)
        valid = bench_selection_count(&app->selection, &app->bench) == 5;
    else if (app->selftest_phase == 97)
        valid = app->bench.groups[0].x == 660 && app->bench.groups[0].y == 320 && app->bench.groups[1].x == 1200;
    else if (app->selftest_phase == 98)
        valid = app->bench.groups[0].x == 620 && app->bench.groups[0].y == 280;
    else if (app->selftest_phase == 99)
        valid = app->projects_open;
    else if (app->selftest_phase == 100)
        valid = !app->projects_open;
    else if (app->selftest_phase == 101)
        valid = app->hovered_pin.component == 0 && app->hovered_pin.pin == 0;
    else if (app->selftest_phase == 102)
        valid = app->hovered_pin.component == 0 && app->hovered_pin.pin == 1;
    else if (app->selftest_phase == 103)
        valid = app->hovered_pin.component == -1;
    else if (app->selftest_phase == 104)
        valid = app->settings;
    else if (app->selftest_phase == 105)
        valid = app->hovered_pin.component == -1 && app->settings;
    else if (app->selftest_phase == 106)
        valid = !app->settings;
    else if (app->selftest_phase == 107)
        valid = app->editing != APP_EDIT_GROUP_NAME && !app->editor_child;
    else if (app->selftest_phase == 115)
        valid = !app->editor_child && !strcmp(app->bench.groups[0].name, "Indicador \xc3\xa1rvore");
    else if (app->selftest_phase == 120)
        valid = !app->editor_child && !strcmp(app->bench.groups[0].name, "Driver \xc3\xa1rvore") &&
                app->bench.parts[0].value == 1000;
    else if (app->selftest_phase == 108 || app->selftest_phase == 116)
        valid = app->editor_child && !app_editor_dirty(app->editor_child);
    else if (app->selftest_phase == 109 || app->selftest_phase == 117)
        valid = app->editor_child && app->editor_child->editing == APP_EDIT_GROUP_NAME;
    else if (app->selftest_phase == 110)
        valid = app->editor_child && !app_editor_dirty(app->editor_child) &&
                !strcmp(app->bench.groups[0].name, "Indicador \xc3\xa1rvore");
    else if (app->selftest_phase == 111)
        valid = app->editor_child && app_editor_dirty(app->editor_child) &&
                !strcmp(app->bench.groups[0].name, "Indicador \xc3\xa1rvore");
    else if (app->selftest_phase == 118)
        valid = app->editor_child && app_editor_dirty(app->editor_child) &&
                !strcmp(app->bench.groups[0].name, "Driver \xc3\xa1rvore");
    else if (app->selftest_phase == 112 || app->selftest_phase == 114)
        valid = app->editor_child && app->editor_child->editor_confirm_close;
    else if (app->selftest_phase == 113)
        valid = app->editor_child && !app->editor_child->editor_confirm_close;
    else if (app->selftest_phase == 119)
        valid = !app->editor_child && !strcmp(app->bench.groups[0].name, "Driver \xc3\xa1rvore");
    else if (app->selftest_phase == 121)
        valid = app->settings;
    else if (app->selftest_phase == 122)
        valid = app->snap_grid;
    else if (app->selftest_phase == 123)
        valid = app->editing == APP_EDIT_GRID;
    else if (app->selftest_phase == 124)
        valid = app->editing < 0 && app->grid_spacing == 16;
    else if (app->selftest_phase == 125)
        valid = !app->snap_board;
    else if (app->selftest_phase == 126)
        valid = !app->settings;
    else if (app->selftest_phase == 127)
        valid = app->bench.part_count == 3 && app->bench.parts[2].x == 528 && app->bench.parts[2].y == 260;
    else if (app->selftest_phase == 128)
        valid = app->bench.parts[2].x == 560 && app->bench.parts[2].y == 276;
    else if (app->selftest_phase == 129)
        valid = app->bench.parts[2].x == 569 && app->bench.parts[2].y == 281;
    else if (app->selftest_phase == 131)
        valid = app->grid_spacing == 16 && app->snap_grid && !app->snap_board;
    else if (app->selftest_phase == 132 || app->selftest_phase == 134)
        valid = app->bench.wires[0].knee_count == 1 && app->bench.wires[0].knees[0].x == 500;
    else if (app->selftest_phase == 133)
        valid = app->bench.wires[0].knee_count == 0;
    else if (app->selftest_phase == 135 || app->selftest_phase == 137)
        valid = app->bench.wires[0].knee_count == 2 && app->bench.wires[0].knees[0].x == 500 &&
                app->bench.wires[0].knees[1].x == 620;
    else if (app->selftest_phase == 138)
        valid = app->editing == APP_EDIT_SCALE;
    else if (app->selftest_phase == 139 || app->selftest_phase == 143 || app->selftest_phase == 145)
        valid = bench_part_scale(&app->bench.parts[1]) == .25f;
    else if (app->selftest_phase == 140)
        valid = app->bench.parts[1].x == 700 && app->bench.parts[1].y == 250;
    else if (app->selftest_phase == 141)
        valid = app->bench.parts[1].x == 650 && app->bench.parts[1].y == 200;
    else if (app->selftest_phase == 142)
        valid = bench_part_scale(&app->bench.parts[1]) == 1;
    else if (app->selftest_phase == 146)
        valid = fabsf(bench_part_scale(&app->bench.parts[1]) - .275f) < .00001f && app->view.zoom == 1;
    else if (app->selftest_phase == 147)
        valid = bench_part_scale(&app->bench.parts[1]) == .25f;
    else if (app->selftest_phase == 148)
        valid = app->settings;
    else if (app->selftest_phase == 151)
        valid = !app->dark_theme;
    else if (app->selftest_phase == 149)
        valid = app->dark_theme;
    else if (app->selftest_phase == 152)
        valid = !app->settings;
    else if (app->selftest_phase == 153)
        valid = app->selected == 1 && app->pending.component < 0;
    else if (app->selftest_phase == 155)
        valid = app->palette_offset == 1;
    else if (app->selftest_phase == 156)
        valid = app->palette_offset == 2;
    else if (app->selftest_phase == 157)
        valid = app->placing == PART_POTENTIOMETER;
    else if (app->selftest_phase == 158)
        valid = fabs(app->bench.parts[0].control - .55) < 1e-9;
    else if (app->selftest_phase == 159)
        valid = app->bench.parts[0].control == .5;
    else if (app->selftest_phase == 160)
        valid = app->held_button == 1 && app->bench.parts[1].on;
    else if (app->selftest_phase == 161)
        valid = app->held_button < 0 && !app->bench.parts[1].on;
    else if (app->selftest_phase == 162)
        valid = app->palette_page == 1 && app->palette_offset == 0;
    else if (app->selftest_phase == 164)
        valid = app->placing == PART_NOR;
    else if (app->selftest_phase == 165)
        valid = app->bench.part_count == 3 && app->bench.parts[2].type == PART_NOR;
    else if (app->selftest_phase == 167)
        valid = app->gui_font_size == 14;
    else if (app->selftest_phase == 168)
        valid = fabsf(bench_group_width(&app->bench, 1) - 107.8f) < .001f;
    else if (app->selftest_phase == 169)
        valid = fabsf(bench_group_height(&app->bench, 1) - 81.4f) < .001f;
    else if (app->selftest_phase == 170)
        valid = bench_group_height(&app->bench, 1) == 74 &&
                fabsf(bench_group_width(&app->bench, 1) - 107.8f) < .001f;
    else if (app->selftest_phase == 171 || app->selftest_phase == 174 || app->selftest_phase == 176)
        valid = bench_lead_count(&app->bench.parts[0], 0) == 1 &&
                fabsf(bench_lead_point(&app->bench.parts[0], 0, 0).x - 600) < .01f;
    else if (app->selftest_phase == 172)
        valid = bench_lead_count(&app->bench.parts[0], 0) == 2;
    else if (app->selftest_phase == 173)
        valid = bench_lead_count(&app->bench.parts[0], 0) == 2 &&
                fabsf(bench_lead_point(&app->bench.parts[0], 0, 0).y - 400) < .01f;
    else if (app->selftest_phase == 175 || app->selftest_phase == 177 || app->selftest_phase == 179)
    {
        Pin contact;
        valid = bench_find_contact(&app->bench, 0, 0, &contact) && contact.component == 1 && contact.pin == 0;
    }
    else if (app->selftest_phase == 180)
        valid = app->dc_job.live_reset_pending && app->sim.sim_running && !app->sim.auto_requested_valid;
    else if (app->selftest_phase == 181)
        valid = app->probe_pin == 13;
    else if (app->selftest_phase == 182)
        valid = app->probe_pin == 0;
    else if (app->selftest_phase == 183 || app->selftest_phase == 185)
        valid = fabs(app->bench.parts[app->bench.part_count - 1].control - .75) < 1e-6 && app->dragging_environment == -1;
    else if (app->selftest_phase == 184)
        valid = app->selected == -1 && fabs(app->bench.parts[app->bench.part_count - 1].control - .1) < 1e-6;
    else if (app->selftest_phase == 186)
        valid = fabs(app->bench.parts[app->selected].control - .8) < 1e-6;
    else if (app->selftest_phase == 187)
        valid = app->examples_open && !app->settings && !app->projects_open;
    else if (app->selftest_phase == 188)
        valid = !app->examples_open && app->example_index == 13 && app->bench.part_count == 3 &&
                app->bench.parts[0].type == PART_CLOCK;
    else if (app->selftest_phase == 189)
    {
        Pin contact;
        valid = bench_find_contact(&app->bench, 0, 1, &contact) && contact.component == 1 && contact.pin == 0;
    }
    else if (app->selftest_phase == 190)
        valid = app->library_delete_armed == deleted_block && app->library_available[deleted_block];
    else if (app->selftest_phase == 191)
        valid = !app->library_available[deleted_block] && app->library_delete_armed == -1;
    else if (app->selftest_phase == 192)
        valid = app->bench.groups[0].rotation == 1 && app->bench.parts[0].rotation == 0;
    else if (app->selftest_phase >= 193 && app->selftest_phase <= 197)
    {
        Pin contact;
        int connected = bench_find_contact(&app->bench, 1, 0, &contact) && contact.component == 0;
        valid = connected == (app->selftest_phase == 193 || app->selftest_phase == 195 || app->selftest_phase == 197);
    }
    else if (app->selftest_phase == 198)
        valid = app->inspector_minimized && app->selected == 0;
    else if (app->selftest_phase == 199)
        valid = app->inspector_minimized && app->bench.parts[0].states == 0;
    else if (app->selftest_phase == 200)
        valid = !app->inspector_minimized && app->selected == 0;
    else if (app->selftest_phase == 201)
        valid = !app->inspector_minimized && app->bench.parts[0].states == 2;
    else if (app->selftest_phase == 202 || app->selftest_phase == 204)
        valid = app->examples_open && !app->projects_open && !app->settings;
    else if (app->selftest_phase == 203 || app->selftest_phase == 206)
        valid = app->projects_open && !app->examples_open && !app->settings;
    else if (app->selftest_phase == 205)
        valid = app->settings && !app->projects_open && !app->examples_open;
    else if (app->selftest_phase == 207)
        valid = !app->projects_open && !app->examples_open && !app->settings;
    else if (app->selftest_phase == 208 || app->selftest_phase == 220)
        valid = app->selecting_area;
    else if (app->selftest_phase == 219)
        valid = !app->selecting_area && app->view.pan_x < -122 && app->selection.parts[0] && app->selection.parts[1];
    else if (app->selftest_phase == 222)
        valid = !app->selecting_area && app->view.pan_x == 0 && app->selection.parts[0] && !app->selection.parts[1];
    else if (app->selftest_phase == 223 || app->selftest_phase == 224)
    {
        float x, y;
        bench_terminal_position(&app->bench, (Pin){1, 1}, &x, &y);
        int left = x < app->bench.groups[0].x;
        valid = app->bench.groups[0].active && app->drag_port.component < 0 && left == (app->selftest_phase == 223);
    }
    else if (app->selftest_phase >= 225 && app->selftest_phase <= 228)
    {
        char name[BENCH_PORT_NAME_CAPACITY];
        int named = bench_port_name(&app->bench.parts[1], 2, name, sizeof(name)) && !strcmp(name, "Y");
        if (app->selftest_phase == 225)
            valid = app->editing == APP_EDIT_PORT_NAME && app->pending.component < 0;
        else if (app->selftest_phase == 226)
            valid = app->editing < 0 && named;
        else if (app->selftest_phase == 227)
            valid = app->editing < 0 && !named;
        else
            valid = app->editing < 0 && app->pending.component == 1 && app->pending.pin == 2;
    }
    else if (app->selftest_phase >= 229 && app->selftest_phase <= 232)
        valid = junction_phase_valid(app);
    if (!valid)
    {
        fprintf(stderr, "Autoteste de eventos falhou na fase %d. child=%p editing=%d status=%s\n", app->selftest_phase, (void *)app->editor_child, app->editor_child ? app->editor_child->editing : app->editing, app->editor_child ? app->editor_child->status : app->status);
        fprintf(stderr, "hover=%d/%d mouse=%.1f/%.1f view=%.2f/%.1f/%.1f drag=%d/%d area=%d mods=%u\n",
                app->hovered_pin.component, app->hovered_pin.pin, app->mouse_screen.x, app->mouse_screen.y,
                app->view.zoom, app->view.pan_x, app->view.pan_y, app->dragging, app->drag_wire,
                app->selecting_area, (unsigned)SDL_GetModState());
        app->exit_code = 1;
        app->running = 0;
    }
    if (++app->selftest_phase == 233)
    {
        if (!app->exit_code)
            puts("Autoteste de eventos, historico, navegacao, encaixe, rotacao e cores passou.");
        app->running = 0;
    }
}
