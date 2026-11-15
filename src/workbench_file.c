#include "workbench_file.h"
#include "workbench_pack_file.h"
#include "component_registry.h"
#include <math.h>
#include <string.h>
#include <windows.h>

WorkbenchSettings workbench_settings_default(void)
{
    return (WorkbenchSettings){1, 1, 1, 0, 0, 0, 0, 1e-5, .005, 24, 0, 1};
}
static int settings_valid(const WorkbenchSettings *s)
{
    return s && (s->show_grid == 0 || s->show_grid == 1) && (s->show_handles == 0 || s->show_handles == 1) &&
           (s->round_wires == 0 || s->round_wires == 1) && (s->show_current == 0 || s->show_current == 1) &&
           s->wire_color >= 0 && s->wire_color < WIRE_COLOR_COUNT && (s->probe_pin >= 0 && s->probe_pin < 14) &&
           (s->probe_current == 0 || s->probe_current == 1) && isfinite(s->transient_step) &&
           isfinite(s->transient_stop) && s->transient_step >= 1e-12 && s->transient_stop <= 1e12 &&
           isfinite(s->grid_spacing) && s->grid_spacing >= 1 && s->grid_spacing <= 10000 &&
           (s->snap_grid == 0 || s->snap_grid == 1) && (s->snap_board == 0 || s->snap_board == 1) &&
           s->transient_step <= s->transient_stop && s->transient_stop / s->transient_step <= 500.0 * (1.0 + 1e-12);
}

int workbench_write_project(FILE *f, const Workbench *b, const WorkbenchSettings *s)
{
    int i, k;
    if (!f || !b || !settings_valid(s))
        return 0;
    if (fprintf(f, "workbench 19\n%d %d\n", b->part_count, b->wire_count) < 0)
        return 0;
    for (i = 0; i < b->part_count; ++i)
    {
        const Part *p = &b->parts[i];
        if (fprintf(f, "%d %.9g %.9g %d %.17g %d %d %.9g\n", (int)p->type, p->x, p->y, p->on, p->value, p->rotation,
                    p->board_layout, bench_part_scale(p)) < 0)
            return 0;
    }
    for (i = 0; i < b->wire_count; ++i)
    {
        const Wire *w = &b->wires[i];
        if (fprintf(f, "%d %d %d %d %d %d", w->a.component, w->a.pin, w->b.component, w->b.pin, w->knee_count,
                    w->color) < 0)
            return 0;
        for (k = 0; k < w->knee_count; ++k)
            if (fprintf(f, " %.9g %.9g", w->knees[k].x, w->knees[k].y) < 0)
                return 0;
        if (fputc('\n', f) == EOF)
            return 0;
    }
    if (!bench_groups_write(f, b))
        return 0;
    if (fprintf(f, "settings %d %d %d %d %d %d %d %.17g %.17g\n", s->show_grid, s->show_handles, s->round_wires,
                s->show_current, s->wire_color, s->probe_pin, s->probe_current, s->transient_step,
                s->transient_stop) < 0)
        return 0;
    if (fprintf(f, "grid %.17g %d %d\n", s->grid_spacing, s->snap_grid, s->snap_board) < 0)
        return 0;
    return !ferror(f);
}

