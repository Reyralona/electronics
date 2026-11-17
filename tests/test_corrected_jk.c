#include "workbench_file.h"
#include "workbench_pack.h"
#include "spice_process.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    for (int bits = 0; bits < 8; ++bits)
    {
        Workbench b = {0};
        assert(workbench_load("blocks/block-1.elec", &b));
        assert(bench_group_ports(&b, b.parts[0].group) == 4);
        int supply = bench_add(&b, PART_SOURCE, -1000, -1000);
        for (int input = 0; input < 3; ++input)
        {
            int source = bench_add(&b, PART_SOURCE, -1300 - input * 200, -1000);
            bench_set_value(&b, source, bits & (1 << input) ? 5 : .001);
            assert(bench_toggle_wire(&b, (Pin){source, 0}, (Pin){2 + input, 0}));
            assert(bench_toggle_wire(&b, (Pin){source, 1}, (Pin){supply, 1}));
        }
        SpiceTrace *trace = calloc(1, sizeof(*trace));
        char message[256];
        assert(trace && spice_transient_analyze(&b, argv[1], (Pin){5,0}, .00001, .001, trace, message, sizeof(message)));
        assert((trace->samples[trace->count - 1].value > 2.5) == (bits == 7));
        free(trace);
        bench_snapshot_clear(&b);
    }
    puts("3AND: all eight input combinations passed");
    for (int bits = 0; bits < 4; ++bits)
    {
        Workbench b = {0};
        assert(workbench_load("blocks/block-4.elec", &b));
        assert(bench_group_ports(&b, b.parts[0].group) == 5);
        int supply = bench_add(&b, PART_SOURCE, -1000, -1000);
        int j = bench_add(&b, PART_SOURCE, -1200, -1000);
        int k = bench_add(&b, PART_SOURCE, -1400, -1000);
        int clk = bench_add(&b, PART_CLOCK, -1600, -1000);
        bench_set_value(&b, supply, 5);
        bench_set_value(&b, j, bits & 2 ? 5 : .001);
        bench_set_value(&b, k, bits & 1 ? 5 : .001);
        bench_set_value(&b, clk, 1000);
        bench_set_on(&b, clk, 1);
        assert(bench_toggle_wire(&b, (Pin){j,0}, (Pin){6,0}));
        assert(bench_toggle_wire(&b, (Pin){k,0}, (Pin){7,0}));
        assert(bench_toggle_wire(&b, (Pin){clk,0}, (Pin){8,0}));
        for (int p = j; p <= clk; ++p)
            assert(bench_toggle_wire(&b, (Pin){p,1}, (Pin){supply,1}));
        SpiceTrace *trace = calloc(1, sizeof(*trace));
        char message[256];
        assert(trace && spice_transient_analyze(&b, argv[1], (Pin){9,0}, .00001, .005, trace, message, sizeof(message)));
        int transitions = 0;
        int previous = trace->samples[0].value > 2.5;
        for (int s = 1; s < trace->count; ++s)
        {
            int high = trace->samples[s].value > 2.5;
            transitions += high != previous;
            previous = high;
        }
        printf("J=%d K=%d: transitions=%d final=%.3f V\n", !!(bits & 2), bits & 1, transitions, trace->samples[trace->count-1].value);
        if (bits == 0 || bits == 1)
            assert(!previous && transitions <= 1);
        if (bits == 2)
            assert(previous && transitions == 1);
        if (bits == 3)
            assert(transitions >= 4 && transitions <= 6);
        free(trace);
        bench_snapshot_clear(&b);
    }
}
