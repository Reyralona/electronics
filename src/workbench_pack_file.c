#include "workbench_pack_file.h"
#include <math.h>
#include <string.h>
#define PROJECT_COORDINATE_LIMIT 100000.0f

static int exposed(const Part *part, int pin)
{
    return (part->exposed[pin / 64] & (UINT64_C(1) << (pin % 64))) != 0;
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
        if (bench->groups[i].active &&
            fprintf(file, "group %d %.9g %.9g\n", i + 1, bench->groups[i].x, bench->groups[i].y) < 0)
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

static int read_groups(FILE *file, Workbench *bench)
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
        bench->groups[id - 1] = (BenchGroup){1, x, y};
    }
    return 1;
}

static int read_members(FILE *file, Workbench *bench)
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
        if (bench->groups[g].active && !members)
            return 0;
    }
    return 1;
}

int bench_groups_read(FILE *file, Workbench *bench)
{
    int count;
    if (!read_groups(file, bench) || !read_members(file, bench) ||
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
