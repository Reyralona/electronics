#include "workbench.h"
#include <string.h>

int bench_pin_count(PartType type)
{
    return type == PART_BREADBOARD ? 100 : (type >= PART_SOURCE && type <= PART_LED ? 2 : 0);
}

static int pin_valid(const Workbench *b, Pin p)
{
    return p.component >= 0 && p.component < b->part_count && p.pin >= 0 &&
           p.pin < bench_pin_count(b->parts[p.component].type);
}

static int same_pin(Pin a, Pin b) { return a.component == b.component && a.pin == b.pin; }

int bench_add(Workbench *b, PartType type, float x, float y)
{
    int id;
    if (b->part_count >= WORKBENCH_MAX_COMPONENTS || bench_pin_count(type) == 0) return -1;
    id = b->part_count++;
    b->parts[id] = (Part){type, x, y, type == PART_SOURCE};
    return id;
}

int bench_toggle_wire(Workbench *b, Pin a, Pin c)
{
    int i;
    if (!pin_valid(b, a) || !pin_valid(b, c) || same_pin(a, c)) return 0;
    for (i = 0; i < b->wire_count; ++i) {
        Wire w = b->wires[i];
        if ((same_pin(w.a, a) && same_pin(w.b, c)) || (same_pin(w.a, c) && same_pin(w.b, a))) {
            memmove(&b->wires[i], &b->wires[i + 1], (size_t)(b->wire_count - i - 1) * sizeof(Wire));
            --b->wire_count;
            return 2;
        }
    }
    if (b->wire_count == WORKBENCH_MAX_WIRES) return 0;
    b->wires[b->wire_count++] = (Wire){a, c};
    return 1;
}

void bench_remove(Workbench *b, int index)
{
    int i;
    if (index < 0 || index >= b->part_count) return;
    for (i = b->wire_count - 1; i >= 0; --i) {
        Wire *w = &b->wires[i];
        if (w->a.component == index || w->b.component == index) {
            memmove(w, w + 1, (size_t)(b->wire_count - i - 1) * sizeof(Wire));
            --b->wire_count;
        } else {
            if (w->a.component > index) --w->a.component;
            if (w->b.component > index) --w->b.component;
        }
    }
    memmove(&b->parts[index], &b->parts[index + 1], (size_t)(b->part_count - index - 1) * sizeof(Part));
    --b->part_count;
}

static int root(int *parent, int n)
{
    while (parent[n] != n) n = parent[n];
    return n;
}

static void join(int *parent, int a, int b) { parent[root(parent, a)] = root(parent, b); }

void bench_simulate(const Workbench *b, int led_on[WORKBENCH_MAX_COMPONENTS])
{
    int parent[WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS];
    int i, j;
    memset(led_on, 0, sizeof(int) * WORKBENCH_MAX_COMPONENTS);
    for (i = 0; i < WORKBENCH_MAX_COMPONENTS * WORKBENCH_MAX_PINS; ++i) parent[i] = i;
    for (i = 0; i < b->wire_count; ++i)
        join(parent, b->wires[i].a.component * WORKBENCH_MAX_PINS + b->wires[i].a.pin,
             b->wires[i].b.component * WORKBENCH_MAX_PINS + b->wires[i].b.pin);
    for (i = 0; i < b->part_count; ++i) {
        int base = i * WORKBENCH_MAX_PINS;
        if (b->parts[i].type == PART_SWITCH && b->parts[i].on) join(parent, base, base + 1);
        if (b->parts[i].type == PART_BREADBOARD) {
            int group, hole;
            for (group = 0; group < 20; ++group)
                for (hole = 1; hole < 5; ++hole) join(parent, base + group * 5, base + group * 5 + hole);
        }
    }
    for (i = 0; i < b->part_count; ++i) {
        if (b->parts[i].type != PART_LED) continue;
        for (j = 0; j < b->part_count; ++j) {
            int positive, negative;
            if (b->parts[j].type != PART_SOURCE || !b->parts[j].on) continue;
            positive = root(parent, j * WORKBENCH_MAX_PINS);
            negative = root(parent, j * WORKBENCH_MAX_PINS + 1);
            if (positive != negative && root(parent, i * WORKBENCH_MAX_PINS) == positive && root(parent, i * WORKBENCH_MAX_PINS + 1) == negative)
                led_on[i] = 1;
        }
    }
}
