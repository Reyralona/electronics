#ifndef WORKBENCH_FILE_H
#define WORKBENCH_FILE_H
#include <stdio.h>
/* Requested simulated seconds per real second, from slow motion (1 ms per second) to real time. */
#define WORKBENCH_SPEED_MIN .001
#define WORKBENCH_SPEED_MAX 1.0
#include "workbench.h"

typedef struct {
    int show_grid, show_handles, round_wires, show_current, wire_color, probe_pin, probe_current;
    double transient_step, transient_stop;
    double grid_spacing;
    int snap_grid, snap_board;
    int dark_theme, gui_font_size;
    int show_pin_labels; /* app preference only; not stored in project files */
    int language; /* UiLanguage; app preference only, not stored in project files */
    double simulation_speed; /* Wall-clock pacing preference only; electrical parameters are unchanged. */
} WorkbenchSettings;
WorkbenchSettings workbench_settings_default(void);
int workbench_settings_valid(const WorkbenchSettings *settings);
int workbench_read_project(FILE *file, Workbench *destination, WorkbenchSettings *settings);
int workbench_write_project(FILE *file, const Workbench *bench, const WorkbenchSettings *settings);
int workbench_save_project(const char *path, const Workbench *bench, const WorkbenchSettings *settings);
int workbench_load_project(const char *path, Workbench *bench, WorkbenchSettings *settings);

/* Parsing is transactional: on failure the destination remains unchanged. */
int workbench_read(FILE *file, Workbench *destination);
int workbench_write(FILE *file, const Workbench *bench);
int workbench_save(const char *path, const Workbench *bench);
int workbench_load(const char *path, Workbench *bench);
#endif
