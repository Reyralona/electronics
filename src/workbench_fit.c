#include "workbench_view.h"
#include "component_registry.h"
#include "workbench_pack.h"
#include <math.h>
typedef struct
{
    float min_x, min_y, max_x, max_y;
    int valid;
} ViewBounds;

static void bounds_add(ViewBounds *bounds, WirePoint point)
{
    if (!isfinite(point.x) || !isfinite(point.y))
        return;
    if (!bounds->valid)
    {
        *bounds = (ViewBounds){point.x, point.y, point.x, point.y, 1};
        return;
    }
    bounds->min_x = fminf(bounds->min_x, point.x);
    bounds->min_y = fminf(bounds->min_y, point.y);
    bounds->max_x = fmaxf(bounds->max_x, point.x);
    bounds->max_y = fmaxf(bounds->max_y, point.y);
}

void bench_view_fit_workbench(BenchView *view, const Workbench *bench)
{
    ViewBounds bounds = {0};
    for (int i = 0; i < bench->part_count; ++i)
    {
        const Part *part = &bench->parts[i];
        if (part->group)
            continue;
        float half_width, half_height;
        component_body_extent(part->type, &half_width, &half_height);
        float scale = bench_part_scale(part);
        for (int x = -1; x <= 1; x += 2)
            for (int y = -1; y <= 1; y += 2)
                bounds_add(&bounds, bench_part_to_world(part, (WirePoint){x * half_width * scale, y * half_height * scale}));
        for (int pin = 0; pin < bench_pin_count(part->type); ++pin)
        {
            float x, y;
            bench_terminal_position(bench, (Pin){i, pin}, &x, &y);
            bounds_add(&bounds, (WirePoint){x, y});
        }
    }
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
    {
        const BenchGroup *group = &bench->groups[i];
        if (!group->active || group->parent)
            continue;
        float half_width = bench_group_width(bench, i) / 2;
        float half_height = bench_group_height(bench, i) / 2;
        for (int x = -1; x <= 1; x += 2)
            for (int y = -1; y <= 1; y += 2)
                bounds_add(&bounds, bench_group_to_world(group, (WirePoint){x * half_width, y * half_height}));
    }
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        if (bench_group_wire_internal(bench, wire))
            continue;
        for (int k = 0; k < wire->knee_count; ++k)
            bounds_add(&bounds, wire->knees[k]);
    }
    if (bounds.valid)
        bench_view_fit(view, bounds.min_x, bounds.min_y, bounds.max_x, bounds.max_y);
}


