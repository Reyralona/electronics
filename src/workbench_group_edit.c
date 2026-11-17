#include "workbench_group_edit.h"
#include "workbench_group_name.h"
#include "workbench_pack.h"
#include "component_registry.h"
#include "ui_text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int bench_group_extract(const Workbench *parent, int group, Workbench *draft)
{
    if (!parent || !draft || parent == draft || group < 1 || group > WORKBENCH_MAX_COMPONENTS ||
        !parent->groups[group - 1].active)
        return 0;
    BenchSelection selection = {0};
    BenchClipboard copy = {0};
    for (int i = 0; i < parent->part_count; ++i)
        selection.parts[i] = parent->parts[i].group == group;
    if (!bench_selection_copy(parent, &selection, &copy))
        return 0;
    int next = 0;
    for (int i = 0; i < parent->part_count; ++i)
        if (selection.parts[i])
        {
            copy.bench.parts[next++].edit_origin = i + 1;
        }
    for (int i = 0; i < copy.bench.part_count; ++i)
        if (copy.bench.parts[i].layers)
            bench_part_pop_layer(&copy.bench.parts[i]);
        else
            copy.bench.parts[i].group = 0;
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (copy.bench.groups[g].parent == group)
            copy.bench.groups[g].parent = 0;
    memset(&copy.bench.groups[group - 1], 0, sizeof(BenchGroup));
    int published = bench_replace(draft, &copy.bench);
    bench_snapshot_clear(&copy.bench);
    return published;
}

static int valid_draft(const Workbench *parent, int group, const Workbench *draft)
{
    unsigned char seen[WORKBENCH_MAX_COMPONENTS] = {0};
    if (draft->part_count < 1 || draft->part_count > WORKBENCH_MAX_COMPONENTS || draft->wire_count < 0 ||
        draft->wire_count > WORKBENCH_MAX_WIRES)
        return 0;
    for (int i = 0; i < draft->part_count; ++i)
    {
        const Part *part = &draft->parts[i];
        const ComponentInfo *info = component_info(part->type);
        if (!info || !isfinite(part->control) || part->control < 0 || part->control > 1 ||
            !isfinite(bench_part_scale(part)) ||
            bench_part_scale(part) < PART_SCALE_MIN || bench_part_scale(part) > PART_SCALE_MAX || !isfinite(part->x) || !isfinite(part->y) ||
            (info->primary && !component_value_valid(part->type, part->value)) || part->rotation < 0 ||
            part->rotation > 3 || (part->on != 0 && part->on != 1))
            return 0;
        if (part->edit_origin < 0 || part->edit_origin > parent->part_count)
            return 0;
        int origin = part->edit_origin - 1;
        if (part->edit_origin)
        {
            if (origin < 0 || origin >= parent->part_count || seen[origin] || parent->parts[origin].group != group ||
                parent->parts[origin].type != part->type)
                return 0;
            seen[origin] = 1;
        }
    }
    for (int i = 0; i < draft->wire_count; ++i)
    {
        const Wire *wire = &draft->wires[i];
        Pin ends[] = {wire->a, wire->b};
        for (int e = 0; e < 2; ++e)
            if (ends[e].component < 0 || ends[e].component >= draft->part_count || ends[e].pin < 0 ||
                ends[e].pin >= bench_pin_count(draft->parts[ends[e].component].type))
                return 0;
        if (wire->knee_count < 0 || wire->knee_count > WIRE_MAX_KNEES || wire->color < 0 ||
            wire->color >= WIRE_COLOR_COUNT)
            return 0;
        for (int k = 0; k < wire->knee_count; ++k)
            if (!isfinite(wire->knees[k].x) || !isfinite(wire->knees[k].y))
                return 0;
    }
    return 1;
}

static int copy_external_wires(const Workbench *parent, int group, const int map[], Workbench *result)
{
    for (int i = 0; i < parent->wire_count; ++i)
    {
        Wire wire = parent->wires[i];
        if (parent->parts[wire.a.component].group == group && parent->parts[wire.b.component].group == group)
            continue;
        Pin *ends[] = {&wire.a, &wire.b};
        for (int e = 0; e < 2; ++e)
        {
            Pin *pin = ends[e];
            int target = map[pin->component];
            if (target < 0 || pin->pin >= bench_pin_count(result->parts[target].type) ||
                (parent->parts[pin->component].group == group && !bench_pin_exposed(&result->parts[target], pin->pin)))
                return 0;
            pin->component = target;
        }
        if (result->wire_count == WORKBENCH_MAX_WIRES)
            return 0;
        result->wires[result->wire_count++] = wire;
    }
    return 1;
}

