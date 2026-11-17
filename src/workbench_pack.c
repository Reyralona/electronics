#include "workbench_pack.h"
#include "ui_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static void expose_pin(Part *part, int pin)
{
    part->exposed[pin / 64] |= UINT64_C(1) << (pin % 64);
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
            int after = -1;
            while (bench_find_contact_after(bench, i, p, after, &contact))
            {
                after = contact.component * WORKBENCH_MAX_PINS + contact.pin;
                if (((selection->parts[i] != 0) != (selection->parts[contact.component] != 0) ||
                     (selection->parts[i] && bench->parts[i].group != bench->parts[contact.component].group)) &&
                    !ensure_wire(bench, (Pin){i, p}, contact))
                    return 0;
            }
        }
    return 1;
}

/* ---- Port planning: a block exposes one terminal per net that is free or reaches the outside ---- */
#define PACK_PIN_TOTAL (WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS)

static int pin_slot(Pin pin)
{
    return pin.component * WORKBENCH_MAX_PINS + pin.pin;
}

static Pin slot_pin(int slot)
{
    return (Pin){slot / WORKBENCH_MAX_PINS, slot % WORKBENCH_MAX_PINS};
}

static int pack_root(int *parents, int slot)
{
    while (parents[slot] != slot)
    {
        parents[slot] = parents[parents[slot]];
        slot = parents[slot];
    }
    return slot;
}

static void pack_join(int *parents, int a, int b)
{
    parents[pack_root(parents, a)] = pack_root(parents, b);
}

/* inner_only keeps just the links whose both ends belong to the selection. */
static void pack_nets(const Workbench *bench, const BenchSelection *selection, int *parents, int inner_only)
{
    for (int i = 0; i < PACK_PIN_TOTAL; ++i)
        parents[i] = i;
    for (int i = 0; i < bench->wire_count; ++i)
    {
        Pin a = bench->wires[i].a, b = bench->wires[i].b;
        if (!inner_only || (selection->parts[a.component] && selection->parts[b.component]))
            pack_join(parents, pin_slot(a), pin_slot(b));
    }
    for (int i = 0; i < bench->part_count; ++i)
    {
        int board = bench->parts[i].type == PART_BREADBOARD;
        if (inner_only && !selection->parts[i])
            continue;
        for (int p = 0; board && p < BREADBOARD_PINS; ++p)
            pack_join(parents, pin_slot((Pin){i, bench_board_group_first(p)}), pin_slot((Pin){i, p}));
        for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
        {
            Pin contact;
            int after = -1;
            while (bench_find_contact_after(bench, i, p, after, &contact))
            {
                after = pin_slot(contact);
                if (!inner_only || selection->parts[contact.component])
                    pack_join(parents, pin_slot((Pin){i, p}), after);
            }
        }
    }
}

typedef struct
{
    int *full, *inner, *representative;
    unsigned char *external, *members;
} PortPlan;

static void port_plan_free(PortPlan *plan)
{
    free(plan->full);
    free(plan->inner);
    free(plan->representative);
    free(plan->external);
    free(plan->members);
}

static int port_plan_alloc(PortPlan *plan)
{
    plan->full = malloc(sizeof(int) * PACK_PIN_TOTAL);
    plan->inner = malloc(sizeof(int) * PACK_PIN_TOTAL);
    plan->representative = malloc(sizeof(int) * PACK_PIN_TOTAL);
    plan->external = calloc(PACK_PIN_TOTAL, 1);
    plan->members = calloc(PACK_PIN_TOTAL, 1);
    return plan->full && plan->inner && plan->representative && plan->external && plan->members;
}

static int member_pin(const Workbench *bench, const BenchSelection *selection, int component)
{
    return selection->parts[component] && bench->parts[component].type != PART_BREADBOARD;
}

