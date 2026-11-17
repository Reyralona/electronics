#include "workbench_pack.h"
#include "workbench_file.h"
#include "workbench_group_name.h"
#include <assert.h>
#include <stdio.h>

static void wire(Workbench *b, int a, int ap, int c, int cp)
{
    assert(bench_toggle_wire(b, (Pin){a, ap}, (Pin){c, cp}) == 1);
}

static int terminal(Workbench *b, const char *name, int target, int pin, float x, float y)
{
    int index = bench_add(b, PART_JUNCTION, x, y);
    assert(index >= 0 && bench_set_port_name(b, (Pin){index, 0}, name));
    wire(b, index, 0, target, pin);
    return index;
}

static void save(Workbench *b, const char *name, const char *path, int inputs)
{
    BenchSelection all = {0};
    char message[256];
    for (int i = 0; i < b->part_count; ++i)
        all.parts[i] = 1;
    int group = bench_pack_selection(b, &all, message, sizeof(message));
    assert(group && bench_group_set_name(b, group, name));
    assert(bench_group_resize(b, group, 320, 120));
    int ports = bench_group_ports(b, group);
    for (int i = 0; i < ports; ++i)
        assert(bench_group_move_port(b, group, i, i < inputs ? 0 : 1, i < inputs ? i : i - inputs));
    Workbench original = {0};
    if (workbench_load(path, &original) && original.part_count && original.parts[0].group)
        b->groups[group - 1].model = original.groups[original.parts[0].group - 1].model;
    assert(workbench_save(path, b));
    bench_snapshot_clear(&original);
    bench_snapshot_clear(b);
}

int main(void)
{
    Workbench b = {0};
    bench_add(&b, PART_AND, 450, 200);
    bench_add(&b, PART_AND, 700, 280);
    wire(&b, 0, 2, 1, 0);
    terminal(&b, "A", 0, 0, 200, 150);
    terminal(&b, "B", 0, 1, 200, 220);
    terminal(&b, "C", 1, 1, 200, 350);
    terminal(&b, "Y", 1, 2, 950, 280);
    save(&b, "3AND", "blocks/block-1.elec", 3);
    /* Rising-edge JK: D = J.!Q + !K.Q, built with the existing edge-triggered DFF. */
    bench_add(&b, PART_NOT, 400, 400); /* !K */
    bench_add(&b, PART_AND, 650, 200); /* J.!Q */
    bench_add(&b, PART_AND, 650, 400); /* !K.Q */
    bench_add(&b, PART_OR, 900, 280);
    bench_add(&b, PART_DFF, 1150, 280);
    bench_add(&b, PART_NOT, 1400, 400); /* !Q */
    wire(&b, 0, 1, 2, 0);
    wire(&b, 4, 2, 2, 1);
    wire(&b, 4, 2, 5, 0);
    wire(&b, 5, 1, 1, 1);
    wire(&b, 1, 2, 3, 0);
    wire(&b, 2, 2, 3, 1);
    wire(&b, 3, 2, 4, 0);
    terminal(&b, "J", 1, 0, 200, 150);
    terminal(&b, "K", 0, 0, 200, 400);
    terminal(&b, "CLOCK", 4, 1, 200, 550);
    terminal(&b, "Q", 4, 2, 1600, 250);
    terminal(&b, "Q_bar", 5, 1, 1600, 450);
    save(&b, "JK Flip Flop", "blocks/block-4.elec", 3);
    puts("3AND and rising-edge JK saved with explicit terminals");
}
