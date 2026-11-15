#include "workbench_pack.h"
#include <stdio.h>
#include <string.h>

int bench_pin_exposed(const Part *part, int pin)
{
    return pin >= 0 && pin < WORKBENCH_MAX_PINS && (part->exposed[pin / 64] & (UINT64_C(1) << (pin % 64))) != 0;
}

static void expose_pin(Part *part, int pin)
{
    part->exposed[pin / 64] |= UINT64_C(1) << (pin % 64);
}

int bench_group_first(const Workbench *bench, int group)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            return i;
    return -1;
}

int bench_group_port(const Workbench *bench, int group, int index, Pin *pin)
{
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (bench_pin_exposed(&bench->parts[i], p) && index-- == 0)
                {
                    *pin = (Pin){i, p};
                    return 1;
                }
    return 0;
}

int bench_group_ports(const Workbench *bench, int group)
{
    int count = 0;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group == group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                count += bench_pin_exposed(&bench->parts[i], p);
    return count;
}

float bench_group_height(const Workbench *bench, int group)
{
    int rows = (bench_group_ports(bench, group) + 1) / 2;
    return rows > 2 ? rows * BENCH_GROUP_PIN_SPACING + 14.0f : 44.0f;
}

void bench_terminal_position(const Workbench *bench, Pin pin, float *x, float *y)
{
    const Part *part = &bench->parts[pin.component];
    if (!part->group || !bench_pin_exposed(part, pin.pin))
    {
        bench_pin_position(part, pin.pin, x, y);
        return;
    }
    int count = bench_group_ports(bench, part->group), index = 0;
    for (int i = 0; i <= pin.component; ++i)
        if (bench->parts[i].group == part->group)
            for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
                if (bench_pin_exposed(&bench->parts[i], p) && (i < pin.component || p < pin.pin))
                    ++index;
    int left = (count + 1) / 2, row = index < left ? index : index - left;
    int rows = index < left ? left : count - left;
    const BenchGroup *group = &bench->groups[part->group - 1];
    *x = group->x + (index < left ? -60.0f : 60.0f);
    *y = group->y + (row - (rows - 1) * .5f) * BENCH_GROUP_PIN_SPACING;
}

static int same_pin(Pin a, Pin b)
{
    return a.component == b.component && a.pin == b.pin;
}

static int ensure_wire(Workbench *bench, Pin a, Pin b)
{
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        if ((same_pin(a, wire->a) && same_pin(b, wire->b)) || (same_pin(a, wire->b) && same_pin(b, wire->a)))
            return 1;
    }
    return bench_toggle_wire(bench, a, b) == 1;
}

static int freeze_boundary_contacts(Workbench *bench, const BenchSelection *selection)
{
    for (int i = 0; i < bench->part_count; ++i)
        for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
        {
            Pin contact;
            if (bench_find_contact(bench, i, p, &contact) &&
                (selection->parts[i] != 0) != (selection->parts[contact.component] != 0) &&
                !ensure_wire(bench, (Pin){i, p}, contact))
                return 0;
        }
    return 1;
}

static int port_needed(const Workbench *bench, const BenchSelection *selection, Pin pin)
{
    int internal = 0;
    Pin contact;
    if (bench_find_contact(bench, pin.component, pin.pin, &contact) && selection->parts[contact.component])
        internal = 1;
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        Pin other;
        if (same_pin(pin, wire->a))
            other = wire->b;
        else if (same_pin(pin, wire->b))
            other = wire->a;
        else
            continue;
        if (!selection->parts[other.component])
            return 1;
        internal = 1;
    }
    return !internal && bench->parts[pin.component].type != PART_BREADBOARD;
}

int bench_pack_selection(Workbench *bench, const BenchSelection *selection, char *message, size_t capacity)
{
    int count = bench_selection_count(selection, bench), slot = -1;
    if (!count)
    {
        snprintf(message, capacity, "Selecione componentes antes de criar um bloco.");
        return 0;
    }
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        if (!bench->groups[i].active && slot < 0)
            slot = i;
    for (int i = 0; i < bench->part_count; ++i)
        if (selection->parts[i] && bench->parts[i].group)
        {
            snprintf(message, capacity, "Desempacote os blocos selecionados antes de combinar seus interiores.");
            return 0;
        }
    if (slot < 0)
    {
        snprintf(message, capacity, "Nao ha espaco para outro bloco na capacidade atual da montagem.");
        return 0;
    }
    Workbench packed = *bench;
    if (!freeze_boundary_contacts(&packed, selection))
    {
        snprintf(message, capacity, "Nao cabe: contatos de fronteira precisam de fios adicionais (limite atual 128).");
        return 0;
    }
    BenchGroup group = {.active = 1};
    snprintf(group.name, sizeof(group.name), "Bloco %d", slot + 1);
    for (int i = 0; i < packed.part_count; ++i)
        if (selection->parts[i])
        {
            Part *part = &packed.parts[i];
            for (int p = 0; p < bench_pin_count(part->type); ++p)
                if (port_needed(&packed, selection, (Pin){i, p}))
                    expose_pin(part, p);
            group.x += part->x / count;
            group.y += part->y / count;
        }
    for (int i = 0; i < packed.part_count; ++i)
        if (selection->parts[i])
            packed.parts[i].group = slot + 1;
    packed.groups[slot] = group;
    bench_replace(bench, &packed);
    snprintf(message, capacity, "Bloco criado com %d terminais. Ctrl+Shift+G desempacota na bancada.",
             bench_group_ports(bench, slot + 1));
    return slot + 1;
}

int bench_unpack_selection(Workbench *bench, const BenchSelection *selection)
{
    int changed = 0;
    unsigned char groups[WORKBENCH_MAX_COMPONENTS] = {0};
    for (int i = 0; i < bench->part_count; ++i)
        if (selection->parts[i] && bench->parts[i].group)
            groups[bench->parts[i].group - 1] = 1;
    for (int i = 0; i < bench->part_count; ++i)
        if (bench->parts[i].group && groups[bench->parts[i].group - 1])
        {
            bench->groups[bench->parts[i].group - 1].active = 0;
            bench->parts[i].group = 0;
            memset(bench->parts[i].exposed, 0, sizeof(bench->parts[i].exposed));
            changed = 1;
        }
    if (changed)
        ++bench->revision;
    return changed;
}

int bench_group_wire_internal(const Workbench *bench, const Wire *wire)
{
    int group = bench->parts[wire->a.component].group;
    return group && group == bench->parts[wire->b.component].group;
}
