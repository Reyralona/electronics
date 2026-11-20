#include "workbench_file.h"
#include "component_devices.h"
#include "bench_path.h"
#include <stdlib.h>
#include "workbench_pack_file.h"
#include "ui_text.h"
#include "component_registry.h"
#include <math.h>
#include <string.h>
#include <windows.h>

WorkbenchSettings workbench_settings_default(void)
{
    return (WorkbenchSettings){1, 1, 1, 0, BENCH_DEFAULT_WIRE_COLOR, 0, 0, 1e-5, .005, 24, 0, 1, 0, 15, 0, UI_LANG_PT_BR, 1.0};
}
int workbench_settings_valid(const WorkbenchSettings *s)
{
    return s && s->gui_font_size >= 12 && s->gui_font_size <= 20 && (s->dark_theme == 0 || s->dark_theme == 1) && (s->show_grid == 0 || s->show_grid == 1) && (s->show_handles == 0 || s->show_handles == 1) &&
           (s->round_wires == 0 || s->round_wires == 1) && (s->show_current == 0 || s->show_current == 1) &&
           s->wire_color >= 0 && s->wire_color < WIRE_COLOR_COUNT && (s->probe_pin >= 0 && s->probe_pin < 14) &&
           (s->probe_current == 0 || s->probe_current == 1) && isfinite(s->transient_step) &&
           isfinite(s->transient_stop) && s->transient_step >= 1e-12 && s->transient_stop <= 1e12 &&
           isfinite(s->grid_spacing) && s->grid_spacing >= 1 && s->grid_spacing <= 10000 &&
           (s->snap_grid == 0 || s->snap_grid == 1) && (s->snap_board == 0 || s->snap_board == 1) &&
           (s->show_pin_labels == 0 || s->show_pin_labels == 1) && s->language >= 0 && s->language < UI_LANG_COUNT &&
           s->transient_step <= s->transient_stop && s->transient_stop / s->transient_step <= 500.0 * (1.0 + 1e-12) &&
           isfinite(s->simulation_speed) && s->simulation_speed >= WORKBENCH_SPEED_MIN && s->simulation_speed <= WORKBENCH_SPEED_MAX;
}

static int write_leads(FILE *file, const Workbench *bench)
{
    size_t records = 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].terminals)
            for (size_t pin = 0; pin < bench->parts[i].terminals->pin_count; ++pin)
                records += bench->parts[i].terminals->paths[pin].count != 0;
    if (fprintf(file, "leads %zu\n", records) < 0)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].terminals)
            for (size_t pin = 0; pin < bench->parts[i].terminals->pin_count; ++pin)
            {
                const BenchPath *path = &bench->parts[i].terminals->paths[pin];
                if (!path->count)
                    continue;
                if (fprintf(file, "lead %d %zu %zu", i, pin, path->count) < 0)
                    return 0;
                for (size_t point = 0; point < path->count; ++point)
                    if (fprintf(file, " %.9g %.9g", path->points[point].x, path->points[point].y) < 0)
                        return 0;
                if (fputc('\n', file) == EOF)
                    return 0;
            }
    return 1;
}

/* Format 26: per-part state words (DIP bits, instrument modes); only non-zero entries are listed. */
static int write_states(FILE *file, const Workbench *bench)
{
    int records = 0;
    for (int i = 0; i < bench->part_count; ++i)
        records += bench->parts[i].states != 0;
    if (fprintf(file, "states %d\n", records) < 0)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].states && fprintf(file, "%d %u\n", i, bench->parts[i].states) < 0)
            return 0;
    return 1;
}

static int read_states(FILE *file, Workbench *bench)
{
    char tag[32];
    int records;
    if (fscanf(file, "%31s %d", tag, &records) != 2 || strcmp(tag, "states") || records < 0 ||
        records > bench->part_count)
        return 0;
    for (int k = 0; k < records; ++k)
    {
        int index;
        unsigned states;
        if (fscanf(file, "%d %u", &index, &states) != 2 || index < 0 || index >= bench->part_count ||
            !component_states_valid(bench->parts[index].type, states))
            return 0;
        bench->parts[index].states = states;
    }
    return 1;
}

