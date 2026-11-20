#include "workbench_selection.h"
#include <math.h>
#include <string.h>

void bench_selection_expand_groups(BenchSelection *selection, const Workbench *bench)
{
    unsigned char groups[WORKBENCH_MAX_COMPONENTS] = {0};
    for (int i = 0; i < bench->part_count; ++i)
        if (selection->parts[i] && bench->parts[i].group)
            groups[bench->parts[i].group - 1] = 1;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group && groups[bench->parts[i].group - 1])
            selection->parts[i] = 1;
}

void bench_selection_clear(BenchSelection *selection)
{
    memset(selection, 0, sizeof(*selection));
}

int bench_selection_count(const BenchSelection *selection, const Workbench *bench)
{
    int count = 0;
    for (int i = 0; i < bench->part_count; ++i)
        count += selection->parts[i] != 0;
    return count;
}

int bench_selection_knee_count(const BenchSelection *selection, const Workbench *bench)
{
    int count = 0;
    for (int i = 0; i < bench->wire_count; ++i)
        for (int k = 0; k < bench->wires[i].knee_count; ++k)
            count += (selection->knees[i] & (1u << k)) != 0;
    return count;
}

void bench_selection_rect(BenchSelection *selection, const Workbench *bench, WirePoint a, WirePoint b)
{
    bench_selection_clear(selection);
    float left = fminf(a.x, b.x), right = fmaxf(a.x, b.x);
    float top = fminf(a.y, b.y), bottom = fmaxf(a.y, b.y);
    for (int i = 0; i < bench->part_count; ++i)
    {
        const Part *part = &bench->parts[i];
        if (part->group)
        {
            const BenchGroup *group = &bench->groups[part->group - 1];
            selection->parts[i] = group->x >= fminf(a.x, b.x) && group->x <= fmaxf(a.x, b.x) &&
                                  group->y >= fminf(a.y, b.y) && group->y <= fmaxf(a.y, b.y);
            continue;
        }
        selection->parts[i] = part->x >= fminf(a.x, b.x) && part->x <= fmaxf(a.x, b.x) && part->y >= fminf(a.y, b.y) &&
                              part->y <= fmaxf(a.y, b.y);
    }
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        int group = bench->parts[wire->a.component].group;
        if (group && group == bench->parts[wire->b.component].group)
            continue;
        for (int k = 0; k < bench->wires[i].knee_count; ++k)
        {
            WirePoint point = bench->wires[i].knees[k];
            if (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom)
                selection->knees[i] |= (unsigned short)(1u << k);
        }
    }
}

int bench_selection_copy(const Workbench *bench, const BenchSelection *selection, BenchClipboard *clipboard)
{
    BenchSelection expanded = *selection;
    bench_selection_expand_groups(&expanded, bench);
    selection = &expanded;
    Workbench copy = {0};
    int map[WORKBENCH_MAX_COMPONENTS];
    for (int i = 0; i < bench->part_count; ++i)
    {
        map[i] = -1;
        if (selection->parts[i])
        {
            map[i] = copy.part_count;
            if (!bench_part_clone(&copy.parts[copy.part_count], &bench->parts[i]))
            {
                bench_snapshot_clear(&copy);
                return 0;
            }
            ++copy.part_count;
            if (bench->parts[i].group)
                copy.groups[bench->parts[i].group - 1] = bench->groups[bench->parts[i].group - 1];
            for (const BenchPartLayer *layer = bench->parts[i].layers; layer; layer = layer->next)
                if (layer->group)
                    copy.groups[layer->group - 1] = bench->groups[layer->group - 1];
        }
    }
    if (!copy.part_count)
        return 0;
    for (int i = 0; i < bench->wire_count; ++i)
    {
        Wire wire = bench->wires[i];
        if (map[wire.a.component] < 0 || map[wire.b.component] < 0)
            continue;
        wire.a.component = map[wire.a.component];
        wire.b.component = map[wire.b.component];
        copy.wires[copy.wire_count++] = wire;
    }
    bench_snapshot_move(&clipboard->bench, &copy);
    clipboard->valid = 1;
    return 1;
}

