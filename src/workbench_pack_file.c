#include "workbench_pack_file.h"
#include "workbench_pack.h"
#include "workbench_group_name.h"
#include "ui_text.h"
#include <inttypes.h>
#include <math.h>
#include <string.h>
#define PROJECT_COORDINATE_LIMIT 100000.0f

static int exposed(const Part *part, int pin)
{
    return (part->exposed[pin / 64] & (UINT64_C(1) << (pin % 64))) != 0;
}

/* Format 30: "<count> <side> <row>..." after the rotation; count 0 keeps the automatic arrangement. */
static int write_port_layout(FILE *file, const BenchGroup *group)
{
    int count = group->port_layout;
    if (count < 0 || count > BENCH_GROUP_LAYOUT_MAX)
        return 0;
    if (fprintf(file, " %d", count) < 0)
        return 0;
    for (int index = 0; index < count; ++index)
        if (fprintf(file, " %u %u", group->port_places[index].side, group->port_places[index].row) < 0)
            return 0;
    return 1;
}

static int read_port_layout(FILE *file, BenchGroup *group)
{
    if (fscanf(file, "%d", &group->port_layout) != 1 || group->port_layout < 0 ||
        group->port_layout > BENCH_GROUP_LAYOUT_MAX)
        return 0;
    for (int index = 0; index < group->port_layout; ++index)
    {
        unsigned side, row;
        if (fscanf(file, "%u %u", &side, &row) != 2 || side > 1 || row >= BENCH_GROUP_LAYOUT_MAX)
            return 0;
        group->port_places[index] = (BenchPortPlace){(unsigned char)side, (unsigned char)row};
    }
    return 1;
}

static int write_group(FILE *file, int index, const BenchGroup *group)
{
    char fallback[BENCH_GROUP_NAME_CAPACITY];
    snprintf(fallback, sizeof(fallback), tr(TXT_CHIP_DEFAULT_NAME), index + 1);
    const char *name = group->name[0] ? group->name : fallback;
    if (!bench_group_name_valid(name) || group->rotation < 0 || group->rotation > 3 ||
        group->rotation_diagonal < 0 || group->rotation_diagonal > 1)
        return 0;
    size_t length = strlen(name);
    return fprintf(file, "group %d %.9g %.9g %.9g %.9g %" PRIu64 " %d %d %d", index + 1, group->x, group->y,
                   group->width, group->height, group->model, group->rotation, group->mirrored,
                   group->rotation_diagonal) >= 0 &&
           write_port_layout(file, group) && fprintf(file, " %d %zu\n", group->parent, length) >= 0 &&
           fwrite(name, 1, length, file) == length && fputc('\n', file) != EOF;
}

int bench_groups_write(FILE *file, const Workbench *bench)
{
    int groups = 0, members = 0, ports = 0;
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        groups += bench->groups[i].active != 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group)
        {
            ++members;
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                ports += exposed(&bench->parts[i], p);
        }
    if (fprintf(file, "groups %d\n", groups) < 0)
        return 0;
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        if (bench->groups[i].active && !write_group(file, i, &bench->groups[i]))
            return 0;
    if (fprintf(file, "members %d\n", members) < 0)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group && fprintf(file, "member %d %d\n", i, bench->parts[i].group) < 0)
            return 0;
    if (fprintf(file, "ports %d\n", ports) < 0)
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (exposed(&bench->parts[i], p) && fprintf(file, "port %d %d\n", i, p) < 0)
                    return 0;
    return !ferror(file);
}

static int count_record(FILE *file, const char *expected, int maximum, int *count)
{
    char tag[32];
    return fscanf(file, "%31s %d", tag, count) == 2 && !strcmp(tag, expected) && *count >= 0 && *count <= maximum;
}

static int end_line(FILE *file)
{
    int c;
    do
        c = fgetc(file);
    while (c == ' ' || c == '\t');
    if (c == '\r')
        c = fgetc(file);
    return c == '\n';
}

static int read_group_name(FILE *file, BenchGroup *group)
{
    char token[32];
    size_t length = 0;
    if (fscanf(file, "%31s", token) != 1)
        return 0;
    for (size_t i = 0; token[i]; ++i)
    {
        if (token[i] < '0' || token[i] > '9')
            return 0;
        length = length * 10 + (unsigned)(token[i] - '0');
        if (length >= sizeof(group->name))
            return 0;
    }
    if (!length || !end_line(file) || fread(group->name, 1, length, file) != length || memchr(group->name, 0, length) ||
        !end_line(file))
        return 0;
    group->name[length] = 0;
    return bench_group_name_valid(group->name);
}