/* Format 27: program text of microcontroller parts, stored as byte length + raw UTF-8 bytes. */
#define CODE_MAX_BYTES (256 * 1024)
static int write_code(FILE *file, const Workbench *bench)
{
    int records = 0;
    for (int i = 0; i < bench->part_count; ++i)
        records += bench->parts[i].code != NULL;
    if (fprintf(file, "code %d\n", records) < 0)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
    {
        const char *code = bench->parts[i].code;
        if (!code)
            continue;
        size_t length = strlen(code);
        if (fprintf(file, "%d %zu\n", i, length) < 0 || fwrite(code, 1, length, file) != length || fputc('\n', file) == EOF)
            return 0;
    }
    return 1;
}

static int read_code(FILE *file, Workbench *bench)
{
    char tag[32];
    int records;
    if (fscanf(file, "%31s %d", tag, &records) != 2 || strcmp(tag, "code") || records < 0 || records > bench->part_count)
        return 0;
    for (int k = 0; k < records; ++k)
    {
        int index;
        size_t length;
        if (fscanf(file, "%d %zu", &index, &length) != 2 || index < 0 || index >= bench->part_count ||
            length > CODE_MAX_BYTES || fgetc(file) != '\n')
            return 0;
        char *text = malloc(length + 1);
        if (!text)
            return 0;
        int ok = fread(text, 1, length, file) == length;
        text[length] = 0;
        ok = ok && strlen(text) == length && bench_set_code(bench, index, text);
        free(text);
        if (!ok)
            return 0;
    }
    return 1;
}

/* Format 31: chip terminal names, one "<part> <pin> <bytes>" header plus raw UTF-8 bytes per name. */
static int write_port_names(FILE *file, const Workbench *bench)
{
    int records = 0;
    char name[BENCH_PORT_NAME_CAPACITY];
    for (int i = 0; i < bench->part_count; ++i)
        for (int pin = 0; pin < bench_pin_count(bench->parts[i].type); ++pin)
            records += bench_port_name(&bench->parts[i], pin, name, sizeof(name));
    if (fprintf(file, "portnames %d\n", records) < 0)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
        for (int pin = 0; pin < bench_pin_count(bench->parts[i].type); ++pin)
        {
            if (!bench_port_name(&bench->parts[i], pin, name, sizeof(name)))
                continue;
            size_t length = strlen(name);
            if (fprintf(file, "%d %d %zu\n", i, pin, length) < 0 || fwrite(name, 1, length, file) != length ||
                fputc('\n', file) == EOF)
                return 0;
        }
    return 1;
}

static int read_port_names(FILE *file, Workbench *bench)
{
    char tag[32];
    int records;
    if (fscanf(file, "%31s %d", tag, &records) != 2 || strcmp(tag, "portnames") || records < 0 ||
        records > bench->part_count * WORKBENCH_MAX_PINS)
        return 0;
    for (int k = 0; k < records; ++k)
    {
        int index, pin;
        size_t length;
        char name[BENCH_PORT_NAME_CAPACITY];
        if (fscanf(file, "%d %d %zu", &index, &pin, &length) != 3 || length == 0 || length >= sizeof(name) ||
            fgetc(file) != '\n' || fread(name, 1, length, file) != length)
            return 0;
        name[length] = 0;
        if (strlen(name) != length || !bench_set_port_name(bench, (Pin){index, pin}, name))
            return 0;
    }
    return 1;
}

/* Layers use the same validated terminal-name encoding as visible parts. */
static int write_layers(FILE *file, const Workbench *bench)
{
    if (fprintf(file, "layers %d\n", bench->part_count) < 0)
        return 0;
    Workbench names = {0};
    names.part_count = 1;
    for (int i = 0; i < bench->part_count; ++i)
    {
        int count = 0;
        for (const BenchPartLayer *layer = bench->parts[i].layers; layer; layer = layer->next)
            ++count;
        if (fprintf(file, "%d\n", count) < 0)
            return 0;
        for (const BenchPartLayer *layer = bench->parts[i].layers; layer; layer = layer->next)
        {
            if (fprintf(file, "%d", layer->group) < 0)
                return 0;
            for (size_t p = 0; p < sizeof(layer->exposed) / sizeof(layer->exposed[0]); ++p)
                if (fprintf(file, " %llu", (unsigned long long)layer->exposed[p]) < 0)
                    return 0;
            if (fputc('\n', file) == EOF)
                return 0;
            names.parts[0].type = bench->parts[i].type;
            names.parts[0].port_names = layer->port_names;
            if (!write_port_names(file, &names))
                return 0;
        }
    }
    return 1;
}