static int paste_into(Workbench *bench, BenchSelection *selection, const BenchClipboard *clipboard, float x,
                          float y)
{
    if (!clipboard->valid || !isfinite(x) || !isfinite(y))
        return 0;
    const Workbench *copy = &clipboard->bench;
    if (bench->part_count + copy->part_count > WORKBENCH_MAX_COMPONENTS ||
        bench->wire_count + copy->wire_count > WORKBENCH_MAX_WIRES)
        return 0;
    float anchor_x = copy->parts[0].x, anchor_y = copy->parts[0].y;
    int group = copy->parts[0].group;
    if (group)
    {
        anchor_x = copy->groups[group - 1].x;
        anchor_y = copy->groups[group - 1].y;
    }
    float dx = x - anchor_x, dy = y - anchor_y;
    if (!isfinite(dx) || !isfinite(dy))
        return 0;
    for (int i = 0; i < copy->part_count; ++i)
        if (!isfinite(copy->parts[i].x + dx) || !isfinite(copy->parts[i].y + dy))
            return 0;
    for (int i = 0; i < copy->wire_count; ++i)
        for (int k = 0; k < copy->wires[i].knee_count; ++k)
            if (!isfinite(copy->wires[i].knees[k].x + dx) || !isfinite(copy->wires[i].knees[k].y + dy))
                return 0;
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (copy->groups[g].active && (!isfinite(copy->groups[g].x + dx) || !isfinite(copy->groups[g].y + dy)))
            return 0;
    int group_map[WORKBENCH_MAX_COMPONENTS] = {0};
    unsigned char reserved[WORKBENCH_MAX_COMPONENTS] = {0};
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (copy->groups[g].active)
        {
            int target = 0;
            while (target < WORKBENCH_MAX_COMPONENTS && (bench->groups[target].active || reserved[target]))
                ++target;
            if (target == WORKBENCH_MAX_COMPONENTS)
                return 0;
            group_map[g] = target + 1;
            reserved[target] = 1;
        }
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (group_map[g])
        {
            BenchGroup group = copy->groups[g];
            group.x += dx;
            group.y += dy;
            if (group.parent)
                group.parent = group_map[group.parent - 1];
            bench->groups[group_map[g] - 1] = group;
        }
    int offset = bench->part_count;
    bench_selection_clear(selection);
    for (int i = 0; i < copy->part_count; ++i)
    {
        Part part = copy->parts[i];
        part.edit_origin = 0;
        part.runtime_identity = 0;
        part.x += dx;
        part.y += dy;
        if (part.group)
            part.group = group_map[part.group - 1];
        selection->parts[bench->part_count] = 1;
        if (!bench_part_clone(&bench->parts[bench->part_count], &part))
            return 0;
        for (BenchPartLayer *layer = bench->parts[bench->part_count].layers; layer; layer = layer->next)
            if (layer->group)
                layer->group = group_map[layer->group - 1];
        ++bench->part_count;
    }
    for (int i = 0; i < copy->wire_count; ++i)
    {
        Wire wire = copy->wires[i];
        wire.a.component += offset;
        wire.b.component += offset;
        for (int k = 0; k < wire.knee_count; ++k)
        {
            wire.knees[k].x += dx;
            wire.knees[k].y += dy;
        }
        bench->wires[bench->wire_count++] = wire;
    }
    ++bench->revision;
    return 1;
}

int bench_selection_paste(Workbench *bench, BenchSelection *selection, const BenchClipboard *clipboard, float x,
                          float y)
{
    if (!bench || !selection || !clipboard || !clipboard->valid || clipboard->bench.part_count < 1 ||
        clipboard->bench.part_count > WORKBENCH_MAX_COMPONENTS || clipboard->bench.wire_count < 0 ||
        clipboard->bench.wire_count > WORKBENCH_MAX_WIRES)
        return 0;
    Workbench candidate = {0};
    BenchSelection candidate_selection = *selection;
    if (!bench_snapshot_clone(&candidate, bench))
        return 0;
    int success = paste_into(&candidate, &candidate_selection, clipboard, x, y);
    if (success)
        success = bench_replace(bench, &candidate);
    if (success)
        *selection = candidate_selection;
    bench_snapshot_clear(&candidate);
    return success;
}

void bench_selection_delete(Workbench *bench, BenchSelection *selection)
{
    bench_selection_expand_groups(selection, bench);
    for (int i = bench->part_count - 1; i >= 0; --i)
        if (selection->parts[i])
            bench_remove(bench, i);
    bench_selection_clear(selection);
}

static int move_valid(const Workbench *bench, const BenchSelection *selection, float dx, float dy)
{
    if (!isfinite(dx) || !isfinite(dy))
        return 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (selection->parts[i])
        {
            const Part *part = &bench->parts[i];
            if (!isfinite(part->x + dx) || !isfinite(part->y + dy))
                return 0;
            if (part->group &&
                (!isfinite(bench->groups[part->group - 1].x + dx) || !isfinite(bench->groups[part->group - 1].y + dy)))
                return 0;
        }
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        int whole_wire = selection->parts[wire->a.component] && selection->parts[wire->b.component];
        for (int k = 0; k < wire->knee_count; ++k)
            if ((whole_wire || (selection->knees[i] & (1u << k))) &&
                (!isfinite(wire->knees[k].x + dx) || !isfinite(wire->knees[k].y + dy)))
                return 0;
    }
    return 1;
}

void bench_selection_move(Workbench *bench, const BenchSelection *selection, float dx, float dy)
{
    BenchSelection expanded = *selection;
    bench_selection_expand_groups(&expanded, bench);
    selection = &expanded;
    if (!move_valid(bench, selection, dx, dy))
        return;
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
    {
        int selected_group = 0;
        for (int i = 0; i < bench->part_count; ++i)
        {
            selected_group |= selection->parts[i] && bench->parts[i].group == g + 1;
            for (const BenchPartLayer *layer = bench->parts[i].layers; selection->parts[i] && layer; layer = layer->next)
                selected_group |= layer->group == g + 1;
        }
        if (selected_group)
        {
            bench->groups[g].x += dx;
            bench->groups[g].y += dy;
        }
    }
    for (int i = 0; i < bench->part_count; ++i)
        if (selection->parts[i])
            bench_move_part(bench, i, bench->parts[i].x + dx, bench->parts[i].y + dy);
    for (int i = 0; i < bench->wire_count; ++i)
    {
        Wire *wire = &bench->wires[i];
        int whole_wire = selection->parts[wire->a.component] && selection->parts[wire->b.component];
        for (int k = 0; k < wire->knee_count; ++k)
            if (whole_wire || (selection->knees[i] & (1u << k)))
                bench_move_knee(bench, i, k, (WirePoint){wire->knees[k].x + dx, wire->knees[k].y + dy});
    }
}