static int candidate_pin(const Part *part, int pin)
{
    const BenchPartLayer *layer = part->layers;
    return !layer || !layer->group ||
           (layer->exposed[pin / 64] & (UINT64_C(1) << (pin % 64))) != 0;
}

/* For each net: its first selected terminal, how many selected terminals it joins (saturating at 2)
 * and whether any part outside the selection (other than a bare breadboard strip) is on it. */
static void port_plan_classify(const Workbench *bench, const BenchSelection *selection, PortPlan *plan)
{
    for (int i = 0; i < PACK_PIN_TOTAL; ++i)
        plan->representative[i] = -1;
    for (int i = 0; i < bench->part_count; ++i)
        for (int p = 0; p < bench_pin_count(bench->parts[i].type); ++p)
        {
            int slot = pin_slot((Pin){i, p}), net = pack_root(plan->full, slot);
            if (member_pin(bench, selection, i))
            {
                if (!candidate_pin(&bench->parts[i], p))
                    continue;
                if (plan->representative[net] < 0)
                    plan->representative[net] = slot;
                plan->members[net] = plan->members[net] < 2 ? plan->members[net] + 1 : 2;
            }
            else if (!selection->parts[i] && bench->parts[i].type != PART_BREADBOARD)
                plan->external[net] = 1;
        }
}

/* Members reach their representative inside the block; links through an outside breadboard are
 * replaced by internal wires so the block keeps working after it leaves the board. */
static int port_plan_expose(Workbench *bench, const BenchSelection *selection, PortPlan *plan)
{
    for (int i = 0; i < bench->part_count; ++i)
        for (int p = 0; member_pin(bench, selection, i) && p < bench_pin_count(bench->parts[i].type); ++p)
        {
            int slot = pin_slot((Pin){i, p}), net = pack_root(plan->full, slot);
            int representative = plan->representative[net];
            if (representative < 0)
                continue;
            if (slot == representative)
            {
                if (plan->external[net] || plan->members[net] == 1)
                    expose_pin(&bench->parts[i], p);
                continue;
            }
            if (pack_root(plan->inner, slot) == pack_root(plan->inner, representative))
                continue;
            if (!ensure_wire(bench, (Pin){i, p}, slot_pin(representative)))
                return 0;
            pack_join(plan->inner, slot, representative);
        }
    return 1;
}

static int wire_duplicate(const Workbench *bench, int skip, Pin a, Pin b)
{
    for (int i = 0; i < bench->wire_count; ++i)
    {
        const Wire *wire = &bench->wires[i];
        if (i != skip && ((same_pin(a, wire->a) && same_pin(b, wire->b)) || (same_pin(a, wire->b) && same_pin(b, wire->a))))
            return 1;
    }
    return 0;
}

/* Returns 0 when the wire must go: its inner end has no port to move to. */
static int reroute_end(const Workbench *bench, const BenchSelection *selection, PortPlan *plan, Pin *end, Pin other)
{
    if (!selection->parts[end->component] || selection->parts[other.component] ||
        (bench->parts[end->component].type != PART_BREADBOARD && bench_pin_exposed(&bench->parts[end->component], end->pin)))
        return 1;
    int representative = plan->representative[pack_root(plan->full, pin_slot(*end))];
    if (representative < 0 || !bench_pin_exposed(&bench->parts[representative / WORKBENCH_MAX_PINS],
                                                 representative % WORKBENCH_MAX_PINS))
        return 0;
    *end = slot_pin(representative);
    return 1;
}

/* Boundary wires on hidden terminals move to the exposed terminal of the same net. */
static void port_plan_reroute(Workbench *bench, const BenchSelection *selection, PortPlan *plan)
{
    for (int i = bench->wire_count - 1; i >= 0; --i)
    {
        Wire *wire = &bench->wires[i];
        Pin a = wire->a, b = wire->b;
        int keep = reroute_end(bench, selection, plan, &a, b) && reroute_end(bench, selection, plan, &b, a);
        if (keep && !wire_duplicate(bench, i, a, b))
        {
            wire->a = a;
            wire->b = b;
        }
        else
            bench_remove_wire(bench, i);
    }
}