int workbench_read_project(FILE *f, Workbench *destination, WorkbenchSettings *settings)
{
    Workbench b = {0};
    int published = 0;
    WorkbenchSettings saved = workbench_settings_default();
    char magic[32], extra;
    int version, parts, wires, i, k;
    if (!f || !destination || !settings)
        goto cleanup;
    if (fscanf(f, "%31s %d %d %d", magic, &version, &parts, &wires) != 4 || strcmp(magic, "workbench") ||
        (version < 1 || version > 19) || parts < 0 || parts > WORKBENCH_MAX_COMPONENTS || wires < 0 ||
        wires > WORKBENCH_MAX_WIRES)
        goto cleanup;
    for (i = 0; i < parts; ++i)
    {
        int code, on;
        float x, y;
        if (fscanf(f, "%d %f %f %d", &code, &x, &y, &on) != 4)
            goto cleanup;
        const ComponentInfo *info = component_from_legacy(code, version);
        if (!info || !isfinite(x) || !isfinite(y) || fabsf(x) > 100000 || fabsf(y) > 100000 || (on != 0 && on != 1) ||
            bench_add(&b, info->type, x, y) < 0)
            goto cleanup;
        PartType type = info->type;
        b.parts[i].on = on;
        if (version < 9 && type == PART_BREADBOARD)
            b.parts[i].board_layout = 0;
        if (version >= 4)
        {
            double value;
            if (fscanf(f, "%lf", &value) != 1 || !isfinite(value))
                goto cleanup;
            if (info->primary)
            {
                if (!bench_set_value(&b, i, value))
                    goto cleanup;
            }
            else if (value != 0)
                goto cleanup;
        }
        if (version >= 8)
        {
            int rotation;
            if (fscanf(f, "%d", &rotation) != 1 || rotation < 0 || rotation > 3 ||
                (!(info->flags & COMPONENT_ROTATES) && rotation != 0))
                goto cleanup;
            b.parts[i].rotation = rotation;
        }
        if (version >= 9)
        {
            int layout;
            if (fscanf(f, "%d", &layout) != 1 || layout < 0 || layout > 1 || (type != PART_BREADBOARD && layout != 0))
                goto cleanup;
            b.parts[i].board_layout = layout;
        }
        if (version >= 19)
        {
            float scale;
            if (fscanf(f, "%f", &scale) != 1 || !bench_set_scale(&b, i, scale))
                goto cleanup;
        }
    }
    for (i = 0; i < wires; ++i)
    {
        Pin a, c;
        int knees;
        if (fscanf(f, "%d %d %d %d %d", &a.component, &a.pin, &c.component, &c.pin, &knees) != 5 || knees < 0 ||
            knees > WIRE_MAX_KNEES || (version < 5 && (a.pin >= 100 || c.pin >= 100)) ||
            (version == 5 && (a.pin >= 180 || c.pin >= 180)) || (version == 6 && (a.pin >= 300 || c.pin >= 300)) ||
            bench_toggle_wire(&b, a, c) != 1)
            goto cleanup;
        b.wires[i].knee_count = knees;
        if (version >= 9 &&
            (fscanf(f, "%d", &b.wires[i].color) != 1 || b.wires[i].color < 0 || b.wires[i].color >= WIRE_COLOR_COUNT))
            goto cleanup;
        for (k = 0; k < knees; ++k)
        {
            WirePoint *p = &b.wires[i].knees[k];
            if (fscanf(f, "%f %f", &p->x, &p->y) != 2 || !isfinite(p->x) || !isfinite(p->y) || fabsf(p->x) > 100000 ||
                fabsf(p->y) > 100000)
                goto cleanup;
        }
    }
    if (version >= 16 && !bench_groups_read(f, &b, version))
        goto cleanup;
    if (version >= 10)
    {
        char tag[32];
        if (fscanf(f, "%31s %d %d %d %d %d %d %d %lf %lf", tag, &saved.show_grid, &saved.show_handles,
                   &saved.round_wires, &saved.show_current, &saved.wire_color, &saved.probe_pin, &saved.probe_current,
                   &saved.transient_step, &saved.transient_stop) != 10 ||
            strcmp(tag, "settings") || !settings_valid(&saved))
            goto cleanup;
    }
    if (version >= 18)
    {
        char tag[32];
        if (fscanf(f, "%31s %lf %d %d", tag, &saved.grid_spacing, &saved.snap_grid, &saved.snap_board) != 4 ||
            strcmp(tag, "grid") || !settings_valid(&saved))
            goto cleanup;
    }
    if (fscanf(f, " %c", &extra) != EOF || ferror(f))
        goto cleanup;
    published = bench_replace(destination, &b);
    if (published)
        *settings = saved;
cleanup:
    bench_snapshot_clear(&b);
    return published;
}

int workbench_save_project(const char *path, const Workbench *bench, const WorkbenchSettings *settings)
{
    char directory[MAX_PATH], temporary[MAX_PATH];
    const char *slash = strrchr(path, '/'), *backslash = strrchr(path, '\\'), *end = slash;
    size_t length;
    FILE *file;
    int ok;
    if (backslash && (!end || backslash > end))
        end = backslash;
    length = end ? (size_t)(end - path + 1) : 0;
    if (length >= sizeof(directory))
        return 0;
    if (length)
    {
        memcpy(directory, path, length);
        directory[length] = '\0';
    }
    else
        strcpy(directory, ".");
    if (!GetTempFileNameA(directory, "ele", 0, temporary))
        return 0;
    file = fopen(temporary, "w");
    if (!file)
    {
        DeleteFileA(temporary);
        return 0;
    }
    ok = workbench_write_project(file, bench, settings);
    if (fclose(file) != 0)
        ok = 0;
    if (ok)
        ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok)
        DeleteFileA(temporary);
    return ok;
}

int workbench_load_project(const char *path, Workbench *bench, WorkbenchSettings *settings)
{
    FILE *file = fopen(path, "r");
    int ok;
    if (!file)
        return 0;
    ok = workbench_read_project(file, bench, settings);
    fclose(file);
    return ok;
}

int workbench_write(FILE *file, const Workbench *bench)
{
    WorkbenchSettings s = workbench_settings_default();
    return workbench_write_project(file, bench, &s);
}
int workbench_read(FILE *file, Workbench *bench)
{
    WorkbenchSettings s;
    return workbench_read_project(file, bench, &s);
}
int workbench_save(const char *path, const Workbench *bench)
{
    WorkbenchSettings s = workbench_settings_default();
    return workbench_save_project(path, bench, &s);
}
int workbench_load(const char *path, Workbench *bench)
{
    WorkbenchSettings s;
    return workbench_load_project(path, bench, &s);
}