int bench_group_apply(Workbench *parent, int group, uint64_t expected_revision, const Workbench *draft,
                      const char *name, char *message, size_t capacity)
{
    if (!parent || !draft || group < 1 || group > WORKBENCH_MAX_COMPONENTS || !parent->groups[group - 1].active ||
        parent->revision != expected_revision)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_EDIT_BENCH_CHANGED));
        return 0;
    }
    if (!valid_draft(parent, group, draft) || !bench_group_name_valid(name))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_EDIT_INVALID));
        return 0;
    }
    Workbench packed = {0}, result = {0};
    int published = 0;
    if (!bench_snapshot_clone(&packed, draft))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY_BENCH_KEPT));
        goto cleanup;
    }
    BenchSelection all = {0};
    for (int i = 0; i < draft->part_count; ++i)
        all.parts[i] = 1;
    int packed_group = bench_pack_selection(&packed, &all, message, capacity);
    if (!packed_group)
        goto cleanup;
    int map[WORKBENCH_MAX_COMPONENTS];
    memcpy(result.groups, parent->groups, sizeof(result.groups));
    for (int i = 0; i < parent->part_count; ++i)
    {
        map[i] = -1;
        if (parent->parts[i].group != group)
        {
            map[i] = result.part_count;
            if (!bench_part_clone(&result.parts[result.part_count], &parent->parts[i]))
            {
                snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY_BENCH_KEPT));
                goto cleanup;
            }
            ++result.part_count;
        }
    }
    int offset = result.part_count;
    if (offset + packed.part_count > WORKBENCH_MAX_COMPONENTS)
    {
        snprintf(message, capacity, tr(TXT_ENG_EDIT_PART_LIMIT), WORKBENCH_MAX_COMPONENTS);
        goto cleanup;
    }
    BenchGroup outer = parent->groups[group - 1];
    bench_groups_prune(&result);
    result.groups[group - 1] = outer;
    BenchClipboard clipboard = {.valid = 1};
    /* Borrowed only for this paste; the clipboard does not own packed here. */
    clipboard.bench = packed;
    BenchSelection pasted = {0};
    if (!bench_selection_paste(&result, &pasted, &clipboard, packed.parts[0].x, packed.parts[0].y))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_EDIT_APPLY_FAILED));
        goto cleanup;
    }
    int inserted_group = result.parts[offset].group;
    for (int i = 0; i < packed.part_count; ++i)
    {
        if (packed.parts[i].edit_origin)
        {
            map[packed.parts[i].edit_origin - 1] = offset + i;
            result.parts[offset + i].edit_origin = parent->parts[packed.parts[i].edit_origin - 1].edit_origin;
        }
        result.parts[offset + i].group = group;
    }
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
        if (result.groups[g].parent == inserted_group)
            result.groups[g].parent = group;
    memset(&result.groups[inserted_group - 1], 0, sizeof(BenchGroup));
    Wire internal[WORKBENCH_MAX_WIRES];
    int internal_count = result.wire_count;
    memcpy(internal, result.wires, (size_t)internal_count * sizeof(Wire));
    result.wire_count = 0;
    if (!copy_external_wires(parent, group, map, &result))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_EDIT_TERMINAL_REMOVED));
        goto cleanup;
    }
    if (result.wire_count + internal_count > WORKBENCH_MAX_WIRES)
    {
        snprintf(message, capacity, tr(TXT_ENG_EDIT_WIRE_LIMIT), WORKBENCH_MAX_WIRES);
        goto cleanup;
    }
    memcpy(&result.wires[result.wire_count], internal, (size_t)internal_count * sizeof(Wire));
    result.wire_count += internal_count;
    memcpy(result.groups[group - 1].name, name, strlen(name) + 1);
    published = bench_replace(parent, &result);
    if (!published)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_EDIT_APPLY_FAILED));
        goto cleanup;
    }
    snprintf(message, capacity, "%s", tr(TXT_ENG_EDIT_APPLIED));
cleanup:
    bench_snapshot_clear(&packed);
    bench_snapshot_clear(&result);
    return published;
}
