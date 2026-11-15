#include "workbench_group_edit.h"
#include "workbench_group_name.h"
#include "workbench_pack.h"
#include "component_registry.h"
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
            copy.bench.parts[next].group = 0;
            copy.bench.parts[next++].edit_origin = i + 1;
        }
    memset(copy.bench.groups, 0, sizeof(copy.bench.groups));
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
        if (!info || part->group || !isfinite(bench_part_scale(part)) ||
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
        snprintf(message, capacity, "A bancada mudou; reabra o editor antes de aplicar.");
        return 0;
    }
    if (!valid_draft(parent, group, draft) || !bench_group_name_valid(name))
    {
        snprintf(message, capacity,
                 "Modelo invalido, nome invalido ou bloco aninhado. Abra os blocos internos antes de aplicar.");
        return 0;
    }
    Workbench packed = {0}, result = {0};
    int published = 0;
    if (!bench_snapshot_clone(&packed, draft))
    {
        snprintf(message, capacity, "Nao foi possivel copiar o rascunho; bancada preservada.");
        goto cleanup;
    }
    BenchSelection all = {0};
    for (int i = 0; i < draft->part_count; ++i)
        all.parts[i] = 1;
    if (!bench_pack_selection(&packed, &all, message, capacity))
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
                snprintf(message, capacity, "Memoria insuficiente para preservar componentes externos.");
                goto cleanup;
            }
            ++result.part_count;
        }
    }
    int offset = result.part_count;
    if (offset + packed.part_count > WORKBENCH_MAX_COMPONENTS)
    {
        snprintf(message, capacity, "Nao cabe na bancada: limite atual de 64 pecas internas.");
        goto cleanup;
    }
    for (int i = 0; i < packed.part_count; ++i)
    {
        Part part = packed.parts[i];
        if (part.edit_origin)
            map[part.edit_origin - 1] = result.part_count;
        part.edit_origin = 0;
        part.group = group;
        if (!bench_part_clone(&result.parts[result.part_count], &part))
        {
            snprintf(message, capacity, "Memoria insuficiente para copiar componente do chip.");
            goto cleanup;
        }
        ++result.part_count;
    }
    if (!copy_external_wires(parent, group, map, &result))
    {
        snprintf(message, capacity, "Uma porta conectada foi removida ou faltam fios livres; bancada preservada.");
        goto cleanup;
    }
    for (int i = 0; i < packed.wire_count; ++i)
    {
        if (result.wire_count == WORKBENCH_MAX_WIRES)
        {
            snprintf(message, capacity, "Nao cabe na bancada: limite atual de 128 fios.");
            goto cleanup;
        }
        Wire wire = packed.wires[i];
        wire.a.component += offset;
        wire.b.component += offset;
        result.wires[result.wire_count++] = wire;
    }
    memcpy(result.groups[group - 1].name, name, strlen(name) + 1);
    published = bench_replace(parent, &result);
    if (!published)
    {
        snprintf(message, capacity, "Nao foi possivel publicar o modelo; bancada preservada.");
        goto cleanup;
    }
    snprintf(message, capacity, "Modelo aplicado nesta instancia. Ctrl+Z desfaz.");
cleanup:
    bench_snapshot_clear(&packed);
    bench_snapshot_clear(&result);
    return published;
}