static int plan_ports(Workbench *bench, const BenchSelection *selection)
{
    PortPlan plan = {0};
    int planned = port_plan_alloc(&plan);
    if (planned)
    {
        pack_nets(bench, selection, plan.full, 0);
        pack_nets(bench, selection, plan.inner, 1);
        port_plan_classify(bench, selection, &plan);
        planned = port_plan_expose(bench, selection, &plan);
    }
    if (planned)
        port_plan_reroute(bench, selection, &plan);
    port_plan_free(&plan);
    return planned;
}

int bench_pack_selection(Workbench *bench, const BenchSelection *selection, char *message, size_t capacity)
{
    BenchSelection expanded = *selection;
    bench_selection_expand_groups(&expanded, bench);
    selection = &expanded;
    int count = bench_selection_count(selection, bench), slot = -1;
    if (!count)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_PACK_SELECT_PARTS));
        return 0;
    }
    for (int i = 0; i < WORKBENCH_MAX_COMPONENTS; ++i)
        if (!bench->groups[i].active && slot < 0)
            slot = i;
    if (slot < 0)
    {
        snprintf(message, capacity, tr(TXT_ENG_PACK_CHIP_LIMIT), WORKBENCH_MAX_COMPONENTS);
        return 0;
    }
    Workbench packed = {0};
    if (!bench_snapshot_clone(&packed, bench))
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY_BENCH_KEPT));
        return 0;
    }
    if (!freeze_boundary_contacts(&packed, selection))
    {
        bench_snapshot_clear(&packed);
        snprintf(message, capacity, tr(TXT_ENG_PACK_EDGE_WIRES), WORKBENCH_MAX_WIRES);
        return 0;
    }
    for (int i = 0; i < packed.part_count; ++i)
        if (selection->parts[i] && packed.parts[i].group && !bench_part_push_layer(&packed.parts[i]))
        {
            bench_snapshot_clear(&packed);
            snprintf(message, capacity, "%s", tr(TXT_ENG_NO_MEMORY_BENCH_KEPT));
            return 0;
        }
    if (!plan_ports(&packed, selection))
    {
        bench_snapshot_clear(&packed);
        snprintf(message, capacity, tr(TXT_ENG_PACK_INNER_WIRES), WORKBENCH_MAX_WIRES);
        return 0;
    }
    BenchGroup group = {.active = 1};
    snprintf(group.name, sizeof(group.name), tr(TXT_CHIP_DEFAULT_NAME), slot + 1);
    for (int i = 0; i < packed.part_count; ++i)
        if (selection->parts[i])
        {
            group.x += packed.parts[i].x / count;
            group.y += packed.parts[i].y / count;
        }
    for (int i = 0; i < packed.part_count; ++i)
        if (selection->parts[i])
        {
            if (packed.parts[i].group)
                packed.groups[packed.parts[i].group - 1].parent = slot + 1;
            packed.parts[i].group = slot + 1;
        }
    packed.groups[slot] = group;
    int published = bench_replace(bench, &packed);
    bench_snapshot_clear(&packed);
    if (!published)
    {
        snprintf(message, capacity, "%s", tr(TXT_ENG_PACK_FAILED));
        return 0;
    }
    snprintf(message, capacity, tr(TXT_ENG_PACK_CREATED), bench_group_ports(bench, slot + 1));
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
            bench_part_pop_layer(&bench->parts[i]);
            changed = 1;
        }
    for (int g = 0; g < WORKBENCH_MAX_COMPONENTS; ++g)
    {
        if (bench->groups[g].parent && groups[bench->groups[g].parent - 1])
            bench->groups[g].parent = 0;
        if (groups[g])
            memset(&bench->groups[g], 0, sizeof(bench->groups[g]));
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