static int read_layers(FILE *file, Workbench *bench)
{
    char tag[32];
    int parts;
    unsigned char used[WORKBENCH_MAX_COMPONENTS] = {0};
    if (fscanf(file, "%31s %d", tag, &parts) != 2 || strcmp(tag, "layers") || parts != bench->part_count)
        return 0;
    for (int i = 0; i < parts; ++i)
    {
        int count, outer = bench->parts[i].group;
        if (fscanf(file, "%d", &count) != 1 || count < 0 || count > WORKBENCH_MAX_COMPONENTS)
            return 0;
        unsigned char seen[WORKBENCH_MAX_COMPONENTS] = {0};
        if (outer)
        {
            if (bench->groups[outer - 1].parent)
                return 0;
            seen[outer - 1] = used[outer - 1] = 1;
        }
        BenchPartLayer **tail = &bench->parts[i].layers;
        for (int k = 0; k < count; ++k)
        {
            if (!outer)
                return 0;
            *tail = calloc(1, sizeof(**tail));
            if (!*tail)
                return 0;
            BenchPartLayer *layer = *tail;
            if (fscanf(file, "%d", &layer->group) != 1 || layer->group < 0 ||
                layer->group > WORKBENCH_MAX_COMPONENTS)
                return 0;
            if (layer->group)
            {
                int g = layer->group - 1;
                if (seen[g] || !bench->groups[g].active || bench->groups[g].parent != outer)
                    return 0;
                seen[g] = used[g] = 1;
            }
            for (size_t p = 0; p < sizeof(layer->exposed) / sizeof(layer->exposed[0]); ++p)
            {
                unsigned long long bits;
                if (fscanf(file, "%llu", &bits) != 1)
                    return 0;
                layer->exposed[p] = (uint64_t)bits;
            }
            for (int p = bench_pin_count(bench->parts[i].type); p < (int)(sizeof(layer->exposed) * 8); ++p)
                if (layer->exposed[p / 64] & (UINT64_C(1) << (p % 64)))
                    return 0;
            Workbench names = {0};
            names.part_count = 1;
            names.parts[0].type = bench->parts[i].type;
            int valid = read_port_names(file, &names);
            layer->port_names = names.parts[0].port_names;
            names.parts[0].port_names = NULL;
            if (!valid)
                return 0;
            outer = layer->group;
            tail = &layer->next;
        }
    }
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (bench->groups[g].active && !used[g])
            return 0;
    return 1;
}

static int read_leads(FILE *file, Workbench *bench)
{
    char tag[32];
    size_t records;
    if (fscanf(file, "%31s %zu", tag, &records) != 2 || strcmp(tag, "leads") ||
        records > (size_t)bench->part_count * WORKBENCH_MAX_PINS)
        return 0;
    for (size_t record = 0; record < records; ++record)
    {
        int component, pin;
        size_t count;
        if (fscanf(file, "%31s %d %d %zu", tag, &component, &pin, &count) != 4 || strcmp(tag, "lead") ||
            component < 0 || component >= bench->part_count || !count ||
            count > SIZE_MAX / sizeof(WirePoint) || !bench_lead_count(&bench->parts[component], pin))
            return 0;
        Part *part = &bench->parts[component];
        if (!part->terminals)
        {
            part->terminals = calloc(1, sizeof(*part->terminals));
            if (!part->terminals)
                return 0;
            part->terminals->pin_count = (size_t)bench_pin_count(part->type);
            part->terminals->paths = calloc(part->terminals->pin_count, sizeof(BenchPath));
            if (!part->terminals->paths)
            {
                free(part->terminals);
                part->terminals = NULL;
                return 0;
            }
        }
        BenchPath *path = &part->terminals->paths[pin];
        if (path->count || bench_path_reserve(path, count) != BENCH_PATH_OK)
            return 0;
        for (size_t point = 0; point < count; ++point)
        {
            WirePoint p;
            if (fscanf(file, "%f %f", &p.x, &p.y) != 2 || !isfinite(p.x) || !isfinite(p.y) ||
                fabsf(p.x) > 100000 || fabsf(p.y) > 100000)
                return 0;
            path->points[path->count++] = p;
        }
    }
    return 1;
}

