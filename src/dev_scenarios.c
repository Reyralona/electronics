#include "dev_scenarios.h"
#include "dev_capture.h"
#include "workbench_examples.h"
#include "spice_trace.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int dev_scenario_prepare(AppState *app, int argc, char **argv)
{
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
        }
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
    if (app->capture && (strcmp(argv[1], "--capture-example0") == 0 || strcmp(argv[1], "--capture-example1") == 0 ||
                         strcmp(argv[1], "--capture-example2") == 0))
    {
        int which = strcmp(argv[1], "--capture-example2") == 0 ? 2 : strcmp(argv[1], "--capture-example1") == 0;
        if (!workbench_example_create(which, &app->bench, &app->selected))
        {
            fprintf(stderr, "Falha no exemplo de captura.\n");
            return 1;
        }
        app->probe_pin = which ? 2 : 0;
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
        app->probe_pin = which ? 2 : 0;
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
    return 0;
}

int dev_scenario_analyze(AppState *app, char **argv)
{
    if (app->auto_capture)
    {
        /* Captures need the result in the first frame: start the analysis now and wait for it. */
        SimAutoOptions options = {app->show_current, sim_state_probe(&app->bench, app->selected, app->probe_pin),
                                  app->transient_step, app->transient_stop, burn_limits_default()};
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

void dev_scenario_queue(AppState *app, SDL_Window *window)
{

    int queued = 1;
    if (app->selftest_phase == 0)
        queued = dev_test_click(window, 50, 150) && dev_test_click(window, 400, 250);
    else if (app->selftest_phase == 1)
        queued = dev_test_click(window, 50, 438) && dev_test_click(window, 600, 250);
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
        queued = dev_test_click(window, 50, 366) && dev_test_click(window, 850, 430);
    else if (app->selftest_phase == 20)
        queued = dev_test_click(window, 50, 438) && dev_test_click(window, 838, 377);
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
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 29)
        queued = dev_test_click(window, 915, 330);
    else if (app->selftest_phase == 30)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 31)
        queued = dev_test_key(window, SDLK_Z);
    else if (app->selftest_phase == 32)
        queued = dev_test_key(window, SDLK_Y);
    else if (app->selftest_phase == 33)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 34)
        queued = dev_test_click(window, 900, 405);
    else if (app->selftest_phase == 35)
        queued = dev_test_click(window, 900, 445);
    else if (app->selftest_phase == 36)
        queued = dev_test_key(window, SDLK_RETURN);
    else if (app->selftest_phase == 37)
        queued = dev_test_click(window, 900, 485);
    else if (app->selftest_phase == 38)
        queued = dev_test_key(window, SDLK_ESCAPE);
    else if (app->selftest_phase == 39)
        queued = dev_test_click(window, 900, 445);
    else if (app->selftest_phase == 40)
        queued = dev_test_replace_text(window, "2e-5");
    else if (app->selftest_phase == 41)
        queued = dev_test_click(window, 900, 445);
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
        queued = dev_test_click(window, 900, 370);
    else if (app->selftest_phase == 52)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 53)
        queued = dev_test_click(window, 505, 220);
    else if (app->selftest_phase == 54)
        queued = dev_test_key(window, SDLK_T);
    else if (app->selftest_phase == 55)
        queued = dev_test_key(window, SDLK_S);
    else if (app->selftest_phase == 56)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 57)
        queued = dev_test_click(window, 900, 370);
    else if (app->selftest_phase == 58)
        queued = dev_test_key(window, SDLK_L);
    else if (app->selftest_phase == 59)
        queued = dev_test_key(window, SDLK_F1);
    else if (app->selftest_phase == 60)
        queued = dev_test_click(window, 50, 670) && dev_test_click(window, 1000, 300);
    else if (app->selftest_phase == 61)
        queued = dev_test_key(window, SDLK_TAB);
    else if (app->selftest_phase == 62)
        queued = dev_test_click(window, 50, 390) && dev_test_click(window, 1100, 400);
    else if (app->selftest_phase == 63)
        queued = dev_test_click(window, 50, 510) && dev_test_click(window, 1000, 500);
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
        queued = dev_test_click(window, 50, 600) && dev_test_click(window, 750, 550);
    else if (app->selftest_phase == 74)
        queued = dev_test_click(window, 50, 670) && dev_test_click(window, 750, 450);
    else if (app->selftest_phase == 75)
        queued = dev_test_key(window, SDLK_F2);
    else if (app->selftest_phase == 76)
        queued = dev_test_key(window, SDLK_F2);
    else if (app->selftest_phase == 77)
        queued = dev_test_key(window, SDLK_F2);
    else if (app->selftest_phase == 78)
        queued = dev_test_key(window, SDLK_Z);
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
        valid = app->settings == 1;
    else if (app->selftest_phase == 29)
        valid = app->bench.wires[0].color == 1;
    else if (app->selftest_phase == 30)
        valid = app->settings == 0;
    else if (app->selftest_phase == 31)
        valid = app->bench.wires[0].color == 0;
    else if (app->selftest_phase == 32)
        valid = app->bench.wires[0].color == 1;
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
        valid = strcmp(app->status, "Salvo em bancada.elec.") == 0;
    else if (app->selftest_phase == 56)
        valid = app->settings == 1;
    else if (app->selftest_phase == 57)
        valid = app->probe_current == 0;
    else if (app->selftest_phase == 58)
        valid = app->probe_current == 1 && app->probe_pin == 1 && app->transient_step == 2e-5 && app->wire_color == 1;
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
    if (!valid)
    {
        fprintf(stderr, "Autoteste de eventos falhou na fase %d.\n", app->selftest_phase);
        app->exit_code = 1;
        app->running = 0;
    }
    if (++app->selftest_phase == 79)
    {
        if (!app->exit_code)
            puts("Autoteste de eventos, historico, navegacao, encaixe, rotacao e cores passou.");
        app->running = 0;
    }
}