static int read_groups(FILE *file, Workbench *bench, int version)
{
    int count;
    if (!count_record(file, "groups", WORKBENCH_MAX_COMPONENTS, &count))
        return 0;
    for (int i = 0; i < count; ++i)
    {
        int id;
        float x, y;
        char tag[32];
        if (fscanf(file, "%31s %d %f %f", tag, &id, &x, &y) != 4 || strcmp(tag, "group") || id < 1 ||
            id > WORKBENCH_MAX_COMPONENTS || bench->groups[id - 1].active || !isfinite(x) || !isfinite(y) ||
            fabsf(x) > PROJECT_COORDINATE_LIMIT || fabsf(y) > PROJECT_COORDINATE_LIMIT)
            return 0;
        BenchGroup *group = &bench->groups[id - 1];
        *group = (BenchGroup){.active = 1, .x = x, .y = y};
        if (version >= 22)
        {
            if (fscanf(file, "%f %f", &group->width, &group->height) != 2 ||
                !isfinite(group->width) || !isfinite(group->height) ||
                (group->width != 0 && (group->width < BENCH_GROUP_MIN_WIDTH ||
                                       group->width > BENCH_GROUP_MAX_EXTENT)) ||
                (group->height != 0 && (group->height < BENCH_GROUP_MIN_HEIGHT ||
                                        group->height > BENCH_GROUP_MAX_EXTENT)))
                return 0;
        }
        if (version >= 28 && fscanf(file, "%" SCNu64, &group->model) != 1)
            return 0;
        if (version >= 29 && (fscanf(file, "%d", &group->rotation) != 1 || group->rotation < 0 || group->rotation > 3))
            return 0;
        if (version >= 34 && (fscanf(file, "%d", &group->mirrored) != 1 || group->mirrored < 0 || group->mirrored > 1))
            return 0;
        if (version >= 39 && (fscanf(file, "%d", &group->rotation_diagonal) != 1 ||
                              group->rotation_diagonal < 0 || group->rotation_diagonal > 1))
            return 0;
        if (version >= 30 && !read_port_layout(file, group))
            return 0;
        if (version >= 32 && (fscanf(file, "%d", &group->parent) != 1 || group->parent < 0 ||
                             group->parent > WORKBENCH_MAX_COMPONENTS || group->parent == id))
            return 0;
        if (version >= 17)
        {
            if (!read_group_name(file, group))
                return 0;
        }
        else
            snprintf(group->name, sizeof(group->name), tr(TXT_CHIP_DEFAULT_NAME), id);
    }
    return 1;
}

static int read_members(FILE *file, Workbench *bench, int version)
{
    int count;
    if (!count_record(file, "members", bench->part_count, &count))
        return 0;
    for (int i = 0; i < count; ++i)
    {
        char tag[32];
        int part, group;
        if (fscanf(file, "%31s %d %d", tag, &part, &group) != 3 || strcmp(tag, "member") || part < 0 ||
            part >= bench->part_count || group < 1 || group > WORKBENCH_MAX_COMPONENTS ||
            !bench->groups[group - 1].active || bench->parts[part].group)
            return 0;
        bench->parts[part].group = group;
    }
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
    {
        int members = 0;
        for (int p = 0; p < bench->part_count; ++p)
            members += bench->parts[p].group == g + 1;
        if (bench->groups[g].active && !members && version < 32)
            return 0;
    }
    return 1;
}

int bench_groups_read(FILE *file, Workbench *bench, int version)
{
    int count;
    if (!read_groups(file, bench, version) || !read_members(file, bench, version) ||
        !count_record(file, "ports", bench->part_count * WORKBENCH_MAX_PINS, &count))
        return 0;
    for (int i = 0; i < count; ++i)
    {
        char tag[32];
        int part, pin;
        if (fscanf(file, "%31s %d %d", tag, &part, &pin) != 3 || strcmp(tag, "port") || part < 0 ||
            part >= bench->part_count || !bench->parts[part].group || pin < 0 ||
            pin >= bench_pin_count(bench->parts[part].type) || exposed(&bench->parts[part], pin))
            return 0;
        bench->parts[part].exposed[pin / 64] |= UINT64_C(1) << (pin % 64);
    }
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        const Part *a = &bench->parts[wire->a.component], *b = &bench->parts[wire->b.component];
        if (a->group == b->group)
            continue;
        if ((a->group && !exposed(a, wire->a.pin)) || (b->group && !exposed(b, wire->b.pin)))
            return 0;
    }
    return 1;
}