/* Format 33 indexes the shared palette; older files use the six legacy wire colors. */
static int read_wire_color(FILE *f, int version, int *color)
{
    int saved;
    if (fscanf(f, "%d", &saved) != 1)
        return 0;
    *color = version < 33 ? bench_color_from_legacy_wire(saved) : saved;
    return *color >= 0 && *color < WIRE_COLOR_COUNT;
}

int workbench_write_project(FILE *f, const Workbench *b, const WorkbenchSettings *s)
{
    int i, k;
    if (!f || !b || !workbench_settings_valid(s))
        return 0;
    if (fprintf(f, "workbench 35\n%d %d\n", b->part_count, b->wire_count) < 0)
        return 0;
    for (i = 0; i < b->part_count; ++i)
    {
        const Part *p = &b->parts[i];
        if (fprintf(f, "%d %.9g %.9g %d %.17g %d %d %.9g %.17g %d\n", (int)p->type, p->x, p->y,
                    p->type == PART_BUTTON ? 0 : p->on, p->value, p->rotation, p->board_layout, bench_part_scale(p),
                    p->control, p->mirrored) < 0)
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
    if (!bench_groups_write(f, b) || !write_leads(f, b) || !write_states(f, b) || !write_code(f, b) || !write_port_names(f, b))
        return 0;
    if (!write_layers(f, b))
        return 0;
    if (fprintf(f, "settings %d %d %d %d %d %d %d %.17g %.17g\n", s->show_grid, s->show_handles, s->round_wires,
                s->show_current, s->wire_color, s->probe_pin, s->probe_current, s->transient_step,
                s->transient_stop) < 0)
        return 0;
    if (fprintf(f, "grid %.17g %d %d\n", s->grid_spacing, s->snap_grid, s->snap_board) < 0)
        return 0;
    if (fprintf(f, "theme %d %d\n", s->dark_theme, s->gui_font_size) < 0)
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
        (version < 1 || version > 35) || parts < 0 || parts > WORKBENCH_MAX_COMPONENTS || wires < 0 ||
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
        if (version >= 21)
        {
            double control;
            if (fscanf(f, "%lf", &control) != 1 || !isfinite(control) || control < 0 || control > 1)
                goto cleanup;
            b.parts[i].control = control;
        }
        if (version >= 34)
        {
            int mirrored;
            if (fscanf(f, "%d", &mirrored) != 1 || (mirrored != 0 && mirrored != 1) ||
                (!(info->flags & COMPONENT_ROTATES) && mirrored))
                goto cleanup;
            b.parts[i].mirrored = mirrored;
        }
        if (type == PART_BUTTON)
            b.parts[i].on = 0;
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
        if (version >= 9 && !read_wire_color(f, version, &b.wires[i].color))
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
    if (version >= 23 && !read_leads(f, &b))
        goto cleanup;
    if (version >= 26 && !read_states(f, &b))
        goto cleanup;
    if (version >= 27 && !read_code(f, &b))
        goto cleanup;
    if (version >= 31 && !read_port_names(f, &b))
        goto cleanup;
    if (version >= 32 && !read_layers(f, &b))
        goto cleanup;
    if (version >= 10)
    {
        char tag[32];
        if (fscanf(f, "%31s %d %d %d %d %d %d %d %lf %lf", tag, &saved.show_grid, &saved.show_handles,
                   &saved.round_wires, &saved.show_current, &saved.wire_color, &saved.probe_pin, &saved.probe_current,
                   &saved.transient_step, &saved.transient_stop) != 10 ||
            strcmp(tag, "settings"))
            goto cleanup;
        if (version < 33)
            saved.wire_color = bench_color_from_legacy_wire(saved.wire_color);
        if (!workbench_settings_valid(&saved))
            goto cleanup;
    }
    if (version >= 18)
    {
        char tag[32];
        if (fscanf(f, "%31s %lf %d %d", tag, &saved.grid_spacing, &saved.snap_grid, &saved.snap_board) != 4 ||
            strcmp(tag, "grid") || !workbench_settings_valid(&saved))
            goto cleanup;
    }
    if (version >= 20)
    {
        char tag[32];
        if (fscanf(f, "%31s %d", tag, &saved.dark_theme) != 2 || strcmp(tag, "theme") || !workbench_settings_valid(&saved))
            goto cleanup;
    }
    if (version >= 21 && (fscanf(f, "%d", &saved.gui_font_size) != 1 || !workbench_settings_valid(&saved)))
        goto cleanup;
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
